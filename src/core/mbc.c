
/* tinygb - a tiny gameboy emulator
   (c) 2022 by jewel */

#include <tinygb.h>
#include <ioports.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

//#define MBC_LOG

// Memory Bank Controller Implementation

/*

 No MBC:
 - 32 KiB ROM is mapped directly at 0x0000-0x7FFF
 - Writes to this region are ignored

 MBC1: (ROM up to ALMOST 2 MiB and RAM up to 32 KiB)
 - Memory regions:
  - 0xA000-0xBFFF   up to 4 banks of 8 KiB RAM
  - 0x0000-0x1FFF   RAM enable (0x00 = disable, 0x0A in the lower 4 bits = enable)
  - 0x2000-0x3FFF   BANK1: lower 5 bits of ROM bank select; value zero is read as one (i.e. 0x00 and 0x01 both select the same bank)
  - 0x4000-0x5FFF   BANK2: upper 2 bits of ROM bank select OR RAM bank select according to next register
  - 0x6000-0x7FFF   ROM/RAM banking toggle (0 = ROM, 1 = RAM)
   - In mode 0, ROM bank (BANK2 << 5) | BANK1 is available at 0x4000-0x7FFF and RAM bank zero is available at 0xA000-0xBFFF
   - In mode 1, ROM bank (BANK1) is available at 0x4000-0x7FFF and RAM bank (BANK2) is available at 0xA000-0xBFFF

 MBC3: (ROM up to full 2 MiB and RAM up to 32 KiB and real-time clock)
 - Memory regions:
  - 0xA000-0xBFFF   up to 4 banks of 8 KiB RAM or RTC registers
  - 0x0000-0x1FFF   RAM/RTC enable (0x00 = disable, 0x0A in the lower 4 bits = enable)
  - 0x2000-0x3FFF   ROM bank select (full 7 bits, highest bit ignored); value zero is read as one just like MBC1
  - 0x4000-0x5FFF   RAM bank select or RTC register select (0-3 = RAM bank, 0x08-0x0C = RTC register)
  - 0x6000-0x7FFF   latch clock data (writing zero -> one latches the data onto the RTC registers)

 - RTC registers according to RTC register select:
  - 0x08    seconds
  - 0x09    minutes
  - 0x0A    hours
  - 0x0B    lower 8 bits of day counter
  - 0x0C:
        Bit 0   highest bit of day counter
        Bit 6   halt flag (0 = running, 1 = clock stopped)
        Bit 7   day counter carry bit (1 = overflown)

 MBC5: (ROM up to 8 MiB and RAM up to 128 KiB)
 - Memory regions:
  - 0xA000-0xBFFF   up to 16 banks of 8 KiB RAM
  - 0x0000-0x1FFF   RAM enable (0x00 = disable, 0x0A = enable) (*)
  - 0x2000-0x2FFF   ROM bank select (low 8 bits) (**)
  - 0x3000-0x3FFF   ROM bank select (9th bit in bit 0, all other bits ignored)
  - 0x4000-0x5FFF   RAM bank select (low 4 bits, upper 4 bits ignored)

  (*) Unlike MBC1 and MBC3, the RAM enable register in MBC5 is a full 8-bit
      register, and ONLY 0x0A enables RAM, and not just in the low nibble.
  (**) This is the only known MBC that allows ROM bank 0 to appear in the
       0x4000-0x7FFF region of memory by writing zero to the ROM select
       register. The MBC does not increment zeroes, unlike MBC1 and MBC3.

 */

mbc1_t mbc1;
mbc3_t mbc3;
mbc5_t mbc5;

uint8_t *ex_ram;    // pointer to cart RAM
int ex_ram_size;
char *ex_ram_filename;
int ex_ram_modified = 0;

int ex_ram_size_banks;
int rom_size_banks;

void mbc_start(void *cart_ram) {
    ex_ram_filename = calloc(strlen(rom_filename) + 5, 1);
    if(!ex_ram_filename) {
        write_log("[mbc] unable to allocate memory for filename\n");
        die(-1, "");
    }

    strcpy(ex_ram_filename, rom_filename);
    strcpy(ex_ram_filename+strlen(rom_filename), ".mbc");

    ex_ram = (uint8_t *)cart_ram;

    uint8_t *rom_bytes = (uint8_t *)rom;
    switch(rom_bytes[0x149]) {
    case 0:
        ex_ram_size = 0;
        break;
    case 1:
        ex_ram_size = 2048;     // bytes
        break;
    case 2:
        ex_ram_size = 8192;     // bytes
        break;
    case 3:
        ex_ram_size = 32768;
        break;
    case 4:
        ex_ram_size = 131072;   // 128 KiB for MBC5
        break;
    default:
        write_log("[mbc] undefined RAM size value 0x%02X, assuming 128 KiB RAM\n", rom_bytes[0x149]);
        ex_ram_size = 131072;   // biggest possible value to stay on the safest size
    }

    ex_ram_size_banks = ex_ram_size / 8192;
    rom_size_banks = rom_size / 16384;

    switch(mbc_type) {
    case 1:
        mbc1.bank1 = 1;
        mbc1.bank2 = 0;
        mbc1.ram_enable = 0;
        mbc1.mode = 0;
        break;
    case 3:
        mbc3.ram_rtc_bank = 0;
        mbc3.rom_bank = 1;
        mbc3.ram_rtc_enable = 0;
        mbc3.ram_rtc_toggle = 0;    // RAM
        break;
    case 5:
        mbc5.ram_bank = 0;
        mbc5.rom_bank = 1;
        mbc5.ram_enable = 0;
        break;
    default:
        write_log("[mbc] unimplemented MBC type %d\n", mbc_type);
        die(-1, "");
    }

    write_log("[mbc] MBC started with %d KiB of external RAM\n", ex_ram_size/1024);
    if(ex_ram_size) {
        write_log("[mbc] battery-backed RAM will read from and dumped to %s\n", ex_ram_filename);

        // read ram file
        FILE *file = fopen(ex_ram_filename, "r");
        if(!file) {
            write_log("[mbc] unable to open %s for reading, assuming no RAM file\n", ex_ram_filename);
            return;
        }

        if(!fread(ex_ram, 1, ex_ram_size, file)) {
            write_log("[mbc] unable to read from file %s, assuming no RAM file\n", ex_ram_filename);
            memset(ex_ram, 0, ex_ram_size);
            fclose(file);
            return;
        }

        fclose(file);
    }

    write_log("[mbc] ROM size in banks is %d\n", rom_size_banks);
}

void write_ramfile() {
    if(!ex_ram_size || !ex_ram_modified) return;

    remove(ex_ram_filename);
    FILE *file = fopen(ex_ram_filename, "wb");
    if(!file) {
        write_log("[mbc] unable to open %s for writing\n", ex_ram_filename);
        return;
    }

    if(fwrite(ex_ram, 1, ex_ram_size, file) != ex_ram_size) {
        write_log("[mbc] unable to write to file %s\n", ex_ram_filename);
        fclose(file);
        return;
    }

    fflush(file);
    fclose(file);
    write_log("[mbc] wrote RAM file to %s\n", ex_ram_filename);
}

// MBC3 functions here
static inline uint8_t mbc3_read(uint16_t addr) {
    uint8_t *rom_bytes = (uint8_t *)rom;
    if(addr >= 0x4000 && addr <= 0x7FFF) {
        addr -= 0x4000;
        return rom_bytes[(mbc3.rom_bank * 16384) + addr];
    } else if(addr >= 0xA000 && addr <= 0xBFFF) {
        if(!mbc3.ram_rtc_enable) {
            write_log("[mbc] warning: attempt to read from address 0x%04X when external RAM/RTC is disabled, returning ones\n", addr);
            return 0xFF;
        }

        if(mbc3.ram_rtc_bank <= 3) {
            // ram
            return ex_ram[(mbc3.ram_rtc_bank * 8192) + (addr - 0xA000)];
        } else if(mbc3.ram_rtc_bank >= 0x08 && mbc3.ram_rtc_bank <= 0x0C) {
            // rtc
            time_t rawtime;
            struct tm *timeinfo;

            time(&rawtime);
            timeinfo = localtime(&rawtime);

            uint8_t status;

            switch(mbc3.ram_rtc_bank) {
            case 0x08:
                if(timeinfo->tm_sec == 60) return 59;
                else return timeinfo->tm_sec;
            case 0x09:
                return timeinfo->tm_min;
            case 0x0A:
                return timeinfo->tm_hour;
            case 0x0B:
                // lower 8 bits
                return timeinfo->tm_yday & 0xFF;
            case 0x0C:
                status = (timeinfo->tm_yday >> 8) & 1;  // highest bit
                if(mbc3.halt) status |= 0x40;   // halt flag
                return status;
            default:
                write_log("[mbc] undefined read from RTC/RAM bank 0x%02X address 0x%04X, returning ones\n", mbc3.ram_rtc_bank, addr);
                return 0xFF;
            }
        } else {
            // undefined
            write_log("[mbc] undefined read from RTC/RAM bank 0x%02X address 0x%04X, returning ones\n", mbc3.ram_rtc_bank, addr);
            return 0xFF;
        }
    } else {
        write_log("[mbc] unimplemented read at address 0x%04X in MBC%d\n", addr, mbc_type);
        die(-1, NULL);
        return 0xFF;    // unreachable
    }
}

static inline void mbc3_write(uint16_t addr, uint8_t byte) {
    if(addr >= 0x2000 && addr <= 0x3FFF) {
        byte &= 0x7F;
        if(!byte) byte = 1;

        #ifdef MBC_LOG
        write_log("[mbc] selecting ROM bank %d\n", byte);
        #endif

        mbc3.rom_bank = byte;
    } else if(addr >= 0x4000 && addr <= 0x5FFF) {
        byte &= 0x0F;

        #ifdef MBC_LOG
        if(byte <= 3) {
            write_log("[mbc] selecting RAM bank %d\n", byte);
        } else if(byte >= 0x08 && byte <= 0x0C) {
            write_log("[mbc] selecting RTC register 0x%02X\n", byte);
        } else {
            write_log("[mbc] selecting undefined RAM/RTC register %d, ignoring...\n", byte);
        }
        #endif

        mbc3.ram_rtc_bank = byte;
    } else if(addr >= 0x0000 && addr <= 0x1FFF) {
        byte &= 0x0F;
        if(byte == 0x0A) {
            mbc3.ram_rtc_enable = 1;
            ex_ram_modified = 0;
            #ifdef MBC_LOG
            write_log("[mbc] enabled access to external RAM and RTC\n");
            #endif
        } else {
            mbc3.ram_rtc_enable = 0;
            #ifdef MBC_LOG
            write_log("[mbc] disabled access to external RAM and RTC\n");
            #endif

            // dump the ram file here
            write_ramfile();
        }
    } else if(addr >= 0xA000 && addr <= 0xBFFF) {
        if(!mbc3.ram_rtc_enable) {
            write_log("[mbc] warning: attempt to write to address 0x%04X value 0x%02X when external RAM/RTC is disabled\n", addr, byte);
            return;
        }

        if(mbc3.ram_rtc_bank <= 3) {
            // ram
            ex_ram[(mbc3.ram_rtc_bank * 8192) + (addr - 0xA000)] = byte;
            ex_ram_modified = 1;
        } else {
            // rtc
            //write_log("[mbc] TODO: implement writing to RTC registers (register 0x%02X value 0x%02X)\n", mbc3.ram_rtc_bank, byte);
            return;     // ignore for now
        }
    } else if(addr >= 0x6000 && addr <= 0x7FFF) {
        mbc3.old_latch_data = mbc3.latch_data;
        mbc3.latch_data = byte;
    } else {
        write_log("[mbc] unimplemented write at address 0x%04X value 0x%02X in MBC%d\n", addr, byte, mbc_type);
        die(-1, NULL);
    }
}

// MBC1 functions here
static inline void mbc1_write(uint16_t addr, uint8_t byte) {
    if(addr >= 0x2000 && addr <= 0x3FFF) {
        byte &= 0x1F;   // lower 5 bits
        mbc1.bank1 = byte;
    } else if(addr >= 0x4000 && addr <= 0x5FFF) {
        byte &= 3;      // 2 bits
        mbc1.bank2 = byte;
    } else if(addr >= 0x6000 && addr <= 0x7FFF) {
        byte &= 1;      // one bit
        mbc1.mode = byte;
    } else if(addr >= 0x0000 && addr <= 0x1FFF) {
        byte &= 0x0F;
        if(byte == 0x0A) {
            mbc1.ram_enable = 1;
            ex_ram_modified = 0;
            #ifdef MBC_LOG
            write_log("[mbc] enabled access to external RAM\n");
            #endif
        } else {
            mbc1.ram_enable = 0;
            #ifdef MBC_LOG
            write_log("[mbc] disabled access to external RAM\n");
            #endif

            write_ramfile();
        }
    } else if(addr >= 0xA000 && addr <= 0xBFFF) {
        // ram
        if(!mbc1.ram_enable) {
            write_log("[mbc] warning: attempt to write to address 0x%04X value 0x%02X when external RAM is disabled\n", addr, byte);
            return;
        }

        int ram_bank;
        if(mbc1.mode) ram_bank = mbc1.bank2 & 3;
        else ram_bank = 0;

        ex_ram[(ram_bank * 8192) + (addr - 0xA000)] = byte;
        ex_ram_modified = 1;
    } else {
        write_log("[mbc] unimplemented write at address 0x%04X value 0x%02X in MBC%d\n", addr, byte, mbc_type);
        die(-1, NULL);
    }
}

static inline uint8_t mbc1_read(uint16_t addr) {
    int rom_bank, ram_bank;
    uint8_t *rom_bytes = (uint8_t *)rom;

    if(addr >= 0x0000 && addr <= 0x3FFF) {
        /*if(mbc1.mode) {
            rom_bank = mbc1.bank2 << 5;
        } else {
            rom_bank = 0;
        }*/

        rom_bank = 0;

        return rom_bytes[(rom_bank * 16384) + addr];
    } else if(addr >= 0x4000 && addr <= 0x7FFF) {
        //rom_bank = (mbc1.bank2 << 5) | mbc1.bank1;

        if(mbc1.mode) {
            rom_bank = mbc1.bank1 & 0x1F;
        } else {
            rom_bank = ((mbc1.bank2 << 5) & 3) | (mbc1.bank1 & 0x1F);
        }

        //rom_bank &= 

        if(rom_bank) {
            rom_bank &= (rom_size_banks-1);
        } else {
            rom_bank++;
        }

        //rom_bank &= (rom_size_banks-1);
        //if(!rom_bank) rom_bank++;

        addr -= 0x4000;
        return rom_bytes[(rom_bank * 16384) + addr];
    } else if(addr >= 0xA000 && addr <= 0xBFFF) {
        if(!mbc1.ram_enable) {
            write_log("[mbc] warning: attempt to read from address 0x%04X when external RAM is disabled, returning ones\n", addr);
            return 0xFF;
        }

        if(mbc1.mode) ram_bank = mbc1.bank2 & 3;
        else ram_bank = 0;

        return ex_ram[(ram_bank * 8192) + (addr - 0xA000)];
    } else {
        write_log("[mbc] unimplemented read at address 0x%04X in MBC%d\n", addr, mbc_type);
        die(-1, NULL);
        return 0xFF;
    }
}


// MBC5 functions here
static inline void mbc5_write(uint16_t addr, uint8_t byte) {
    if(addr >= 0x2000 && addr <= 0x2FFF) {
        mbc5.rom_bank &= 0x100;
        mbc5.rom_bank |= byte;      // low 8 bits of ROM bank select

        #ifdef MBC_LOG
        write_log("[mbc] selecting ROM bank %d\n", mbc5.rom_bank);
        #endif
    } else if(addr >= 0x3000 && addr <= 0x3FFF) {
        mbc5.rom_bank &= 0xFF;
        mbc5.rom_bank |= ((byte & 1) << 8);   // high bit of ROM bank select

        #ifdef MBC_LOG
        write_log("[mbc] selecting ROM bank %d\n", mbc5.rom_bank);
        #endif
    } else if(addr >= 0x4000 && addr <= 0x5FFF) {
        byte &= 0x0F;
        mbc5.ram_bank = byte;

        #ifdef MBC_LOG
        write_log("[mbc] selecting RAM bank %d\n", byte);
        #endif
    } else if(addr >= 0x0000 && addr <= 0x1FFF) {
        if(byte == 0x0A) {
            mbc5.ram_enable = 1;
            ex_ram_modified = 0;
            #ifdef MBC_LOG
            write_log("[mbc] enabled access to external RAM\n");
            #endif
        } else {
            mbc5.ram_enable = 0;
            #ifdef MBC_LOG
            write_log("[mbc] disabled access to external RAM\n");
            #endif

            write_ramfile();
        }
    } else if(addr >= 0xA000 && addr <= 0xBFFF) {
        if(!mbc5.ram_enable) {
            write_log("[mbc] warning: attempt to write to address 0x%04X value 0x%02X when external RAM is disabled\n", addr, byte);
            return;
        }

        ex_ram[(mbc5.ram_bank * 8192) + (addr - 0xA000)] = byte;
        ex_ram_modified = 1;
    } else if(addr <= 0x6000 && addr <= 0x7FFF) {
        // i can't find any info on what this does but apparently pokemon yellow does this?
        write_log("[mbc] warning: undefined write at address 0x%04X value 0x%02X in MBC5, ignoring\n", addr, byte);
        return;
    } else {
        write_log("[mbc] unimplemented write at address 0x%04X value 0x%02X in MBC%d\n", addr, byte, mbc_type);
        die(-1, NULL);
    }
}

static inline uint8_t mbc5_read(uint16_t addr) {
    uint8_t *rom_bytes = (uint8_t *)rom;
    if(addr >= 0x4000 && addr <= 0x7FFF) {
        addr -= 0x4000;
        return rom_bytes[((mbc5.rom_bank & (rom_size_banks-1)) * 16384) + addr];
    } else if(addr >= 0xA000 && addr <= 0xBFFF) {
        if(!mbc5.ram_enable) {
            write_log("[mbc] warning: attempt to read from address 0x%04X when external RAM is disabled, returning ones\n", addr);
            return 0xFF;
        }

        return ex_ram[(mbc5.ram_bank * 8192) + (addr - 0xA000)];
    } else {
        write_log("[mbc] unimplemented read at address 0x%04X in MBC%d\n", addr, mbc_type);
        die(-1, NULL);
        return 0xFF;
    }
}

// general fucntions called from memory.c
uint8_t mbc_read(uint16_t addr) {
    switch(mbc_type) {
    case 1:
        return mbc1_read(addr);
    case 3:
        return mbc3_read(addr);
    case 5:
        return mbc5_read(addr);
    default:
        write_log("[mbc] unimplemented read at address 0x%04X in MBC%d\n", addr, mbc_type);
        die(-1, NULL);
        return 0xFF;    // unreachable
    }
}

void mbc_write(uint16_t addr, uint8_t byte) {
    switch(mbc_type) {
    case 0:
        write_log("[mbc] undefined write to read-only region 0x%04X value 0x%02X in MBC%d, ignoring...\n", addr, byte, mbc_type);
        return;
    case 1:
        return mbc1_write(addr, byte);
    case 3:
        return mbc3_write(addr, byte);
    case 5:
        return mbc5_write(addr, byte);
    default:
        write_log("[mbc] unimplemented write at address 0x%04X value 0x%02X in MBC%d\n", addr, byte, mbc_type);
        die(-1, NULL);
    }
}
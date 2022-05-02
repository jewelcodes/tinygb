
/* tinygb - a tiny gameboy emulator
   (c) 2022 by jewel */

#include <tinygb.h>
#include <ioports.h>

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
  - 0x2000-0x3FFF   lower 5 bits of ROM bank select; value zero is read as one (i.e. 0x00 and 0x01 both select the same bank)
  - 0x4000-0x5FFF   upper 2 bits of ROM bank select OR RAM bank select according to next register
  - 0x6000-0x7FFF   ROM/RAM banking toggle (0 = ROM, 1 = RAM)
  - (only RAM bank 0 can be used in ROM mode, and only ROM banks 0x00-0x1F can be used in mode 1)

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

 */

mbc1_t mbc1;
mbc3_t mbc3;

uint8_t *ex_ram;    // pointer to cart RAM

void mbc_start(void *cart_ram) {
    ex_ram = (uint8_t *)cart_ram;

    switch(mbc_type) {
    case 3:
        mbc3.ram_rtc_bank = 0;
        mbc3.rom_bank = 1;
        mbc3.ram_rtc_enable = 1;
        mbc3.ram_rtc_toggle = 0;    // RAM
        break;
    default:
        write_log("[mbc] unimplemented MBC type %d\n", mbc_type);
        die(-1, "");
    }

    write_log("[mbc] MBC started\n");
}

uint8_t mbc_read(uint16_t addr) {
    uint8_t *rom_bytes = (uint8_t *)rom;
    switch(mbc_type) {
    case 3:
        if(addr >= 0x4000 && addr <= 0x7FFF) {
            addr -= 0x4000;
            return rom_bytes[(mbc3.rom_bank * 16384) + addr];
        } else {
            write_log("[mbc] unimplemented read at address 0x%04X in MBC%d\n", addr, mbc_type);
            die(-1, NULL);
            return 0xFF;    // unreachable
        }
        break;
    default:
        write_log("[mbc] unimplemented read at address 0x%04X in MBC%d\n", addr, mbc_type);
        die(-1, NULL);
        return 0xFF;    // unreachable
    }
}

void mbc_write(uint16_t addr, uint8_t byte) {
    switch(mbc_type) {
    case 3:
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
            if(byte < 3) {
                write_log("[mbc] selecting RAM bank %d\n", byte);
            } else if(byte >= 0x08 && byte <= 0x0C) {
                write_log("[mbc] selecting RTC register 0x%02X\n", byte);
            } else {
                write_log("[mbc] selecting undefined RAM/RTC register %d, ignoring...\n", byte);
            }
            #endif

            mbc3.ram_rtc_bank = byte;
        } else {
            write_log("[mbc] unimplemented write at address 0x%04X value 0x%02X in MBC%d\n", addr, byte, mbc_type);
            die(-1, NULL);
        }
        break;
    default:
        write_log("[mbc] unimplemented write at address 0x%04X value 0x%02X in MBC%d\n", addr, byte, mbc_type);
        die(-1, NULL);
    }
}
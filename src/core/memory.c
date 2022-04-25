
/* tinygb - a tiny gameboy emulator
   (c) 2022 by jewel */

#include <tinygb.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ioports.h>

#define MEMORY_LOG

/*
                          *** MEMORY MAP *** 

  0000-3FFF   16KB ROM Bank 00     (in cartridge, fixed at bank 00)
  4000-7FFF   16KB ROM Bank 01..NN (in cartridge, switchable bank number)
  8000-9FFF   8KB Video RAM (VRAM) (switchable bank 0-1 in CGB Mode)
  A000-BFFF   8KB External RAM     (in cartridge, switchable bank, if any)
  C000-CFFF   4KB Work RAM Bank 0 (WRAM)
  D000-DFFF   4KB Work RAM Bank 1 (WRAM)  (switchable bank 1-7 in CGB Mode)
  E000-FDFF   Same as C000-DDFF (ECHO)    (typically not used)
  FE00-FE9F   Sprite Attribute Table (OAM)
  FEA0-FEFF   Not Usable
  FF00-FF7F   I/O Ports
  FF80-FFFE   High RAM (HRAM)
  FFFF        Interrupt Enable Register

 */

void *ram = NULL, *rom = NULL;
char game_title[17];

uint8_t *cartridge_type, *cgb_compatibility;

int mbc_type;

#define WORK_RAM            (0)     // +0
#define HIGH_RAM            (32768) // +32k assuming work RAM has a maximum of 8x4k banks
#define CART_RAM            (HIGH_RAM+128)  // +128 because HRAM is exactly 127 bytes long but add one byte for alignment
#define OAM                 (CART_RAM+1048576)  // +1 MB

int rom_bank = 1;
int cart_ram_bank = 0;
int work_ram_bank = 1;
int is_cgb = 0;

void memory_start() {
    // rom was already initialized in main.c
    ram = calloc(1024, 1058);   // 1 MB is the maximum RAM size in MBC5 + 33 KB for WRAM + HRAM
    if(!ram) {
        die(1, "unable to allocate RAM\n");
    }

    // copy the game's title
    memset(game_title, 0, 17);
    memcpy(game_title, rom+0x134, 16);
    write_log("game title is %s\n", game_title);

    cgb_compatibility = (uint8_t *)rom + 0x143;
    if(*cgb_compatibility == 0x80) {
        write_log("game supports both CGB and original GB\n");
        is_cgb = 1;
    } else if(*cgb_compatibility == 0xC0) {
        write_log("game only works on CGB\n");
        is_cgb = 1;
    } else if(!*cgb_compatibility) {
        write_log("game doesn't support CGB\n");
        is_cgb = 0;
    } else {
        die(-1, "undefined CGB compatibility value 0x%02X\n", *cgb_compatibility);
    }

    cartridge_type = (uint8_t *)rom + 0x147;

    switch(*cartridge_type) {
    case 0x00:
        mbc_type = 0;
        break;
    case 0x01:
    case 0x02:
    case 0x03:
        mbc_type = 1;
        break;
    case 0x05:
    case 0x06:
        mbc_type = 2;
        break;
    case 0x0F:
    case 0x10:
    case 0x11:
    case 0x12:
    case 0x13:
        mbc_type = 3;
        break;
    case 0x15:
    case 0x16:
    case 0x17:
        mbc_type = 4;
        break;
    case 0x19:
    case 0x1A:
    case 0x1B:
    case 0x1C:
    case 0x1D:
    case 0x1E:
        mbc_type = 5;
        break;
    default:
        die(-1, "[mbc] cartridge type is 0x%02X: unimplemented\n", *cartridge_type);
        break;
    }

    if(!mbc_type) {
        write_log("[mbc] cartridge type is 0x%02X: no MBC\n", *cartridge_type);
    } else {
        write_log("[mbc] cartridge type is 0x%02X: MBC%d\n", *cartridge_type, mbc_type);
    }
}

inline uint8_t read_wram(int bank, uint16_t addr) {
    uint8_t *bytes = (uint8_t *)ram;
    return bytes[(bank * 4096) + addr + WORK_RAM];
}

inline uint8_t read_hram(uint16_t addr) {
    uint8_t *bytes = (uint8_t *)ram;
    return bytes[addr + HIGH_RAM];
}

uint8_t read_io(uint16_t addr) {
    switch(addr) {
    case IE:
        return ie_read();
    case LCDC:
    case STAT:
    case SCY:
    case SCX:
    case LY:
    case LYC:
    case DMA:
    case BGP:
    case OBP0:
    case OBP1:
    case WX:
    case WY:
    case VBK:
    case HDMA1:
    case HDMA2:
    case HDMA3:
    case HDMA4:
    case HDMA5:
        return read_display_io(addr);
    case P1:
        return joypad_read(addr);
    default:
        write_log("[memory] unimplemented read from IO port 0x%04X\n", addr);
        die(-1, NULL);
    }

    return 0xFF;    // unreachable
}

uint8_t read_byte(uint16_t addr) {
    uint8_t *rom_bytes = (uint8_t *)rom;
    if(!mbc_type && addr <= 0x7FFF) {
        return rom_bytes[addr];
    } else if(addr <= 0x3FFF) {
        return rom_bytes[addr];
    } else if(addr >= 0xC000 && addr <= 0xCFFF) {
        return read_wram(0, addr - 0xC000);
    } else if(addr >= 0xD000 && addr <= 0xDFFF) {
        return read_wram(work_ram_bank, addr - 0xD000);
    } else if(addr >= 0xE000 && addr <= 0xEFFF) {
        return read_wram(0, addr - 0xE000); // echo bank 0
    } else if(addr >= 0xF000 && addr <= 0xFDFF) {
        return read_wram(work_ram_bank, addr - 0xF000); // echo bank n
    } else if(addr >= 0xFF80 && addr <= 0xFFFE) {
        return read_hram(addr - 0xFF80);
    } else if(addr >= 0xFF00 && addr <= 0xFF7F) {
        return read_io(addr);
    } else if(addr == 0xFFFF) {
        return ie_read();
    }

    write_log("[memory] unimplemented read at address 0x%04X in MBC%d ROM\n", addr, mbc_type);
    die(-1, NULL);
    return 0xFF;    // unreachable anyway
}

inline uint16_t read_word(uint16_t addr) {
    return (uint16_t)(read_byte(addr) | ((uint16_t)read_byte(addr+1) << 8));
}

inline void write_wram(int bank, uint16_t addr, uint8_t byte) {
    uint8_t *bytes = (uint8_t *)ram;
    bytes[(bank * 4096) + addr + WORK_RAM] = byte;
}

inline void write_hram(uint16_t addr, uint8_t byte) {
    uint8_t *bytes = (uint8_t *)ram;
    bytes[addr + HIGH_RAM] = byte;
}

void write_io(uint16_t addr, uint8_t byte) {
    switch(addr) {
    case IF:
        return if_write(byte);
    case LCDC:
    case STAT:
    case SCY:
    case SCX:
    case LY:
    case LYC:
    case DMA:
    case BGP:
    case OBP0:
    case OBP1:
    case WX:
    case WY:
    case VBK:
    case HDMA1:
    case HDMA2:
    case HDMA3:
    case HDMA4:
    case HDMA5:
        return write_display_io(addr, byte);
    case SB:
        return sb_write(byte);
    case SC:
        return sc_write(byte);
    case DIV:
    case TIMA:
    case TMA:
    case TAC:
        return timer_write(addr, byte);
    case NR10:
    case NR11:
    case NR12:
    case NR13:
    case NR14:
    case NR21:
    case NR22:
    case NR23:
    case NR24:
    case NR30:
    case NR31:
    case NR32:
    case NR33:
    case NR34:
    case NR41:
    case NR42:
    case NR43:
    case NR44:
    case NR50:
    case NR51:
    case NR52:
    case WAV00:
    case WAV01:
    case WAV02:
    case WAV03:
    case WAV04:
    case WAV05:
    case WAV06:
    case WAV07:
    case WAV08:
    case WAV09:
    case WAV10:
    case WAV11:
    case WAV12:
    case WAV13:
    case WAV14:
    case WAV15:
        return sound_write(addr, byte);
    case P1:
        return joypad_write(addr, byte);
    default:
        write_log("[memory] unimplemented write to I/O port 0x%04X value 0x%02X\n", addr, byte);
        return;
    }
}

inline void write_oam(uint16_t addr, uint8_t byte) {
    uint8_t *bytes = (uint8_t *)ram;
    bytes[OAM + addr] = byte;
}

void write_byte(uint16_t addr, uint8_t byte) {
/*#ifdef MEMORY_LOG
    write_log("[memory] write 0x%02X to 0x%04X\n", byte, addr);
#endif*/

    if(addr >= 0xC000 && addr <= 0xCFFF) {
        return write_wram(0, addr - 0xC000, byte);
    } else if(addr >= 0xD000 && addr <= 0xDFFF) {
        return write_wram(work_ram_bank, addr - 0xD000, byte);
    } else if(addr >= 0xE000 && addr <= 0xEFFF) {
        return write_wram(0, addr - 0xE000, byte); // echo bank 0
    } else if(addr >= 0xF000 && addr <= 0xFDFF) {
        return write_wram(work_ram_bank, addr - 0xF000, byte); // echo bank n
    } else if(addr >= 0xFF80 && addr <= 0xFFFE) {
        return write_hram(addr - 0xFF80, byte);
    } else if(addr <= 0x7FFF) {
        switch(mbc_type) {
        case 0:
            write_log("[memory] undefined write at address 0x%04X value 0x%02X in a ROM without an MBC, ignoring...\n", addr, byte);
            return;
        default:
            write_log("[memory] unimplemented write at address 0x%04X value 0x%02X in MBC%d ROM\n", addr, byte, mbc_type);
            die(-1, NULL);
            break;
        }
    } else if (addr >= 0xFF00 && addr <= 0xFF7F) {
        return write_io(addr, byte);
    } else if(addr == 0xFFFF) {
        return ie_write(byte);
    } else if(addr >= 0x8000 && addr <= 0x9FFF) {
        return vram_write(addr, byte);
    } else if(addr >= 0xFEA0 && addr <= 0xFEFF) {
        //write_log("[memory] undefined write at unusable address 0x%04X value 0x%02X, ignoring...\n", addr, byte);
        return;
    } else if(addr >= 0xFE00 && addr <= 0xFE9F) {
        return write_oam(addr - 0xFE00, byte);        
    }

    write_log("[memory] unimplemented write at address 0x%04X value 0x%02X in MBC%d ROM\n", addr, byte, mbc_type);
    die(-1, NULL);
}

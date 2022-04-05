
/* tinygb - a tiny gameboy emulator
   (c) 2022 by jewel */

#include <tinygb.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define MEMORY_LOG

void *ram = NULL, *rom = NULL;
char game_title[17];

uint8_t *cartridge_type;

int mbc_type;

void memory_start() {
    // rom was already initialized in main.c
    ram = calloc(1024, 1024);   // 1 MB is the maximum RAM size in MBC5
    if(!ram) {
        die(1, "unable to allocate RAM\n");
    }

    // copy the game's title
    memset(game_title, 0, 17);
    memcpy(game_title, rom+0x134, 16);
    write_log("game title is %s\n", game_title);

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
        die(-1, "cartridge type is 0x%02X: unimplemented\n", *cartridge_type);
        break;
    }

    if(!mbc_type) {
        write_log("cartridge type is 0x%02X: no MBC\n", *cartridge_type);
    } else {
        write_log("cartridge type is 0x%02X: MBC%d\n", *cartridge_type, mbc_type);
    }
}

uint8_t read_byte(uint16_t addr) {
    uint8_t *rom_bytes = (uint8_t *)rom;
    if(!mbc_type && addr <= 0x7FFF) {
        return rom_bytes[addr];
    } else if (addr <= 0x7FFF) {
        return rom_bytes[addr];
    }
    return 0;
}

inline uint16_t read_word(uint16_t addr) {
    return (uint16_t)(read_byte(addr) | ((uint16_t)read_byte(addr+1) << 8));
}

void write_byte(uint16_t addr, uint8_t byte) {
#ifdef MEMORY_LOG
    write_log("[memory] write 0x%02X to 0x%04X\n", byte, addr);
#endif

    if(addr <= 0x7FFF) {
        switch(mbc_type) {
        case 0:
            write_log("[memory] undefined write at address 0x%04X in a ROM without an MBC, ignoring...\n", addr);
            return;
        default:
            write_log("[memory] unimplemented write at address 0x%04X in MBC%d ROM\n", addr, mbc_type);
            die(-1, NULL);
            break;
        }
    }

    die(-1, "unimplemented memory write\n");
}

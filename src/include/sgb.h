
/* tinygb - a tiny gameboy emulator
   (c) 2022 by jewel */

#pragma once

#include <stdint.h>

// SGB Commands
#define SGB_PAL01       0x00        // these set palettes
#define SGB_PAL23       0x01
#define SGB_PAL03       0x02
#define SGB_PAL12       0x03
#define SGB_ATTR_BLK    0x04
#define SGB_ATTR_LIN    0x05
#define SGB_ATTR_DIV    0x06
#define SGB_ATTR_CHR    0x07
#define SGB_SOUND       0x08
#define SGB_SOU_TRN     0x09
#define SGB_PAL_SET     0x0A
#define SGB_PAL_TRN     0x0B
#define SGB_ATRC_EN     0x0C
#define SGB_TEST_EN     0x0D
#define SGB_ICON_EN     0x0E
#define SGB_DATA_SND    0x0F        // transfer SNES WRAM
#define SGB_DATA_TRN    0x10
#define SGB_MLT_REQ     0x11        // used to detect SGB functions
#define SGB_JUMP        0x12
#define SGB_CHR_TRN     0x13
#define SGB_PCT_TRN     0x14
#define SGB_ATTR_TRN    0x15
#define SGB_ATTR_SET    0x16
#define SGB_MASK_EN     0x17
#define SGB_OBJ_TRN     0x18

typedef struct {
    int stopped;
    uint8_t command_length;     // length in lower 3 bits, 1-7, in number of packets; command in higher 5 bits
    uint8_t data[111];          // maximum data that can be transferred
} sgb_command_t;

typedef struct {
    uint32_t colors[4];
} sgb_palette_t;

typedef struct {
    uint32_t colors[16];
} sgb_border_palette_t;

typedef struct {
    int inside, outside, surrounding;   // what the fuck does surrounding even mean bruh
    int palette_inside, palette_outside, palette_surrounding;
    int x1, y1, x2, y2;
} sgb_attr_block_t;

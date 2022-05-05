
/* tinygb - a tiny gameboy emulator
   (c) 2022 by jewel */

#include <tinygb.h>
#include <ioports.h>
#include <string.h>

// Super Gameboy implementation

#define SGB_LOG

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

int sgb_transferring = 0;   // interfering with writes to 0xFF00
int sgb_interfere = 0;      // interfering with reads from 0xFF00
int sgb_current_bit = 0;
sgb_command_t sgb_command;
sgb_palette_t sgb_palettes[4];

int sgb_screen_mask = 0;

uint8_t sgb_current_joypad = 0x0F;      // 0x0C-0x0F
uint8_t sgb_joypad_return;

uint8_t *sgb_palette_data;

void sgb_start() {
    sgb_palette_data = calloc(1, 4096);
    if(!sgb_palette_data) {
        write_log("[sgb] unable to allocate memory\n");
        die(-1, "");
    }
}

inline uint32_t truecolor(uint16_t color16) {
    uint32_t color32;
    int r, g, b; 

    r = color16 & 31;
    g = (color16 >> 5) & 31;
    b = (color16 >> 10) & 31;

    r <<= 3;    // x8
    g <<= 3;
    b <<= 3;

    color32 = (r << 16) | (g << 8) | b;
    return color32;
}

void create_sgb_palette(int sgb_palette, int system_palette) {
    uint16_t *data = (uint16_t *)(sgb_palette_data + (system_palette * 8));

    sgb_palettes[sgb_palette].colors[0] = truecolor(data[0]);
    sgb_palettes[sgb_palette].colors[1] = truecolor(data[1]);
    sgb_palettes[sgb_palette].colors[2] = truecolor(data[2]);
    sgb_palettes[sgb_palette].colors[3] = truecolor(data[3]);
}

void handle_sgb_command() {
    uint8_t command;
    command = sgb_command.command_length >> 3;

    switch(command) {
    case SGB_MLT_REQ:
#ifdef SGB_LOG
        write_log("[sgb] handling command 0x%02X: MLT_REQ\n", command);
#endif
        if(sgb_command.data[0] & 0x01) {
            write_log("[sgb] MLT_REQ: enabled multiplayer joypads\n");
            sgb_current_joypad = 0x0C;
            sgb_interfere = 1;
        } else {
            write_log("[sgb] MLT_REQ: disabled multiplayer joypads\n");
            sgb_interfere = 0;
        }
        break;
    case SGB_MASK_EN:
#ifdef SGB_LOG
        write_log("[sgb] handling command 0x%02X: MASK_EN\n", command);
#endif

        sgb_command.data[0] %= 3;
        sgb_screen_mask = sgb_command.data[0];

        if(sgb_command.data[0] == 0) {
            write_log("[sgb] MASK_EN: cancelling screen mask\n");
        } else if(sgb_command.data[0] == 1) {
            write_log("[sgb] MASK_EN: freezing current screen\n");
        } else if(sgb_command.data[0] == 2) {
            write_log("[sgb] MASK_EN: freezing screen at black\n");
        } else {
            write_log("[sgb] MASK_EN: freezing screen at color zero\n");
        }
        break;
    case SGB_PAL_TRN:
#ifdef SGB_LOG
        write_log("[sgb] handling command 0x%02X: PAL_TRN\n", command);
#endif

        write_log("[sgb] PAL_TRN: transferring 4 KiB of palette data from VRAM 0x8000-0x8FFF to SNES\n");

        for(int i = 0; i < 4096; i++) {
            sgb_palette_data[i] = read_byte(0x8000+i);
        }
        break;
    case SGB_PAL_SET:
#ifdef SGB_LOG
        write_log("[sgb] handling command 0x%02X: PAL_SET\n", command);
#endif

        uint16_t *palette_numbers = (uint16_t *)((void *)&sgb_command+1);

        for(int i = 0; i < 4; i++) {
            write_log("[sgb] PAL_SET: palette %d -> system palette %d\n", i, palette_numbers[i]);

            create_sgb_palette(i, palette_numbers[i]);
        }
        break;

    case SGB_ATTR_BLK:
#ifdef SGB_LOG
        write_log("[sgb] handling command 0x%02X: ATTR_BLK\n", command);
#endif

        write_log("[sgb] ATTR_BLK: setting attributes with %d datasets\n", sgb_command.data[0]);
        die(-1, "");
        break;
    default:
        write_log("[sgb] unhandled command 0x%02X, ignoring...\n", command);
        return;
    }
}

void sgb_write(uint8_t byte) {
    uint8_t p14 = (byte >> 4) & 1;
    uint8_t p15 = (byte >> 5) & 1;

    if(!sgb_transferring && !p14 && !p15) {
        // reset signal
        sgb_transferring = 1;
        sgb_current_bit = 0;
        memset(&sgb_command, 0, sizeof(sgb_command_t));
    }

    if(!sgb_transferring && sgb_interfere) {
        // here the program is trying to read SGB state
        if(p14 && p15) {
            // both ones, return current joypad
            sgb_joypad_return = sgb_current_joypad;
            sgb_current_joypad--;
            if(sgb_current_joypad < 0x0C) sgb_current_joypad = 0x0F;    // wrap
        } else if(!p14 && p15) {
            // p14 = 0; p15 = 1; read directions
            if(sgb_joypad_return == 0x0F) sgb_joypad_return = (~(pressed_keys >> 4)) & 0x0F;
            else sgb_joypad_return = 0x0F;
        } else if(p14 && !p15) {
            // p14 = 1; p15 = 0; read buttons
            if(sgb_joypad_return == 0x0F) sgb_joypad_return = (~pressed_keys) & 0x0F;
            else sgb_joypad_return = 0x0F;
        } else {
            write_log("[sgb] unhandled unreachable code\n");
            die(-1, "");
        }

        return;
    }

    if(p14 == p15) {
        // if both zero, it's a reset pulse, ignore
        // likewise if both 1, it's a "wait" pulse, also ignore
        return;
    }

    // here we know they're different, so keep going
    if(!p15) {
        // a one bit is being transferred
        int byte_number = sgb_current_bit / 8;
        int bit_number = sgb_current_bit % 8;

        if(!byte_number) {
            // command/length byte
            sgb_command.command_length |= (1 << bit_number);
        } else {
            // any other byte
            sgb_command.data[byte_number-1] |= (1 << bit_number);
        }
    }

    sgb_current_bit++;

    if(sgb_current_bit >= 128) {
        int final_length = (sgb_command.command_length & 7) * 16 * 8;   // in bits
        if(sgb_current_bit >= final_length) {
            sgb_transferring = 0;
            handle_sgb_command();
            return;
        }
    }
}

uint8_t sgb_read() {
    return sgb_joypad_return;
}
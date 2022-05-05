
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

int sgb_screen_mask = 0;

uint8_t sgb_current_joypad = 0x0F;      // 0x0C-0x0F
uint8_t sgb_joypad_return;

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
        } else if(sgb_command.data[1] == 1) {
            write_log("[sgb] MASK_EN: freezing current screen\n");
        } else if(sgb_command.data[2] == 2) {
            write_log("[sgb] MASK_EN: freezing screen at black\n");
        } else {
            write_log("[sgb] MASK_EN: freezing screen at color zero\n");
        }
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
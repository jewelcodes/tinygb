
/* tinygb - a tiny gameboy emulator
   (c) 2022 by jewel */

#include <tinygb.h>
#include <ioports.h>
#include <string.h>

// Super Gameboy implementation

#define SGB_LOG

#define SGB_COMMAND_MLT_REQ     0x11    // used to detect SGB functions

int sgb_active = 0;         // interfering with writes to 0xFF00
int sgb_interfere = 0;      // interfering with reads from 0xFF00
int sgb_current_bit = 0;
sgb_command_t sgb_command;

void handle_sgb_command() {
    uint8_t command;
    command = sgb_command.command_length >> 3;

#ifdef SGB_LOG
    write_log("[sgb] handling command 0x%02X\n", command);
#endif
}

void sgb_write(uint8_t byte) {
    if(!sgb_active) {
        // reset signal
        sgb_active = 1;
        sgb_current_bit = 0;
        memset(&sgb_command, 0, sizeof(sgb_command_t));
    }

    uint8_t p14 = (byte >> 4) & 1;
    uint8_t p15 = (byte >> 5) & 1;

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
            sgb_active = 0;
            handle_sgb_command();
            return;
        }
    }
}

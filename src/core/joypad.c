
/* tinygb - a tiny gameboy emulator
   (c) 2022 by jewel */

#include <tinygb.h>
#include <ioports.h>

uint8_t pressed_keys = 0;   // high 4 bits directions, low 4 bits buttons

int selection = 0;  // 0 = buttons, 1 = directions

#define BUTTON_A        0x01
#define BUTTON_B        0x02
#define BUTTON_SELECT   0x04
#define BUTTON_START    0x08
#define BUTTON_RIGHT    0x10
#define BUTTON_LEFT     0x20
#define BUTTON_UP       0x40
#define BUTTON_DOWN     0x80

uint8_t joypad_read(uint16_t addr) {
    if(is_sgb && sgb_interfere) return sgb_read();
    uint8_t val;

    if(selection == 1) {
        // directions
        val = (~(pressed_keys >> 4)) & 0x0F;
        //write_log("[joypad] directions return value 0x%02X\n", val);
    } else if(selection == 0) {
        // buttons
        val = (~pressed_keys) & 0x0F;
        //write_log("[joypad] buttons return value 0x%02X\n", val);
    } else {
        val = 0xFF;
    }

    return val;
}

void joypad_write(uint16_t addr, uint8_t byte) {
    /*if(is_sgb && sgb_transferring) return sgb_write(byte);

    if(is_sgb && sgb_interfere) return sgb_write(byte);

    if(is_sgb) {
        if(!(byte & 0x20) && !(byte & 0x10)) {
            return sgb_write(byte);
        }
    }*/

    if(is_sgb) {
        if(sgb_transferring || sgb_interfere) return sgb_write(byte);

        if(!(byte & 0x20) && !(byte & 0x10)) {
            return sgb_write(byte);
        }
    }

    byte = ~byte;
    if(byte & 0x20) {
        // button keys
        selection = 0;
    } else if(byte & 0x10) {
        // direction
        selection = 1;
        //write_log("[joypad] write value 0x%02X, selecting directions\n", (~byte) & 0xFF);
    } else {
        // undefined so we'll return ones
        selection = 2;
    }
}

void joypad_handle(int is_down, int key) {
    uint8_t val;
    switch(key) {
    case JOYPAD_RIGHT:
        val = BUTTON_RIGHT;
        break;
    case JOYPAD_LEFT:
        val = BUTTON_LEFT;
        break;
    case JOYPAD_UP:
        val = BUTTON_UP;
        break;
    case JOYPAD_DOWN:
        val = BUTTON_DOWN;
        break;
    case JOYPAD_A:
        val = BUTTON_A;
        break;
    case JOYPAD_B:
        val = BUTTON_B;
        break;
    case JOYPAD_START:
        val = BUTTON_START;
        break;
    case JOYPAD_SELECT:
        val = BUTTON_SELECT;
        break;
    default:
        die(-1, "undefined key %d in joypad_handle()\n", key);
        val = 0xFF;     // unreachable
    }

    if(is_down) {
        pressed_keys |= val;
    } else {
        pressed_keys &= ~val;
    }
}
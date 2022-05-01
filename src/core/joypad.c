
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
    uint8_t val;

    if(selection) {
        // directions
        val = (~(pressed_keys >> 4)) & 0x0F;
        //write_log("[joypad] directions return value 0x%02X\n", val);
    } else {
        // buttons
        val = (~pressed_keys) & 0x0F;
        //write_log("[joypad] buttons return value 0x%02X\n", val);
    }

    return val;
}

void joypad_write(uint16_t addr, uint8_t byte) {
    byte = ~byte;
    if(byte & 0x20) {
        // button keys
        selection = 0;
    } else if(byte & 0x10) {
        // direction
        selection = 1;
        //write_log("[joypad] write value 0x%02X, selecting directions\n", (~byte) & 0xFF);
    } else {
        // undefined but we'll just use buttons
        //write_log("[joypad] undefined write value 0x%02X, ignoring...\n", (~byte) & 0xFF);
        selection = 0;
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
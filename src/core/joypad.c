
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
    if(selection) {
        // directions
        return (~(pressed_keys >> 4)) & 0x0F;
    } else {
        // buttons
        return (~pressed_keys) & 0x0F;
    }
}

void joypad_write(uint16_t addr, uint8_t byte) {
    byte = ~byte;
    if(byte & 0x20) {
        // button keys
        selection = 0;
    } else if(byte & 0x10) {
        // direction
        selection = 1;
    } else {
        // undefined but we'll just use buttons
        write_log("[joypad] undefined write value 0x%02X, ignoring...\n", (~byte) & 0xFF);
        selection = 0;
    }
}

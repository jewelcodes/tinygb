
/* tinygb - a tiny gameboy emulator
   (c) 2022 by jewel */

#include <tinygb.h>
#include <ioports.h>
#include <string.h>

#define DISPLAY_LOG

display_t display;

void display_start() {
    memset(&display, 0, sizeof(display_t));
    display.lcdc = 0x91;
    display.scy = 0;
    display.scx = 0;
    display.lyc = 0;
    display.bgp = 0xFC;
    display.obp0 = 0xFF;
    display.obp1 = 0xFF;
    display.wy = 0;
    display.wx = 0;

    write_log("[display] initialized display\n");
}

void write_display_io(uint16_t addr, uint8_t byte) {
    switch(addr) {
    case LCDC:
#ifdef DISPLAY_LOG
        write_log("[display] write to LCDC register value 0x%02X\n", byte);
#endif
        display.lcdc = byte;
        return;
    case STAT:
#ifdef DISPLAY_LOG
        write_log("[display] write to STAT register value 0x%02X\n", byte);
#endif
        display.stat = byte;
        return;
    case SCX:
#ifdef DISPLAY_LOG
        write_log("[display] write to SCX register value 0x%02X\n", byte);
#endif
        display.scx = byte;
        return;
    case SCY:
#ifdef DISPLAY_LOG
        write_log("[display] write to SCY register value 0x%02X\n", byte);
#endif
        display.scy = byte;
        return;
    default:
        write_log("[memory] unimplemented write to I/O port 0x%04X value 0x%02X\n", addr, byte);
        die(-1, NULL);
    }
}
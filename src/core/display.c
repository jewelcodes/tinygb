
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
    display.ly = 0;
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
    case LY:
#ifdef DISPLAY_LOG
        write_log("[display] write to LY register, resetting...\n");
#endif
        display.ly = 0;
        return;
    case LYC:
#ifdef DISPLAY_LOG
        write_log("[display] write to LY register value 0x%02X\n", byte);
#endif
        display.lyc = byte;
        return;
    case BGP:
#ifdef DISPLAY_LOG
        write_log("[display] write to BGP register value 0x%02X\n", byte);
#endif
        display.bgp = byte;
        return;
    case OBP0:
#ifdef DISPLAY_LOG
        write_log("[display] write to OBP0 register value 0x%02X\n", byte);
#endif
        display.obp0 = byte;
        return;
    case OBP1:
#ifdef DISPLAY_LOG
        write_log("[display] write to OBP1 register value 0x%02X\n", byte);
#endif
        display.obp1 = byte;
        return;
    case WX:
#ifdef DISPLAY_LOG
        write_log("[display] write to WX register value 0x%02X\n", byte);
#endif
        display.wx = byte;
        return;
    case WY:
#ifdef DISPLAY_LOG
        write_log("[display] write to WY register value 0x%02X\n", byte);
#endif
        display.wy = byte;
        return;
    default:
        write_log("[memory] unimplemented write to I/O port 0x%04X value 0x%02X\n", addr, byte);
        die(-1, NULL);
    }
}

uint8_t read_display_io(uint16_t addr) {
    switch(addr) {
    case LCDC:
        return display.lcdc;
    case STAT:
        return display.stat;
    case SCY:
        return display.scy;
    case SCX:
        return display.scx;
    case LY:
        return display.ly;
    case LYC:
        return display.lyc;
    case DMA:
        write_log("[display] undefined read from write-only DMA register, returning ones\n");
        return 0xFF;
    case BGP:
        return display.bgp;
    case OBP0:
        return display.obp0;
    case OBP1:
        return display.obp1;
    default:
        write_log("[memory] unimplemented read from IO port 0x%04X\n", addr);
        die(-1, NULL);
    }

    return 0xFF;    // unreachable
}

void display_cycle() {
    // this should be executed upon every v-line refresh
    // that is once every 0.108769 ms
    //write_log("[display] display cycle\n");

    if(display.lcdc & LCDC_ENABLE) {
        display.ly++;
        if(display.ly >= 154) display.ly = 0;

        if(display.ly == display.lyc) {
            write_log("[display] STAT interrupt\n");
        }
    }
}

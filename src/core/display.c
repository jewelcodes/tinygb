
/* tinygb - a tiny gameboy emulator
   (c) 2022 by jewel */

#include <tinygb.h>
#include <ioports.h>
#include <string.h>
#include <stdlib.h>

#define DISPLAY_LOG

#define OAM_SIZE        256     // bytes

display_t display;
int display_cycles = 0;

void *vram;
uint32_t *framebuffer;

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

    vram = calloc(1, 16384);    // 8 KB for original gb, 2x8 KB for CGB
    if(!vram) {
        die(-1, "unable to allocate memory for VRAM\n");
    }

    framebuffer = calloc(GB_WIDTH*GB_HEIGHT, 4);
    if(!framebuffer) {
        die(-1, "unable to allocate memory for framebuffer\n");
    }

    write_log("[display] initialized display\n");
}

void display_write(uint16_t addr, uint8_t byte) {
    switch(addr) {
    case LCDC:
#ifdef DISPLAY_LOG
        write_log("[display] write to LCDC register value 0x%02X\n", byte);
#endif
        display.lcdc = byte;
        return;
    case STAT:
#ifdef DISPLAY_LOG
        write_log("[display] write to STAT register value 0x%02X, ignoring lowest 3 bits\n", byte);
#endif
        byte &= 0xF8;
        display.stat &= 7;
        display.stat |= byte;
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
    case DMA:
#ifdef DISPLAY_LOG
        write_log("[display] write to DMA register value 0x%02X\n", byte);
#endif
        display.dma = byte;
        return;
    default:
        write_log("[memory] unimplemented write to I/O port 0x%04X value 0x%02X\n", addr, byte);
        die(-1, NULL);
    }
}

uint8_t display_read(uint16_t addr) {
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

/*void display_cycle() {
    //write_log("[display] display cycle\n");

    if(display.lcdc & LCDC_ENABLE) {
        int mode = display.stat & 0xFC;

        if(mode == 1) {  // v-blank
            int vblank_cycles = timing.current_cycles % 4570;
            if(vblank_cycles >= 4560) {
                // vblank is over; go back to mode zero
                display.stat &= 0xFC;
                display.ly = 0;

                if(display.ly == display.lyc) {
                    display.stat |= 0x04;
                } else {
                    display.stat &= 0xFB;
                }
            } else if(vblank_cycles >= 456) {
                // completed one line
                display.ly++;

                if(display.ly == display.lyc) {
                    display.stat |= 0x04;
                } else {
                    display.stat &= 0xFB;
                }
            }

            return;
        }

        // any other mode
        int cycles = timing.current_cycles % 466;
        if(cycles <= 204) {
            // mode 0
            display.stat &= 0xFC;
        } else if(cycles <= 285) {
            // mode 2
            display.stat &= 0xFC;
            display.stat |= 2;
        } else if(cycles <= 455) {
            // mode 3
            display.stat &= 0xFC;
            display.stat |= 3;
        } else if(cycles >= 456) {
            // a line was completed
            display.ly++;

            if(display.ly == 144) {
                // enter v-blank mode
                display.stat &= 0xFC;
                display.stat |= 1;
            }

            if(display.ly == display.lyc) {
                display.stat |= 0x04;
            } else {
                display.stat &= 0xFB;
            }
        }
    }
}*/

void display_cycle() {
    if(!(display.lcdc & LCDC_ENABLE)) return;
    display_cycles += timing.last_instruction_cycles;

    // handle OAM DMA transfers if ongoing
    if(display.dma) {
        uint16_t dma_src = display.dma << 8;

#ifdef DISPLAY_LOG
        write_log("[display] DMA transfer from 0x%04X to sprite OAM region\n", dma_src);
#endif

        for(int i = 0; i < OAM_SIZE; i++) {
            write_byte(0xFE00+i, read_byte(dma_src+i));
        }

        display.dma = 0;
    }

    // mode 0 = 0 -> 203
    // mode 2 = 204 -> 283
    // mode 3 = 284 -> 455

    // mode 1 is a special case where it goes through all of these cycles 10 times
    uint8_t mode = display.stat & 3;
    //write_log("[display] cycles = %d, mode = %d, LY = %d, STAT = 0x%02X\n", display_cycles, mode, display.ly, display.stat);
    if(mode == 1) {  // vblank is a special case
        //write_log("[display] in vblank, io_if = 0x%02X\n", io_if);
        if(display_cycles >= 456) {
            display_cycles -= 456;  // dont lose any cycles

            display.ly++;
            if(display.ly >= 154) {
                // vblank is now over
                display.stat &= 0xFC;
                display.ly = 0;
            }

            if(display.ly == display.lyc) {
                // TODO: send STAT interrupt
                display.stat |= 0x04;   // coincidence flag
            } else {
                display.stat &= 0xFB;
            }
        }
    } else {
        // all other modes
        if(display_cycles <= 203) {
            // mode 0
            display.stat &= 0xFC;
        } else if(display_cycles <= 283) {
            // mode 2
            display.stat &= 0xFC;
            display.stat |= 2;
        } else if(display_cycles <= 455) {
            // mode 3
            display.stat &= 0xFC;
            display.stat |= 3;
        } else if(display_cycles >= 456) {
            // a horizontal line has been completed
            display_cycles -= 456;  // dont lose any cycles

            display.ly++;
            if(display.ly >= 144) {
                // begin vblank (mode 1)
                display.stat &= 0xFC;
                display.stat |= 1;

                //write_log("[display] entering vblank state, STAT = 0x%02X\n", display.stat);

                send_interrupt(0);
            } else {
                // return to mode zero
                display.stat &= 0xFC;
            }

            if(display.ly == display.lyc) {
                // TODO: send STAT interrupt
                display.stat |= 0x04;   // coincidence flag
            } else {
                display.stat &= 0xFB;
            }
        }
    }
}

void vram_write(uint16_t addr, uint8_t byte) {
    uint8_t *ptr = (uint8_t *)vram + byte;
    ptr += (8192 * display.vbk);    // for CGB banking

    *ptr = addr;
}
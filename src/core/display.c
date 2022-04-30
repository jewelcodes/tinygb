
/* tinygb - a tiny gameboy emulator
   (c) 2022 by jewel */

#include <tinygb.h>
#include <ioports.h>
#include <string.h>
#include <stdlib.h>

#define DISPLAY_LOG

/*

Notes to self regarding how the display works:
- Vertical line starts at mode 2 (reading OAM)
- Next mode is mode 3 (reading both OAM and VRAM)
- Next mode is mode 0 (H-blank, not reading anything)
- After 144 lines are completed, enter mode 1 (V-blank)
- V-blank lasts for 10 "lines" in which nothing is being read

- The program does not write direct pixels to the screen, instead it has tile
  data stored in VRAM. The background is drawn as a map of tiles, and the
  window is drawn on top of the background also as a map of tiles, and finally
  the sprites (OAM) are drawn as a final map.

- Background maps are mandatory, but the window and sprites can be switched
  on/off.

- Mode (2 --> 3 --> 0) 144 times
- Mode (1) 10 times

 */

#define OAM_SIZE        256     // bytes

display_t display;
int display_cycles = 0;

void *vram;
uint32_t *framebuffer, *scaled_framebuffer, *temp_framebuffer;
uint32_t *background_buffer;
char oam[OAM_SIZE];

extern int scaling;

int scaled_w, scaled_h;

uint32_t bw_pallete[4] = {
    0xFFFFFF, 0xAAAAAA, 0x555555, 0x000000
};

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

    scaled_w = scaling*GB_WIDTH;
    scaled_h = scaling*GB_HEIGHT;

    vram = calloc(1, 16384);    // 8 KB for original gb, 2x8 KB for CGB
    if(!vram) {
        die(-1, "unable to allocate memory for VRAM\n");
    }

    framebuffer = calloc(GB_WIDTH*GB_HEIGHT, 4);
    temp_framebuffer = calloc(GB_WIDTH*GB_HEIGHT, 4);
    background_buffer = calloc(256*256, 4);
    if(scaling != 1) scaled_framebuffer = calloc(GB_WIDTH*GB_HEIGHT, 4*scaling*scaling*4);
    else scaled_framebuffer = framebuffer;

    if(!framebuffer || !scaled_framebuffer || !temp_framebuffer || !background_buffer) {
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

inline void scale_xline(uint32_t *new, uint32_t *old) {
    for(int i = 0; i < scaled_w; i++) {
        //printf("copy new X %d, old X %d\n", i, i/scaling);
        new[i] = old[(i/scaling)];
        //new[i] = 0xFFFFFF;

        //printf("old x = %d, old y = %d\n", i/scaling, y);
    }
}

void update_framebuffer() {
    // scale up the buffer
    for(int y = 0; y < scaled_h; y++) {
        uint32_t *dst = scaled_framebuffer + (y * scaled_w);
        uint32_t *src = framebuffer + ((y / scaling) * GB_WIDTH);

        scale_xline(dst, src);
    }

    // write it to the screen
    if(surface->format->BytesPerPixel == 4) {
        // 32-bpp
        for(int i = 0; i < scaled_h; i++) {
            //void *src = (void *)(scaled_framebuffer + (i * GB_WIDTH * 4));
            //void *src = (void *)(scaled_framebuffer + (i * scaled_w));
            void *src = (void *)(scaled_framebuffer + (i * scaled_w));
            void *dst = (void *)(surface->pixels + (i * surface->pitch));
            memcpy(dst, src, scaled_w*4);
        }
    } else {
        die(-1, "unimplemented non 32-bpp surfaces\n");
    }
}

void plot_bg_tile(int x, int y, uint8_t tile, uint8_t *tile_data) {
    // x and y are in tiles, not pixels
    int xp = x * 8;
    int yp = y * 8;

    uint32_t color;
    uint8_t data;
    uint8_t *ptr;

    /*write_log("[display] rendering bg tile %d, data bytes ", tile);

    for(int i = 0; i < 16; i++) {
        write_log("%02X ", tile_data[(tile * 16) + i]);
    }

    write_log("\n");*/

    // 8x8 tiles
    for(int i = 0; i < 8; i++) {
        for(int j = 0; j < 8; j++) {
            ptr = tile_data + (tile * 16) + (i * 2) + (j / 2);
            data = *ptr >> ((j % 3) * 2);
            data &= 3;
            color = bw_pallete[data];

            background_buffer[(yp * 256) + xp] = color;

            /*if(color != 0xFFFFFF) {
                printf("h");
            }*/

            xp++;
        }

        yp++;
        xp = x * 8;
    }
}

void render_line() {
    // renders a single horizontal line
    copy_oam(oam);

    uint8_t *bg_win_tiles;
    if(display.lcdc & 0x10) bg_win_tiles = vram + 0;    // 0x8000-0x8FFF
    else bg_win_tiles = vram + 0x800;    // 0x8800-0x97FF

    // test if background is enabled
    if(display.lcdc & 0x01) {
        uint8_t *bg_map;
        if(display.lcdc & 0x08) {
            bg_map = vram + 0x1C00;     // 0x9C00-0x9FFF

            // TODO 
            die(-1, "unimplemented 0x9C00 signed tile numbering\n");
        } else {
            bg_map = vram + 0x1800;     // 0x9800-0x9BFF

            for(int y = 0; y < 32; y++) {
                for(int x = 0; x < 32; x++) {
                    plot_bg_tile(x, y, *bg_map, bg_win_tiles);
                    bg_map++;
                }
            }
        }

        // here the background has been drawn, copy the visible part of it
        //write_log("[display] rendering background, SCY = %d, SCX = %d\n", display.scy, display.scx);

        for(int y = 0; y < GB_HEIGHT; y++) {
            for(int x = 0; x < GB_WIDTH; x++) {
                temp_framebuffer[(y * GB_WIDTH) + x] = background_buffer[((y + display.scy) * 256) + (x + display.scx)];
            }
        }

    } else {
        // no background, clear to white
        for(int i = 0; i < GB_WIDTH*GB_HEIGHT; i++) {
            temp_framebuffer[i] = bw_pallete[0];
        }
    }

    // done, copy the singular line we were at
    uint32_t *src = temp_framebuffer + (display.ly * GB_WIDTH);
    uint32_t *dst = framebuffer + (display.ly * GB_WIDTH);

    for(int i = 0; i < GB_WIDTH; i++) {
        dst[i] = src[i];
    }
}

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

    // mode 2 = 0 -> 79
    // mode 3 = 80 -> 251
    // mode 0 = 252 -> 455

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
        if(display_cycles <= 79) {
            // mode 2 -- reading OAM
            display.stat &= 0xFC;
            display.stat |= 2;
        } else if(display_cycles <= 251) {
            // mode 3 -- reading OAM and VRAM
            display.stat &= 0xFC;
            display.stat |= 3;

            // complete one line
            render_line();
        } else if(display_cycles <= 455) {
            // mode 0
            display.stat &= 0xFC;
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

                // update the actual screen
                update_framebuffer();
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
    addr -= 0x8000;

    uint8_t *ptr = (uint8_t *)vram + addr;
    ptr += (8192 * display.vbk);    // for CGB banking

    *ptr = addr;
}
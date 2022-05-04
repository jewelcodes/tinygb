
/* tinygb - a tiny gameboy emulator
   (c) 2022 by jewel */

#include <tinygb.h>
#include <ioports.h>
#include <string.h>
#include <stdlib.h>

//#define DISPLAY_LOG

/*

Notes to self regarding how the display works:
- Horizontal line starts at mode 2 (reading OAM)
- Next mode is mode 3 (reading both OAM and VRAM)
- Next mode is mode 0 (H-blank, not reading anything)
- After 144 lines are completed, enter mode 1 (V-blank)
- V-blank lasts for 10 "lines" in which nothing is being read

- The program does not write direct pixels to the screen, instead it has tile
  data stored in VRAM. The background is drawn as a map of tiles, and the
  window is drawn on top of the background also as a map of tiles, and finally
  the sprites (OAM) are drawn as a final map.

- Mode (2 --> 3 --> 0) 144 times
- Mode (1) 10 times

 */

display_t display;
int display_cycles = 0;

void *vram;
uint32_t *framebuffer, *scaled_framebuffer, *temp_framebuffer;
uint32_t *background_buffer;
uint8_t oam[OAM_SIZE];

int scaled_w, scaled_h;

int framecount = 0;

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
    case VBK:
        if(is_cgb) {
#ifdef DISPLAY_LOG
            write_log("[display] write to VBK register value 0x%02X\n", byte);
#endif
            display.vbk = byte;
        } else {
            write_log("[display] write to VBK register value 0x%02X in non-CGB mode, ignoring...\n", byte);
        }
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
    case WX:
        return display.wx;
    case WY:
        return display.wy;
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
    if(scaling != 1) {
        for(int y = 0; y < scaled_h; y++) {
            uint32_t *dst = scaled_framebuffer + (y * scaled_w);
            uint32_t *src = framebuffer + ((y / scaling) * GB_WIDTH);

            scale_xline(dst, src);
        }
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

    //framecount++;
    if(framecount > frameskip) {
        SDL_UpdateWindowSurface(window);
        framecount = 0;
    }
}

void plot_bg_tile(int is_window, int x, int y, uint8_t tile, uint8_t *tile_data) {
    // x and y are in tiles, not pixels
    int xp = x << 3;    // x8
    int yp = y << 3;

    int bgy;

    if(!is_window) {
        if(display.scy >= 113) {    // 255-143
            // an wraparound will inevitably occur
            int bg_line = display.scy + display.ly;
            int last_bg_line = GB_HEIGHT - (256-display.scy);
            last_bg_line -= 8;

            int wrapped_ly;

            if(bg_line >= 256) {
                // wrap occured
                bg_line -= 256;

                wrapped_ly = display.ly - (256 - display.scy);
                if(!((wrapped_ly) >= yp && (wrapped_ly) <= (yp+8))) {
                    return;
                }
            } else {
                // no wrap
                if(!((display.ly+display.scy) >= yp && (display.ly+display.scy) <= (yp+8))) {
                    return;   // save a fuckton of performance
                }
            }
        } else {
            if(!((display.ly+display.scy) >= yp && (display.ly+display.scy) <= (yp+8))) {
                return;   // save a fuckton of performance
            }
        }
    } else {
        if(xp >= GB_WIDTH || yp >= GB_HEIGHT) return;
        if(!(display.ly >= (yp+display.wy) && display.ly <= (yp+display.wy+8))) return;
    }

    uint32_t color;
    uint8_t data, color_index;
    uint8_t data_lo, data_hi;
    uint8_t *ptr;
    uint8_t positive_tile;

    /*write_log("[display] rendering bg tile %d, data bytes ", tile);

    for(int i = 0; i < 16; i++) {
        write_log("%02X ", tile_data[(tile * 16) + i]);
    }

    write_log("\n");*/

    if(display.lcdc & 0x10) ptr = tile_data + (tile * 16);  // normal positive
    else {
        tile_data += 0x800;     // to 0x9000

        if(tile & 0x80) {
            // negative
            positive_tile = ~tile;
            positive_tile++;

            ptr = tile_data - (positive_tile * 16);
        } else {
            // positive
            ptr = tile_data + (tile * 16);
        }
    }

    // 8x8 tiles
    for(int i = 0; i < 8; i++) {
        //printf("data for row %d is %02X %02X\n", i, ptr[0], ptr[1]);

        for(int j = 0; j < 8; j++) {

            /*int s = 6 - ((j % 3) * 2);
            data = *ptr >> s;
            data &= 3;*/

            /*data = (ptr[1] >> (7 - j));
            data <<= 1;
            data &= 2;  // keep only bit 1
            data |= ptr[0] >> (7 - j) & 1;*/

            data_hi = (ptr[1] >> (7 - j)) & 1;
            data_hi <<= 1;

            data_lo = (ptr[0] >> (7 - j));
            data_lo &= 1;

            data = data_hi | data_lo;

            //printf("data for x/y %d/%d is %d\n", i, j, data);

            color_index = (display.bgp >> (data * 2)) & 3;
            //color_index = data;
            color = bw_pallete[color_index];
            background_buffer[(yp * 256) + xp] = color;

            /*if(color != 0xFFFFFF) {
                printf("h");
            }*/

            xp++;
        }

        yp++;
        xp = x << 3;    // x8
        ptr += 2;
    }
}

inline void hflip_sprite(uint32_t *sprite_colors, uint8_t *sprite_data) {
    // horizontal flip
    uint32_t temp_color;
    uint8_t temp_data;

    for(int y = 0; y < 8; y++) {
        for(int x = 0; x < 4; x++) {
            temp_color = sprite_colors[(y*8)+7-x];
            temp_data = sprite_data[(y*8)+7-x];

            sprite_colors[(y*8)+7-x] = sprite_colors[(y*8)+x];
            sprite_data[(y*8)+7-x] = sprite_data[(y*8)+x];

            sprite_colors[(y*8)+x] = temp_color;
            sprite_data[(y*8)+x] = temp_data;
        }
    }
}

void plot_small_sprite(int n) {
    // n max 40
    /*if(n >= 40) {
        write_log("[display] warning: attempt to draw non-existent sprite number %d, ignoring...\n", n);
        return;
    }*/

    uint8_t *oam_data = oam + (n * 4);

    uint8_t x, y, tile, flags;
    y = oam_data[0];
    x = oam_data[1];
    tile = oam_data[2];
    flags = oam_data[3];

    uint8_t data, data_lo, data_hi, color_index;
    uint32_t color, bg_color, bg_color_zero;

    if(!y || y >= 152 || !x || x >= 168) return;    // invisible sprite

    x -= 8;
    y -= 16;

    if(!(display.ly >= y && display.ly <= y+8)) return;   // performance

    //write_log("[display] plotting tile %d at x/y %d/%d\n", tile, x, y);

    // get bg color zero for layering
    bg_color_zero = bw_pallete[display.bgp & 3];

    // 8x8 tiles
    uint8_t *tile_data = vram + 0x0000;     // always starts at 0x8000, unlike bg/window
    uint8_t *ptr = tile_data + (tile * 16);
    uint32_t sprite_colors[64];    // 8x8
    uint8_t sprite_data[64];
    int sprite_data_index = 0;

    /*for(int i = 0; i < 8; i++) {
        for(int j = 0; j < 8; j++) {
            data_hi = (ptr[1] >> (7 - j)) & 1;
            data_hi <<= 1;

            data_lo = (ptr[0] >> (7 - j));
            data_lo &= 1;

            data = data_hi | data_lo;

            if(flags & 0x10) color_index = (display.obp1 >> (data * 2)) & 3;    // pallete 1
            else color_index = (display.obp0 >> (data * 2)) & 3;    // pallete 0

            color = bw_pallete[color_index];

            // sprites may be on top of or under the background
            if(flags & 0x80) {
                // sprite is behind bg colors 1-3, on top of bg color 0

                // get bg color
                bg_color = temp_framebuffer[((i + y) * GB_WIDTH) + (j + x)];
                if((bg_color == bg_color_zero) && data) temp_framebuffer[((i + y) * GB_WIDTH) + (j + x)] = color;
            } else {
                // sprite is on top of bg, normal scenario
                // sprite color value zero means transparent, so only plot non-zero values
                if(data) temp_framebuffer[((i + y) * GB_WIDTH) + (j + x)] = color;
            }
        }

        ptr += 2;
    }*/

    sprite_data_index = 0;
    for(int i = 0; i < 8; i++) {
        for(int j = 0; j < 8; j++) {
            data_hi = (ptr[1] >> (7 - j)) & 1;
            data_hi <<= 1;

            data_lo = (ptr[0] >> (7 - j));
            data_lo &= 1;

            data = data_hi | data_lo;

            if(flags & 0x10) color_index = (display.obp1 >> (data * 2)) & 3;    // pallete 1
            else color_index = (display.obp0 >> (data * 2)) & 3;    // pallete 0

            color = bw_pallete[color_index];

            sprite_colors[sprite_data_index] = color;
            sprite_data[sprite_data_index] = data;

            sprite_data_index++;
        }

        ptr += 2;
    }

    // check if we need to flip this sprite
    if(flags & 0x20) hflip_sprite(sprite_colors, sprite_data);    // horizontal flip
    //if(flags & 0x40) vflip_sprite(&sprite_colors, &sprite_data);   // vertical flip

    // now plot the actual sprite
    sprite_data_index = 0;
    for(int i = 0; i < 8; i++) {
        for(int j = 0; j < 8; j++) {
            if(flags & 0x80) {
                // sprite is behind bg colors 1-3, on top of bg color 0

                // get bg color
                bg_color = temp_framebuffer[((i + y) * GB_WIDTH) + (j + x)];
                if((bg_color == bg_color_zero) && sprite_data[sprite_data_index]) temp_framebuffer[((i + y) * GB_WIDTH) + (j + x)] = sprite_colors[sprite_data_index];
            } else {
                // sprite is on top of bg, normal scenario
                // sprite color value zero means transparent, so only plot non-zero values
                if(sprite_data[sprite_data_index]) temp_framebuffer[((i + y) * GB_WIDTH) + (j + x)] = sprite_colors[sprite_data_index];
            }

            sprite_data_index++;
        }
    }

    return;
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
        if(display.lcdc & 0x08) bg_map = vram + 0x1C00;     // 0x9C00-0x9FFF
        else bg_map = vram + 0x1800;     // 0x9800-0x9BFF

        for(int y = 0; y < 32; y++) {
            for(int x = 0; x < 32; x++) {
                plot_bg_tile(0, x, y, *bg_map, bg_win_tiles);
                bg_map++;
            }
        }

        // here the background has been drawn, copy the visible part of it
        //write_log("[display] rendering background, SCY = %d, SCX = %d\n", display.scy, display.scx);
        int temp_index = 0;
        unsigned int bg_index = display.scy * 256;
        unsigned int bg_x = display.scx, bg_y = display.scy;

        for(int y = 0; y < GB_HEIGHT; y++) {
            if(bg_y > 255) {
                bg_y = 0;
            }

            bg_x = display.scx;
            bg_index = (bg_y * 256) + bg_x;

            for(int x = 0; x < GB_WIDTH; x++) {
                if(bg_x > 255) {
                    bg_index -= 256;
                    bg_x = 0;
                }

                //temp_framebuffer[(y * GB_WIDTH) + x] = background_buffer[((y + display.scy) * 256) + (x + display.scx)];
                temp_framebuffer[temp_index+x] = background_buffer[bg_index+x];

                bg_x++;
            }
 
            temp_index += GB_WIDTH;
            bg_y++;
        }

    } else {
        // no background, clear to white
        for(int i = 0; i < GB_WIDTH*GB_HEIGHT; i++) {
            temp_framebuffer[i] = bw_pallete[0];
        }
    }

    // window layer on top of the background
    if(display.lcdc & 0x20 && display.wx >= 7 && display.wx <= 166 && display.wy <= 143) {
        // window enabled
        uint8_t *win_map;
        if(display.lcdc & 0x40) win_map = vram + 0x1C00;    // 0x9C00-0x9FFF
        else win_map = vram + 0x1800;   // 0x9800-0x9BFF

        // windows have the same format as backgrounds
        for(int y = 0; y < 32; y++) {
            for(int x = 0; x < 32; x++) {
                plot_bg_tile(1, x, y, *win_map, bg_win_tiles);
                win_map++;
            }
        }

        // draw the window
        int wx;
        if(display.wx <= 7) wx = 0;
        else wx = display.wx - 7;

        int wy = display.wy;
        int temp_index = (wy * GB_WIDTH) + (wx);
        int bg_index = 0;

        for(int y = 0; y < GB_HEIGHT - wy; y++) {
            for(int x = 0; x < GB_WIDTH - wx; x++) {
                temp_framebuffer[temp_index + x] = background_buffer[bg_index + x];
            }

            temp_index += GB_WIDTH;
            bg_index += 256;
        }
    }

    // object layer
    if(display.lcdc & 0x02) {
        // sprites are enabled
        if(display.lcdc & 0x04) {
            die(-1, "unimplemented 8x16 sprites\n");
        } else {
            for(int i = 0; i < 40; i++) {   // 40 sprites
                plot_small_sprite(i);
            }
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
        //write_log("[display] DMA transfer from 0x%04X to sprite OAM region\n", dma_src);
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
            if(framecount > frameskip) render_line();
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
                framecount++;
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
    //write_log("[display] write to VRAM 0x%04X value 0x%02X\n", addr, byte);
    addr -= 0x8000;

    uint8_t *ptr = (uint8_t *)vram + addr;
    ptr += (8192 * display.vbk);    // for CGB banking

    *ptr = byte;
}

uint8_t vram_read(uint16_t addr) {
    addr -= 0x8000;

    uint8_t *ptr = (uint8_t *)vram + addr;
    ptr += (8192 * display.vbk);

    return *ptr;
}
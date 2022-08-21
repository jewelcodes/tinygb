
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

#define HDMA_GENERAL        0
#define HDMA_HBLANK         1

display_t display;
int display_cycles = 0;

void *vram;
uint32_t *framebuffer, *scaled_framebuffer, *temp_framebuffer;
uint32_t *background_buffer;
uint8_t oam[OAM_SIZE];

int scaled_w, scaled_h;

int framecount = 0;
int hdma_active = 0;
int hdma_type;

static int line_rendered = 0;

int hdma_hblank_next_line;
int hdma_hblank_cycles = 0;

int drawn_frames = 0;

uint32_t bw_palette[4] = {
    0xC4CFA1, 0x8B956D, 0x4D533C, 0x1F1F1F
};

uint32_t preset_palettes[10][4] = {
    {0xC4CFA1, 0x8B956D, 0x4D533C, 0x1F1F1F},   // 0
    {0x9BEBEB, 0x6DA1DF, 0x6653CB, 0x501A68},   // 1
    {0xFFF5DE, 0xFD9785, 0xF60983, 0x15017A},   // 2
    {0xDCEDEB, 0x90ADBB, 0x56689D, 0x262338},   // 3
    {0xF7FFB7, 0xA5D145, 0x2A8037, 0x001B27},   // 4
    {0xFBDFB7, 0xFFB037, 0xEE316B, 0x842D72},   // 5
    {0xFE7BBF, 0x974EC3, 0x504099, 0x313866},   // 6
    {0x58CCED, 0x3895D3, 0x1261A0, 0x072F5F},   // 7
    {0xFEFDDF, 0xFDD037, 0xFAB22C, 0xDA791A},   // 8
    {0xFFFFFF, 0xAAAAAA, 0x555555, 0x000000},   // 9
};

uint32_t cgb_palette[4];

int monochrome_palette;

// dummy debug function
void cgb_dump_bgpd() {
    uint32_t color32;
    uint16_t color16;
    uint8_t r, g, b;

    for(int i = 0; i < 8; i++) {
        for(int j = 0; j < 4; j++) {
            color16 = display.bgpd[(i*8)+(j*2)];
            color16 |= (display.bgpd[(i*8)+(j*2)+1] << 8);

            color32 = truecolor(color16);
            r = (color32 >> 16) & 0xFF;
            g = (color32 >> 8) & 0xFF;
            b = color32 & 0xFF;

            write_log("[display] CGB bg palette %d color %d = \e[38;2;%d;%d;%dm#%06X\e[0m\n", i, j, r, g, b, color32);
        }
    }
}

void cgb_dump_obpd() {
    uint32_t color32;
    uint16_t color16;
    uint8_t r, g, b;

    for(int i = 0; i < 8; i++) {
        for(int j = 0; j < 4; j++) {
            color16 = display.obpd[(i*8)+(j*2)];
            color16 |= (display.obpd[(i*8)+(j*2)+1] << 8);

            color32 = truecolor(color16);
            r = (color32 >> 16) & 0xFF;
            g = (color32 >> 8) & 0xFF;
            b = color32 & 0xFF;

            write_log("[display] CGB obj palette %d color %d = \e[38;2;%d;%d;%dm#%06X\e[0m\n", i, j, r, g, b, color32);
        }
    }
}

void load_bw_palette() {
    if(monochrome_palette > 9) monochrome_palette = 0;

    write_log("[display] loaded monochrome palette %d\n", monochrome_palette);

    bw_palette[0] = preset_palettes[monochrome_palette][0];
    bw_palette[1] = preset_palettes[monochrome_palette][1];
    bw_palette[2] = preset_palettes[monochrome_palette][2];
    bw_palette[3] = preset_palettes[monochrome_palette][3];
}

void next_palette() {
    if(monochrome_palette >= 9) monochrome_palette = 0;
    else monochrome_palette++;

    load_bw_palette();
}

void prev_palette() {
    if(monochrome_palette == 0) monochrome_palette = 9;
    else monochrome_palette--;

    load_bw_palette();
}

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

    load_bw_palette();

    if(is_cgb) {
        for(int i = 0; i < 32; i++) {
            // bg palette is initialized to white in CGB
            display.bgpd[i*2] = 0xFF;
            display.bgpd[(i*2)+1] = 0x7F;
        }
    }

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

void handle_general_hdma() {
    uint16_t src = (display.hdma1 << 8) | (display.hdma2 & 0xF0);
    uint16_t dst = ((display.hdma3 & 0x1F) << 8) | (display.hdma4 & 0xF0);
    dst += 0x8000;

    int count = (display.hdma5 + 1) << 4;

#ifdef DISPLAY_LOG
    write_log("[display] handle general HDMA transfer from 0x%04X to 0x%04X, %d bytes\n", src, dst, count);
#endif

    for(int i = 0; i < count; i++) {
        write_byte(dst+i, read_byte(src+i));
    }

    display.hdma5 = 0xFF;
}

void handle_hblank_hdma() {
    uint16_t src = (display.hdma1 << 8) | (display.hdma2 & 0xF0);
    uint16_t dst = ((display.hdma3 & 0x1F) << 8) | (display.hdma4 & 0xF0);
    dst += 0x8000; 

#ifdef DISPLAY_LOG
    write_log("[display] handle H-blank HDMA transfer from 0x%04X to 0x%04X, 16 bytes at LY=%d\n", src, dst, display.ly);
#endif

    for(int i = 0; i < 16; i++) {
        write_byte(dst+i, read_byte(src+i));
    }

    src += 16;
    dst += 16;
    dst -= 0x8000;

    display.hdma1 = (src >> 8) & 0xFF;
    display.hdma2 = src & 0xF0;
    display.hdma3 = (dst >> 8) & 0x1F;
    display.hdma4 = dst & 0xF0;

    display.hdma5--;
    if(display.hdma5 == 0x7F) {
        // done
#ifdef DISPLAY_LOG
        write_log("[display] completed H-blank transfer\n");
#endif
        display.hdma5 = 0xFF;
    }
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
        write_log("[display] write to LYC register value 0x%02X\n", byte);
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
            write_log("[display] write to VBK register value 0x%02X, ignoring upper 7 bits...\n", byte);
#endif

            byte &= 1;  // only lowest bit matters
            display.vbk = byte;
        } else {
            //write_log("[display] write to VBK register value 0x%02X in non-CGB mode, ignoring...\n", byte);
        }
    case HDMA1:
        if(is_cgb) {
#ifdef DISPLAY_LOG
            write_log("[display] write to HDMA1 register value 0x%02X\n", byte);
#endif
            display.hdma1 = byte;
        } else {
            //write_log("[display] write to HDMA1 register value 0x%02X in non-CGB mode, ignoring...\n", byte);
        }
        return;
    case HDMA2:
        if(is_cgb) {
#ifdef DISPLAY_LOG
            write_log("[display] write to HDMA2 register value 0x%02X\n", byte);
#endif
            display.hdma2 = byte;
        } else {
            //write_log("[display] write to HDMA2 register value 0x%02X in non-CGB mode, ignoring...\n", byte);
        }
        return;
    case HDMA3:
        if(is_cgb) {
#ifdef DISPLAY_LOG
            write_log("[display] write to HDMA3 register value 0x%02X\n", byte);
#endif
            display.hdma3 = byte;
        } else {
            //write_log("[display] write to HDMA3 register value 0x%02X in non-CGB mode, ignoring...\n", byte);
        }
        return;
    case HDMA4:
        if(is_cgb) {
#ifdef DISPLAY_LOG
            write_log("[display] write to HDMA4 register value 0x%02X\n", byte);
#endif
            display.hdma4 = byte;
        } else {
            //write_log("[display] write to HDMA4 register value 0x%02X in non-CGB mode, ignoring...\n", byte);
        }
        return;
    case HDMA5:
        if(is_cgb) {
#ifdef DISPLAY_LOG
            write_log("[display] write to HDMA5 register value 0x%02X\n", byte);
#endif

            if(byte & 0x80) {
                // H-blank DMA
#ifdef DISPLAY_LOG
                write_log("[display] H-blank DMA %d bytes from 0x%02X%02X to VRAM 0x%02X%02X\n", ((byte & 0x7F) + 1)*16, display.hdma1, display.hdma2, (display.hdma3 & 0x1F) + 0x80, display.hdma4);
#endif
                display.hdma5 = byte;   // display_cycle() will handle the rest from here
                if(!(display.stat & 3)) {   // already in mode 0 (H-blank)
                    handle_hblank_hdma();
                }
            } else {
                // differentiate between general purpose DMA and cancelling H-blank DMA
                if(display.hdma5 == 0xFF || !(display.hdma5 & 0x80)) {
                    display.hdma5 = byte;
                    handle_general_hdma();
                } else {
#ifdef DISPLAY_LOG
                    write_log("[display] cancelled H-blank DMA transfer\n");
#endif
                    display.hdma5 &= 0x7F;
                }
            }

        } else {
            //write_log("[display] write to HDMA5 register value 0x%02X in non-CGB mode, ignoring...\n", byte);
        }
        return;
    case BGPI:
        if(!is_cgb) {
            //write_log("[display] write to BGPI register value 0x%02X in non-CGB mode, ignoring...\n", byte);
        } else {
            display.bgpi = byte;
        }
        return;
    case OBPI:
        if(!is_cgb) {
            //write_log("[display] write to OBPI register value 0x%02X in non-CGB mode, ignoring...\n", byte);
        } else {
            display.obpi = byte;
        }
        return;
    case BGPD:
        if(!is_cgb) {
            //write_log("[display] write to BGPD register value 0x%02X in non-CGB mode, ignoring...\n", byte);
        } else {
            int index = display.bgpi & 0x3F;
            display.bgpd[index] = byte;

            if(display.bgpi & 0x80) {   // auto increment
                index++;
                display.bgpi = (index & 0x3F) | 0x80;
            }
        }
        return;
    case OBPD:
        if(!is_cgb) {
            //write_log("[display] write to OBPD register value 0x%02X in non-CGB mode, ignoring...\n", byte);
        } else {
            int index = display.obpi & 0x3F;
            display.obpd[index] = byte;

            if(display.obpi & 0x80) {   // auto increment
                index++;
                display.obpi = (index & 0x3F) | 0x80;
            }
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
    case VBK:
        if(is_cgb) {
            return display.vbk;
        } else {
            //write_log("[display] undefined read from VBK in non-CGB mode, returning ones\n");
            return 0xFF;
        }
    case HDMA1:
        if(is_cgb) return display.hdma1;
        else return 0xFF;
    case HDMA2:
        if(is_cgb) return display.hdma2;
        else return 0xFF;
    case HDMA3:
        if(is_cgb) return display.hdma3;
        else return 0xFF;
    case HDMA4:
        if(is_cgb) return display.hdma4;
        else return 0xFF;
    case HDMA5:
        if(is_cgb) {
            if(display.hdma5 == 0xFF) return 0xFF;
            else return display.hdma5 ^ 0x80;   // 0 = active, 1 = inactive, contrary to common sense 
        }
        else return 0xFF;
    default:
        write_log("[memory] unimplemented read from IO port 0x%04X\n", addr);
        die(-1, NULL);
    }

    return 0xFF;    // unreachable
}

void scale_xline(uint32_t *new, uint32_t *old, int scaled_width) {
    for(int i = 0; i < scaled_width; i++) {
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

            scale_xline(dst, src, scaled_w);
        }
    }

    // write it to the screen
    /*if(surface->format->BytesPerPixel == 4) {
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
        drawn_frames++;
    }*/

    update_window(scaled_framebuffer);
}

void cgb_bg_palette(int palette) {  // dump the palette into cgb_palette[]
    uint16_t color16;
    uint32_t color32;

    for(int i = 0; i < 4; i++) {
        color16 = display.bgpd[(palette<<3)+(i<<1)] & 0xFF;
        color16 |= (display.bgpd[(palette<<3)+(i<<1)+1] & 0xFF) << 8;
        color32 = truecolor(color16);

        cgb_palette[i] = color32;
    }
}

void cgb_obj_palette(int palette) {  // dump the palette into cgb_palette[]
    uint16_t color16;
    uint32_t color32;

    for(int i = 0; i < 4; i++) {
        color16 = display.obpd[(palette<<3)+(i<<1)] & 0xFF;
        color16 |= (display.obpd[(palette<<3)+(i<<1)+1] & 0xFF) << 8;
        color32 = truecolor(color16);

        cgb_palette[i] = color32;
    }
}

void hflip_tile(uint32_t *buffer, int x, int y) {
    // flips an 8x8 tile within a 256x256 buffer
    // to be used in backgrounds, windows, and SGB borders
    //write_log("flipping tile at x,y %d,%d\n", x, y);

    uint32_t temp_color;

    uint32_t *ptr = (uint32_t *)((void *)buffer + (y * 256*4) + (x * 4));

    for(int i = 0; i < 8; i++) {
        for(int j = 0; j < 4; j++) {
            temp_color = ptr[7-j];
            ptr[7-j] = ptr[j];
            ptr[j] = temp_color;
        }

        ptr += 256;
    }
}

void vflip_tile(uint32_t *buffer, int x, int y) {
    // flips an 8x8 tile within a 256x256 buffer
    uint32_t temp_color;

    uint32_t *ptr1 = (uint32_t *)((void *)buffer + (y * 256*4) + (x * 4));
    uint32_t *ptr2 = (uint32_t *)((void *)buffer + ((y+7) * 256*4) + (x * 4));

    for(int i = 0; i < 4; i++) {
        for(int j = 0; j < 8; j++) {
            temp_color = ptr1[j];
            ptr1[j] = ptr2[j];
            ptr2[j] = temp_color;
        }

        ptr1 += 256;
        ptr2 -= 256;
    }
}

void plot_bg_tile(int is_window, int x, int y, uint8_t tile, uint8_t *tile_data, uint8_t cgb_flags) {
    // x and y are in tiles, not pixels
    int xp = x << 3;    // x8
    int yp = y << 3;

    int visible_row;    // only draw one row, save 8x performance

    if(!is_window) {
        if(display.scy >= 113) {    // 255 minus 143
            // a wraparound will inevitably occur
            int bg_line = display.scy + display.ly;

            if(bg_line >= 256) {
                // wrap occured
                bg_line -= 256;

                int wrapped_ly = display.ly - (256 - display.scy);
                if(!((wrapped_ly) >= yp && (wrapped_ly) <= (yp+7))) {
                    return;
                }

                visible_row = wrapped_ly - yp;
            } else {
                // no wrap
                if(!((display.ly+display.scy) >= yp && (display.ly+display.scy) <= (yp+7))) {
                    return;   // save a fuckton of performance
                }

                visible_row = (display.ly+display.scy) - yp;
            }
        } else {
            if(!((display.ly+display.scy) >= yp && (display.ly+display.scy) <= (yp+7))) {
                return;   // save a fuckton of performance
            }

            visible_row = (display.ly+display.scy) - yp;
        }
    } else {
        if(xp >= GB_WIDTH || yp >= GB_HEIGHT) return;
        if(!(display.ly >= (yp+display.wy) && display.ly <= (yp+display.wy+7))) return;

        visible_row = display.ly - (yp+display.wy);
    }

    //if(!is_window) {
        //write_log("tile xp %d yp %d LY %d visible row %d\n", xp, yp, display.ly, visible_row);
    //}

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

    int cgb_palette_number;

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

    if(is_cgb && (cgb_flags & 0x08)) {
        // tile is in bank 1
        ptr += 8192;
    }

    if(is_cgb) {
        cgb_palette_number = cgb_flags & 7;
        cgb_bg_palette(cgb_palette_number);
    }

    // 8x8 tiles
    for(int i = 0; i < 8; i++) {
        //printf("data for row %d is %02X %02X\n", i, ptr[0], ptr[1]);

        if(i == visible_row) {
            for(int j = 7; j >= 0; j--) {

                /*int s = 6 - ((j % 3) * 2);
                data = *ptr >> s;
                data &= 3;*/

                /*data = (ptr[1] >> (7 - j));
                data <<= 1;
                data &= 2;  // keep only bit 1
                data |= ptr[0] >> (7 - j) & 1;*/

                data_hi = (ptr[1] >> (j)) & 1;
                data_hi <<= 1;

                data_lo = (ptr[0] >> (j));
                data_lo &= 1;

                data = data_hi | data_lo;

                //printf("data for x/y %d/%d is %d\n", i, j, data);

                if(!is_cgb) {
                    color_index = (display.bgp >> (data * 2)) & 3;
                    color = bw_palette[color_index];
                } else {
                    color = cgb_palette[data];
                }

                background_buffer[(yp * 256) + xp] = color;

                /*if(color != 0xFFFFFF) {
                    printf("h");
                }*/

                xp++;
            }
        }

        yp++;
        xp = x << 3;    // x8
        ptr += 2;
    }

    if(is_cgb) {
        if(cgb_flags & 0x20) hflip_tile(background_buffer, x << 3, y << 3);
        if(cgb_flags & 0x40) vflip_tile(background_buffer, x << 3, y << 3);
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

inline void vflip_sprite(uint32_t *sprite_colors, uint8_t *sprite_data) {
    // vertical flip
    uint32_t temp_color;
    uint8_t temp_data;

    uint32_t *ptr2_colors = sprite_colors + 56;
    uint8_t *ptr2_data = sprite_data + 56;

    for(int y = 0; y < 4; y++) {
        for(int x = 0; x < 8; x++) {
            temp_color = sprite_colors[x];
            temp_data = sprite_data[x];

            sprite_colors[x] = ptr2_colors[x];
            sprite_data[x] = ptr2_data[x];

            ptr2_colors[x] = temp_color;
            ptr2_data[x] = temp_data;
        }

        sprite_colors += 8;
        sprite_data += 8;
        ptr2_colors -= 8;
        ptr2_data -= 8;
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

    // 8x8 tiles
    uint8_t *tile_data = vram + 0x0000;     // always starts at 0x8000, unlike bg/window
    uint8_t *ptr = tile_data + (tile * 16);

    uint32_t sprite_colors[64];    // 8x8
    uint8_t sprite_data[64];
    int sprite_data_index = 0;
    int cgb_palette_number;

    if(!is_cgb) {
        // get bg color zero for layering
        bg_color_zero = bw_palette[display.bgp & 3];
    } else {
        cgb_bg_palette(0);
        bg_color_zero = cgb_palette[0];

        // prepare cgb palette
        cgb_palette_number = flags & 7;
        cgb_obj_palette(cgb_palette_number);

        if(flags & 0x08) ptr += 8192;   // bank 1
    }

    sprite_data_index = 0;
    for(int i = 0; i < 8; i++) {
        for(int j = 7; j >= 0; j--) {
            data_hi = (ptr[1] >> (j)) & 1;
            data_hi <<= 1;

            data_lo = (ptr[0] >> (j));
            data_lo &= 1;

            data = data_hi | data_lo;

            if(!is_cgb) {
                // monochrome palettes
                if(flags & 0x10) color_index = (display.obp1 >> (data * 2)) & 3;    // palette 1
                else color_index = (display.obp0 >> (data * 2)) & 3;    // palette 0
                color = bw_palette[color_index];
            } else {
                // cgb palettes
                color = cgb_palette[data];
            }

            sprite_colors[sprite_data_index] = color;
            sprite_data[sprite_data_index] = data;

            sprite_data_index++;
        }

        ptr += 2;
    }

    // check if we need to flip this sprite
    if(flags & 0x20) hflip_sprite(sprite_colors, sprite_data);      // horizontal flip
    if(flags & 0x40) vflip_sprite(sprite_colors, sprite_data);    // vertical flip

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
    uint32_t *src = temp_framebuffer + (display.ly * GB_WIDTH);
    uint32_t *dst = framebuffer + (display.ly * GB_WIDTH);

    if(is_sgb && sgb_screen_mask) {
        uint32_t sgb_blank_color;
        switch(sgb_screen_mask) {
        case 1:         // freeze at current frame
            return;
        case 2:         // freeze black
            sgb_blank_color = bw_palette[3];
            break;
        case 3:         // freeze color zero
        default:
            sgb_blank_color = bw_palette[0];
        }

        for(int i = 0; i < GB_WIDTH; i++) {
            dst[i] = sgb_blank_color;
        }

        return;
    }

    // renders a single horizontal line
    copy_oam(oam);

    uint8_t *bg_win_tiles;
    if(display.lcdc & 0x10) bg_win_tiles = vram + 0;    // 0x8000-0x8FFF
    else bg_win_tiles = vram + 0x800;    // 0x8800-0x97FF

    // test if background is enabled
    if(display.lcdc & 0x01) {
        uint8_t *bg_map;
        uint8_t *bg_cgb_flags;
        if(display.lcdc & 0x08) bg_map = vram + 0x1C00;     // 0x9C00-0x9FFF
        else bg_map = vram + 0x1800;     // 0x9800-0x9BFF

        bg_cgb_flags = bg_map + 8192;   // next bank

        for(int y = 0; y < 32; y++) {
            for(int x = 0; x < 32; x++) {
                plot_bg_tile(0, x, y, *bg_map, bg_win_tiles, *bg_cgb_flags);
                bg_map++;
                bg_cgb_flags++;
            }
        }

        // here the background has been drawn, copy the visible part of it
        //write_log("[display] rendering background, SCY = %d, SCX = %d, LY = %d\n", display.scy, display.scx, display.ly);
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
            temp_framebuffer[i] = bw_palette[0];
        }
    }

    // window layer on top of the background
    if(display.lcdc & 0x20) { // && display.wx >= 7 && display.wx <= 166 && display.wy <= 143) {
        // window enabled
        uint8_t *win_map;
        uint8_t *win_cgb_flags;
        if(display.lcdc & 0x40) win_map = vram + 0x1C00;    // 0x9C00-0x9FFF
        else win_map = vram + 0x1800;   // 0x9800-0x9BFF

        win_cgb_flags = win_map + 8192;     // next bank

        // windows have the same format as backgrounds
        for(int y = 0; y < 32; y++) {
            for(int x = 0; x < 32; x++) {
                plot_bg_tile(1, x, y, *win_map, bg_win_tiles, *win_cgb_flags);
                win_map++;
                win_cgb_flags++;
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
            // 8x16 sprites
            uint8_t *oam_data = oam;
            uint8_t tile_store;

            for(int i = 0; i < 40; i++) {
                tile_store = oam_data[2];

                oam_data[2] &= 0xFE;    // upper tile
                plot_small_sprite(i);
                oam_data[2] |= 0x01;    // lower tile
                oam_data[0] += 8;       // y - lower tile
                plot_small_sprite(i);

                oam_data[2] = tile_store;
                oam_data[0] -= 8;       // back to what it was

                oam_data += 4;
            }
        } else {
            for(int i = 0; i < 40; i++) {   // 40 sprites
                plot_small_sprite(i);
            }
        }
    }

    line_rendered = 1;

    // done, copy the singular line we were at
    if(using_sgb_palette) {
        return sgb_recolor(dst, src, display.ly, bw_palette);
    }

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
            line_rendered = 0;
            if(display.ly >= 154) {
                // vblank is now over
                display.stat &= 0xFC;
                display.stat |= 2;      // enter mode 2
                display.ly = 0;

                if(display.stat & 0x20) {
                    send_interrupt(1);
                }
            }

            if(display.ly == display.lyc) {
                // TODO: send STAT interrupt
                display.stat |= 0x04;   // coincidence flag
                if(display.stat & 0x40) {
                    //write_log("[display] sending STAT interrupt at LY=LYC=%d\n", display.ly);
                    send_interrupt(1);
                }
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

            if(mode != 2 && display.stat & 0x20) {
                // just entered mode 2
                //write_log("entered mode 2 on line %d\n", display.ly);
                send_interrupt(1);
            }
        } else if(display_cycles <= 251) {
            // mode 3 -- reading OAM and VRAM
            display.stat &= 0xFC;
            display.stat |= 3;

            // complete one line
            if((framecount > frameskip) && !line_rendered) render_line();
        } else if(display_cycles <= 455) {
            // mode 0
            display.stat &= 0xFC;

            if(mode != 0 && display.stat & 0x08) {
                // just entered mode 0
                //die(-1, "entered mode 0 STAT\n");
                send_interrupt(1);

                // handle CGB HDMA transfer
                if(is_cgb && display.hdma5 & 0x80 && display.hdma5 != 0xFF) {
                    handle_hblank_hdma();
                }
            }

        } else if(display_cycles >= 456) {
            // a horizontal line has been completed
            display_cycles -= 456;  // dont lose any cycles

            display.ly++;
            line_rendered = 0;
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
                /* // return to mode zero       -- what?
                display.stat &= 0xFC; */

                // mode TWO not zero
                display.stat &= 0xFC;
                display.stat |= 2;

                if(mode != 2 && display.stat & 0x20) {
                    // just entered mode 2
                    //write_log("entered mode 2 on line %d\n", display.ly);
                    send_interrupt(1);
                }
            }

            if(display.ly == display.lyc) {
                // TODO: send STAT interrupt
                display.stat |= 0x04;   // coincidence flag
                if(display.stat & 0x40) {
                    //write_log("[display] sending STAT interrupt at LY=LYC=%d\n", display.ly);
                    send_interrupt(1);
                }
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
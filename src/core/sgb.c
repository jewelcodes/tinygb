
/* tinygb - a tiny gameboy emulator
   (c) 2022 by jewel */

#include <tinygb.h>
#include <ioports.h>
#include <string.h>
#include <sgb.h>

// Super Gameboy implementation

#define SGB_LOG

int sgb_transferring = 0;   // interfering with writes to 0xFF00
int sgb_interfere = 0;      // interfering with reads from 0xFF00
int sgb_current_bit = 0;
int sgb_command_size;
int using_sgb_palette = 0;
int using_sgb_border = 0;
int gb_x, gb_y;
int sgb_scaled_h, sgb_scaled_w;

sgb_command_t sgb_command;
sgb_palette_t sgb_palettes[4];
sgb_attr_block_t sgb_attr_blocks[18];   // maximum

int sgb_attr_block_count = 0;
int sgb_screen_mask = 0;

uint8_t sgb_current_joypad = 0x0F;      // 0x0C-0x0F
int sgb_joypad_count;
uint8_t sgb_joypad_return;

uint8_t *sgb_palette_data;
uint8_t *sgb_tiles;
uint8_t *sgb_border_map;

uint32_t *sgb_border;
uint32_t *sgb_scaled_border;
sgb_border_palette_t sgb_border_palettes[4];
uint32_t sgb_color_zero;

void render_sgb_border();

void sgb_start() {
    sgb_palette_data = calloc(1, 4096);
    sgb_tiles = calloc(1, 8192);
    sgb_border_map = calloc(1, 4096);

    sgb_border = calloc(SGB_WIDTH*SGB_HEIGHT, 4);
    if(scaling != 1) sgb_scaled_border = calloc(SGB_WIDTH*SGB_HEIGHT, 4*scaling*scaling*4);
    else sgb_scaled_border = sgb_border;

    if(!sgb_palette_data || !sgb_tiles || !sgb_border_map || !sgb_border || !sgb_scaled_border) {
        write_log("[sgb] unable to allocate memory\n");
        die(-1, "");
    }

    sgb_scaled_h = SGB_HEIGHT*scaling;
    sgb_scaled_w = SGB_WIDTH*scaling;
}

inline uint32_t truecolor(uint16_t color16) {
    uint32_t color32;
    int r, g, b; 

    r = color16 & 31;
    g = (color16 >> 5) & 31;
    b = (color16 >> 10) & 31;

    r <<= 3;    // x8
    g <<= 3;
    b <<= 3;

    color32 = (r << 16) | (g << 8) | b;
    return color32;
}

void sgb_vram_transfer(uint8_t *dst) {
    // transfers data from VRAM into SNES memory
    uint8_t lcdc = read_byte(LCDC);     // display control IO port
    if(!(lcdc & LCDC_ENABLE)) {
        write_log("[sgb] warning: attempting to transfer data from VRAM when display is disabled, returning zeroes\n");
        memset(dst, 0, 4096);
        return;
    }

    uint16_t tiles;

    if(lcdc & 0x10) tiles = 0x8000;
    else tiles = 0x8800;

    for(int i = 0; i < 4096; i++) {
        dst[i] = read_byte(tiles+i);
    }
}

void create_sgb_palette(int sgb_palette, int system_palette) {
    uint16_t *data = (uint16_t *)(sgb_palette_data + (system_palette * 8));

    sgb_palettes[sgb_palette].colors[0] = truecolor(data[0]);
    sgb_palettes[sgb_palette].colors[1] = truecolor(data[1]);
    sgb_palettes[sgb_palette].colors[2] = truecolor(data[2]);
    sgb_palettes[sgb_palette].colors[3] = truecolor(data[3]);

    if(sgb_palettes[sgb_palette].colors[0] != sgb_color_zero) {
        sgb_color_zero = sgb_palettes[sgb_palette].colors[0];
        if(using_sgb_border) render_sgb_border();
    }

#ifdef SGB_LOG
    for(int i = 0; i < 4; i++) {
        int r, g, b; 
        r = (sgb_palettes[sgb_palette].colors[i] >> 16) & 0xFF;
        g = (sgb_palettes[sgb_palette].colors[i] >> 8) & 0xFF;
        b = sgb_palettes[sgb_palette].colors[i] & 0xFF;

        write_log("[sgb]  SGB palette %d color %d = \e[38;2;%d;%d;%dm#%06X\e[0m\n", sgb_palette, i, r, g, b, sgb_palettes[sgb_palette].colors[i]);
    }
#endif
}

void create_sgb_border_palettes() {
    uint8_t *data = (uint8_t *)(sgb_border_map + 0x800);
    uint16_t color16;
    uint32_t color32;

    for(int i = 0; i < 4; i++) {
        for(int j = 0; j < 16; j++) {
            color16 = data[(i * 32)+(j*2)] & 0xFF;
            color16 |= (data[(i * 32)+(j*2)+1]) << 8;

            color32 = truecolor(color16);

#ifdef SGB_LOG
            int r = (color32 >> 16) & 0xFF;
            int g = (color32 >> 8) & 0xFF;
            int b = color32 & 0xFF;

            write_log("[sgb]  SGB border palette %d color %d = \e[38;2;%d;%d;%dm#%06X\e[0m\n", i, j, r, g, b, color32);
#endif

            sgb_border_palettes[i].colors[j] = color32;
        }
    }

    //sgb_color_zero = sgb_border_palettes[3].colors[0];
}

void plot_sgb_tile(int x, int y, uint8_t tile, int palette, int xflip, int yflip) {
    //write_log("[sgb] plotting tile %d at x,y %d,%d with palette number %d\n", tile, x, y, palette);
    int xp = x << 3;
    int yp = y << 3;

    uint8_t *ptr = (uint8_t *)((sgb_tiles) + (tile * 32));

    uint8_t color_index;
    uint8_t data3, data2, data1, data0;

    uint32_t color;

    // 8x8 tiles
    for(int i = 0; i < 8; i++) {
        for(int j = 7; j >= 0; j--) {
            data3 = (ptr[16+1] >> (j)) & 1;
            data2 = (ptr[16] >> (j)) & 1;
            data1 = (ptr[1] >> (j)) & 1;
            data0 = (ptr[0] >> (j)) & 1;

            data3 <<= 3;
            data2 <<= 2;
            data1 <<= 1;

            color_index = data3 | data2 | data1 | data0;

            //write_log("color index: %d\n", color_index);

            if(color_index) color = sgb_border_palettes[palette].colors[color_index];
            else color = sgb_color_zero;
            sgb_border[(yp * 256) + xp] = color;
            xp++;
        }

        yp++;
        xp = x << 3;
        ptr += 2;
    }

    if(xflip) hflip_tile(sgb_border, x << 3, y << 3);
    if(yflip) vflip_tile(sgb_border, x << 3, y << 3);
}

void render_sgb_border() {
    if(sgb_screen_mask) return;

#ifdef SGB_LOG
    write_log("[sgb]  SGB border was modified, rendering...\n");
#endif

    create_sgb_border_palettes();

    uint8_t *map = sgb_border_map;

    uint8_t tile, palette, xflip, yflip;

    // sgb border is 32x28 tiles
    for(int i = 0; i < 28; i++) {
        for(int j = 0; j < 32; j++) {
            //write_log("map entry %d,%d is 0x%04X\n", j, i, (uint16_t)(map[0] | (map[1] << 8)));

            tile = map[0];
            palette = (map[1] >> 2) & 3;
            if(map[1] & 0x40) xflip = 1;
            else xflip = 0;

            if(map[1] & 0x80) yflip = 1;
            else yflip = 0;

            map += 2;

            plot_sgb_tile(j, i, tile, palette, xflip, yflip);
        }
    }

    // scale up the buffer
    if(scaling != 1) {
        for(int y = 0; y < sgb_scaled_h; y++) {
            uint32_t *dst = sgb_scaled_border + (y * sgb_scaled_w);
            uint32_t *src = sgb_border + ((y / scaling) * SGB_WIDTH);

            scale_xline(dst, src, sgb_scaled_w);
        }
    }

    update_border(sgb_scaled_border);
}

//
// individual SGB commands
//

void sgb_mlt_req() {
    if(sgb_command.data[0] & 0x01) {
        if(sgb_command.data[0] & 0x02) {
            // four players
            sgb_joypad_count = 4;
        } else {
            sgb_joypad_count = 2;
        }

#ifdef SGB_LOG
        write_log("[sgb] MLT_REQ: enabled %d multiplayer joypads\n", sgb_joypad_count);
#endif
        sgb_current_joypad = 0x0F;
        sgb_interfere = 1;
    } else {
#ifdef SGB_LOG
        write_log("[sgb] MLT_REQ: disabled multiplayer joypads\n");
#endif
        sgb_joypad_count = 1;
        sgb_interfere = 0;
    }
}

void sgb_mask_en() {
    sgb_command.data[0] %= 3;
    sgb_screen_mask = sgb_command.data[0];

#ifdef SGB_LOG
    if(sgb_command.data[0] == 0) {
        write_log("[sgb] MASK_EN: cancelling screen mask\n");
    } else if(sgb_command.data[0] == 1) {
        write_log("[sgb] MASK_EN: freezing current screen\n");
    } else if(sgb_command.data[0] == 2) {
        write_log("[sgb] MASK_EN: freezing screen at black\n");
    } else {
        write_log("[sgb] MASK_EN: freezing screen at color zero\n");
    }
#endif

    if(using_sgb_border) render_sgb_border();
}

void sgb_pal_trn() {
#ifdef SGB_LOG
    write_log("[sgb] PAL_TRN: transferring palette data from VRAM to SNES\n");
#endif

    sgb_vram_transfer(sgb_palette_data);
}

void sgb_pal_set() {
    uint16_t *palette_numbers = (uint16_t *)(&sgb_command.data[0]);

    for(int i = 0; i < 4; i++) {
#ifdef SGB_LOG
        write_log("[sgb] PAL_SET: palette %d -> system palette %d\n", i, palette_numbers[i]);
#endif

        create_sgb_palette(i, palette_numbers[i]);
    }
}

void sgb_attr_blk() {
#ifdef SGB_LOG
    write_log("[sgb] ATTR_BLK: setting color attributes with %d datasets\n", sgb_command.data[0]);
#endif

    sgb_attr_block_count = sgb_command.data[0];

    memset(&sgb_attr_blocks, 0, sizeof(sgb_attr_block_t)*18);

    uint8_t *ptr = &sgb_command.data[1];
    for(int i = 0; i < sgb_command.data[0]; i++) {
        //write_log("[sgb] ATTR_BLK entry %d: flags 0x%02X from X/Y %d/%d to %d/%d\n", i, ptr[0], ptr[2], ptr[3], ptr[4], ptr[5]);
        if(ptr[0] & 0x01) sgb_attr_blocks[i].inside = 1;
        if(ptr[0] & 0x02) sgb_attr_blocks[i].surrounding = 1;
        if(ptr[0] & 0x04) sgb_attr_blocks[i].outside = 1;

        sgb_attr_blocks[i].palette_inside = ptr[1] & 3;
        sgb_attr_blocks[i].palette_surrounding = (ptr[1] >> 2) & 3;
        sgb_attr_blocks[i].palette_outside = (ptr[1] >> 4) & 3;

        sgb_attr_blocks[i].x1 = ptr[2] * 8;
        sgb_attr_blocks[i].y1 = ptr[3] * 8;
        sgb_attr_blocks[i].x2 = (ptr[4] + 1) * 8;
        sgb_attr_blocks[i].y2 = (ptr[5] + 1) * 8;

#ifdef SGB_LOG
        write_log("[sgb]  %d: flags 0x%02X from X,Y %d,%d to %d,%d", i, ptr[0], sgb_attr_blocks[i].x1, sgb_attr_blocks[i].y1, sgb_attr_blocks[i].x2, sgb_attr_blocks[i].y2);
        if(ptr[0]) {
            write_log(", ");
            if(sgb_attr_blocks[i].inside) {
                write_log("in = %d ", sgb_attr_blocks[i].palette_inside);
            }

            if(sgb_attr_blocks[i].outside) {
                write_log("out = %d ", sgb_attr_blocks[i].palette_outside);
            }

            if(sgb_attr_blocks[i].surrounding) {
                write_log("surround = %d ", sgb_attr_blocks[i].palette_surrounding);
            }
        }

        write_log("\n");
#endif

        ptr += 6;
    }

    using_sgb_palette = 1;
}

void sgb_chr_trn() {
#ifdef SGB_LOG
    write_log("[sgb] CHR_TRN: transferring data for tiles %s from VRAM to SNES\n", (sgb_command.data[0] & 1) ? "0x80-0xFF" : "0x00-0x7F");
#endif

    if(sgb_command.data[0] & 1) {
        sgb_vram_transfer(sgb_tiles+4096);
    } else {
        sgb_vram_transfer(sgb_tiles);
    }

    if(using_sgb_border) render_sgb_border();
}

void sgb_pct_trn() {
#ifdef SGB_LOG
    write_log("[sgb] PCT_TRN: transferring data for SGB border from VRAM to SNES\n");
#endif

    sgb_vram_transfer(sgb_border_map);

    if(config_border) {
        if(!using_sgb_border) {
            resize_sgb_window();
        }

        using_sgb_border = 1;
        gb_x = (SGB_WIDTH / 2) - (GB_WIDTH / 2);
        gb_y = (SGB_HEIGHT / 2) - (GB_HEIGHT / 2);
        gb_x *= scaling;
        gb_y *= scaling;

        render_sgb_border();
    }
}

void handle_sgb_command() {
    uint8_t command;
    command = sgb_command.command_length >> 3;

    switch(command) {
    case SGB_MLT_REQ:
        return sgb_mlt_req();
    case SGB_MASK_EN:
        return sgb_mask_en();
    case SGB_PAL_TRN:
        return sgb_pal_trn();
    case SGB_PAL_SET:
        return sgb_pal_set();
    case SGB_ATTR_BLK:
        return sgb_attr_blk();
    case SGB_CHR_TRN:
        return sgb_chr_trn();
    case SGB_PCT_TRN:
        return sgb_pct_trn();
    default:
        write_log("[sgb] unimplemented command 0x%02X, ignoring...\n", command);
        return;
    }
}

// for joypad.c

void sgb_write(uint8_t byte) {
    uint8_t p14 = (byte >> 4) & 1;
    uint8_t p15 = (byte >> 5) & 1;

    if(!sgb_transferring && !p14 && !p15) {
        // reset signal
        sgb_transferring = 1;

        if(sgb_current_bit >= sgb_command_size) {
            sgb_current_bit = 0;
            memset(&sgb_command, 0, sizeof(sgb_command_t));
        } else {
            // continuing a transfer
            sgb_command.stopped = 1;
            sgb_current_bit--;
            //write_log("continuing a transfer from bit %d\n", sgb_current_bit);
        }
    }

    if(!sgb_transferring && sgb_interfere) {
        // here the program is trying to read SGB state
        if(p14 && p15) {
            // both ones, return current joypad
            sgb_joypad_return = sgb_current_joypad;

            write_log("[sgb] current joypad is 0x%02X\n", sgb_joypad_return);

            sgb_current_joypad--;
            if(sgb_current_joypad < 0x0C) sgb_current_joypad = 0x0F;    // wrap
        } else if(!p14 && p15) {
            // p14 = 0; p15 = 1; read directions
            if(sgb_joypad_return == 0x0F) sgb_joypad_return = (~(pressed_keys >> 4)) & 0x0F;
            else sgb_joypad_return = 0x0F;
        } else if(p14 && !p15) {
            // p14 = 1; p15 = 0; read buttons
            if(sgb_joypad_return == 0x0F) sgb_joypad_return = (~pressed_keys) & 0x0F;
            else sgb_joypad_return = 0x0F;
        } else {
            write_log("[sgb] unhandled unreachable code\n");
            die(-1, "");
        }

        return;
    }

    if(p14 == p15) {
        // if both zero, it's a reset pulse, ignore
        // likewise if both 1, it's a "wait" pulse, also ignore
        return;
    }

    // here we know they're different, so keep going
    if(!p14) {
        // a zero bit is being transferred
        // check if the PREVIOUS bit was a stop bit
        if(sgb_command.stopped) {
            sgb_command.stopped = 0;
            goto count;
        } else {
            // previous bit was NOT a stop bit, check if the current one is
            if((sgb_current_bit >= 128) && !(sgb_current_bit % 128)) {
                // this is a stop bit
                sgb_command.stopped = 1;
                sgb_transferring = 0;

                //write_log("[sgb] stop bit at %d\n", sgb_current_bit);

                sgb_command_size = (sgb_command.command_length & 7) * 16 * 8;   // in bits
                if(sgb_current_bit >= sgb_command_size) {
                    handle_sgb_command();
                    return;
                }
            }

            // nope, still not a stop bit
            sgb_command.stopped = 0;
            goto count;
        }
    }

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

count:
    //write_log("write bit %d\n", sgb_current_bit);
    sgb_current_bit++;
}

inline uint8_t sgb_read() {
    return sgb_joypad_return;
}

inline int get_index_from_palette(uint32_t color, uint32_t *palette) {
    for(int i = 0; i < 4; i++) {
        if(palette[i] == color) return i;
    }

    write_log("[sgb] somehow landed on a color that isn't in an existing palette, quitting due to data corruption\n");
    die(-1, "");
    return -1;  // unreachale
}

inline int get_palette_from_pos(int x, int y) {
    // THESE HAVE TO BE READ IN REVERSE ORDER
    // aka priority is for the one stated later
    for(int i = sgb_attr_block_count - 1; i >= 0; i--) {
        // check if inside or outside, in that order
        if(sgb_attr_blocks[i].inside) {
            if(x >= sgb_attr_blocks[i].x1 && x <= sgb_attr_blocks[i].x2 && y >= sgb_attr_blocks[i].y1 && y <= sgb_attr_blocks[i].y2) {
                return sgb_attr_blocks[i].palette_inside;
            }
        }

        if(sgb_attr_blocks[i].outside) {
            if(!(x >= sgb_attr_blocks[i].x1 && x <= sgb_attr_blocks[i].x2 && y >= sgb_attr_blocks[i].y1 && y <= sgb_attr_blocks[i].y2)) {
                return sgb_attr_blocks[i].palette_outside;
            }
        }

        if(sgb_attr_blocks[i].surrounding) {
            if((x >= sgb_attr_blocks[i].x1 && x <= sgb_attr_blocks[i].x2 && y >= sgb_attr_blocks[i].y1 && y <= sgb_attr_blocks[i].y2)) {
                return sgb_attr_blocks[i].palette_surrounding;
            }
        }
    }

    // somehow couldn't return anything, so just return zero
    return 0;
}

// recolors one line
void sgb_recolor(uint32_t *dst, uint32_t *src, int ly, uint32_t *bw_palette) {
    int color_index, sgb_palette;
    for(int i = 0; i < GB_WIDTH; i++) {
        color_index = get_index_from_palette(src[i], bw_palette);
        sgb_palette = get_palette_from_pos(i, ly);

        dst[i] = sgb_palettes[sgb_palette].colors[color_index];
    }
}
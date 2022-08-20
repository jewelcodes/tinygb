
/* tinygb - a tiny gameboy emulator
   (c) 2022 by jewel */

#include <tinygb.h>
#include <stdio.h>
#include <stdlib.h>
#include <SDL.h>
#include <time.h>

// SDL specific code

long rom_size;
int scaling = 4;
int frameskip = 0;  // no skip

SDL_Window *window;
SDL_Surface *surface;
timing_t timing;
char *rom_filename;

// SDL Config
SDL_Keycode key_a;
SDL_Keycode key_b;
SDL_Keycode key_start;
SDL_Keycode key_select;
SDL_Keycode key_up;
SDL_Keycode key_down;
SDL_Keycode key_left;
SDL_Keycode key_right;
SDL_Keycode key_throttle;

SDL_Keycode sdl_get_key(char *keyname) {
    if(!keyname) return SDLK_UNKNOWN;

    if(!strcmp("a", keyname)) return SDLK_a;
    else if(!strcmp("b", keyname)) return SDLK_b;
    else if(!strcmp("c", keyname)) return SDLK_c;
    else if(!strcmp("d", keyname)) return SDLK_d;
    else if(!strcmp("e", keyname)) return SDLK_e;
    else if(!strcmp("f", keyname)) return SDLK_f;
    else if(!strcmp("g", keyname)) return SDLK_g;
    else if(!strcmp("h", keyname)) return SDLK_h;
    else if(!strcmp("i", keyname)) return SDLK_i;
    else if(!strcmp("j", keyname)) return SDLK_j;
    else if(!strcmp("k", keyname)) return SDLK_k;
    else if(!strcmp("l", keyname)) return SDLK_l;
    else if(!strcmp("m", keyname)) return SDLK_m;
    else if(!strcmp("n", keyname)) return SDLK_n;
    else if(!strcmp("o", keyname)) return SDLK_o;
    else if(!strcmp("p", keyname)) return SDLK_p;
    else if(!strcmp("q", keyname)) return SDLK_q;
    else if(!strcmp("r", keyname)) return SDLK_r;
    else if(!strcmp("s", keyname)) return SDLK_s;
    else if(!strcmp("t", keyname)) return SDLK_t;
    else if(!strcmp("u", keyname)) return SDLK_u;
    else if(!strcmp("v", keyname)) return SDLK_v;
    else if(!strcmp("w", keyname)) return SDLK_w;
    else if(!strcmp("x", keyname)) return SDLK_x;
    else if(!strcmp("y", keyname)) return SDLK_y;
    else if(!strcmp("z", keyname)) return SDLK_z;
    else if(!strcmp("0", keyname)) return SDLK_0;
    else if(!strcmp("1", keyname)) return SDLK_1;
    else if(!strcmp("2", keyname)) return SDLK_2;
    else if(!strcmp("3", keyname)) return SDLK_3;
    else if(!strcmp("4", keyname)) return SDLK_4;
    else if(!strcmp("5", keyname)) return SDLK_5;
    else if(!strcmp("6", keyname)) return SDLK_6;
    else if(!strcmp("7", keyname)) return SDLK_7;
    else if(!strcmp("8", keyname)) return SDLK_8;
    else if(!strcmp("9", keyname)) return SDLK_9;
    else if(!strcmp("space", keyname)) return SDLK_SPACE;
    else if(!strcmp("rshift", keyname)) return SDLK_RSHIFT;
    else if(!strcmp("lshift", keyname)) return SDLK_LSHIFT;
    else if(!strcmp("backspace", keyname)) return SDLK_BACKSPACE;
    else if(!strcmp("delete", keyname)) return SDLK_DELETE;
    else if(!strcmp("tab", keyname)) return SDLK_TAB;
    else if(!strcmp("escape", keyname)) return SDLK_ESCAPE;
    else if(!strcmp("exclamation", keyname)) return SDLK_EXCLAIM;
    else if(!strcmp("at", keyname)) return SDLK_AT;
    else if(!strcmp("hash", keyname)) return SDLK_HASH;
    else if(!strcmp("dollar", keyname)) return SDLK_DOLLAR;
    else if(!strcmp("percent", keyname)) return SDLK_PERCENT;
    else if(!strcmp("caret", keyname)) return SDLK_CARET;
    else if(!strcmp("ampersand", keyname)) return SDLK_AMPERSAND;
    else if(!strcmp("asterisk", keyname)) return SDLK_ASTERISK;
    else if(!strcmp("leftparenthesis", keyname)) return SDLK_LEFTPAREN;
    else if(!strcmp("rightparenthesis", keyname)) return SDLK_RIGHTPAREN;

    else return SDLK_UNKNOWN;
}

static void set_sdl_keys() {
    key_a = sdl_get_key(config_file.a);
    if(key_a == SDLK_UNKNOWN) key_a = SDLK_z;

    key_b = sdl_get_key(config_file.b);
    if(key_b == SDLK_UNKNOWN) key_b = SDLK_x;

    key_start = sdl_get_key(config_file.start);
    if(key_start == SDLK_UNKNOWN) key_start = SDLK_RETURN;

    key_select = sdl_get_key(config_file.select);
    if(key_select == SDLK_UNKNOWN) key_select = SDLK_RSHIFT;

    key_up = sdl_get_key(config_file.up);
    if(key_up == SDLK_UNKNOWN) key_up = SDLK_UP;

    key_down = sdl_get_key(config_file.down);
    if(key_down == SDLK_UNKNOWN) key_down = SDLK_DOWN;

    key_left = sdl_get_key(config_file.left);
    if(key_left == SDLK_UNKNOWN) key_left = SDLK_LEFT;

    key_right = sdl_get_key(config_file.right);
    if(key_right == SDLK_UNKNOWN) key_right = SDLK_RIGHT;

    key_throttle = sdl_get_key(config_file.throttle);
    if(key_throttle == SDLK_UNKNOWN) key_throttle = SDLK_SPACE;
}

inline void delay(int ms) {
    SDL_Delay(ms);
}

void destroy_window() {
    SDL_DestroyWindow(window);
    SDL_Quit();
}

void update_window(uint32_t *framebuffer) {
    void *src, *dst;

    if(surface->format->BytesPerPixel == 4) {
        // 32-bpp
        for(int i = 0; i < scaled_h; i++) {
            src = (void *)(framebuffer + (i * scaled_w));

            if(!using_sgb_border) {
                dst = (void *)(surface->pixels + (i * surface->pitch));
            } else {
                dst = (void *)(surface->pixels + ((i + gb_y) * surface->pitch) + (gb_x * 4));
            }
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
    }
}

void update_border(uint32_t *framebuffer) {
    void *src, *dst;

    if(surface->format->BytesPerPixel == 4) {
        // 32-bpp
        for(int i = 0; i < sgb_scaled_h; i++) {
            src = (void *)(framebuffer + (i * sgb_scaled_w));
            dst = (void *)(surface->pixels + (i * surface->pitch));

            memcpy(dst, src, sgb_scaled_w*4);
        }
    } else {
        die(-1, "unimplemented non 32-bpp surfaces\n");
    }

    //SDL_UpdateWindowSurface(window);
}

void resize_sgb_window() {
    SDL_SetWindowSize(window, SGB_WIDTH*scaling, SGB_HEIGHT*scaling);
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    surface = SDL_GetWindowSurface(window);
}

int main(int argc, char **argv) {
    if(argc != 2) {
        fprintf(stdout, "usage: %s rom_name\n", argv[0]);
        return -1;
    }

    open_log();
    open_config();
    set_sdl_keys();

    // open the rom
    rom_filename = argv[1];
    FILE *rom_file = fopen(argv[1], "r");
    if(!rom_file) {
        write_log("unable to open %s for reading\n", argv[1]);
        return -1;
    }

    fseek(rom_file, 0L, SEEK_END);
    rom_size = ftell(rom_file);
    fseek(rom_file, 0L, SEEK_SET);

    write_log("loading rom from file %s, %d KiB\n", argv[1], rom_size/1024);

    rom = malloc(rom_size);
    if(!rom) {
        write_log("unable to allocate memory\n");
        fclose(rom_file);
        return -1;
    }

    if(!fread(rom, 1, rom_size, rom_file)) {
        write_log("an error occured while reading from rom file\n");
        fclose(rom_file);
        free(rom);
        return -1;
    }

    fclose(rom_file);

    // make the main window
    if(SDL_Init(SDL_INIT_VIDEO) < 0) {
        write_log("failed to init SDL: %s\n", SDL_GetError());
        free(rom);
        return -1;
    }

    window = SDL_CreateWindow("tinygb", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, GB_WIDTH*scaling, GB_HEIGHT*scaling, SDL_WINDOW_SHOWN);
    if(!window) {
        write_log("couldn't create SDL window: %s\n", SDL_GetError());
        free(rom);
        SDL_Quit();
        return -1;
    }

    surface = SDL_GetWindowSurface(window);

    write_log("SDL pixel format: %s\n", SDL_GetPixelFormatName(surface->format->format));
    write_log("SDL bits per pixel: %d\n", surface->format->BitsPerPixel);
    write_log("SDL bytes per pixel: %d\n", surface->format->BytesPerPixel);
    write_log("SDL Rmask: 0x%06X\n", surface->format->Rmask);
    write_log("SDL Gmask: 0x%06X\n", surface->format->Gmask);
    write_log("SDL Bmask: 0x%06X\n", surface->format->Bmask);
    write_log("SDL Amask: 0x%08X\n", surface->format->Amask);

    // disgustingly lazy thing for now
    if(surface->format->BytesPerPixel != 3 && surface->format->BytesPerPixel != 4 &&
        surface->format->Rmask != 0xFF0000 && surface->format->Gmask != 0xFF00 && surface->format->Bmask != 0xFF) {
        die(-1, "unsupported surface format; only RGB 24-bpp or 32-bpp are supported\n");
    }

    SDL_UpdateWindowSurface(window);

    // start emulation
    memory_start();
    cpu_start();
    display_start();
    timer_start();
    sound_start();

    SDL_Event e;
    int key, is_down;
    time_t rawtime;
    struct tm *timeinfo;
    int sec = 500;  // any invalid number
    char new_title[256];
    int percentage;
    int throttle_underflow = 0;

    while(1) {
        key = 0;
        is_down = 0;

        while(SDL_PollEvent(&e)) {
            switch(e.type) {
            case SDL_QUIT:
                SDL_DestroyWindow(window);
                SDL_Quit();
                die(0, "");
            case SDL_KEYDOWN:
            case SDL_KEYUP:
                is_down = (e.type == SDL_KEYDOWN);

                // convert SDL keys to internal keys
                // TODO: read these keys from a config file
                /*switch(e.key.keysym.sym) {
                case key_left:
                    key = JOYPAD_LEFT;
                    break;
                case key_right:
                    key = JOYPAD_RIGHT;
                    break;
                case key_up:
                    key = JOYPAD_UP;
                    break;
                case key_down:
                    key = JOYPAD_DOWN;
                    break;
                case key_a:
                    key = JOYPAD_A;
                    break;
                case key_b:
                    key = JOYPAD_B;
                    break;
                case key_start:
                    key = JOYPAD_START;
                    break;
                case key_select:
                    key = JOYPAD_SELECT;
                    break;
                case key_throttle:
                    if(is_down) throttle_enabled = 0;
                    else throttle_enabled = 1;
                default:
                    key = 0;
                    break;
                }*/

                if(e.key.keysym.sym == key_left) key = JOYPAD_LEFT;
                else if(e.key.keysym.sym == key_right) key = JOYPAD_RIGHT;
                else if(e.key.keysym.sym == key_up) key = JOYPAD_UP;
                else if(e.key.keysym.sym == key_down) key = JOYPAD_DOWN;
                else if(e.key.keysym.sym == key_a) key = JOYPAD_A;
                else if(e.key.keysym.sym == key_b) key = JOYPAD_B;
                else if(e.key.keysym.sym == key_start) key = JOYPAD_START;
                else if(e.key.keysym.sym == key_select) key = JOYPAD_SELECT;
                else if(e.key.keysym.sym == key_throttle) {
                    if(is_down) throttle_enabled = 0;
                    else throttle_enabled = 1;
                } else {
                    key = 0;
                }

                break;
            default:
                break;
            }
        }

        if(key) joypad_handle(is_down, key);

        for(timing.current_cycles = 0; timing.current_cycles < timing.main_cycles; ) {
            cpu_cycle();
            display_cycle();
            timer_cycle();
        }


        time(&rawtime);
        timeinfo = localtime(&rawtime);

        if(sec != timeinfo->tm_sec) {
            sec = timeinfo->tm_sec;
            percentage = (drawn_frames * 10000) / 5973;
            sprintf(new_title, "tinygb (%d fps - %d%%)", drawn_frames, percentage);
            SDL_SetWindowTitle(window, new_title);

            // adjust cpu throttle according to acceptable fps (98%-102%)
            if(throttle_enabled) {
                if(percentage < 98) {
                    // emulation is too slow
                    if(!throttle_time) {
                        // throttle_time--;

                        if(!throttle_underflow) {
                            throttle_underflow = 1;
                            write_log("WARNING: CPU throttle interval has underflown, emulation may be too slow\n");
                        }
                    } else {
                        throttle_time--;
                    }
                } else if(percentage > 102) {
                    // emulation is too fast
                    throttle_time++;
                }
            }

            drawn_frames = 0;
        }
    }

    die(0, "");
    return 0;
}
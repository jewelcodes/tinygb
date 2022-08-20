
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

inline void delay(int ms) {
    SDL_Delay(ms);
}

void destroy_window() {
    SDL_DestroyWindow(window);
    SDL_Quit();
}

void update_window(uint32_t *framebuffer) {
    if(surface->format->BytesPerPixel == 4) {
        // 32-bpp
        for(int i = 0; i < scaled_h; i++) {
            void *src = (void *)(framebuffer + (i * scaled_w));
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
    }
}

int main(int argc, char **argv) {
    if(argc != 2) {
        fprintf(stdout, "usage: %s rom_name\n", argv[0]);
        return -1;
    }

    open_log();
    //open_config();

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

    window = SDL_CreateWindow("tinygb", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, GB_WIDTH*scaling, GB_HEIGHT*scaling, SDL_WINDOW_SHOWN);
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
                switch(e.key.keysym.sym) {
                case SDLK_LEFT:
                    key = JOYPAD_LEFT;
                    break;
                case SDLK_RIGHT:
                    key = JOYPAD_RIGHT;
                    break;
                case SDLK_UP:
                    key = JOYPAD_UP;
                    break;
                case SDLK_DOWN:
                    key = JOYPAD_DOWN;
                    break;
                case SDLK_z:
                    key = JOYPAD_A;
                    break;
                case SDLK_x:
                    key = JOYPAD_B;
                    break;
                case SDLK_RETURN:
                    key = JOYPAD_START;
                    break;
                case SDLK_RSHIFT:
                    key = JOYPAD_SELECT;
                    break;

                case SDLK_SPACE:
                    if(is_down) throttle_enabled = 0;
                    else throttle_enabled = 1;
                default:
                    key = 0;
                    break;
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
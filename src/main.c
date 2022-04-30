
/* tinygb - a tiny gameboy emulator
   (c) 2022 by jewel */

#include <tinygb.h>
#include <stdio.h>
#include <stdlib.h>
#include <SDL.h>

long rom_size;
int scaling = 4;

SDL_Window *window;
SDL_Surface *surface;
timing_t timing;

int main(int argc, char **argv) {
    if(argc != 2) {
        fprintf(stdout, "usage: %s rom_name\n", argv[0]);
        return -1;
    }

    open_log();
    //open_config();

    // open the rom
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

    if(fread(rom, 1, rom_size, rom_file) != rom_size) {
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

    // disgustingly lazy thing rn
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
    while(1) {
        while(SDL_PollEvent(&e)) {
            switch(e.type) {
            case SDL_QUIT:
                SDL_DestroyWindow(window);
                SDL_Quit();
                die(0, "");
            case SDL_KEYDOWN:
                cpu_log();
                break;
            default:
                break;
            }
        }

        // one frame
        /*
        // regardless of whether the timer or display is faster, we wanna loop
        // for exactly one frame (59 Hz)
        if(timing.cpu_cycles_timer > timing.cpu_cycles_vline) {
            // refresh rate faster than timer, so keep cycling the display
            // until it's time to update the timer
            for(int i = 0; i < timing.main_cycles; i++) {
                for(cycles = 0; cycles < timing.cpu_cycles_timer; cycles += timing.current_cycles) {
                    for(timing.current_cycles = 0; timing.current_cycles < timing.cpu_cycles_vline; ) {
                        cpu_cycle();
                    }

                    display_cycle();
                }

                timer_cycle();
            }
        } else {
            // here the timer is faster, so we use the timer to track time
            for(int i = 0; i < timing.main_cycles; i++) {
                for(cycles = 0; cycles < timing.cpu_cycles_vline; cycles += timing.current_cycles) {
                    for(timing.current_cycles = 0; timing.current_cycles < timing.cpu_cycles_timer; ) {
                        cpu_cycle();
                    }

                    timer_cycle();
                }

                display_cycle();
            }
        }

        */

        for(timing.current_cycles = 0; timing.current_cycles < timing.main_cycles; ) {
            cpu_cycle();
            display_cycle();
            timer_cycle();
        }

        // one frame was drawn by here
        SDL_UpdateWindowSurface(window);
    }

    die(0, "");
    return 0;
}
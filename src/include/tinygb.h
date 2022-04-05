
/* tinygb - a tiny gameboy emulator
   (c) 2022 by jewel */

#pragma once

#include <stdint.h>
#include <SDL.h>

#define GB_WIDTH    160
#define GB_HEIGHT   144

#define CPU_SPEED   4194304 // Hz
#define CPU_CYCLES  (CPU_SPEED/1000)

typedef struct {
    uint16_t af, bc, de, hl, sp, pc;
} cpu_t;

#define FLAG_ZF     0x80
#define FLAG_N      0x40
#define FLAG_H      0x20
#define FLAG_CY     0x10

extern long rom_size;
extern void *rom, *ram;

extern SDL_Window *window;

void open_log();
void write_log(const char *, ...);
void die(int, const char *, ...);
void memory_start();
void cpu_start();
void cpu_cycle();
void cpu_log();

uint8_t read_byte(uint16_t);
uint16_t read_word(uint16_t);
void write_byte(uint16_t, uint8_t);

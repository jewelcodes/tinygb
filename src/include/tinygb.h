
/* tinygb - a tiny gameboy emulator
   (c) 2022 by jewel */

#pragma once

#include <stdint.h>
#include <SDL.h>

#define GB_WIDTH    160
#define GB_HEIGHT   144

#define GB_CPU_SPEED        4194304 // Hz
#define CGB_CPU_SPEED       8388608

/* DISPLAY:

    Width                       160 px
    Height                      144 px
    Refresh rate                59.7 Hz
    Visible v-lines             144 lines
    Invisible v-lines           10 lines
    Total v-lines               154 lines
    Total refresh time          16.7504 ms
    Time per v-line             0.108769 ms
    Time for visible lines      15.6627 ms
    Time for invisible lines    1.08769 ms
    V-sync pause                1.08769 ms

 * VALUES TO BE USED TO KEEP THINGS IN SYNC:

    CPU Cycles/Millisecond      CPU_SPEED/1000
    CPU Cycles/Refresh Line     Above * Time per v-line

 */

#define REFRESH_RATE            59.7        // Hz
#define TOTAL_REFRESH_TIME      16.7504     // ms
#define REFRESH_TIME_LINE       0.108769    // ms
#define VSYNC_PAUSE             1.08769     // ms
#define OAM_SIZE                160         // bytes

#define JOYPAD_A                1
#define JOYPAD_B                2
#define JOYPAD_START            3
#define JOYPAD_SELECT           4
#define JOYPAD_RIGHT            5
#define JOYPAD_LEFT             6
#define JOYPAD_UP               7
#define JOYPAD_DOWN             8

typedef struct {
    int cpu_cycles_ms, cpu_cycles_vline, cpu_cycles_timer;
    int current_cycles;
    int main_cycles;    // how many times we should cycle in main()
    int last_instruction_cycles;
} timing_t;

typedef struct {
    uint16_t af, bc, de, hl, sp, pc, ime;
} cpu_t;

#define FLAG_ZF     0x80
#define FLAG_N      0x40
#define FLAG_H      0x20
#define FLAG_CY     0x10

extern long rom_size;
extern void *rom, *ram, *vram;
extern int is_cgb, is_sgb;

extern char *rom_filename;

extern int cpu_speed;

extern int scaling, frameskip;

extern SDL_Window *window;
extern SDL_Surface *surface;
extern timing_t timing;
extern int mbc_type;

void open_log();
void write_log(const char *, ...);
void die(int, const char *, ...);
void memory_start();
void cpu_start();
void display_start();
void timer_start();
void sound_start();

// cpu
void cpu_cycle();
void cpu_log();

// memory
extern int work_ram_bank;
uint8_t read_byte(uint16_t);
uint16_t read_word(uint16_t);
void write_byte(uint16_t, uint8_t);
void copy_oam(void *);
void mbc_start();
void mbc_write(uint16_t, uint8_t);
uint8_t mbc_read(uint16_t);

// interrupts
uint8_t if_read();
uint8_t ie_read();
void send_interrupt(int);

// display
void display_write(uint16_t, uint8_t);
uint8_t display_read(uint16_t);
void display_cycle();
void vram_write(uint16_t, uint8_t);
uint8_t vram_read(uint16_t);

// serial
void sb_write(uint8_t);
void sc_write(uint8_t);

// timer
void timer_write(uint16_t, uint8_t);
uint8_t timer_read(uint16_t);
void timer_cycle();

// sound
void sound_write(uint16_t, uint8_t);
uint8_t sound_read(uint16_t);

// joypad
extern uint8_t pressed_keys;
void joypad_write(uint16_t, uint8_t);
uint8_t joypad_read(uint16_t);
void joypad_handle(int, int);

// SGB functions
int sgb_transferring, sgb_interfere, sgb_screen_mask, using_sgb_palette;
void sgb_start();
void sgb_write(uint8_t);
uint8_t sgb_read();
void sgb_recolor(uint32_t *, uint32_t *, int, uint32_t *);

// CGB functions
int is_double_speed;
void cgb_write(uint16_t, uint8_t);
uint8_t cgb_read(uint16_t);

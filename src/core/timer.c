
/* tinygb - a tiny gameboy emulator
   (c) 2022 by jewel */

#include <tinygb.h>
#include <ioports.h>
#include <string.h>
#include <math.h>

#define TIMER_LOG

timer_regs_t timer;
int timer_cycles = 0;
int div_cycles = 0;

int timer_freqs[4] = {
    4096, 262144, 65536, 16384  // Hz
};

int current_timer_freq;

void set_timer_freq(uint8_t freq) {
    freq &= 3;

    current_timer_freq = timer_freqs[freq];

    // values that will be used to track timing
    double time_per_tick = 1000.0/(double)current_timer_freq;
    timing.cpu_cycles_timer = (int)((double)timing.cpu_cycles_ms * time_per_tick);

    write_log("[timer] set timer frequency to %d Hz\n", current_timer_freq);
    write_log("[timer] cpu cycles per tick = %d\n", timing.cpu_cycles_timer);

    /*if(timing.cpu_cycles_vline > timing.cpu_cycles_timer) {
        timing.main_cycles = GB_HEIGHT+10;
    } else {
        timing.main_cycles = (int)((double)round(TOTAL_REFRESH_TIME / (double)time_per_tick));
    }*/

    //write_log("[timer] main loop will repeat %d times per cycle\n", timing.main_cycles);
}

void timer_start() {
    memset(&timer, 0, sizeof(timer_regs_t));

    write_log("[timer] timer started\n");

    set_timer_freq(0);
}

uint8_t timer_read(uint16_t addr) {
    switch(addr) {
    case DIV:
        return timer.div;
    case TIMA:
        return timer.tima;
    case TMA:
        return timer.tma;
    case TAC:
        return timer.tac;
    default:
        write_log("[memory] unimplemented read from I/O port 0x%04X\n", addr);
        die(-1, NULL);
        return 0xFF;
    }
}

void timer_write(uint16_t addr, uint8_t byte) {
    switch(addr) {
    case DIV:
        timer.div = 0;      // writing to DIV clears it to zero
        break;
    case TIMA:
        timer.tima = byte;
        break;
    case TMA:
        timer.tma = byte;
        break;
    case TAC:
        timer.tac = byte;
        set_timer_freq(byte & 3);
        break;
    default:
        write_log("[memory] unimplemented write to I/O port 0x%04X value 0x%02X\n", addr, byte);
        die(-1, NULL);
    }
}

/*void timer_cycle() {
#ifdef TIMER_LOG
    //write_log("[timer] timer cycle\n");
#endif

    if(timer.tac & TAC_START) {
        timer.tima++;
        if(!timer.tima) {
            timer.tima = timer.tma;
            //write_log("[timer] timer interrupt\n");
        }
    }
}*/

void timer_cycle() {
    div_cycles += timing.last_instruction_cycles;

    if(div_cycles >= 256) {
        div_cycles -= 256;
        timer.div++;
    }

    if(!(timer.tac & TAC_START)) return;

    timer_cycles += timing.last_instruction_cycles;
    if(timer_cycles >= timing.cpu_cycles_timer) {
        timer_cycles -= timing.cpu_cycles_timer;
        timer.tima++;
        if(!timer.tima) {
            timer.tima = timer.tma;
            // TODO: timer interrupt
        }
    }
}

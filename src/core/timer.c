
/* tinygb - a tiny gameboy emulator
   (c) 2022 by jewel */

#include <tinygb.h>
#include <ioports.h>
#include <string.h>
#include <math.h>

//#define TIMER_LOG

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

    if(is_double_speed) timing.cpu_cycles_timer >>= 1;

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
    timing.cpu_cycles_div = 256;    // standard speed
}

uint8_t timer_read(uint16_t addr) {
    switch(addr) {
    case DIV:
#ifdef TIMER_LOG
        write_log("[timer] read value 0x%02X from DIV register\n", timer.div);
#endif
        return timer.div;
    case TIMA:
#ifdef TIMER_LOG
        write_log("[timer] read value 0x%02X from TIMA register\n", timer.tima);
#endif
        return timer.tima;
    case TMA:
#ifdef TIMER_LOG
        write_log("[timer] read value 0x%02X from TMA register\n", timer.tma);
#endif
        return timer.tma;
    case TAC:
#ifdef TIMER_LOG
        write_log("[timer] read value 0x%02X from TAC register\n", timer.tac);
#endif
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
#ifdef TIMER_LOG
        write_log("[timer] write to DIV register; clearing to zero\n");
#endif
        timer.div = 0;      // writing to DIV clears it to zero
        break;
    case TIMA:
#ifdef TIMER_LOG
        write_log("[timer] write to TIMA register value 0x%02X\n", byte);
#endif
        timer.tima = byte;
        break;
    case TMA:
#ifdef TIMER_LOG
        write_log("[timer] write to TMA register value 0x%02X\n", byte);
#endif
        timer.tma = byte;
        break;
    case TAC:
#ifdef TIMER_LOG
        write_log("[timer] write to TAC register value 0x%02X\n", byte);
#endif
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

    if(div_cycles >= timing.cpu_cycles_div) {
        div_cycles -= timing.cpu_cycles_div;
        timer.div++;
    }

    if(!(timer.tac & TAC_START)) return;

    timer_cycles += timing.last_instruction_cycles;
    if(timer_cycles >= timing.cpu_cycles_timer) {
        timer_cycles -= timing.cpu_cycles_timer;
        timer.tima++;
        if(!timer.tima) {
            timer.tima = timer.tma;
            //write_log("[timer] sending timer interrupt\n");
            send_interrupt(2);
        }
    }
}

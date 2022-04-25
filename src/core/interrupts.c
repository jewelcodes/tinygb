
/* tinygb - a tiny gameboy emulator
   (c) 2022 by jewel */

#include <tinygb.h>

#define INTERRUPTS_LOG

uint8_t io_if = 0, io_ie = 0;

void if_write(uint8_t byte) {
#ifdef INTERRUPTS_LOG
    write_log("[int] write to IF register with value 0x%02X\n", byte);
#endif

    io_if = byte;
}

void ie_write(uint8_t byte) {
#ifdef INTERRUPTS_LOG
    write_log("[int] write to IE register with value 0x%02X\n", byte);
#endif

    io_ie = byte;
}

uint8_t ie_read() {
    return io_ie;
}

uint8_t if_read() {
    return io_if;
}

void send_interrupt(int n) {
#ifdef INTERRUPTS_LOG
    //write_log("[int] sending interrupt 0x%02X\n", (n << 3) + 0x40);
#endif

    io_if |= (1 << n);
}
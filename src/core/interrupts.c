
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

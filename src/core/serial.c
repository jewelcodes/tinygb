
/* tinygb - a tiny gameboy emulator
   (c) 2022 by jewel */

#include <tinygb.h>
#include <ioports.h>

//#define SERIAL_LOG

uint8_t sb = 0, sc = 0;

void sb_write(uint8_t byte) {
#ifdef SERIAL_LOG
    write_log("[serial] write to SB register value 0x%02X\n", byte);
#endif

    sb = byte;
}

void sc_write(uint8_t byte) {
#ifdef SERIAL_LOG
    write_log("[serial] write to SC register value 0x%02X\n", byte);
#endif

    sc = byte;
}

/* tinygb - a tiny gameboy emulator
   (c) 2022 by jewel */

#include <tinygb.h>
#include <ioports.h>
#include <string.h>

#define SOUND_LOG

sound_t sound;

void sound_start() {
    memset(&sound, 0, sizeof(sound_t));

    sound.nr10 = 0x80;
    sound.nr11 = 0xBF;
    sound.nr12 = 0xF3;
    sound.nr14 = 0xBF;
    sound.nr21 = 0x3F;
    sound.nr22 = 0x00;
    sound.nr24 = 0xBF;
    sound.nr30 = 0x7F;
    sound.nr31 = 0xFF;
    sound.nr32 = 0x9F;
    sound.nr33 = 0xBF;
    sound.nr41 = 0xFF;
    sound.nr42 = 0x00;
    sound.nr43 = 0x00;
    sound.nr44 = 0xBF;
    sound.nr50 = 0x77;
    sound.nr51 = 0xF3;
    sound.nr52 = 0xF1;

    write_log("[sound] started sound device\n");
}

void sound_write(uint16_t addr, uint8_t byte) {
    switch(addr) {
    case NR50:
#ifdef SOUND_LOG
        write_log("[sound] write to NR50 register value 0x%02X\n", byte);
#endif
        sound.nr50 = byte;
        return;
    case NR51:
#ifdef SOUND_LOG
        write_log("[sound] write to NR51 register value 0x%02X\n", byte);
#endif
        sound.nr51 = byte;
        return;
    case NR52:
#ifdef SOUND_LOG
        write_log("[sound] write to NR52 register value 0x%02X, ignoring lower 7 bits\n", byte);
#endif
        if(byte & 0x80) sound.nr52 |= 0x80;
        else sound.nr52 &= 0x7F;
        return;
    default:
        write_log("[memory] unimplemented write to I/O port 0x%04X value 0x%02X\n", addr, byte);
        die(-1, NULL);
    }
}
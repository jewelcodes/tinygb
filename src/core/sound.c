
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
    case NR10:
#ifdef SOUND_LOG
        write_log("[sound] write to NR10 register value 0x%02X\n", byte);
#endif
        sound.nr10 = byte;
        return;
    case NR11:
#ifdef SOUND_LOG
        write_log("[sound] write to NR11 register value 0x%02X\n", byte);
#endif
        sound.nr11 = byte;
        return;
    case NR12:
#ifdef SOUND_LOG
        write_log("[sound] write to NR12 register value 0x%02X\n", byte);
#endif
        sound.nr12 = byte;
        return;
    case NR13:
#ifdef SOUND_LOG
        write_log("[sound] write to NR13 register value 0x%02X\n", byte);
#endif
        sound.nr13 = byte;
        return;
    case NR14:
#ifdef SOUND_LOG
        write_log("[sound] write to NR15 register value 0x%02X\n", byte);
#endif
        sound.nr14 = byte;
        return;
    case NR21:
#ifdef SOUND_LOG
        write_log("[sound] write to NR21 register value 0x%02X\n", byte);
#endif
        sound.nr21 = byte;
        return;
    case NR22:
#ifdef SOUND_LOG
        write_log("[sound] write to NR22 register value 0x%02X\n", byte);
#endif
        sound.nr22 = byte;
        return;
    case NR23:
#ifdef SOUND_LOG
        write_log("[sound] write to NR23 register value 0x%02X\n", byte);
#endif
        sound.nr23 = byte;
        return;
    case NR24:
#ifdef SOUND_LOG
        write_log("[sound] write to NR24 register value 0x%02X\n", byte);
#endif
        sound.nr24 = byte;
        return;
    case NR30:
#ifdef SOUND_LOG
        write_log("[sound] write to NR30 register value 0x%02X\n", byte);
#endif
        sound.nr30 = byte;
        return;
    case NR31:
#ifdef SOUND_LOG
        write_log("[sound] write to NR31 register value 0x%02X\n", byte);
#endif
        sound.nr31 = byte;
        return;
    case NR32:
#ifdef SOUND_LOG
        write_log("[sound] write to NR32 register value 0x%02X\n", byte);
#endif
        sound.nr32 = byte;
        return;
    case NR33:
#ifdef SOUND_LOG
        write_log("[sound] write to NR33 register value 0x%02X\n", byte);
#endif
        sound.nr33 = byte;
        return;
    case NR34:
#ifdef SOUND_LOG
        write_log("[sound] write to NR34 register value 0x%02X\n", byte);
#endif
        sound.nr34 = byte;
        return;
    case WAV00:
    case WAV01:
    case WAV02:
    case WAV03:
    case WAV04:
    case WAV05:
    case WAV06:
    case WAV07:
    case WAV08:
    case WAV09:
    case WAV10:
    case WAV11:
    case WAV12:
    case WAV13:
    case WAV14:
    case WAV15:
#ifdef SOUND_LOG
        write_log("[sound] write to WAX%02d register value 0x%02X\n", addr-WAV00, byte);
#endif
        sound.wav[addr-WAV00] = byte;
        return;
    case NR41:
#ifdef SOUND_LOG
        write_log("[sound] write to NR41 register value 0x%02X\n", byte);
#endif
        sound.nr41 = byte;
        return;
    case NR42:
#ifdef SOUND_LOG
        write_log("[sound] write to NR42 register value 0x%02X\n", byte);
#endif
        sound.nr42 = byte;
        return;
    case NR43:
#ifdef SOUND_LOG
        write_log("[sound] write to NR43 register value 0x%02X\n", byte);
#endif
        sound.nr43 = byte;
        return;
    case NR44:
#ifdef SOUND_LOG
        write_log("[sound] write to NR44 register value 0x%02X\n", byte);
#endif
        sound.nr44 = byte;
        return;
    default:
        write_log("[memory] unimplemented write to I/O port 0x%04X value 0x%02X\n", addr, byte);
        die(-1, NULL);
    }
}

/* tinygb - a tiny gameboy emulator
   (c) 2022 by jewel */

#include <tinygb.h>
#include <ioports.h>

//#define CGB_LOG

/* Misc Color Gameboy functions that dont fit anywhere else */

int is_double_speed = 0;
int prepare_speed_switch = 0;

uint8_t cgb_read(uint16_t addr) {
    switch(addr) {
    case KEY1:
        return (is_double_speed & 1) << 7;
    case SVBK:
        return work_ram_bank;
    default:
        die(-1, "undefined read from IO port 0x%04X\n", addr);
        return 0xFF;
    }
}

void cgb_write(uint16_t addr, uint8_t byte) {
    switch(addr) {
    case KEY1:
        if(byte & 0x01) prepare_speed_switch = 1;
        else write_log("[cgb] undefined write to KEY1 register value 0x%02X without attempting a speed switch\n");
        break;
    case RP:
        write_log("[cgb] unimplemented write to RP register value 0x%02X\n", byte);
        break;
    case SVBK:
        byte &= 7;
        if(!byte) byte++;
#ifdef CGB_LOG
        write_log("[cgb] selecting WRAM bank %d\n", byte);
#endif

        work_ram_bank = byte;
        break;
    default:
        die(-1, "undefined write to IO port 0x%04X value 0x%02X\n", addr, byte);
    }
}

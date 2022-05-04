
/* tinygb - a tiny gameboy emulator
   (c) 2022 by jewel */

#include <tinygb.h>
#include <ioports.h>

// Super Gameboy implementation

#define SGB_LOG

int sgb_current_bit = 0;
sgb_command_t sgb_command;


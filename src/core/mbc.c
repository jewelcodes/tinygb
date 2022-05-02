
/* tinygb - a tiny gameboy emulator
   (c) 2022 by jewel */

#include <tinygb.h>

// Memory Bank Controller Implementation

/* 

 No MBC:
 - 32 KiB ROM is mapped directly at 0x0000-0x7FFF
 - Writes to this region are ignored

 MBC1: (ROM up to ALMOST 2 MiB and RAM up to 32 KiB)
 - Memory regions:
  - 0xA000-0xBFFF   up to 4 banks of 8 KiB RAM
  - 0x0000-0x1FFF   RAM enable (0x00 = disable, 0x0A in the lower 4 bits = enable)
  - 0x2000-0x3FFF   lower 5 bits of ROM bank select; value zero is read as one (i.e. 0x00 and 0x01 both select the same bank)
  - 0x4000-0x5FFF   upper 2 bits of ROM bank select OR RAM bank select according to next register
  - 0x6000-0x7FFF   ROM/RAM banking toggle (0 = ROM, 1 = RAM)
  - (only RAM bank 0 can be used in ROM mode, and only ROM banks 0x00-0x1F can be used in mode 1)

 MBC3: (ROM up to full 2 MiB and RAM up to 32 KiB and real-time clock)
 - Memory regions:
  - 0xA000-0xBFFF   up to 4 banks of 8 KiB RAM or RTC registers
  - 0x0000-0x1FFF   RAM/RTC enable (0x00 = disable, 0x0A in the lower 4 bits = enable)
  - 0x2000-0x3FFF   ROM bank select (full 7 bits, highest bit ignored); value zero is read as one just like MBC1
  - 0x4000-0x5FFF   RAM bank select or RTC register select (0-3 = RAM bank, 0x08-0x0C = RTC register)
  - 0x6000-0x7FFF   latch clock data (writing zero -> one latches the data onto the RTC registers)

 - RTC registers according to RTC register select:
  - 0x08    seconds
  - 0x09    minutes
  - 0x0A    hours
  - 0x0B    lower 8 bits of day counter
  - 0x0C:
        Bit 0   highest bit of day counter
        Bit 6   halt flag (0 = running, 1 = clock stopped)
        Bit 7   day counter carry bit (1 = overflown)

 */

void mbc_start() {
    write_log("[mbc] MBC started\n");
}


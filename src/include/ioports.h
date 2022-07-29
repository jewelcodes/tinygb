
/* tinygb - a tiny gameboy emulator
   (c) 2022 by jewel */

#pragma once

#include <stdint.h>

// misc registers
#define P1      0xFF00  // joypad button status
#define SB      0xFF01  // serial buffer
#define SC      0xFF02  // serial control
#define DIV     0xFF04  // timer divider
#define TIMA    0xFF05  // timer counter
#define TMA     0xFF06  // timer modulo
#define TAC     0xFF07  // timer control
#define IF      0xFF0F  // interrupt flag

// audio controller registers
#define NR10    0xFF10
#define NR11    0xFF11
#define NR12    0xFF12
#define NR13    0xFF13
#define NR14    0xFF14
#define NR21    0xFF16
#define NR22    0xFF17
#define NR23    0xFF18
#define NR24    0xFF19
#define NR30    0xFF1A
#define NR31    0xFF1B
#define NR32    0xFF1C
#define NR33    0xFF1D
#define NR34    0xFF1E
#define NR41    0xFF20
#define NR42    0xFF21
#define NR43    0xFF22
#define NR44    0xFF23
#define NR50    0xFF24
#define NR51    0xFF25
#define NR52    0xFF26
#define WAV00   0xFF30
#define WAV01   0xFF31
#define WAV02   0xFF32
#define WAV03   0xFF33
#define WAV04   0xFF34
#define WAV05   0xFF35
#define WAV06   0xFF36
#define WAV07   0xFF37
#define WAV08   0xFF38
#define WAV09   0xFF39
#define WAV10   0xFF3A
#define WAV11   0xFF3B
#define WAV12   0xFF3C
#define WAV13   0xFF3D
#define WAV14   0xFF3E
#define WAV15   0xFF3F

// display controller registers
#define LCDC    0xFF40
#define STAT    0xFF41
#define SCY     0xFF42
#define SCX     0xFF43
#define LY      0xFF44
#define LYC     0xFF45
#define DMA     0xFF46
#define BGP     0xFF47
#define OBP0    0xFF48
#define OBP1    0xFF49
#define WY      0xFF4A
#define WX      0xFF4B

// CGB-only display controller registers
#define VBK     0xFF4F  // vram bank
#define HDMA1   0xFF51
#define HDMA2   0xFF52
#define HDMA3   0xFF53
#define HDMA4   0xFF54
#define HDMA5   0xFF55
#define BGPI    0xFF68  // bg palette index/data
#define BGPD    0xFF69
#define OBPI    0xFF6A  // obj palette index/data
#define OBPD    0xFF6B

// misc CGB registers
#define KEY1    0xFF4D
#define RP      0xFF56
#define SVBK    0xFF70

/// interrupt enable control register
#define IE      0xFFFF

#define LCDC_ENABLE     0x80

#define TAC_START       0x04

#define IF_VLANK        0x01
#define IF_STAT         0x02
#define IF_TIMER        0x04
#define IF_SERIAL       0x08
#define IF_JOYPAD       0x10

#define IE_VBLANK       0x01
#define IE_STAT         0x02
#define IE_TIMER        0x04
#define IE_SERIAL       0x08
#define IE_JOYPAD       0x10

void if_write(uint8_t);
void ie_write(uint8_t);

extern uint8_t io_if, io_ie;

typedef struct {
    uint8_t lcdc, stat, scy, scx, ly, lyc, dma, bgp, obp0, obp1, wx, wy, vbk, hdma1, hdma2, hdma3, hdma4, hdma5;
    uint8_t bgpi, bgpd[64], obpi, obpd[64];
} display_t;

typedef struct {
    uint8_t div, tima, tma, tac;
} timer_regs_t;

typedef struct {
    uint8_t nr10, nr11, nr12, nr13, nr14;
    uint8_t nr21, nr22, nr23, nr24;
    uint8_t nr30, nr31, nr32, nr33, nr34;
    uint8_t nr41, nr42, nr43, nr44;
    uint8_t nr50, nr51, nr52;
    uint8_t wav[16];
} sound_t;

typedef struct {
    int bank1, bank2, ram_enable, mode;
} mbc1_t;

typedef struct {
    int rom_bank, ram_rtc_bank, ram_rtc_enable, ram_rtc_toggle;
    int latch_data, old_latch_data;

    int h, m, s, d, halt;
} mbc3_t;

typedef struct {
    int rom_bank, ram_bank, ram_enable;
} mbc5_t;


/* tinygb - a tiny gameboy emulator
   (c) 2022 by jewel */

#include <tinygb.h>
#include <stdlib.h>
#include <ioports.h>

//#define INT_LOG
//#define DISASM
//#define THROTTLE_LOG
//#define THROTTLE

#define disasm_log  write_log("[disasm] %16d %04X ", total_cycles, cpu.pc); write_log

#define REG_A       7
#define REG_B       0
#define REG_C       1
#define REG_D       2
#define REG_E       3
#define REG_H       4
#define REG_L       5

#define REG_BC      0
#define REG_DE      1
#define REG_HL      2
#define REG_SP      3

#define THROTTLE_THRESHOLD  20      // ms

const char *registers[] = {
    "b", "c", "d", "e", "h", "l", "UNDEFINED", "a"
};

const char *registers16[] = {
    "bc", "de", "hl", "sp"
};

cpu_t cpu;
int cycles = 0;
int total_cycles = 0;
void (*opcodes[256])();
void (*ex_opcodes[256])();
int cpu_speed;
int cycles_per_throttle;

/*void count_cycles(int n) {
    n++;    // all cpu cycles are practically always one cycle longer
    timing.last_instruction_cycles = n;
    total_cycles += n;
    cycles += n;

    timing.current_cycles += n;

    double msec = (double)(n * 1000.0 / cpu_speed);

    cycles_time += msec;
    if(cycles_time >= THROTTLE_THRESHOLD) {
#ifdef THROTTLE_LOG
        write_log("[cpu] accumulated %d cycles, delaying %d ms\n", cycles, (int)cycles_time);
#endif
        int msec_int = (int)cycles_time;
        SDL_Delay(msec_int);
        cycles_time = 0.0;
        cycles = 0;
    }
}*/

void count_cycles(int n) {
    n += 2;
    //n <<= 1;    // x4 to machine cycles

    timing.last_instruction_cycles = n;
    total_cycles += n;
    cycles += n;
    timing.current_cycles += n;

    if(cycles >= cycles_per_throttle) {
#ifdef THROTTLE_LOG
        write_log("[cpu] accumulated %d cycles, delaying %d ms\n", cycles, (int)cycles_time);
#endif

#ifdef THROTTLE
        SDL_Delay(THROTTLE_THRESHOLD);
#endif

        cycles = 0;
    }
}

void cpu_log() {
    write_log(" AF = 0x%04X   BC = 0x%04X   DE = 0x%04X\n", cpu.af, cpu.bc, cpu.de);
    write_log(" HL = 0x%04X   SP = 0x%04X   PC = 0x%04X\n", cpu.hl, cpu.sp, cpu.pc);
    write_log(" executed total cycles = %d\n", total_cycles);
    //write_log(" time until next CPU throttle = %lf ms\n", THROTTLE_THRESHOLD - cycles_time);
}

void dump_cpu() {
    cpu_log();
    die(-1, NULL);
}

void cpu_start() {
    // initial cpu state
    cpu.af = 0x01B0;
    cpu.bc = 0x0013;
    cpu.de = 0x00D8;
    cpu.hl = 0x014D;
    cpu.sp = 0xFFFE;
    cpu.pc = 0x0100;    // skip the fixed rom and just exec the cartridge
    cpu.ime = 0;

    io_if = 0;
    io_ie = 0;

    // FIX: turns out this is incorrect and the CGB actually supports a double
    // speed function, but it is not turned on by default; it always starts at
    // 4.194 MHz for both original GB and CGB 
    /*if(is_cgb) {
        cpu_speed = CGB_CPU_SPEED;
    } else {
        cpu_speed = GB_CPU_SPEED;
    }*/

    cpu_speed = GB_CPU_SPEED;

    write_log("[cpu] started with speed %lf MHz\n", (double)cpu_speed/1000000);

    cycles_per_throttle = (cpu_speed * THROTTLE_THRESHOLD)/1000;
    write_log("[cpu] throttling every %d cycles\n", cycles_per_throttle);

    // determine values that will be used to keep track of timing
    timing.current_cycles = 0;
    timing.cpu_cycles_ms = cpu_speed / 1000;
    timing.cpu_cycles_vline = (int)((double)timing.cpu_cycles_ms * REFRESH_TIME_LINE);

    write_log("[cpu] cycles per ms = %d\n", timing.cpu_cycles_ms);
    timing.main_cycles = 70224 * 2 * (frameskip+1);
    //write_log("[cpu] cycles per v-line refresh = %d\n", timing.cpu_cycles_vline);
}

inline void push(uint16_t word) {
    cpu.sp--;
    write_byte(cpu.sp, (uint8_t)(word >> 8));
    cpu.sp--;
    write_byte(cpu.sp, (uint8_t)word & 0xFF);
}

inline uint16_t pop() {
    uint16_t val;
    val = read_byte(cpu.sp);
    cpu.sp++;
    val |= read_byte(cpu.sp) << 8;
    cpu.sp++;

    return val;
}

void cpu_cycle() {
    // handle interrupts
    uint8_t queued_ints = io_if & io_ie;
    if(cpu.ime && queued_ints) {
        for(int i = 0; i <= 4; i++) {
            if(queued_ints & (1 << i)) {
                // disable interrupts and call handler
                io_if &= ~(1 << i);     // mark as handled

#ifdef INT_LOG
                disasm_log("<HANDLING INTERRUPT 0x%02X>\n", (i << 3) + 0x40);
#endif

                cpu.ime = 0;
                push(cpu.pc);
                cpu.pc = (i << 3) + 0x40;
                break;
            }
        }
    }

    uint8_t opcode = read_byte(cpu.pc);

    if(!opcodes[opcode]) {
        write_log("undefined opcode %02X %02X %02X, dumping CPU state...\n", opcode, read_byte(cpu.pc+1), read_byte(cpu.pc+2));
        dump_cpu();
    } else {
        opcodes[opcode]();
    }
}

void write_reg8(int reg, uint8_t r) {
    switch(reg) {
    case REG_A:
        cpu.af &= 0x00FF;
        cpu.af |= (uint16_t)r << 8;
        break;
    case REG_B:
        cpu.bc &= 0x00FF;
        cpu.bc |= (uint16_t)r << 8;
        break;
    case REG_C:
        cpu.bc &= 0xFF00;
        cpu.bc |= (uint16_t)r;
        break;
    case REG_D:
        cpu.de &= 0x00FF;
        cpu.de |= (uint16_t)r << 8;
        break;
    case REG_E:
        cpu.de &= 0xFF00;
        cpu.de |= (uint16_t)r;
        break;
    case REG_H:
        cpu.hl &= 0x00FF;
        cpu.hl |= (uint16_t)r << 8;
        break;
    case REG_L:
        cpu.hl &= 0xFF00;
        cpu.hl |= (uint16_t)r;
        break;
    default:
        write_log("undefined opcode %02X %02X %02X, dumping CPU state...\n", read_byte(cpu.pc), read_byte(cpu.pc+1), read_byte(cpu.pc+2));
        return dump_cpu();
    }
}

uint8_t read_reg8(int reg) {
    uint8_t ret;
    switch(reg) {
    case REG_A:
        ret = cpu.af >> 8;
        break;
    case REG_B:
        ret = cpu.bc >> 8;
        break;
    case REG_C:
        ret = cpu.bc & 0xFF;
        break;
    case REG_D:
        ret = cpu.de >> 8;
        break;
    case REG_E:
        ret = cpu.de & 0xFF;
        break;
    case REG_H:
        ret = cpu.hl >> 8;
        break;
    case REG_L:
        ret = cpu.hl & 0xFF;
        break;
    default:
        write_log("undefined opcode %02X %02X %02X, dumping CPU state...\n", read_byte(cpu.pc), read_byte(cpu.pc+1), read_byte(cpu.pc+2));
        dump_cpu();
        return 0xFF;    // unreachable anyway
    }

    return ret;
}

void write_reg16(int reg, uint16_t r) {
    switch(reg) {
    case REG_BC:
        cpu.bc = r;
        break;
    case REG_DE:
        cpu.de = r;
        break;
    case REG_HL:
        cpu.hl = r;
        break;
    case REG_SP:
        cpu.sp = r;
        break;
    default:
        write_log("undefined opcode %02X %02X %02X, dumping CPU state...\n", read_byte(cpu.pc), read_byte(cpu.pc+1), read_byte(cpu.pc+2));
        return dump_cpu();
    }
}

uint16_t read_reg16(int reg) {
    switch(reg) {
    case REG_BC:
        return cpu.bc;
    case REG_DE:
        return cpu.de;
    case REG_HL:
        return cpu.hl;
    case REG_SP:
        return cpu.sp;
    default:
        write_log("undefined opcode %02X %02X %02X, dumping CPU state...\n", read_byte(cpu.pc), read_byte(cpu.pc+1), read_byte(cpu.pc+2));
        dump_cpu();
        return 0xFFFF;    // unreachable
    }
}

/*
   INDIVIDUAL INSTRUCTIONS ARE IMPLEMENTED HERE
 */

void nop() {
#ifdef DISASM
    disasm_log("nop\n");
#endif

    cpu.pc++;
    count_cycles(1);
}

void jp_nn() {
    uint16_t new_pc = read_word(cpu.pc+1);

#ifdef DISASM
    disasm_log("jp 0x%04X\n", new_pc);
#endif

    cpu.pc = new_pc;
    count_cycles(4);
}

void ld_r_r() {
    // 0b01xxxyyy
    uint8_t opcode = read_byte(cpu.pc);
    int x = (opcode >> 3) & 7;
    int y = opcode & 7;

#ifdef DISASM
    disasm_log("ld %s, %s\n", registers[x], registers[y]);
#endif

    uint8_t src = read_reg8(y);
    write_reg8(x, src);

    cpu.pc++;
    count_cycles(1);
}

void sbc_a_r() {
    uint8_t opcode = read_byte(cpu.pc);
    int reg = opcode & 7;

#ifdef DISASM
    disasm_log("sbc a, %s\n", registers[reg]);
#endif

    uint8_t a = read_reg8(REG_A);
    uint8_t r;
    r = read_reg8(reg);

    a -= r;
    if(cpu.af & FLAG_CY) a--;

    cpu.af |= FLAG_N;

    if(!a) cpu.af |= FLAG_ZF;
    else cpu.af &= (~FLAG_ZF);

    if(a > read_reg8(REG_A)) cpu.af |= FLAG_CY;
    else cpu.af &= (~FLAG_CY);

    if((a & 0x0F) < (read_reg8(REG_A) & 0x0F)) cpu.af |= FLAG_H;
    else cpu.af &= (~FLAG_H);

    write_reg8(REG_A, a);

    cpu.pc++;
    count_cycles(1);
}

void sub_r() {
    uint8_t opcode = read_byte(cpu.pc);
    int reg = opcode & 7;

#ifdef DISASM
    disasm_log("sub %s\n", registers[reg]);
#endif

    uint8_t a = read_reg8(REG_A);
    uint8_t r;
    r = read_reg8(reg);

    a -= r;

    cpu.af |= FLAG_N;

    if(!a) cpu.af |= FLAG_ZF;
    else cpu.af &= (~FLAG_ZF);

    if(a > read_reg8(REG_A)) cpu.af |= FLAG_CY;
    else cpu.af &= (~FLAG_CY);

    if((a & 0x0F) < (read_reg8(REG_A) & 0x0F)) cpu.af |= FLAG_H;
    else cpu.af &= (~FLAG_H);

    write_reg8(REG_A, a);

    cpu.pc++;
    count_cycles(1);
}

void dec_r() {
    uint8_t opcode = read_byte(cpu.pc);

    int reg = (opcode >> 3) & 7;

#ifdef DISASM
    disasm_log("dec %s\n", registers[reg]);
#endif

    uint8_t r;
    r = read_reg8(reg);

    uint8_t old = r;
    r--;

    cpu.af |= FLAG_N;

    if(!r) cpu.af |= FLAG_ZF;
    else cpu.af &= (~FLAG_ZF);

    if((r & 0x0F) > (old & 0x0F)) cpu.af |= FLAG_H;
    else cpu.af &= (~FLAG_H);

    write_reg8(reg, r);

    cpu.pc++;
    count_cycles(1);
}

void ld_r_xx() {
    uint8_t opcode = read_byte(cpu.pc);
    int reg = (opcode >> 3) & 7;
    uint8_t val = read_byte(cpu.pc+1);

#ifdef DISASM
    disasm_log("ld %s, 0x%02X\n", registers[reg], val);
#endif

    write_reg8(reg, val);

    cpu.pc += 2;
    count_cycles(2);
}

void inc_r() {
    uint8_t opcode = read_byte(cpu.pc);

    int reg = (opcode >> 3) & 7;

#ifdef DISASM
    disasm_log("inc %s\n", registers[reg]);
#endif

    uint8_t r;
    r = read_reg8(reg);

    uint8_t old = r;
    r++;

    cpu.af &= (~FLAG_N);

    if(!r) cpu.af |= FLAG_ZF;
    else cpu.af &= (~FLAG_ZF);

    if((r & 0x0F) < (old & 0x0F)) cpu.af |= FLAG_H;
    else cpu.af &= (~FLAG_H);

    write_reg8(reg, r);

    cpu.pc++;
    count_cycles(1);
}

void jr_e() {
    uint8_t e = read_byte(cpu.pc+1);

    if(e & 0x80) {
        uint8_t pe = ~e;
        pe++;
        #ifdef DISASM
            disasm_log("jr 0x%02X (-%d) (0x%04X)\n", e, pe, (cpu.pc - pe) + 2);
        #endif

        cpu.pc += 2;
        cpu.pc -= pe;
    } else {
        #ifdef DISASM
            disasm_log("jr 0x%02X (+%d) (0x%04X)\n", e, e, cpu.pc + 2 + e);
        #endif

        cpu.pc += 2;
        cpu.pc += e;
    }

    count_cycles(3);
}

void ld_r_hl() {
    uint8_t opcode = read_byte(cpu.pc);
    int reg = (opcode >> 3) & 7;

#ifdef DISASM
    disasm_log("ld %s, (hl)\n", registers[reg]);
#endif

    uint8_t val = read_byte(cpu.hl);

    write_reg8(reg, val);

    cpu.pc++;
    count_cycles(2);
}

void ld_r_xxxx() {
    uint8_t opcode = read_byte(cpu.pc);
    int reg = (opcode >> 4) & 3;
    uint16_t val = read_word(cpu.pc+1);

#ifdef DISASM
    disasm_log("ld %s, 0x%04X\n", registers16[reg], val);
#endif

    write_reg16(reg, val);

    cpu.pc += 3;
    count_cycles(3);
}

void cpl() {
#ifdef DISASM
    disasm_log("cpl\n");
#endif

    write_reg8(REG_A, read_reg8(REG_A) ^ 0xFF);

    cpu.af |= FLAG_N | FLAG_H;

    cpu.pc++;
    count_cycles(1);
}

void ld_bc_a() {
#ifdef DISASM
    disasm_log("ld (bc), a\n");
#endif

    uint8_t a = read_reg8(REG_A);
    write_byte(cpu.bc, a);

    cpu.pc++;
    count_cycles(2);
}

void inc_r16() {
    uint8_t opcode = read_byte(cpu.pc);
    int reg = (opcode >> 4) & 3;

#ifdef DISASM
    disasm_log("inc %s\n", registers16[reg]);
#endif

    uint16_t val = read_reg16(reg);
    val++;
    write_reg16(reg, val);

    cpu.pc++;
    count_cycles(2);
}

void xor_r() {
    uint8_t opcode = read_byte(cpu.pc);
    int reg = opcode & 7;

#ifdef DISASM
    disasm_log("xor %s\n", registers[reg]);
#endif

    uint8_t val = read_reg8(reg);
    uint8_t a = read_reg8(REG_A);

    a ^= val;

    if(!a) cpu.af |= FLAG_ZF;
    else cpu.af &= (~FLAG_ZF);

    cpu.af &= ~(FLAG_N | FLAG_H | FLAG_CY);

    write_reg8(REG_A, a);

    cpu.pc++;
    count_cycles(1);
}

void ldd_hl_a() {
#ifdef DISASM
    disasm_log("ldd (hl), a\n");
#endif

    uint8_t a = read_reg8(REG_A);

    write_byte(cpu.hl, a);
    cpu.hl--;

    cpu.pc++;
    count_cycles(2);
}

void jr_nz() {
    uint8_t e = read_byte(cpu.pc+1);

    if(e & 0x80) {
        uint8_t pe = ~e;
        pe++;
        #ifdef DISASM
            disasm_log("jr nz 0x%02X (-%d) (0x%04X)\n", e, pe, (cpu.pc - pe) + 2);
        #endif

        cpu.pc += 2;

        if(cpu.af & FLAG_ZF) {
            // ZF is set; condition false
            count_cycles(2);
        } else {
            // ZF not set; condition true
            cpu.pc -= pe;
            count_cycles(3);
        }
    } else {
        #ifdef DISASM
            disasm_log("jr nz 0x%02X (+%d) (0x%04X)\n", e, e, cpu.pc + 2 + e);
        #endif

        cpu.pc += 2;

        if(cpu.af & FLAG_ZF) {
            // ZF is set; condition false
            count_cycles(2);
        } else {
            // ZF not set; condition true
            cpu.pc += e;
            count_cycles(3);
        }
    }
}

void di() {
#ifdef DISASM
    disasm_log("di\n");
#endif

    cpu.ime = 0;
    cpu.pc++;
    count_cycles(1);
}

void ldh_a8_a() {
    uint8_t a8 = read_byte(cpu.pc+1);

#ifdef DISASM
    disasm_log("ldh (0x%02X), a\n", a8);
#endif

    uint16_t addr = 0xFF00 + a8;
    write_byte(addr, read_reg8(REG_A));

    cpu.pc += 2;
    count_cycles(3);
}

void cp_xx() {
    uint8_t val = read_byte(cpu.pc+1);

#ifdef DISASM
    disasm_log("cp 0x%02X\n", val);
#endif

    uint8_t a = read_reg8(REG_A);

    a -= val;

    cpu.af |= FLAG_N;

    if(!a) cpu.af |= FLAG_ZF;
    else cpu.af &= (~FLAG_ZF);

    if(a > read_reg8(REG_A)) cpu.af |= FLAG_CY;
    else cpu.af &= (~FLAG_CY);

    if((a & 0x0F) < (read_reg8(REG_A) & 0x0F)) cpu.af |= FLAG_H;
    else cpu.af &= (~FLAG_H);

    cpu.pc += 2;
    count_cycles(2);
}

void jr_z() {
    uint8_t e = read_byte(cpu.pc+1);

    if(e & 0x80) {
        uint8_t pe = ~e;
        pe++;
        #ifdef DISASM
            disasm_log("jr z 0x%02X (-%d) (0x%04X)\n", e, pe, (cpu.pc - pe) + 2);
        #endif

        cpu.pc += 2;

        if(!(cpu.af & FLAG_ZF)) {
            // ZF is false; condition false
            count_cycles(2);
        } else {
            // ZF is set; condition true
            cpu.pc -= pe;
            count_cycles(3);
        }
    } else {
        #ifdef DISASM
            disasm_log("jr z 0x%02X (+%d) (0x%04X)\n", e, e, cpu.pc + 2 + e);
        #endif

        cpu.pc += 2;

        if(!(cpu.af & FLAG_ZF)) {
            // ZF is false; condition false
            count_cycles(2);
        } else {
            // ZF is set; condition true
            cpu.pc += e;
            count_cycles(3);
        }
    }
}

void ld_a16_a() {
    uint16_t addr = read_word(cpu.pc+1);

#ifdef DISASM
    disasm_log("ld (0x%04X), a\n", addr);
#endif

    write_byte(addr, read_reg8(REG_A));
    cpu.pc += 3;
    count_cycles(4);
}

void ldh_a_a8() {
    uint8_t a8 = read_byte(cpu.pc+1);

#ifdef DISASM
    disasm_log("ldh a, (0x%02X)\n", a8);
#endif

    uint16_t addr = 0xFF00 + a8;
    write_reg8(REG_A, read_byte(addr));

    cpu.pc += 2;
    count_cycles(3);
}

void call_a16() {
    uint16_t a16 = read_word(cpu.pc+1);

#ifdef DISASM
    disasm_log("call 0x%04X\n", a16);
#endif

    push(cpu.pc+3);
    cpu.pc = a16;

    count_cycles(6);
}

void and_n() {
    uint8_t n = read_byte(cpu.pc+1);

#ifdef DISASM
    disasm_log("and 0x%02X\n", n);
#endif

    uint8_t a = read_reg8(REG_A);
    a &= n;
    write_reg8(REG_A, a);

    cpu.af &= ~(FLAG_N | FLAG_CY);
    cpu.af |= FLAG_H;

    if(!a) cpu.af |= FLAG_ZF;
    else cpu.af &= (~FLAG_ZF);

    cpu.pc += 2;
    count_cycles(2);
}

void ret() {
#ifdef DISASM
    disasm_log("ret\n");
#endif

    cpu.pc = pop();
    count_cycles(4);
}

void ld_hl_n() {
    uint8_t n = read_byte(cpu.pc+1);

#ifdef DISASM
    disasm_log("ld (hl), 0x%02X\n", n);
#endif

    write_byte(cpu.hl, n);

    cpu.pc += 2;
    count_cycles(3);
}

void dec_r16() {
    uint8_t opcode = read_byte(cpu.pc);
    int reg = (opcode >> 4) & 3;

#ifdef DISASM
    disasm_log("dec %s\n", registers16[reg]);
#endif

    uint16_t val = read_reg16(reg);
    val--;
    write_reg16(reg, val);

    cpu.pc++;
    count_cycles(2);
}

void or_r() {
    uint8_t opcode = read_byte(cpu.pc);

    int reg = opcode & 7;

#ifdef DISASM
    disasm_log("or %s\n", registers[reg]);
#endif

    uint8_t a = read_reg8(REG_A);
    uint8_t r = read_reg8(reg);

    a |= r;
    write_reg8(REG_A, a);

    if(!a) cpu.af |= FLAG_ZF;
    else cpu.af &= (~FLAG_ZF);

    cpu.af &= ~(FLAG_N | FLAG_H | FLAG_CY);

    cpu.pc++;
    count_cycles(1);
}

void push_r16() {
    uint8_t opcode = read_byte(cpu.pc);
    int reg = (opcode >> 4) & 3;

#ifdef DISASM
    disasm_log("push %s\n", registers16[reg]);
#endif

    push(read_reg16(reg));
    cpu.pc++;
    count_cycles(4);
}

void push_af() {
#ifdef DISASM
    disasm_log("push af\n");
#endif

    push(cpu.af);
    cpu.pc++;
    count_cycles(4);
}

void pop_r16() {
    uint8_t opcode = read_byte(cpu.pc);
    int reg = (opcode >> 4) & 3;

#ifdef DISASM
    disasm_log("pop %s\n", registers16[reg]);
#endif

    write_reg16(reg, pop());
    cpu.pc++;
    count_cycles(3);
}

void pop_af() {
#ifdef DISASM
    disasm_log("pop af\n");
#endif

    cpu.af = pop();
    cpu.pc++;
    count_cycles(3);
}

void ldi_hl_a() {
#ifdef DISASM
    disasm_log("ldi (hl), a\n");
#endif

    uint8_t a = read_reg8(REG_A);

    write_byte(cpu.hl, a);
    cpu.hl++;

    cpu.pc++;
    count_cycles(2);
}

void ldi_a_hl() {
#ifdef DISASM
    disasm_log("ldi a, (hl)\n");
#endif

    write_reg8(REG_A, read_byte(cpu.hl));

    cpu.hl++;
    cpu.pc++;
    count_cycles(2);
}

void ldd_a_hl() {
#ifdef DISASM
    disasm_log("ldd a, (hl)\n");
#endif

    write_reg8(REG_A, read_byte(cpu.hl));

    cpu.hl--;
    cpu.pc++;
    count_cycles(2);
}

void ldh_c_a() {
#ifdef DISASM
    disasm_log("ldh (c), a\n");
#endif

    uint16_t addr = 0xFF00 + read_reg8(REG_C);
    write_byte(addr, read_reg8(REG_A));

    cpu.pc++;
    count_cycles(2);
}

void ldh_a_c() {
#ifdef DISASM
    disasm_log("ldh a, (c)\n");
#endif

    uint16_t addr = 0xFF00 + read_reg8(REG_C);
    write_reg8(REG_A, read_byte(addr));

    cpu.pc++;
    count_cycles(2);
}

void ei() {
#ifdef DISASM
    disasm_log("ei\n");
#endif

    cpu.ime = 1;
    cpu.pc++;
    count_cycles(1);
}

void and_r() {
    uint8_t opcode = read_byte(cpu.pc);
    int reg = opcode & 7;

#ifdef DISASM
    disasm_log("and %s\n", registers[reg]);
#endif

    uint8_t val = read_reg8(reg);
    uint8_t a = read_reg8(REG_A);

    a &= val;

    if(!a) cpu.af |= FLAG_ZF;
    else cpu.af &= (~FLAG_ZF);

    cpu.af &= ~(FLAG_N | FLAG_CY);
    cpu.af |= FLAG_H;

    write_reg8(REG_A, a);

    cpu.pc++;
    count_cycles(1);
}

void ret_nz() {
#ifdef DISASM
    disasm_log("ret nz\n");
#endif

    if(cpu.af & FLAG_ZF) {
        // ZF set; condition false
        cpu.pc++;
        count_cycles(2);
    } else {
        // ZF clear; condition true
        cpu.pc = pop();
        count_cycles(5);
    }
}

void ret_z() {
#ifdef DISASM
    disasm_log("ret z\n");
#endif

    if(cpu.af & FLAG_ZF) {
        // ZF set; condition true
        cpu.pc = pop();
        count_cycles(5);
    } else {
        // ZF clear; condition false
        cpu.pc++;
        count_cycles(2);
    }
}

void ld_a_a16() {
    uint16_t addr = read_word(cpu.pc+1);

#ifdef DISASM
    disasm_log("ld a, (0x%04X)\n", addr);
#endif

    write_reg8(REG_A, read_byte(addr));

    cpu.pc += 3;
    count_cycles(4);
}

void inc_hl() {
#ifdef DISASM
    disasm_log("inc (hl)\n");
#endif

    uint8_t n = read_byte(cpu.hl);
    uint8_t old = n;
    n++;

    cpu.af &= (~FLAG_N);

    if(!n) cpu.af |= FLAG_ZF;
    else cpu.af &= (~FLAG_ZF);

    if((n & 0x0F) < (old & 0x0F)) cpu.af |= FLAG_H;
    else cpu.af &= (~FLAG_H);

    write_byte(cpu.hl, n);

    cpu.pc++;
    count_cycles(3);
}

void reti() {
#ifdef DISASM
    disasm_log("reti\n");
#endif

    cpu.ime = 1;
    cpu.pc = pop();
    count_cycles(4);
}

void rst() {
    uint8_t opcode = read_byte(cpu.pc);
    uint8_t n = (opcode >> 3) & 7;

    uint8_t addr = n << 3;

#ifdef DISASM
    disasm_log("rst 0x%02X\n", addr);
#endif

    push(cpu.pc+1);
    cpu.pc = addr;

    count_cycles(4);
}

void add_r() {
    uint8_t opcode = read_byte(cpu.pc);
    int reg = opcode & 7;

#ifdef DISASM
    disasm_log("add %s\n", registers[reg]);
#endif

    uint8_t a = read_reg8(REG_A);
    uint8_t r = read_reg8(reg);
    uint8_t new = a + r;

    cpu.af &= (~FLAG_N);

    if(!new) cpu.af |= FLAG_ZF;
    else cpu.af &= (~FLAG_ZF);

    if((new & 0x0F) < (a & 0x0F)) cpu.af |= FLAG_H;
    else cpu.af &= (~FLAG_H);

    if(new < a) cpu.af |= FLAG_CY;
    else cpu.af &= (~FLAG_CY);

    write_reg8(REG_A, new);

    cpu.pc++;
    count_cycles(1);
}

void add_hl_r16() {
    uint8_t opcode = read_byte(cpu.pc);
    int reg = (opcode >> 4) & 3;

#ifdef DISASM
    disasm_log("add hl, %s\n", registers16[reg]);
#endif

    uint16_t hl = read_reg16(REG_HL);
    uint16_t rr = read_reg16(reg);
    uint16_t new = hl + rr;

    cpu.af &= ~(FLAG_N);

    // flags are set according to higher byte
    uint8_t hi_new = (new >> 8) & 0xFF;
    uint8_t hi_old = (hl >> 8) & 0xFF;

    if(hi_new < hi_old) cpu.af |= FLAG_CY;
    else cpu.af &= (~FLAG_CY);

    if((hi_new & 0x0F) < (hi_old & 0x0F)) cpu.af |= FLAG_H;
    else cpu.af &= (~FLAG_H);

    write_reg16(REG_HL, new);
    cpu.pc++;
    count_cycles(2);
}

void jp_hl() {
#ifdef DISASM
    disasm_log("jp hl\n");
#endif

    cpu.pc = cpu.hl;
    count_cycles(1);
}

void ld_de_a() {
#ifdef DISASM
    disasm_log("ld (de), a\n");
#endif

    uint8_t a = read_reg8(REG_A);
    write_byte(cpu.de, a);

    cpu.pc++;
    count_cycles(2);
}

void ld_a_bc() {
#ifdef DISASM
    disasm_log("ld a, (bc)\n");
#endif

    write_reg8(REG_A, read_byte(cpu.bc));

    cpu.pc++;
    count_cycles(2);
}

void ld_a_de() {
#ifdef DISASM
    disasm_log("ld a, (de)\n");
#endif

    write_reg8(REG_A, read_byte(cpu.de));

    cpu.pc++;
    count_cycles(2);
}

void jp_z_a16() {
    uint16_t new_pc = read_word(cpu.pc+1);

#ifdef DISASM
    disasm_log("jp z 0x%04X\n", new_pc);
#endif

    if(cpu.af & FLAG_ZF) {
        // ZF set, condition true
        cpu.pc = new_pc;
        count_cycles(4);
    } else {
        // ZF clear, condition false
        cpu.pc += 3;
        count_cycles(3);
    }
}

void dec_hl() {
#ifdef DISASM
    disasm_log("dec (hl)\n");
#endif

    uint8_t n = read_byte(cpu.hl);
    uint8_t old = n;
    n--;

    cpu.af |= FLAG_N;

    if(!n) cpu.af |= FLAG_ZF;
    else cpu.af &= (~FLAG_ZF);

    if((n & 0x0F) > (old & 0x0F)) cpu.af |= FLAG_H;
    else cpu.af &= (~FLAG_H);

    write_byte(cpu.hl, n);

    cpu.pc++;
    count_cycles(3);
}

void ld_hl_r() {
    uint8_t opcode = read_byte(cpu.pc);
    int reg = opcode & 7;

#ifdef DISASM
    disasm_log("ld (hl), %s\n", registers[reg]);
#endif

    write_byte(cpu.hl, read_reg8(reg));

    cpu.pc++;
    count_cycles(2);
}

void jp_nz_a16() {
    uint16_t new_pc = read_word(cpu.pc+1);

#ifdef DISASM
    disasm_log("jp nz 0x%04X\n", new_pc);
#endif

    if(!(cpu.af & FLAG_ZF)) {
        // ZF clear, condition true
        cpu.pc = new_pc;
        count_cycles(4);
    } else {
        // ZF set, condition false
        cpu.pc += 3;
        count_cycles(3);
    }
}

void add_d8() {
    uint8_t d8 = read_byte(cpu.pc+1);

#ifdef DISASM
    disasm_log("add 0x%02X\n", d8);
#endif

    uint8_t a = read_reg8(REG_A);
    uint8_t new = a + d8;

    cpu.af &= (~FLAG_N);

    if(!new) cpu.af |= FLAG_ZF;
    else cpu.af &= (~FLAG_ZF);

    if((new & 0x0F) < (a & 0x0F)) cpu.af |= FLAG_H;
    else cpu.af &= (~FLAG_H);

    if(new < a) cpu.af |= FLAG_CY;
    else cpu.af &= (~FLAG_CY);

    write_reg8(REG_A, new);

    cpu.pc += 2;
    count_cycles(2);
}

void xor_d8() {
    uint8_t d8 = read_byte(cpu.pc+1);

#ifdef DISASM
    disasm_log("xor 0x%02X\n", d8);
#endif

    uint8_t a = read_reg8(REG_A);
    a ^= d8;
    cpu.af &= ~(FLAG_N | FLAG_H | FLAG_CY);

    if(!a) cpu.af |= FLAG_ZF;
    else cpu.af &= (~FLAG_ZF);

    write_reg8(REG_A, a);

    cpu.pc += 2;
    count_cycles(2);
}

void jr_nc() {
    uint8_t e = read_byte(cpu.pc+1);

    if(e & 0x80) {
        uint8_t pe = ~e;
        pe++;
        #ifdef DISASM
            disasm_log("jr nc 0x%02X (-%d) (0x%04X)\n", e, pe, (cpu.pc - pe) + 2);
        #endif

        cpu.pc += 2;

        if(cpu.af & FLAG_CY) {
            // C is set; condition false
            count_cycles(2);
        } else {
            // C not set; condition true
            cpu.pc -= pe;
            count_cycles(3);
        }
    } else {
        #ifdef DISASM
            disasm_log("jr nc 0x%02X (+%d) (0x%04X)\n", e, e, cpu.pc + 2 + e);
        #endif

        cpu.pc += 2;

        if(cpu.af & FLAG_CY) {
            // C is set; condition false
            count_cycles(2);
        } else {
            // C not set; condition true
            cpu.pc += e;
            count_cycles(3);
        }
    }
}

void jr_c() {
    uint8_t e = read_byte(cpu.pc+1);

    if(e & 0x80) {
        uint8_t pe = ~e;
        pe++;
        #ifdef DISASM
            disasm_log("jr c 0x%02X (-%d) (0x%04X)\n", e, pe, (cpu.pc - pe) + 2);
        #endif

        cpu.pc += 2;

        if(!(cpu.af & FLAG_CY)) {
            // C is false; condition false
            count_cycles(2);
        } else {
            // C is set; condition true
            cpu.pc -= pe;
            count_cycles(3);
        }
    } else {
        #ifdef DISASM
            disasm_log("jr c 0x%02X (+%d) (0x%04X)\n", e, e, cpu.pc + 2 + e);
        #endif

        cpu.pc += 2;

        if(!(cpu.af & FLAG_CY)) {
            // C is false; condition false
            count_cycles(2);
        } else {
            // C is set; condition true
            cpu.pc += e;
            count_cycles(3);
        }
    }
}

void or_hl() {
#ifdef DISASM
    disasm_log("or (hl)\n");
#endif

    uint8_t a = read_reg8(REG_A);
    uint8_t r = read_byte(cpu.hl);

    a |= r;
    write_reg8(REG_A, a);

    if(!a) cpu.af |= FLAG_ZF;
    else cpu.af &= (~FLAG_ZF);

    cpu.af &= ~(FLAG_N | FLAG_H | FLAG_CY);

    cpu.pc++;
    count_cycles(2);
}

/*void ld_hl_sp_s() {
    uint8_t e = read_byte(cpu.pc+1);
    uint16_t new;
    uint8_t lo_new, lo_old;

    lo_old = cpu.hl & 0xFF;

    if(e & 0x80) {
        // negative
        uint8_t pe = ~e;
        pe++;

#ifdef DISASM
        disasm_log("ld hl, sp-%d (0x%02X)\n", pe, e);
#endif

        new = cpu.sp - (uint16_t)pe;
    } else {
        // positive
#ifdef DISASM
        disasm_log("ld hl, sp+%d (0x%02X)\n", e, e);
#endif

        new = cpu.sp + (uint16_t)e;
    }

    // flags are set according to lower byte
    lo_new = new & 0xFF;

    if(lo_new < lo_old) cpu.af |= FLAG_CY;
    else cpu.af &= (~FLAG_CY);

    if((lo_new & 0x0F) < (lo_old & 0x0F)) cpu.af |= FLAG_H;
    else cpu.af &= (~FLAG_H);

    cpu.af &= ~(FLAG_ZF | FLAG_N);

    write_reg16(REG_HL, new);
    cpu.pc += 2;
    count_cycles(3);
}*/

void ld_hl_sp_s() {
    uint8_t e = read_byte(cpu.pc+1);
    uint16_t ew = e;
    if(ew & 0x80) ew |= 0xFF00;

    uint8_t lo_new, lo_old;

    lo_old = cpu.hl & 0xFF;

#ifdef DISASM
    disasm_log("ld hl, sp+0x%04X\n", ew);
#endif

    uint16_t new = cpu.sp + ew;
    lo_new = new & 0xFF;

    // flags are set according to lower byte
    lo_new = new & 0xFF;

    if(lo_new < lo_old) cpu.af |= FLAG_CY;
    else cpu.af &= (~FLAG_CY);

    if((lo_new & 0x0F) < (lo_old & 0x0F)) cpu.af |= FLAG_H;
    else cpu.af &= (~FLAG_H);

    cpu.af &= ~(FLAG_ZF | FLAG_N);

    write_reg16(REG_HL, new);
    cpu.pc += 2;
    count_cycles(3);
}

void add_sp_s() {
    uint8_t e = read_byte(cpu.pc+1);
    uint16_t ew = e;
    if(ew & 0x80) ew |= 0xFF00;

    uint16_t new;
    uint8_t lo_new, lo_old;

#ifdef DISASM
    disasm_log("add sp, 0x%04X\n", ew);
#endif

    lo_old = cpu.sp & 0xFF;
    new = cpu.sp + ew;

    // flags are set according to lower byte
    lo_new = new & 0xFF;

    if(lo_new < lo_old) cpu.af |= FLAG_CY;
    else cpu.af &= (~FLAG_CY);

    if((lo_new & 0x0F) < (lo_old & 0x0F)) cpu.af |= FLAG_H;
    else cpu.af &= (~FLAG_H);

    cpu.af &= ~(FLAG_ZF | FLAG_N);

    write_reg16(REG_SP, new);
    cpu.pc += 2;
    count_cycles(3);
}

void cp_r() {
    uint8_t opcode = read_byte(cpu.pc);
    int reg = opcode & 7;

#ifdef DISASM
    disasm_log("cp %s\n", registers[reg]);
#endif

    uint8_t val = read_reg8(reg);
    uint8_t a = read_reg8(REG_A);

    a -= val;

    cpu.af |= FLAG_N;

    if(!a) cpu.af |= FLAG_ZF;
    else cpu.af &= (~FLAG_ZF);

    if(a > read_reg8(REG_A)) cpu.af |= FLAG_CY;
    else cpu.af &= (~FLAG_CY);

    if((a & 0x0F) < (read_reg8(REG_A) & 0x0F)) cpu.af |= FLAG_H;
    else cpu.af &= (~FLAG_H);

    cpu.pc++;
    count_cycles(1);
}

void or_d8() {
    uint8_t d8 = read_byte(cpu.pc+1);

#ifdef DISASM
    disasm_log("or 0x%02X\n", d8);
#endif

    uint8_t a = read_reg8(REG_A);
    a |= d8;
    cpu.af &= ~(FLAG_N | FLAG_H | FLAG_CY);

    if(!a) cpu.af |= FLAG_ZF;
    else cpu.af &= (~FLAG_ZF);

    write_reg8(REG_A, a);

    cpu.pc += 2;
    count_cycles(2);
}

void call_nz() {
    uint16_t new_pc = read_word(cpu.pc+1);

#ifdef DISASM
    disasm_log("call nz 0x%04X\n", new_pc);
#endif

    if(cpu.af & FLAG_ZF) {
        // ZF set, condition false
        cpu.pc += 3;
        count_cycles(3);
    } else {
        // ZF clear, condition true
        push(cpu.pc+3);
        cpu.pc = new_pc;
        count_cycles(6);
    }
}

void adc_r() {
    uint8_t opcode = read_byte(cpu.pc);
    int reg = opcode & 7;

#ifdef DISASM
    disasm_log("adc %s\n", registers[reg]);
#endif

    uint8_t a = read_reg8(REG_A);
    uint8_t r = read_reg8(reg);
    uint8_t new = a + r;

    if(cpu.af & FLAG_CY) new++;

    cpu.af &= (~FLAG_N);

    if(!new) cpu.af |= FLAG_ZF;
    else cpu.af &= (~FLAG_ZF);

    if((new & 0x0F) < (a & 0x0F)) cpu.af |= FLAG_H;
    else cpu.af &= (~FLAG_H);

    if(new < a) cpu.af |= FLAG_CY;
    else cpu.af &= (~FLAG_CY);

    write_reg8(REG_A, new);

    cpu.pc++;
    count_cycles(1);
}

void add_hl() {
#ifdef DISASM
    disasm_log("add (hl)\n");
#endif

    uint8_t a = read_reg8(REG_A);
    uint8_t r = read_byte(cpu.hl);
    uint8_t new = a + r;

    cpu.af &= (~FLAG_N);

    if(!new) cpu.af |= FLAG_ZF;
    else cpu.af &= (~FLAG_ZF);

    if((new & 0x0F) < (a & 0x0F)) cpu.af |= FLAG_H;
    else cpu.af &= (~FLAG_H);

    if(new < a) cpu.af |= FLAG_CY;
    else cpu.af &= (~FLAG_CY);

    write_reg8(REG_A, new);

    cpu.pc++;
    count_cycles(2);
}

void cp_hl() {
#ifdef DISASM
    disasm_log("cp (hl)\n");
#endif

    uint8_t val = read_byte(cpu.hl);
    uint8_t a = read_reg8(REG_A);

    a -= val;

    cpu.af |= FLAG_N;

    if(!a) cpu.af |= FLAG_ZF;
    else cpu.af &= (~FLAG_ZF);

    if(a > read_reg8(REG_A)) cpu.af |= FLAG_CY;
    else cpu.af &= (~FLAG_CY);

    if((a & 0x0F) < (read_reg8(REG_A) & 0x0F)) cpu.af |= FLAG_H;
    else cpu.af &= (~FLAG_H);

    cpu.pc++;
    count_cycles(2);
}

void halt() {
#ifdef DISASM
    disasm_log("halt\n");
#endif

    cpu.pc++;
    count_cycles(1);
}

void rra() {
#ifdef DISASM
    disasm_log("rra\n");
#endif

    uint8_t old_cy;
    if(cpu.af & FLAG_CY) old_cy = 0x80;
    else old_cy = 0x00;

    cpu.af &= ~(FLAG_ZF | FLAG_N | FLAG_H);

    uint8_t a = read_reg8(REG_A);
    if(a & 0x01) cpu.af |= FLAG_CY;
    else cpu.af &= (~FLAG_CY);

    a >>= 1;
    a |= old_cy;

    write_reg8(REG_A, a);
    cpu.pc++;
    count_cycles(1);
}

void sub_d8() {
    uint8_t d8 = read_byte(cpu.pc+1);

#ifdef DISASM
    disasm_log("sub 0x%02X\n", d8);
#endif

    uint8_t a = read_reg8(REG_A);
    a -= d8;

    cpu.af |= FLAG_N;

    if(!a) cpu.af |= FLAG_ZF;
    else cpu.af &= (~FLAG_ZF);

    if(a > read_reg8(REG_A)) cpu.af |= FLAG_CY;
    else cpu.af &= (~FLAG_CY);

    if((a & 0x0F) < (read_reg8(REG_A) & 0x0F)) cpu.af |= FLAG_H;
    else cpu.af &= (~FLAG_H);

    write_reg8(REG_A, a);

    cpu.pc += 2;
    count_cycles(2);
}

void rlca() {
#ifdef DISASM
    disasm_log("rlca\n");
#endif

    uint8_t a = read_reg8(REG_A);
    if(a & 0x80) cpu.af |= FLAG_CY;
    else cpu.af &= (~FLAG_CY);

    a <<= 1;
    if(cpu.af & FLAG_CY) a |= 0x01;

    write_reg8(REG_A, a);

    if(!a) cpu.af |= FLAG_ZF;
    else cpu.af &= (~FLAG_ZF);

    cpu.af &= ~(FLAG_N | FLAG_H);

    cpu.pc++;
    count_cycles(1);
}

void sub_hl() {
#ifdef DISASM
    disasm_log("sub (hl)\n");
#endif

    uint8_t a = read_reg8(REG_A);
    uint8_t r;
    r = read_byte(cpu.hl);

    a -= r;

    cpu.af |= FLAG_N;

    if(!a) cpu.af |= FLAG_ZF;
    else cpu.af &= (~FLAG_ZF);

    if(a > read_reg8(REG_A)) cpu.af |= FLAG_CY;
    else cpu.af &= (~FLAG_CY);

    if((a & 0x0F) < (read_reg8(REG_A) & 0x0F)) cpu.af |= FLAG_H;
    else cpu.af &= (~FLAG_H);

    write_reg8(REG_A, a);

    cpu.pc++;
    count_cycles(2);
}

void daa() {
#ifdef DISASM
    disasm_log("daa\n");
#endif

    uint8_t a = read_reg8(REG_A);
    uint8_t correction = 0;

    if((cpu.af & FLAG_H) || ((a & 0x0F) > 0x09))
        correction |= 0x06;

    if((cpu.af & FLAG_CY) || (((a >> 4) & 0x0F) > 0x09)) {
        correction |= 0x60;
        cpu.af |= FLAG_CY;
    } else {
        cpu.af &= (~FLAG_CY);
    }

    if(cpu.af & FLAG_N) a -= correction;
    else a += correction;

    if(!a) cpu.af |= FLAG_ZF;
    else cpu.af &= (~FLAG_ZF);

    cpu.af &= (~FLAG_H); 

    write_reg8(REG_A, a);
    cpu.pc++;
    count_cycles(1);
}

void adc_hl() {
#ifdef DISASM
    disasm_log("adc (hl)\n");
#endif

    uint8_t a = read_reg8(REG_A);
    uint8_t r = read_byte(cpu.hl);
    uint8_t new = a + r;

    if(cpu.af & FLAG_CY) new++;

    cpu.af &= (~FLAG_N);

    if(!new) cpu.af |= FLAG_ZF;
    else cpu.af &= (~FLAG_ZF);

    if((new & 0x0F) < (a & 0x0F)) cpu.af |= FLAG_H;
    else cpu.af &= (~FLAG_H);

    if(new < a) cpu.af |= FLAG_CY;
    else cpu.af &= (~FLAG_CY);

    write_reg8(REG_A, new);

    cpu.pc++;
    count_cycles(2);
}

void ret_nc() {
#ifdef DISASM
    disasm_log("ret nc\n");
#endif

    if(cpu.af & FLAG_CY) {
        // C set; condition false
        cpu.pc++;
        count_cycles(2);
    } else {
        // C clear; condition true
        cpu.pc = pop();
        count_cycles(5);
    }
}

void ret_c() {
#ifdef DISASM
    disasm_log("ret c\n");
#endif

    if(cpu.af & FLAG_CY) {
        // C set; condition true
        cpu.pc = pop();
        count_cycles(5);
    } else {
        // C clear; condition false
        cpu.pc++;
        count_cycles(2);
    }
}

void call_z() {
    uint16_t new_pc = read_word(cpu.pc+1);

#ifdef DISASM
    disasm_log("call z 0x%04X\n", new_pc);
#endif

    if(!(cpu.af & FLAG_ZF)) {
        // ZF clear, condition false
        cpu.pc += 3;
        count_cycles(3);
    } else {
        // ZF set, condition true
        push(cpu.pc+3);
        cpu.pc = new_pc;
        count_cycles(6);
    }
}

void ld_sp_hl() {
#ifdef DISASM
    disasm_log("ld sp, hl\n");
#endif

    cpu.sp = cpu.hl;
    cpu.pc++;
    count_cycles(2);
}

void scf() {
#ifdef DISASM
    disasm_log("scf\n");
#endif

    cpu.af |= FLAG_CY;
    cpu.af &= ~(FLAG_N | FLAG_H);

    cpu.pc++;
    count_cycles(1);
}

void ccf() {
#ifdef DISASM
    disasm_log("ccf\n");
#endif

    if(cpu.af & FLAG_CY) cpu.af &= (~FLAG_CY);
    else cpu.af |= FLAG_CY;

    cpu.af &= ~(FLAG_N | FLAG_H);

    cpu.pc++;
    count_cycles(1);
}

void jp_c_a16() {
    uint16_t new_pc = read_word(cpu.pc+1);

#ifdef DISASM
    disasm_log("jp c 0x%04X\n", new_pc);
#endif

    if(cpu.af & FLAG_CY) {
        // C set, condition true
        cpu.pc = new_pc;
        count_cycles(4);
    } else {
        // C clear, condition false
        cpu.pc += 3;
        count_cycles(3);
    }
}

void jp_nc_a16() {
    uint16_t new_pc = read_word(cpu.pc+1);

#ifdef DISASM
    disasm_log("jp nc 0x%04X\n", new_pc);
#endif

    if(!(cpu.af & FLAG_CY)) {
        // C clear, condition true
        cpu.pc = new_pc;
        count_cycles(4);
    } else {
        // C set, condition false
        cpu.pc += 3;
        count_cycles(3);
    }
}

void rrca() {
#ifdef DISASM
    disasm_log("rrca\n");
#endif

    uint8_t r = read_reg8(REG_A);
    if(r & 0x01) cpu.af |= FLAG_CY;
    else cpu.af &= (~FLAG_CY);

    r >>= 1;

    if(cpu.af & FLAG_CY) r |= 0x80;

    if(!r) cpu.af |= FLAG_ZF;
    else cpu.af &= (~FLAG_ZF);

    cpu.af &= ~(FLAG_N | FLAG_H);

    write_reg8(REG_A, r);
    cpu.pc++;
    count_cycles(1);
}

void and_hl() {
#ifdef DISASM
    disasm_log("and (hl)\n");
#endif

    uint8_t val = read_byte(cpu.hl);
    uint8_t a = read_reg8(REG_A);

    a &= val;

    if(!a) cpu.af |= FLAG_ZF;
    else cpu.af &= (~FLAG_ZF);

    cpu.af &= ~(FLAG_N | FLAG_CY);
    cpu.af |= FLAG_H;

    write_reg8(REG_A, a);

    cpu.pc++;
    count_cycles(2);
}

void sbc_a_a8() {
    uint8_t r = read_byte(cpu.pc+1);

#ifdef DISASM
    disasm_log("sbc a, 0x%02X\n", r);
#endif

    uint8_t a = read_reg8(REG_A);

    a -= r;
    if(cpu.af & FLAG_CY) a--;

    cpu.af |= FLAG_N;

    if(!a) cpu.af |= FLAG_ZF;
    else cpu.af &= (~FLAG_ZF);

    if(a > read_reg8(REG_A)) cpu.af |= FLAG_CY;
    else cpu.af &= (~FLAG_CY);

    if((a & 0x0F) < (read_reg8(REG_A) & 0x0F)) cpu.af |= FLAG_H;
    else cpu.af &= (~FLAG_H);

    write_reg8(REG_A, a);

    cpu.pc += 2;
    count_cycles(2);
}

void call_nc() {
    uint16_t new_pc = read_word(cpu.pc+1);

#ifdef DISASM
    disasm_log("call nc 0x%04X\n", new_pc);
#endif

    if(cpu.af & FLAG_CY) {
        // C set, condition false
        cpu.pc += 3;
        count_cycles(3);
    } else {
        // C clear, condition true
        push(cpu.pc+3);
        cpu.pc = new_pc;
        count_cycles(6);
    }
}

void ld_a16_sp() {
    uint16_t a16 = read_word(cpu.pc+1);

#ifdef DISASM
    disasm_log("ld (0x%04X), sp\n", a16);
#endif

    write_byte(a16, cpu.sp);
    write_byte(a16+1, cpu.sp >> 8);

    cpu.pc += 3;
    count_cycles(5);
}

void call_c() {
    uint16_t new_pc = read_word(cpu.pc+1);

#ifdef DISASM
    disasm_log("call c 0x%04X\n", new_pc);
#endif

    if(!(cpu.af & FLAG_CY)) {
        // C clear, condition false
        cpu.pc += 3;
        count_cycles(3);
    } else {
        // C set, condition true
        push(cpu.pc+3);
        cpu.pc = new_pc;
        count_cycles(6);
    }
}

void rla() {
#ifdef DISASM
    disasm_log("rla\n");
#endif

    uint8_t r = read_reg8(REG_A);
    uint8_t old_cy;
    if(cpu.af & FLAG_CY) old_cy = 0x01;
    else old_cy = 0x00;

    if(r & 0x80) cpu.af |= FLAG_CY;
    else cpu.af &= (~FLAG_CY);

    r <<= 1;
    r |= old_cy;

    if(!r) cpu.af |= FLAG_ZF;
    else cpu.af &= (~FLAG_ZF);

    cpu.af &= ~(FLAG_N | FLAG_H);

    write_reg8(REG_A, r);
    cpu.pc++;
    count_cycles(1);
}

void sbc_a_hl() {
#ifdef DISASM
    disasm_log("sbc a, (hl)\n");
#endif

    uint8_t a = read_reg8(REG_A);
    uint8_t r;
    r = read_byte(cpu.hl);

    a -= r;
    if(cpu.af & FLAG_CY) a--;

    cpu.af |= FLAG_N;

    if(!a) cpu.af |= FLAG_ZF;
    else cpu.af &= (~FLAG_ZF);

    if(a > read_reg8(REG_A)) cpu.af |= FLAG_CY;
    else cpu.af &= (~FLAG_CY);

    if((a & 0x0F) < (read_reg8(REG_A) & 0x0F)) cpu.af |= FLAG_H;
    else cpu.af &= (~FLAG_H);

    write_reg8(REG_A, a);

    cpu.pc++;
    count_cycles(2);
}

void xor_hl() {
#ifdef DISASM
    disasm_log("xor (hl)\n");
#endif

    uint8_t val = read_byte(cpu.hl);
    uint8_t a = read_reg8(REG_A);

    a ^= val;

    if(!a) cpu.af |= FLAG_ZF;
    else cpu.af &= (~FLAG_ZF);

    cpu.af &= ~(FLAG_N | FLAG_H | FLAG_CY);

    write_reg8(REG_A, a);

    cpu.pc++;
    count_cycles(2);
}

void adc_d8() {
    uint8_t d8 = read_byte(cpu.pc+1);

#ifdef DISASM
    disasm_log("adc 0x%02X\n", d8);
#endif

    uint8_t a = read_reg8(REG_A);
    uint8_t new = a + d8;
    if(cpu.af & FLAG_CY) new++;

    cpu.af &= (~FLAG_N);

    if(!new) cpu.af |= FLAG_ZF;
    else cpu.af &= (~FLAG_ZF);

    if((new & 0x0F) < (a & 0x0F)) cpu.af |= FLAG_H;
    else cpu.af &= (~FLAG_H);

    if(new < a) cpu.af |= FLAG_CY;
    else cpu.af &= (~FLAG_CY);

    write_reg8(REG_A, new);

    cpu.pc += 2;
    count_cycles(2);
}

/* 
    EXTENDED OPCODES
    these are all prefixed with 0xCB first
*/

// general handler for extended opcodes
void ex_opcode() {
    uint8_t opcode = read_byte(cpu.pc+1);

    if(!ex_opcodes[opcode]) {
        write_log("undefined opcode %02X %02X %02X, dumping CPU state...\n", read_byte(cpu.pc), read_byte(cpu.pc+1), read_byte(cpu.pc+2));
        dump_cpu();
    } else {
        ex_opcodes[opcode]();
    }
}

// individual 0xCB-prefixed instructions
void res_n_r() {
    uint8_t opcode = read_byte(cpu.pc+1);

    int reg = opcode & 7;
    int n = (opcode >> 3) & 7;

#ifdef DISASM
    disasm_log("res %d, %s\n", n, registers[reg]);
#endif

    uint8_t r = read_reg8(reg);
    r &= ~(1 << n);
    write_reg8(reg, r);

    cpu.pc += 2;
    count_cycles(2);
}

void swap_r() {
    uint8_t opcode = read_byte(cpu.pc+1);

    int reg = opcode & 7;

#ifdef DISASM
    disasm_log("swap %s\n", registers[reg]);
#endif

    uint8_t r = read_reg8(reg);

    uint8_t lo, hi;
    lo = r & 0x0F;
    hi = (r >> 4) & 0x0F;

    uint8_t new_r = (lo << 4) | hi;
    write_reg8(reg, new_r);

    if(!new_r) cpu.af |= FLAG_ZF;
    cpu.af &= ~(FLAG_N | FLAG_H | FLAG_CY);

    cpu.pc += 2;
    count_cycles(2);
}

void sla_r() {
    uint8_t opcode = read_byte(cpu.pc+1);
    int reg = opcode & 7;

#ifdef DISASM
    disasm_log("sla %s\n", registers[reg]);
#endif

    cpu.af &= ~(FLAG_H | FLAG_N);

    uint8_t r = read_reg8(reg);
    if(r & 0x80) cpu.af |= FLAG_CY;
    else cpu.af &= (~FLAG_CY);

    r <<= 1;

    if(!r) cpu.af |= FLAG_ZF;
    else cpu.af &= (~FLAG_ZF);

    write_reg8(reg, r);

    cpu.pc += 2;
    count_cycles(2);
}

void bit_n_hl() {
    uint8_t opcode = read_byte(cpu.pc+1);
    int n = (opcode >> 3) & 7;

#ifdef DISASM
    disasm_log("bit %d, (hl)\n", n);
#endif

    cpu.af &= (~FLAG_N);
    cpu.af |= FLAG_H;

    uint8_t byte = read_byte(cpu.hl);
    if(byte & (1 << n)) cpu.af &= (~FLAG_ZF);
    else cpu.af |= FLAG_ZF;

    cpu.pc += 2;
    count_cycles(3);
}

void bit_n_r() {
    uint8_t opcode = read_byte(cpu.pc+1);
    int n = (opcode >> 3) & 7;
    int reg = opcode & 7;

#ifdef DISASM
    disasm_log("bit %d, %s\n", n, registers[reg]);
#endif

    cpu.af &= (~FLAG_N);
    cpu.af |= FLAG_H;

    uint8_t byte = read_reg8(reg);
    if(byte & (1 << n)) cpu.af &= (~FLAG_ZF);
    else cpu.af |= FLAG_ZF;

    cpu.pc += 2;
    count_cycles(2);
}

void srl_r() {
    uint8_t opcode = read_byte(cpu.pc+1);
    int reg = opcode & 7;

#ifdef DISASM
    disasm_log("srl %s\n", registers[reg]);
#endif

    cpu.af &= ~(FLAG_N | FLAG_H);

    uint8_t r = read_reg8(reg);
    if(r & 0x01) cpu.af |= FLAG_CY;
    else cpu.af &= (~FLAG_CY);

    r >>= 1;

    if(!r) cpu.af |= FLAG_ZF;
    else cpu.af &= (~FLAG_ZF);

    write_reg8(reg, r);
    cpu.pc += 2;
    count_cycles(2);
}

void rr_r() {
    uint8_t opcode = read_byte(cpu.pc+1);
    int reg = opcode & 7;

#ifdef DISASM
    disasm_log("rr %s\n", registers[reg]);
#endif

    cpu.af &= ~(FLAG_N | FLAG_H);

    uint8_t old_cy;
    if(cpu.af & FLAG_CY) old_cy = 0x80;
    else old_cy = 0x00;

    uint8_t r = read_reg8(reg);
    if(r & 0x01) cpu.af |= FLAG_CY;
    else cpu.af &= (~FLAG_CY);

    r >>= 1;
    r |= old_cy;

    if(!r) cpu.af |= FLAG_ZF;
    else cpu.af &= (~FLAG_ZF);

    write_reg8(reg, r);
    cpu.pc += 2;
    count_cycles(2);
}

void set_n_r() {
    uint8_t opcode = read_byte(cpu.pc+1);
    int n = (opcode >> 3) & 7;
    int reg = opcode & 7;

#ifdef DISASM
    disasm_log("set %d, %s\n", n, registers[reg]);
#endif

    uint8_t val = read_reg8(reg);
    val |= (1 << n);
    write_reg8(reg, val);

    cpu.pc += 2;
    count_cycles(2);
}

void set_n_hl() {
    uint8_t opcode = read_byte(cpu.pc+1);
    int n = (opcode >> 3) & 7;

#ifdef DISASM
    disasm_log("set %d, (hl)\n", n);
#endif

    uint8_t val = read_byte(cpu.hl);
    val |= (1 << n);
    write_byte(cpu.hl, val);

    cpu.pc += 2;
    count_cycles(4);
}

void res_n_hl() {
    uint8_t opcode = read_byte(cpu.pc+1);
    int n = (opcode >> 3) & 7;

#ifdef DISASM
    disasm_log("res %d, (hl)\n", n);
#endif

    uint8_t val = read_byte(cpu.hl);
    val &= ~(1 << n);
    write_byte(cpu.hl, val);

    cpu.pc += 2;
    count_cycles(4);
}

void rl_r() {
    uint8_t opcode = read_byte(cpu.pc+1);
    int reg = opcode & 7;

#ifdef DISASM
    disasm_log("rl %s\n", registers[reg]);
#endif

    uint8_t r = read_reg8(reg);
    uint8_t old_cy;
    if(cpu.af & FLAG_CY) old_cy = 0x01;
    else old_cy = 0x00;

    if(r & 0x80) cpu.af |= FLAG_CY;
    else cpu.af &= (~FLAG_CY);

    r <<= 1;
    r |= old_cy;

    if(!r) cpu.af |= FLAG_ZF;
    else cpu.af &= (~FLAG_ZF);

    cpu.af &= ~(FLAG_N | FLAG_H);

    write_reg8(reg, r);
    cpu.pc += 2;
    count_cycles(2);
}

void sra_r() {
    uint8_t opcode = read_byte(cpu.pc+1);
    int reg = opcode & 7;

#ifdef DISASM
    disasm_log("sra %s\n", registers[reg]);
#endif

    uint8_t r = read_reg8(reg);
    uint8_t new_msb;
    if(r & 0x80) new_msb = 0x80;
    else new_msb = 0x00;

    if(r & 0x01) cpu.af |= FLAG_CY;
    else cpu.af &= (~FLAG_CY);

    r >>= 1;
    r |= new_msb;

    if(!r) cpu.af |= FLAG_ZF;
    else cpu.af &= (~FLAG_ZF);

    cpu.af &= ~(FLAG_N | FLAG_H);
    write_reg8(reg, r);
    cpu.pc += 2;
    count_cycles(2);
}

void rrc_r() {
    uint8_t opcode = read_byte(cpu.pc+1);
    int reg = opcode & 7;

#ifdef DISASM
    disasm_log("rrc %s\n", registers[reg]);
#endif

    uint8_t r = read_reg8(reg);
    if(r & 0x01) cpu.af |= FLAG_CY;
    else cpu.af &= (~FLAG_CY);

    r >>= 1;

    if(cpu.af & FLAG_CY) r |= 0x80;

    if(!r) cpu.af |= FLAG_ZF;
    else cpu.af &= (~FLAG_ZF);

    cpu.af &= ~(FLAG_N | FLAG_H);

    write_reg8(reg, r);
    cpu.pc += 2;
    count_cycles(2);
}

void rrc_hl() {
#ifdef DISASM
    disasm_log("rrc (hl)\n");
#endif

    uint8_t r = read_byte(cpu.hl);
    if(r & 0x01) cpu.af |= FLAG_CY;
    else cpu.af &= (~FLAG_CY);

    r >>= 1;

    if(cpu.af & FLAG_CY) r |= 0x80;

    if(!r) cpu.af |= FLAG_ZF;
    else cpu.af &= (~FLAG_ZF);

    cpu.af &= ~(FLAG_N | FLAG_H);

    write_byte(cpu.hl, r);
    cpu.pc += 2;
    count_cycles(4);
}

void swap_hl() {
#ifdef DISASM
    disasm_log("swap (hl)\n");
#endif

    uint8_t r = read_byte(cpu.hl);

    uint8_t lo, hi;
    lo = r & 0x0F;
    hi = (r >> 4) & 0x0F;

    uint8_t new_r = (lo << 4) | hi;
    write_byte(cpu.hl, new_r);

    if(!new_r) cpu.af |= FLAG_ZF;
    cpu.af &= ~(FLAG_N | FLAG_H | FLAG_CY);

    cpu.pc += 2;
    count_cycles(4);
}

void rlc_r() {
    uint8_t opcode = read_byte(cpu.pc+1);
    int reg = opcode & 7;
    
#ifdef DISASM
    disasm_log("rlc %s\n", registers[reg]);
#endif

    uint8_t r = read_reg8(reg);
    if(r & 0x80) cpu.af |= FLAG_CY;
    else cpu.af &= (~FLAG_CY);

    r <<= 1;
    if(cpu.af & FLAG_CY) r |= 0x01;

    write_reg8(reg, r);

    if(!r) cpu.af |= FLAG_ZF;
    else cpu.af &= (~FLAG_ZF);

    cpu.af &= ~(FLAG_N | FLAG_H);

    cpu.pc += 2;
    count_cycles(2);
}

void rlc_hl() {
#ifdef DISASM
    disasm_log("rlc (hl)\n");
#endif

    uint8_t r = read_byte(cpu.hl);
    if(r & 0x80) cpu.af |= FLAG_CY;
    else cpu.af &= (~FLAG_CY);

    r <<= 1;
    if(cpu.af & FLAG_CY) r |= 0x01;

    write_byte(cpu.hl, r);

    if(!r) cpu.af |= FLAG_ZF;
    else cpu.af &= (~FLAG_ZF);

    cpu.af &= ~(FLAG_N | FLAG_H);

    cpu.pc += 2;
    count_cycles(4);
}

// lookup tables
void (*opcodes[256])() = {
    nop, ld_r_xxxx, ld_bc_a, inc_r16, inc_r, dec_r, ld_r_xx, rlca,  // 0x00
    ld_a16_sp, add_hl_r16, ld_a_bc, dec_r16, inc_r, dec_r, ld_r_xx, rrca,  // 0x08
    NULL, ld_r_xxxx, ld_de_a, inc_r16, inc_r, dec_r, ld_r_xx, rla,  // 0x10
    jr_e, add_hl_r16, ld_a_de, dec_r16, inc_r, dec_r, ld_r_xx, rra,  // 0x18
    jr_nz, ld_r_xxxx, ldi_hl_a, inc_r16, inc_r, dec_r, ld_r_xx, daa,  // 0x20
    jr_z, add_hl_r16, ldi_a_hl, dec_r16, inc_r, dec_r, ld_r_xx, cpl,  // 0x28
    jr_nc, ld_r_xxxx, ldd_hl_a, inc_r16, inc_hl, dec_hl, ld_hl_n, scf,  // 0x30
    jr_c, add_hl_r16, ldd_a_hl, dec_r16, inc_r, dec_r, ld_r_xx, ccf,  // 0x38

    // 8-bit loads
    ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_hl, ld_r_r,  // 0x40
    ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_hl, ld_r_r,  // 0x48
    ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_hl, ld_r_r,  // 0x50
    ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_hl, ld_r_r,  // 0x58
    ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_hl, ld_r_r,  // 0x60
    ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_hl, ld_r_r,  // 0x68
    ld_hl_r, ld_hl_r, ld_hl_r, ld_hl_r, ld_hl_r, ld_hl_r, halt, ld_hl_r,  // 0x70
    ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_hl, ld_r_r,  // 0x78

    add_r, add_r, add_r, add_r, add_r, add_r, add_hl, add_r,  // 0x80
    adc_r, adc_r, adc_r, adc_r, adc_r, adc_r, adc_hl, adc_r,  // 0x88
    sub_r, sub_r, sub_r, sub_r, sub_r, sub_r, sub_hl, sub_r,  // 0x90
    sbc_a_r, sbc_a_r, sbc_a_r, sbc_a_r, sbc_a_r, sbc_a_r, sbc_a_hl, sbc_a_r,  // 0x98
    and_r, and_r, and_r, and_r, and_r, and_r, and_hl, and_r,  // 0xA0
    xor_r, xor_r, xor_r, xor_r, xor_r, xor_r, xor_hl, xor_r,  // 0xA8
    or_r, or_r, or_r, or_r, or_r, or_r, or_hl, or_r,  // 0xB0
    cp_r, cp_r, cp_r, cp_r, cp_r, cp_r, cp_hl, cp_r,  // 0xB8
    ret_nz, pop_r16, jp_nz_a16, jp_nn, call_nz, push_r16, add_d8, rst,  // 0xC0
    ret_z, ret, jp_z_a16, ex_opcode, call_z, call_a16, adc_d8, rst,  // 0xC8
    ret_nc, pop_r16, jp_nc_a16, NULL, call_nc, push_r16, sub_d8, rst,  // 0xD0
    ret_c, reti, jp_c_a16, NULL, call_c, NULL, sbc_a_a8, rst,  // 0xD8
    ldh_a8_a, pop_r16, ldh_c_a, NULL, NULL, push_r16, and_n, rst,  // 0xE0
    add_sp_s, jp_hl, ld_a16_a, NULL, NULL, NULL, xor_d8, rst,  // 0xE8
    ldh_a_a8, pop_af, ldh_a_c, di, NULL, push_af, or_d8, rst,  // 0xF0
    ld_hl_sp_s, ld_sp_hl, ld_a_a16, ei, NULL, NULL, cp_xx, rst,  // 0xF8
};

void (*ex_opcodes[256])() = {
    rlc_r, rlc_r, rlc_r, rlc_r, rlc_r, rlc_r, rlc_hl, rlc_r,     // 0x00
    rrc_r, rrc_r, rrc_r, rrc_r, rrc_r, rrc_r, rrc_hl, rrc_r,     // 0x08
    rl_r, rl_r, rl_r, rl_r, rl_r, rl_r, NULL, rl_r,     // 0x10
    rr_r, rr_r, rr_r, rr_r, rr_r, rr_r, NULL, rr_r,     // 0x18
    sla_r, sla_r, sla_r, sla_r, sla_r, sla_r, NULL, sla_r,     // 0x20
    sra_r, sra_r, sra_r, sra_r, sra_r, sra_r, NULL, sra_r,     // 0x28
    swap_r, swap_r, swap_r, swap_r, swap_r, swap_r, swap_hl, swap_r,     // 0x30
    srl_r, srl_r, srl_r, srl_r, srl_r, srl_r, NULL, srl_r,     // 0x38

    bit_n_r, bit_n_r, bit_n_r, bit_n_r, bit_n_r, bit_n_r, bit_n_hl, bit_n_r,     // 0x40
    bit_n_r, bit_n_r, bit_n_r, bit_n_r, bit_n_r, bit_n_r, bit_n_hl, bit_n_r,     // 0x48
    bit_n_r, bit_n_r, bit_n_r, bit_n_r, bit_n_r, bit_n_r, bit_n_hl, bit_n_r,     // 0x50
    bit_n_r, bit_n_r, bit_n_r, bit_n_r, bit_n_r, bit_n_r, bit_n_hl, bit_n_r,     // 0x58
    bit_n_r, bit_n_r, bit_n_r, bit_n_r, bit_n_r, bit_n_r, bit_n_hl, bit_n_r,     // 0x60
    bit_n_r, bit_n_r, bit_n_r, bit_n_r, bit_n_r, bit_n_r, bit_n_hl, bit_n_r,     // 0x68
    bit_n_r, bit_n_r, bit_n_r, bit_n_r, bit_n_r, bit_n_r, bit_n_hl, bit_n_r,     // 0x70
    bit_n_r, bit_n_r, bit_n_r, bit_n_r, bit_n_r, bit_n_r, bit_n_hl, bit_n_r,     // 0x78

    res_n_r, res_n_r, res_n_r, res_n_r, res_n_r, res_n_r, res_n_hl, res_n_r,     // 0x80
    res_n_r, res_n_r, res_n_r, res_n_r, res_n_r, res_n_r, res_n_hl, res_n_r,     // 0x88
    res_n_r, res_n_r, res_n_r, res_n_r, res_n_r, res_n_r, res_n_hl, res_n_r,     // 0x90
    res_n_r, res_n_r, res_n_r, res_n_r, res_n_r, res_n_r, res_n_hl, res_n_r,     // 0x98
    res_n_r, res_n_r, res_n_r, res_n_r, res_n_r, res_n_r, res_n_hl, res_n_r,     // 0xA0
    res_n_r, res_n_r, res_n_r, res_n_r, res_n_r, res_n_r, res_n_hl, res_n_r,     // 0xA8
    res_n_r, res_n_r, res_n_r, res_n_r, res_n_r, res_n_r, res_n_hl, res_n_r,     // 0xB0
    res_n_r, res_n_r, res_n_r, res_n_r, res_n_r, res_n_r, res_n_hl, res_n_r,     // 0xB8

    set_n_r, set_n_r, set_n_r, set_n_r, set_n_r, set_n_r, set_n_hl, set_n_r,     // 0xC0
    set_n_r, set_n_r, set_n_r, set_n_r, set_n_r, set_n_r, set_n_hl, set_n_r,     // 0xC8
    set_n_r, set_n_r, set_n_r, set_n_r, set_n_r, set_n_r, set_n_hl, set_n_r,     // 0xD0
    set_n_r, set_n_r, set_n_r, set_n_r, set_n_r, set_n_r, set_n_hl, set_n_r,     // 0xD8
    set_n_r, set_n_r, set_n_r, set_n_r, set_n_r, set_n_r, set_n_hl, set_n_r,     // 0xE0
    set_n_r, set_n_r, set_n_r, set_n_r, set_n_r, set_n_r, set_n_hl, set_n_r,     // 0xE8
    set_n_r, set_n_r, set_n_r, set_n_r, set_n_r, set_n_r, set_n_hl, set_n_r,     // 0xF0
    set_n_r, set_n_r, set_n_r, set_n_r, set_n_r, set_n_r, set_n_hl, set_n_r,     // 0xF8
};

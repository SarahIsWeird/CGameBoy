//
// Created by Sarah Klocke on 05.09.21.
//

#include "cpu.h"

static uint8_t *decode_src_or_dst_middle(cpu_t *cpu, uint8_t *mem, uint8_t id) {
    switch (id) {
        case 0: return &cpu->registers.w.B;
        case 1: return &cpu->registers.w.C;
        case 2: return &cpu->registers.w.D;
        case 3: return &cpu->registers.w.E;
        case 4: return &cpu->registers.w.H;
        case 5: return &cpu->registers.w.L;
        case 6: return &mem[cpu->registers.dw.HL];
        case 7: return &cpu->registers.w.A;

        default: return NULL; // Should never occur.
    }
}

static uint8_t *decode_src_middle(cpu_t *cpu, uint8_t *mem, uint8_t op) {
    return decode_src_or_dst_middle(cpu, mem, op & 0b0111);
}

static uint8_t *decode_dst_middle(cpu_t *cpu, uint8_t *mem, uint8_t op) {
    return decode_src_or_dst_middle(cpu, mem, op >> 3 & 0b0111);
}

static uint8_t *decode_dst_acb(cpu_t *cpu, uint8_t *mem, uint8_t op) {
    uint8_t id = op >> 3 & 0b111;
    id = id & 0b100 + (id >> 1 & 0b001) + (id << 1 & 0b010);

    return decode_src_or_dst_middle(cpu, mem, id);
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wint-to-pointer-cast"

static uint8_t *decode_src_dst_hl(cpu_t *cpu, uint8_t op) {
    switch (op >> 4 & 0b11) {
        case 0: return (uint8_t *) cpu->registers.dw.BC;
        case 1: return (uint8_t *) cpu->registers.dw.DE;
        case 2: return (uint8_t *) (cpu->registers.dw.HL++);
        case 3: return (uint8_t *) (cpu->registers.dw.HL--);

        default: return NULL; // Should never occur.
    }
}

static uint8_t *decode_src_dst_sp(cpu_t *cpu, uint8_t op) {
    switch (op >> 4 & 0b11) {
        case 0: return (uint8_t *) cpu->registers.dw.BC;
        case 1: return (uint8_t *) cpu->registers.dw.DE;
        case 2: return (uint8_t *) cpu->registers.dw.HL;
        case 3: return (uint8_t *) cpu->registers.dw.SP;

        default: return NULL; // Should never occur.
    }
}

static uint8_t *decode_src_dst_af(cpu_t *cpu, uint8_t op) {
    switch (op >> 4 & 0b11) {
        case 0: return (uint8_t *) cpu->registers.dw.BC;
        case 1: return (uint8_t *) cpu->registers.dw.DE;
        case 2: return (uint8_t *) cpu->registers.dw.HL;
        case 3: return (uint8_t *) cpu->registers.dw.AF;

        default: return NULL; // Should never occur.
    }
}

#pragma clang diagnostic pop

static inline uint8_t did_carry_add_a(cpu_t *cpu, uint8_t old_a) {
    return (cpu->registers.w.A < old_a);
}

static inline uint8_t did_carry_sub_a(cpu_t *cpu, uint8_t old_a) {
    return (cpu->registers.w.A > old_a);
}

static inline uint8_t did_half_carry_add_a(cpu_t *cpu, uint8_t old_a) {
    return (cpu->registers.w.A >= 0x10 && old_a < 0x10);
}

static inline uint8_t did_half_carry_sub_a(cpu_t *cpu, uint8_t old_a) {
    return (cpu->registers.w.A <= 0x10 && old_a > 0x10);
}

static inline void call(cpu_t *cpu, uint8_t *mem, uint16_t target) {
    cpu->registers.dw.SP -= 2;
    ((uint16_t*) mem)[cpu->registers.dw.SP] = cpu->registers.dw.PC;
    cpu->registers.dw.PC = target;
}

static inline void ret(cpu_t *cpu, const uint8_t *mem) {
    cpu->registers.dw.PC = mem[cpu->registers.dw.SP];
    cpu->registers.dw.SP += 2;
}

static uint16_t rst_destinations[] = { 0x00, 0x08, 0x10, 0x18, 0x20, 0x28, 0x30, 0x38 };

void cpu_tick(cpu_t *cpu, uint8_t *mem) {
    op_t op;
    op.value = mem[cpu->registers.dw.PC++];

    if (op.value >= 0x40 && op.value <= 0x7f) { // ld r, r'
        *decode_dst_middle(cpu, mem, op.value) = *decode_src_middle(cpu, mem, op.value);
        return;
    }

    if (op.value >= 0x80 && op.value <= 0xbf) { // Most of 8-bit arithmetic
        uint8_t *src = decode_src_middle(cpu, mem, op.value);

        uint8_t old_a = cpu->registers.w.A;
        uint8_t would_be_a;

        switch (op.value >> 3 & 0b0111) {
            case 0: // add A, r
                cpu->registers.w.A += *src;

                cpu->registers.w.F.z = cpu->registers.w.A == 0;
                cpu->registers.w.F.n = 0;
                cpu->registers.w.F.h = did_half_carry_add_a(cpu, old_a);
                cpu->registers.w.F.c = did_carry_add_a(cpu, old_a);
                return;
            case 1: // adc A, r
                cpu->registers.w.A += *src + cpu->registers.w.F.c;

                cpu->registers.w.F.z = cpu->registers.w.A == 0;
                cpu->registers.w.F.n = 0;
                cpu->registers.w.F.h = did_half_carry_add_a(cpu, old_a);
                cpu->registers.w.F.c = did_carry_add_a(cpu, old_a);
                return;
            case 2: // sub A, r
                cpu->registers.w.A -= *src;

                cpu->registers.w.F.z = cpu->registers.w.A == 0;
                cpu->registers.w.F.n = 1;
                cpu->registers.w.F.h = did_half_carry_sub_a(cpu, old_a);
                cpu->registers.w.F.c = did_carry_sub_a(cpu, old_a);
                return;
            case 3: // sbc A, r
                cpu->registers.w.A -= *src + cpu->registers.w.F.c;

                cpu->registers.w.F.z = cpu->registers.w.A == 0;
                cpu->registers.w.F.n = 1;
                cpu->registers.w.F.h = did_half_carry_sub_a(cpu, old_a);
                cpu->registers.w.F.c = did_carry_sub_a(cpu, old_a);
                return;
            case 4: // and A, r
                cpu->registers.w.A &= *src;

                cpu->registers.w.F.z = cpu->registers.w.A == 0;
                cpu->registers.w.F.n = 0;
                cpu->registers.w.F.h = 1;
                cpu->registers.w.F.c = 0;
                return;
            case 5: // xor A, r
                cpu->registers.w.A ^= *src;

                cpu->registers.w.F.z = cpu->registers.w.A == 0;
                cpu->registers.w.F.n = 0;
                cpu->registers.w.F.h = 0;
                cpu->registers.w.F.c = 0;
                return;
            case 6: // or A, r
                cpu->registers.w.A |= *src;

                cpu->registers.w.F.z = cpu->registers.w.A == 0;
                cpu->registers.w.F.n = 0;
                cpu->registers.w.F.h = 0;
                cpu->registers.w.F.c = 0;
                return;
            case 7: // cp A, r
                would_be_a = cpu->registers.w.F.n - *src;

                cpu->registers.w.F.z = would_be_a == 0;
                cpu->registers.w.F.n = 1;
                cpu->registers.w.F.h = would_be_a <= 0x10 && old_a > 0x10;
                cpu->registers.w.F.c = would_be_a > old_a;
                return;
        }
    }

    if ((op.value & 0x11000000) == 0) { // inc, dec, ld imm
        uint8_t *dst = decode_dst_acb(cpu, mem, op.value);
        uint8_t old_dst = *dst;

        switch (op.value & 0b111) {
            case 0b100: // inc r
                (*dst)++;

                cpu->registers.w.F.z = *dst == 0;
                cpu->registers.w.F.n = 0;
                cpu->registers.w.F.h = old_dst <= 0x10 && *dst > 0x10;

                return;
            case 0b101: // dec r
                (*dst)--;

                cpu->registers.w.F.z = *dst == 0;
                cpu->registers.w.F.n = 1;
                cpu->registers.w.F.h = old_dst >= 0x10 && *dst < 0x10;

                return;
            case 0b110: // ld r, n
                *dst = mem[cpu->registers.dw.PC++];

                return;
        }

        uint8_t *src_dst = decode_src_dst_sp(cpu, op.value);
        uint8_t old_src_dst = *src_dst;

        switch (op.value & 0b1111) {
            case 3: // inc rr
                (*src_dst)++;

                return;
            case 9: // add hl, rr
                cpu->registers.dw.HL = *src_dst;

                cpu->registers.w.F.n = 0;
                cpu->registers.w.F.h = old_src_dst >= 0x10 && *src_dst < 0x10;
                cpu->registers.w.F.c = *src_dst < old_src_dst;

                return;
            case 11: // dec rr
                (*src_dst)--;

                return;
            case 1: // ld rr, nn
                *src_dst = (mem[cpu->registers.dw.PC++] << 8) + mem[cpu->registers.dw.PC++];

                return;
        }
    }

    if ((op.value & 0b1111) == 0b0010) { // ld (rr), A / ld A, (rr)
        if ((op.value >> 6 & 0b11) == 0b00) { // ld (rr), A
            uint8_t *dst = decode_src_dst_hl(cpu, op.value);
            *dst = cpu->registers.w.A;
        }

        if ((op.value >> 6 & 0b11) == 0b10) { // ld A, (rr)
            uint8_t *src = decode_src_dst_hl(cpu, op.value);
            cpu->registers.w.A = *src;
        }
    }

    if ((op.value >> 6 & 0b11) == 0b11) {
        if ((op.value & 0b1111) == 0b0001) { // pop rr
            uint8_t *dst = decode_src_dst_af(cpu, op.value);
            *dst = mem[cpu->registers.dw.SP];
            cpu->registers.dw.SP += 2;

            return;
        }

        if ((op.value & 0b1111) == 0b0101) { // push rr
            uint8_t *src = decode_src_dst_af(cpu, op.value);
            cpu->registers.dw.SP -= 2;
            mem[cpu->registers.dw.SP] = *src;

            return;
        }

        if ((op.value & 0b111) == 0b110) {
            uint8_t id = op.value >> 3 & 0b111;
            id = id & 0b100 + (id >> 1 & 0b001) + (id << 1 & 0b010);

            uint8_t imm = mem[cpu->registers.dw.PC++];
            uint8_t old_a = cpu->registers.w.A;
            uint8_t would_be_a;

            switch (id & 0b111) {
                case 0: // add A, n
                    cpu->registers.w.A += imm;

                    cpu->registers.w.F.z = cpu->registers.w.A == 0;
                    cpu->registers.w.F.n = 0;
                    cpu->registers.w.F.h = did_half_carry_add_a(cpu, old_a);
                    cpu->registers.w.F.c = did_carry_add_a(cpu, old_a);
                    return;
                case 1: // adc A, n
                    cpu->registers.w.A += imm + cpu->registers.w.F.c;

                    cpu->registers.w.F.z = cpu->registers.w.A == 0;
                    cpu->registers.w.F.n = 0;
                    cpu->registers.w.F.h = did_half_carry_add_a(cpu, old_a);
                    cpu->registers.w.F.c = did_carry_add_a(cpu, old_a);
                    return;
                case 2: // sub A, n
                    cpu->registers.w.A -= imm;

                    cpu->registers.w.F.z = cpu->registers.w.A == 0;
                    cpu->registers.w.F.n = 1;
                    cpu->registers.w.F.h = did_half_carry_sub_a(cpu, old_a);
                    cpu->registers.w.F.c = did_carry_sub_a(cpu, old_a);
                    return;
                case 3: // sbc A, n
                    cpu->registers.w.A -= imm + cpu->registers.w.F.c;

                    cpu->registers.w.F.z = cpu->registers.w.A == 0;
                    cpu->registers.w.F.n = 1;
                    cpu->registers.w.F.h = did_half_carry_sub_a(cpu, old_a);
                    cpu->registers.w.F.c = did_carry_sub_a(cpu, old_a);
                    return;
                case 4: // and A, n
                    cpu->registers.w.A &= imm;

                    cpu->registers.w.F.z = cpu->registers.w.A == 0;
                    cpu->registers.w.F.n = 0;
                    cpu->registers.w.F.h = 1;
                    cpu->registers.w.F.c = 0;
                    return;
                case 5: // xor A, n
                    cpu->registers.w.A ^= imm;

                    cpu->registers.w.F.z = cpu->registers.w.A == 0;
                    cpu->registers.w.F.n = 0;
                    cpu->registers.w.F.h = 0;
                    cpu->registers.w.F.c = 0;
                    return;
                case 6: // or A, n
                    cpu->registers.w.A |= imm;

                    cpu->registers.w.F.z = cpu->registers.w.A == 0;
                    cpu->registers.w.F.n = 0;
                    cpu->registers.w.F.h = 0;
                    cpu->registers.w.F.c = 0;
                    return;
                case 7: // cp A, n
                    would_be_a = cpu->registers.w.F.n - imm;

                    cpu->registers.w.F.z = would_be_a == 0;
                    cpu->registers.w.F.n = 1;
                    cpu->registers.w.F.h = would_be_a <= 0x10 && old_a > 0x10;
                    cpu->registers.w.F.c = would_be_a > old_a;
                    return;
            }
        }

        if ((op.value & 0b111) == 0b111) {
            uint8_t id = op.value >> 3 & 0b111;
            id = id & 0b100 + (id >> 1 & 0b001) + (id << 1 & 0b010);

            call(cpu, mem, rst_destinations[id]);

            return;
        }
    }

    uint8_t imm_u;
    int8_t imm_s;
    uint16_t imm16_u;
    uint16_t tmp;
    uint8_t daa_corr;

    switch (op.value) {
        case 0x00: // NOP NOLINT(bugprone-branch-clone)
            return;
        case 0x10: // STOP
            cpu->state.stopped = 1;

            return;
        case 0x20: // JR NZ, n;
            imm_s = (int8_t) mem[cpu->registers.dw.PC++];

            if (!cpu->registers.w.F.n) {
                call(cpu, mem, imm_s);
            }

            return;
        case 0x30: // JR NC, n
            imm_s = (int8_t) mem[cpu->registers.dw.PC++];

            if (!cpu->registers.w.F.c) {
                imm_s = (int8_t) mem[cpu->registers.dw.PC];
                call(cpu, mem, imm_s);
            }

            return;
        case 0xc0: // RET NZ
            if (!cpu->registers.w.F.n) {
                ret(cpu, mem);
            }

            return;
        case 0xd0: // RET NC
            if (!cpu->registers.w.F.c) {
                ret(cpu, mem);
            }

            return;
        case 0xe0: // LD (0xff00 + n), A
            imm_u = mem[cpu->registers.dw.PC++];
            mem[0xff00 + imm_u] = cpu->registers.w.A;

            return;
        case 0xf0: // LD A, (0xff00 + n)
            imm_u = mem[cpu->registers.dw.PC++];
            cpu->registers.w.A = mem[0xff00 + imm_u];

            return;
        case 0xc2: // JP NZ, nn
            imm16_u = (mem[cpu->registers.dw.PC++] << 8) + mem[cpu->registers.dw.PC++];

            if (!cpu->registers.w.F.z) {
                cpu->registers.dw.PC = imm16_u;
            }

            return;
        case 0xd2: // JP NC, nn
            imm16_u = (mem[cpu->registers.dw.PC++] << 8) + mem[cpu->registers.dw.PC++];

            if (!cpu->registers.w.F.c) {
                cpu->registers.dw.PC = imm16_u;
            }

            return;
        case 0xe2: // LD (0xff00 + C), A
            mem[0xff00 + cpu->registers.w.C] = cpu->registers.w.A;

            return;
        case 0xf2: // LD A, (0xff00 + C)
            cpu->registers.w.A = mem[0xff00 + cpu->registers.w.C];

            return;
        case 0xf3: // DI
            cpu->state.IME = 0;

            return;
        case 0xc4: // CALL NZ, nn
            imm16_u = (mem[cpu->registers.dw.PC++] << 8) + mem[cpu->registers.dw.PC++];

            if (!cpu->registers.w.F.z) {
                call(cpu, mem, imm16_u);
            }

            return;
        case 0xc5: // CALL NC, nn
            imm16_u = (mem[cpu->registers.dw.PC++] << 8) + mem[cpu->registers.dw.PC++];

            if (!cpu->registers.w.F.c) {
                call(cpu, mem, imm16_u);
            }

            return;
        case 0x76: // HALT
            cpu->state.halted = 1;

            return;
        case 0x07: // RLCA
            cpu->registers.w.A = cpu->registers.w.A >> 1 | cpu->registers.w.A << 7;

            cpu->registers.w.F.z = 0;
            cpu->registers.w.F.n = 0;
            cpu->registers.w.F.h = 0;
            cpu->registers.w.F.c = cpu->registers.w.A >> 7 & 0x01;

            return;
        case 0x17: // RCA
            tmp = cpu->registers.w.F.c;
            cpu->registers.w.F.c = cpu->registers.w.A >> 7 & 0x01;
            cpu->registers.w.A = cpu->registers.w.A << 1 | tmp;

            cpu->registers.w.F.z = 0;
            cpu->registers.w.F.n = 0;
            cpu->registers.w.F.h = 0;

            return;
        case 0x27: // DAA
            daa_corr = cpu->registers.w.F.h || (!cpu->registers.w.F.n && (cpu->registers.w.A & 0xf) > 9)
                       ? 0x6
                       : 0;

            if (cpu->registers.w.F.c || (!cpu->registers.w.F.n && cpu->registers.w.A > 0x99)) {
                daa_corr += 0x60;
                cpu->registers.w.F.c = 1;
            } else {
                cpu->registers.w.F.c = 0;
            }

            cpu->registers.w.A += cpu->registers.w.F.n ? -daa_corr : daa_corr;
            cpu->registers.w.A &= 0xff;

            cpu->registers.w.F.z = cpu->registers.w.A == 0;

            return;
        case 0x37: // SCF
            cpu->registers.w.F.n = 0;
            cpu->registers.w.F.h = 0;
            cpu->registers.w.F.c = 1;

            return;
        case 0x08: // LD (nn), SP
            imm16_u = (mem[cpu->registers.dw.PC++] << 8) + mem[cpu->registers.dw.PC++];
            mem[imm16_u] = cpu->registers.dw.SP;

            return;
        case 0x18: // JR n
            imm_s = (int8_t) mem[cpu->registers.dw.PC]; // We jump somewhere else anyways
            cpu->registers.dw.PC += imm_s;

            return;
        case 0x28: // JR Z, n
            imm_s = (int8_t) mem[cpu->registers.dw.PC++];

            if (cpu->registers.w.F.z) {
                cpu->registers.dw.PC += imm_s;
            }

            return;
        case 0x38: // JR C, n
            imm_s = (int8_t) mem[cpu->registers.dw.PC++];

            if (cpu->registers.w.F.c) {
                cpu->registers.dw.PC += imm_s;
            }

            return;
        case 0xc8: // RET Z
            if (cpu->registers.w.F.z) {
                ret(cpu, mem);
            }

            return;
        case 0xd8: // RET C
            if (cpu->registers.w.F.c) {
                ret(cpu, mem);
            }

            return;
        case 0xe8: // ADD SP, n
            imm_s = (int8_t) mem[cpu->registers.dw.PC++];
            tmp = cpu->registers.dw.HL;
            cpu->registers.dw.SP += imm_s;

            cpu->registers.w.F.z = 0;
            cpu->registers.w.F.n = 0;
            cpu->registers.w.F.h = (tmp >= 0x10 && cpu->registers.dw.SP < 0x10)
                    || (tmp < 0x10 && cpu->registers.dw.SP >= 0x10);
            cpu->registers.w.F.c = imm_s < 0
                    ? (tmp < cpu->registers.dw.SP)
                    : (tmp > cpu->registers.dw.SP);

            return;
        case 0xf8: // LD HL, SP+n
            imm_s = (int8_t) mem[cpu->registers.dw.PC++];
            tmp = cpu->registers.dw.HL;
            cpu->registers.dw.HL = cpu->registers.dw.SP + imm_s;

            cpu->registers.w.F.z = 0;
            cpu->registers.w.F.n = 0;
            cpu->registers.w.F.h = (tmp >= 0x10 && cpu->registers.dw.HL < 0x10)
                                   || (tmp < 0x10 && cpu->registers.dw.HL >= 0x10);
            cpu->registers.w.F.c = imm_s < 0
                                   ? (tmp < cpu->registers.dw.HL)
                                   : (tmp > cpu->registers.dw.HL);

            return;
        case 0xc9: // RET
            ret(cpu, mem);

            return;
        case 0xd9: // RETI
            ret(cpu, mem);
            cpu->state.IME = 1;

            return;
        case 0xe9: // JP HL
            cpu->registers.dw.PC = cpu->registers.dw.HL;

            return;
        case 0xf9: // LD SP, HL
            cpu->registers.dw.SP = cpu->registers.dw.HL;

            return;
        case 0xca: // JP Z, nn
            imm16_u = (mem[cpu->registers.dw.PC++] << 8) + mem[cpu->registers.dw.PC++];

            if (cpu->registers.w.F.z) {
                cpu->registers.dw.PC = imm16_u;
            }

            return;
        case 0xda: // JP C, nn
            imm16_u = (mem[cpu->registers.dw.PC++] << 8) + mem[cpu->registers.dw.PC++];

            if (cpu->registers.w.F.c) {
                cpu->registers.dw.PC = imm16_u;
            }

            return;
        case 0xea: // LD (nn), A
            imm16_u = (mem[cpu->registers.dw.PC++] << 8) + mem[cpu->registers.dw.PC++];
            mem[imm16_u] = cpu->registers.w.A;

            return;
        case 0xfa: // LD A, (nn)
            imm16_u = (mem[cpu->registers.dw.PC++] << 8) + mem[cpu->registers.dw.PC++];
            cpu->registers.w.A = mem[imm16_u];

            return;
        case 0xfb: // EI
            cpu->state.IME = 1;

            return;
        case 0xcc: // CALL Z, nn
            imm16_u = (mem[cpu->registers.dw.PC++] << 8) + mem[cpu->registers.dw.PC++];

            if (cpu->registers.w.F.z) {
                call(cpu, mem, imm16_u);
            }

            return;
        case 0xdc: // CALL C, nn
            imm16_u = (mem[cpu->registers.dw.PC++] << 8) + mem[cpu->registers.dw.PC++];

            if (cpu->registers.w.F.c) {
                call(cpu, mem, imm16_u);
            }

            return;
        case 0xcd: // CALL nn
            imm16_u = (mem[cpu->registers.dw.PC++] << 8) + mem[cpu->registers.dw.PC++];
            call(cpu, mem, imm16_u);

            return;
        case 0x0f: // RRCA
            cpu->registers.w.A = cpu->registers.w.A << 1 | cpu->registers.w.A >> 7;

            cpu->registers.w.F.z = 0;
            cpu->registers.w.F.n = 0;
            cpu->registers.w.F.h = 0;
            cpu->registers.w.F.c = cpu->registers.w.A >> 7 & 0x01;

            return;
        case 0x1f: // RCA
            tmp = cpu->registers.w.F.c;
            cpu->registers.w.F.c = cpu->registers.w.A & 0x01;
            cpu->registers.w.A = cpu->registers.w.A >> 1 | tmp;

            cpu->registers.w.F.z = 0;
            cpu->registers.w.F.n = 0;
            cpu->registers.w.F.h = 0;

            return;
        case 0x2f: // CPL
            cpu->registers.w.A ^= 0xff;

            return;
        case 0x3f: // CCF
            cpu->registers.w.F.n = 0;
            cpu->registers.w.F.h = 0;
            cpu->registers.w.F.c ^= 1;

            return;
    }

    if (op.value != 0xcb) { // Invalid instruction
        cpu->state.stopped = 1;
    }

    uint8_t cb_op = mem[cpu->registers.dw.PC++];

    if (cb_op <= 0x3f) {
        uint8_t *src = decode_src_or_dst_middle(cpu, mem, cb_op & 0b111);

        switch (cb_op >> 3 & 0b111) {
            // TODO
        }
    }
}
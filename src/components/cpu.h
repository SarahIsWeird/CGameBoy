//
// Created by Sarah Klocke on 05.09.21.
//

#ifndef CGAMEBOY_CPU_H
#define CGAMEBOY_CPU_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t value;
    int length;
    int timing;
    const char *name;
} op_t;

typedef struct {
    union {
        struct {
            uint8_t A;
            union {
                uint8_t w;
                struct {
                    uint8_t z : 1;
                    uint8_t n : 1;
                    uint8_t h : 1;
                    uint8_t c : 1;
                    uint8_t unused : 4;
                };
            } F;
            uint8_t B;
            uint8_t C;
            uint8_t D;
            uint8_t E;
            uint8_t H;
            uint8_t L;

            uint8_t _sp1;
            uint8_t _sp2;
            uint8_t _pc1;
            uint8_t _pc2;
        } w;

        struct {
            uint16_t AF;
            uint16_t BC;
            uint16_t DE;
            uint16_t HL;

            uint16_t SP;
            uint16_t PC;
        } dw;
    } registers;

    struct {
        uint8_t IME : 1;
        uint8_t halted : 1;
        uint8_t stopped : 1;
    } state;
} cpu_t;

void cpu_tick(cpu_t *cpu, uint8_t *mem);

#endif //CGAMEBOY_CPU_H

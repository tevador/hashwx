/* Copyright (c) 2020-2026 tevador <tevador@gmail.com> */
/* See LICENSE for licensing information */

#ifndef INSTRUCTION_H
#define INSTRUCTION_H

#include <stdint.h>

typedef enum instr_type {
    INSTR_MULSUB,   /* subtract and multiply */
    INSTR_MULADD,   /* add and multiply */
    INSTR_MULXOR,   /* xor and multiply */
    INSTR_MULOR,    /* or and multiply */
    INSTR_RMCG,     /* rotated multiplicative congruential generator */
    INSTR_ARXSUB,   /* rotate and subtract */
    INSTR_ARXADD,   /* rotate and add */
    INSTR_ARXROR,   /* rotate and xor */
    INSTR_ARXASR,   /* arithmetic shift and xor */
    INSTR_ARXLSR,   /* logical shift and xor */
    INSTR_BRANCH,   /* conditional branch */
    INSTR_HALT,     /* halt */
} instr_type;

typedef struct instruction {
    instr_type opcode;
    uint32_t src;
    uint32_t dst;
    uint32_t imm;
} instruction;

#endif

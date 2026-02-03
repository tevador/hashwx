/* Copyright (c) 2020-2026 tevador <tevador@gmail.com> */
/* See LICENSE for licensing information */

#ifndef INSTRUCTION_H
#define INSTRUCTION_H

#include <stdint.h>

typedef enum instr_type {
    INSTR_MULOR,    /* or and multiply */
    INSTR_MULXOR,   /* xor and multiply */
    INSTR_MULADD,   /* add and multiply */
    INSTR_RMCG,     /* rotated multiplicative congruential generator */
    INSTR_XORROR,   /* rotate and xor */
    INSTR_ADDROR,   /* rotate and add */
    INSTR_SUBROR,   /* rotate and subtract */
    INSTR_XORASR,   /* arithmetic shift and xor */
    INSTR_ADDASR,   /* arithmetic shift and add */
    INSTR_SUBASR,   /* arithmetic shift and subtract */
    INSTR_XORLSR,   /* logical shift and xor */
    INSTR_ADDLSR,   /* logical shift and add */
    INSTR_SUBLSR,   /* logical shift and subtract */
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

/* Copyright (c) 2020-2026 tevador <tevador@gmail.com> */
/* See LICENSE for licensing information */

#include "program.h"
#include "platform.h"

#include <stdio.h>

static FORCE_INLINE uint64_t rotr64(uint64_t a, unsigned int b) {
    return (a >> b) | (a << (64 - b));
}

static uint32_t program_execute_reg(const hashwx_program* program, uint64_t r[], uint32_t branch_counter) {
    uint32_t branch_flag = 0;
    uint32_t ic = 0;
    uint64_t temp;
    for (;;) { /* loop is exited via the HALT instruction below */
        const instruction* instr = &program->code[ic];
        ic++;
        switch (instr->opcode)
        {
        case INSTR_MULOR:
            r[instr->dst] = (r[instr->dst] | instr->imm) * r[instr->src];
            break;
        case INSTR_MULXOR:
            r[instr->dst] = (r[instr->dst] ^ instr->imm) * r[instr->src];
            break;
        case INSTR_MULADD:
            r[instr->dst] = (r[instr->dst] + instr->imm) * r[instr->src];
            break;
        case INSTR_RMCG:
            temp = rotr64(r[instr->dst] * r[instr->src], instr->imm);
            r[instr->dst] = temp;
            branch_flag = (uint32_t)temp;
            break;
        case INSTR_XORROR:
            r[instr->dst] = rotr64(r[instr->dst], instr->imm) ^ r[instr->src];
            break;
        case INSTR_ADDROR:
            r[instr->dst] = rotr64(r[instr->dst], instr->imm) + r[instr->src];
            break;
        case INSTR_SUBROR:
            r[instr->dst] = rotr64(r[instr->dst], instr->imm) - r[instr->src];
            break;
        case INSTR_XORASR:
            r[instr->dst] = (((int64_t)r[instr->dst]) >> instr->imm) ^ r[instr->src];
            break;
        case INSTR_ADDASR:
            r[instr->dst] = (((int64_t)r[instr->dst]) >> instr->imm) + r[instr->src];
            break;
        case INSTR_SUBASR:
            r[instr->dst] = (((int64_t)r[instr->dst]) >> instr->imm) - r[instr->src];
            break;
        case INSTR_XORLSR:
            r[instr->dst] = (r[instr->dst] >> instr->imm) ^ r[instr->src];
            break;
        case INSTR_ADDLSR:
            r[instr->dst] = (r[instr->dst] >> instr->imm) + r[instr->src];
            break;
        case INSTR_SUBLSR:
            r[instr->dst] = (r[instr->dst] >> instr->imm) - r[instr->src];
            break;
        case INSTR_BRANCH:
            if (branch_counter != 0 && (branch_flag & 32) == 0) {
                branch_counter--;
                ic = 0;
            }
            break;
        case INSTR_HALT:
            return branch_counter;
        default:
            UNREACHABLE;
        }
    }
    UNREACHABLE;
}

static uint32_t program_execute_mem(const hashwx_program* program, uint64_t r[], uint32_t branch_counter, uint64_t mem[]) {
    uint32_t branch_flag = 0;
    uint32_t ic = 0;
    uint64_t temp;
    for (;;) { /* loop is exited via the HALT instruction below */
        const instruction* instr = &program->code[ic];
        ic++;
        switch (instr->opcode)
        {
        case INSTR_MULOR:
            r[instr->dst] = (r[instr->dst] | instr->imm) * mem[(r[instr->src] / 8) % 256];
            break;
        case INSTR_MULXOR:
            r[instr->dst] = (r[instr->dst] ^ instr->imm) * mem[(r[instr->src] / 8) % 256];
            break;
        case INSTR_MULADD:
            r[instr->dst] = (r[instr->dst] + instr->imm) * mem[(r[instr->src] / 8) % 256];
            break;
        case INSTR_RMCG:
            temp = rotr64(r[instr->dst] * r[instr->src], instr->imm);
            r[instr->dst] = temp;
            branch_flag = (uint32_t)temp;
            break;
        case INSTR_XORROR:
            r[instr->dst] = rotr64(r[instr->dst], instr->imm) ^ mem[(r[instr->src] / 8) % 256];
            break;
        case INSTR_ADDROR:
            r[instr->dst] = rotr64(r[instr->dst], instr->imm) + mem[(r[instr->src] / 8) % 256];
            break;
        case INSTR_SUBROR:
            r[instr->dst] = rotr64(r[instr->dst], instr->imm) - mem[(r[instr->src] / 8) % 256];
            break;
        case INSTR_XORASR:
            r[instr->dst] = (((int64_t)r[instr->dst]) >> instr->imm) ^ mem[(r[instr->src] / 8) % 256];
            break;
        case INSTR_ADDASR:
            r[instr->dst] = (((int64_t)r[instr->dst]) >> instr->imm) + mem[(r[instr->src] / 8) % 256];
            break;
        case INSTR_SUBASR:
            r[instr->dst] = (((int64_t)r[instr->dst]) >> instr->imm) - mem[(r[instr->src] / 8) % 256];
            break;
        case INSTR_XORLSR:
            r[instr->dst] = (r[instr->dst] >> instr->imm) ^ mem[(r[instr->src] / 8) % 256];
            break;
        case INSTR_ADDLSR:
            r[instr->dst] = (r[instr->dst] >> instr->imm) + mem[(r[instr->src] / 8) % 256];
            break;
        case INSTR_SUBLSR:
            r[instr->dst] = (r[instr->dst] >> instr->imm) - mem[(r[instr->src] / 8) % 256];
            break;
        case INSTR_BRANCH:
            if (branch_counter != 0 && (branch_flag & 32) == 0) {
                branch_counter--;
                ic = 0;
            }
            break;
        case INSTR_HALT:
            return branch_counter;
        default:
            UNREACHABLE;
        }
    }
    UNREACHABLE;
}

void hashwx_program_list_execute(const hashwx_program_list* program_list, uint64_t r[]) {
    uint32_t branch_counter = 32;
    uint64_t mem[HASHWX_MEM_SIZE];

    for (uint32_t i = 0; i < HASHWX_NUM_PROGRAMS; ++i) {
        branch_counter = program_execute_reg(&program_list->prog[i], r, branch_counter);
        for (int j = 0; j < 8; ++j) {
            mem[HASHWX_MEM_SIZE - 1 - 8 * i - j] = r[j];
        }
    }

    branch_counter = 32;

    for (uint32_t i = 0; i < HASHWX_NUM_PROGRAMS; ++i) {
        branch_counter = program_execute_mem(&program_list->prog[i], r, branch_counter, mem);
    }
}

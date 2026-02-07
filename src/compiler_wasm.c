/* Copyright (c) 2020-2026 tevador <tevador@gmail.com> */
/* See LICENSE for licensing information */

#include "compiler.h"

#ifdef HASHWX_COMPILER_WASM

#include <string.h>
#include <assert.h>

#include "program.h"
#include "platform.h"

#define EMIT(p,x) do {            \
        memcpy(p, &x, sizeof(x)); \
        p += sizeof(x);           \
    } while (0)
#define EMIT_BYTE(p,x) *((p)++) = x

#define WASM_REG_PROGRAM_SIZE 173
#define WASM_MEM_PROGRAM_SIZE 179

#define WASM_BINARY_MAGIC 0x00, 0x61, 0x73, 0x6d
#define WASM_BINARY_VERSION 0x01, 0x00, 0x00, 0x00
#define ALIGN 3

#define PAR_RP 0x00 /* function parameter $rp (reg ptr) */
#define PAR_MP 0x01 /* function parameter $mp (mem ptr) */
#define LOC_R0 0x02 /* VM register R0 */
#define LOC_R1 0x03 /* VM register R1 */
#define LOC_R2 0x04 /* VM register R2 */
#define LOC_R3 0x05 /* VM register R3 */
#define LOC_R4 0x06 /* VM register R4 */
#define LOC_R5 0x07 /* VM register R5 */
#define LOC_R6 0x08 /* VM register R6 */
#define LOC_R7 0x09 /* VM register R7 */
#define LOC_R8 0x0a /* VM register R8 */
#define LOC_R9 0x0b /* VM register R9 */
#define LOC_BC 0x0c /* VM register BC */
#define LOC_BF 0x0d /* VM register BF */
#define LOC_MM 0x0e /* memory mask constant (2040) */

#define OP_INVALID 0x00
#define OP_NOP 0x01
#define OP_LOOP 0x03
#define OP_IF 0x04
#define OP_END 0x0b
#define OP_BR 0x0c
#define OP_GET 0x20
#define OP_SET 0x21
#define OP_TEE 0x22
#define OP_LOAD 0x29
#define OP_STORE 0x37
#define OP_CONST_32 0x41
#define OP_CONST_64 0x42
#define OP_EQZ 0x50
#define OP_ADD_32 0x6a
#define OP_SUB_32 0x6b
#define OP_ADD_64 0x7c
#define OP_SUB_64 0x7d
#define OP_MUL 0x7e
#define OP_AND 0x83
#define OP_OR 0x84
#define OP_XOR 0x85
#define OP_SHRS 0x87
#define OP_SHRU 0x88
#define OP_ROR 0x8a
#define OP_WRAP_32 0xa7

#define TYPE_VOID 0x40
#define TYPE_FUNC 0x60
#define TYPE_I64 0x7e
#define TYPE_I32 0x7f

static const uint8_t code_prologue[] = {
    WASM_BINARY_MAGIC,
    WASM_BINARY_VERSION,
    /* Section Type */
    0x01, 0x06, 0x01,
    TYPE_FUNC, 2, TYPE_I32, TYPE_I32, 0,
    /* Section Import */
    0x02, 0x0f, 0x01, 0x03, 'e', 'n', 'v', 0x06, 'm', 'e', 'm', 'o', 'r', 'y', 0x02, 0x00, 0x01,
    /* Section Function */
    0x03, 0x02, 0x01, 0x00,
    /* Section Export */
    0x07, 0x08, 0x01,
    0x04, 'e', 'x', 'e', 'c', 0x00, 0x00,
    /* Section Code */
    0x0a, 0xdb, 0xd7, 0x00 /*11227*/, 1,
    0xd7, 0xd7, 0x00 /*11223*/, 1, 13, TYPE_I64,
    OP_GET, PAR_RP, OP_LOAD, ALIGN,  0, OP_SET, LOC_R0,
    OP_GET, PAR_RP, OP_LOAD, ALIGN,  8, OP_SET, LOC_R1,
    OP_GET, PAR_RP, OP_LOAD, ALIGN, 16, OP_SET, LOC_R2,
    OP_GET, PAR_RP, OP_LOAD, ALIGN, 24, OP_SET, LOC_R3,
    OP_GET, PAR_RP, OP_LOAD, ALIGN, 32, OP_SET, LOC_R4,
    OP_GET, PAR_RP, OP_LOAD, ALIGN, 40, OP_SET, LOC_R5,
    OP_GET, PAR_RP, OP_LOAD, ALIGN, 48, OP_SET, LOC_R6,
    OP_GET, PAR_RP, OP_LOAD, ALIGN, 56, OP_SET, LOC_R7,
    OP_GET, PAR_RP, OP_LOAD, ALIGN, 64, OP_SET, LOC_R8,
    OP_GET, PAR_RP, OP_LOAD, ALIGN, 72, OP_SET, LOC_R9,
    OP_CONST_64, 0, OP_SET, LOC_BC,
    OP_CONST_64, 0xf8, 0x0f /*2040*/, OP_SET, LOC_MM,
    OP_CONST_32, 0x80, 0x10 /*2048*/, OP_GET, PAR_MP, OP_ADD_32, OP_SET, PAR_MP
};

static const uint8_t code_epilogue[] = {
    OP_GET, PAR_RP,
    OP_GET, LOC_R0,
    OP_STORE, ALIGN, 0,
    OP_GET, PAR_RP,
    OP_GET, LOC_R1,
    OP_STORE, ALIGN, 8,
    OP_GET, PAR_RP,
    OP_GET, LOC_R2,
    OP_STORE, ALIGN, 16,
    OP_GET, PAR_RP,
    OP_GET, LOC_R3,
    OP_STORE, ALIGN, 24,
    OP_GET, PAR_RP,
    OP_GET, LOC_R4,
    OP_STORE, ALIGN, 32,
    OP_GET, PAR_RP,
    OP_GET, LOC_R5,
    OP_STORE, ALIGN, 40,
    OP_GET, PAR_RP,
    OP_GET, LOC_R6,
    OP_STORE, ALIGN, 48,
    OP_GET, PAR_RP,
    OP_GET, LOC_R7,
    OP_STORE, ALIGN, 56,
    OP_END
};

static const uint8_t code_clear_bc[] = {
    OP_CONST_64, /* i64.const */
    0x00,
    OP_SET, /* local.set */
    LOC_BC
};

static const uint8_t code_reg_prologue[] = {
    OP_GET, PAR_MP, /* local.get $mp */
    OP_CONST_32, 0xc0, 0x00, /* i32.const 64 */
    OP_SUB_32, /* i32.sub */
    OP_SET, PAR_MP, /* local.set $mp */
    OP_LOOP, /* loop */
    TYPE_VOID
};

static const uint8_t code_reg_epilogue[] = {
    OP_END, /* end */
    OP_GET, PAR_MP, /* local.get $mp */
    OP_GET, LOC_R0, /* local.get $r0 */
    OP_STORE, ALIGN, 56, /* i64.store align, 56 */
    OP_GET, PAR_MP, /* local.get $mp */
    OP_GET, LOC_R1, /* local.get $r1 */
    OP_STORE, ALIGN, 48, /* i64.store align, 48 */
    OP_GET, PAR_MP, /* local.get $mp */
    OP_GET, LOC_R2, /* local.get $r2 */
    OP_STORE, ALIGN, 40, /* i64.store align, 40 */
    OP_GET, PAR_MP, /* local.get $mp */
    OP_GET, LOC_R3, /* local.get $r3 */
    OP_STORE, ALIGN, 32, /* i64.store align, 32 */
    OP_GET, PAR_MP, /* local.get $mp */
    OP_GET, LOC_R4, /* local.get $r4 */
    OP_STORE, ALIGN, 24, /* i64.store align, 24 */
    OP_GET, PAR_MP, /* local.get $mp */
    OP_GET, LOC_R5, /* local.get $r5 */
    OP_STORE, ALIGN, 16, /* i64.store align, 16 */
    OP_GET, PAR_MP, /* local.get $mp */
    OP_GET, LOC_R6, /* local.get $r6 */
    OP_STORE, ALIGN, 8, /* i64.store align, 8 */
    OP_GET, PAR_MP, /* local.get $mp */
    OP_GET, LOC_R7, /* local.get $r7 */
    OP_STORE, ALIGN, 0, /* i64.store align, 0 */
};

static uint8_t code_branch[] = {
    OP_GET,         /* local.get $bc */
    LOC_BC,
    OP_GET,         /* local.get $bf */
    LOC_BF,
    OP_OR,          /* i64.or */
    OP_CONST_64,    /* i64.const 32 */
    32,
    OP_AND,         /* i64.and */
    OP_EQZ,         /* i64.eqz */
    OP_IF,          /* if */
    TYPE_VOID,
    OP_CONST_64,    /* i64.const 1 */
    1,
    OP_GET,         /* local.get $bc */
    LOC_BC,
    OP_ADD_64,      /* i64.add */
    OP_SET,         /* local.set bc */
    LOC_BC,
    OP_BR,          /* br */
    0x01,
    OP_END,         /* end */
};

static const uint8_t lookup_pre[13] = {
    OP_OR,
    OP_XOR,
    OP_ADD_64,
    OP_INVALID,
    OP_ROR,
    OP_ROR,
    OP_ROR,
    OP_SHRS,
    OP_SHRS,
    OP_SHRS,
    OP_SHRU,
    OP_SHRU,
    OP_SHRU
};

static const uint8_t lookup_post[13] = {
    OP_MUL,
    OP_MUL,
    OP_MUL,
    OP_INVALID,
    OP_XOR,
    OP_ADD_64,
    OP_SUB_64,
    OP_XOR,
    OP_ADD_64,
    OP_SUB_64,
    OP_XOR,
    OP_ADD_64,
    OP_SUB_64
};

static uint8_t* compile_program_reg(const hashwx_program* program, uint8_t* code) {
    uint8_t* pos = code;
    EMIT(pos, code_reg_prologue);
    for (int i = 0; i < HASHWX_PROGRAM_SIZE; ++i) {
        const instruction* instr = &program->code[i];
        switch (instr->opcode)
        {
        case INSTR_MULOR:
        case INSTR_MULXOR:
        case INSTR_MULADD:
        {
            //10 bytes
            EMIT_BYTE(pos, OP_GET); /* local.get $rd */
            EMIT_BYTE(pos, LOC_R0 + instr->dst);
            EMIT_BYTE(pos, OP_CONST_64); /* i64.const imm */
            EMIT_BYTE(pos, instr->imm);
            EMIT_BYTE(pos, lookup_pre[instr->opcode]); /* i64.op */
            EMIT_BYTE(pos, OP_GET); /* local.get $rs */
            EMIT_BYTE(pos, LOC_R0 + instr->src);
            EMIT_BYTE(pos, OP_MUL);
            EMIT_BYTE(pos, OP_SET); /* local.set $rd */
            EMIT_BYTE(pos, LOC_R0 + instr->dst);
            break;
        }
        case INSTR_RMCG:
        {
            //12 bytes
            EMIT_BYTE(pos, OP_GET); /* local.get $rd */
            EMIT_BYTE(pos, LOC_R0 + instr->dst);
            EMIT_BYTE(pos, OP_GET); /* local.get $rs */
            EMIT_BYTE(pos, LOC_R0 + instr->src);
            EMIT_BYTE(pos, OP_MUL); /* i64.mul */
            EMIT_BYTE(pos, OP_CONST_64); /* i64.const imm */
            EMIT_BYTE(pos, instr->imm);
            EMIT_BYTE(pos, OP_ROR); /* i64.rotr */
            EMIT_BYTE(pos, OP_TEE); /* local.tee $rd */
            EMIT_BYTE(pos, LOC_R0 + instr->dst);
            EMIT_BYTE(pos, OP_SET); /* local.set $bf */
            EMIT_BYTE(pos, LOC_BF);
            break;
        }
        case INSTR_XORROR:
        case INSTR_ADDROR:
        case INSTR_SUBROR:
        case INSTR_XORASR:
        case INSTR_ADDASR:
        case INSTR_SUBASR:
        case INSTR_XORLSR:
        case INSTR_ADDLSR:
        case INSTR_SUBLSR:
        {
            // 10 bytes
            EMIT_BYTE(pos, OP_GET); /* local.get $rd */
            EMIT_BYTE(pos, LOC_R0 + instr->dst);
            EMIT_BYTE(pos, OP_CONST_64); /* i64.const imm */
            EMIT_BYTE(pos, instr->imm);
            EMIT_BYTE(pos, lookup_pre[instr->opcode]); /* i64.rotr/shr_s/shr_u */
            EMIT_BYTE(pos, OP_GET); /* local.get $rs */
            EMIT_BYTE(pos, LOC_R0 + instr->src);
            EMIT_BYTE(pos, lookup_post[instr->opcode]); /* i64.sub/add/xor */
            EMIT_BYTE(pos, OP_SET); /* local.set $rd */
            EMIT_BYTE(pos, LOC_R0 + instr->dst);
            break;
        }
        case INSTR_BRANCH:
        {
            EMIT(pos, code_branch);
            break;
        }
        case INSTR_HALT:
            break;
        default:
            UNREACHABLE;
        }
    }
    EMIT(pos, code_reg_epilogue);
    assert(pos - code == WASM_REG_PROGRAM_SIZE);
    return pos;
}

static uint8_t* emit_mem_src(uint8_t* pos, uint32_t src) {
    //12 bytes
    EMIT_BYTE(pos, OP_GET); /* local.get $rs */
    EMIT_BYTE(pos, LOC_R0 + src);
    EMIT_BYTE(pos, OP_GET); /* local.get $mm */
    EMIT_BYTE(pos, LOC_MM);
    EMIT_BYTE(pos, OP_AND); /* i64.and */
    EMIT_BYTE(pos, OP_WRAP_32); /* i32.wrap_64 */
    EMIT_BYTE(pos, OP_GET); /* local.get $mp */
    EMIT_BYTE(pos, PAR_MP);
    EMIT_BYTE(pos, OP_ADD_32); /* i32.add */
    EMIT_BYTE(pos, OP_LOAD); /* i64.load align, 0 */
    EMIT_BYTE(pos, ALIGN);
    EMIT_BYTE(pos, 0);
    return pos;
}

static uint8_t* compile_program_mem(const hashwx_program* program, uint8_t* code) {
    uint8_t* pos = code;
    EMIT_BYTE(pos, OP_LOOP);
    EMIT_BYTE(pos, TYPE_VOID);
    for (int i = 0; i < HASHWX_PROGRAM_SIZE; ++i) {
        const instruction* instr = &program->code[i];
        switch (instr->opcode)
        {
        case INSTR_MULOR:
        case INSTR_MULXOR:
        case INSTR_MULADD:
        {
            //20 bytes
            pos = emit_mem_src(pos, instr->src);
            EMIT_BYTE(pos, OP_GET); /* local.get $rd */
            EMIT_BYTE(pos, LOC_R0 + instr->dst);
            EMIT_BYTE(pos, OP_CONST_64); /* i64.const imm */
            EMIT_BYTE(pos, instr->imm);
            EMIT_BYTE(pos, lookup_pre[instr->opcode]); /* i64.op */
            EMIT_BYTE(pos, OP_MUL);
            EMIT_BYTE(pos, OP_SET); /* local.set $rd */
            EMIT_BYTE(pos, LOC_R0 + instr->dst);
            break;
        }
        case INSTR_RMCG:
        {
            //12 bytes
            EMIT_BYTE(pos, OP_GET); /* local.get $rd */
            EMIT_BYTE(pos, LOC_R0 + instr->dst);
            EMIT_BYTE(pos, OP_GET); /* local.get $rs */
            EMIT_BYTE(pos, LOC_R0 + instr->src);
            EMIT_BYTE(pos, OP_MUL); /* i64.mul */
            EMIT_BYTE(pos, OP_CONST_64); /* i64.const imm */
            EMIT_BYTE(pos, instr->imm);
            EMIT_BYTE(pos, OP_ROR); /* i64.rotr */
            EMIT_BYTE(pos, OP_TEE); /* local.tee $rd */
            EMIT_BYTE(pos, LOC_R0 + instr->dst);
            EMIT_BYTE(pos, OP_SET); /* local.set $bf */
            EMIT_BYTE(pos, LOC_BF);
            break;
        }
        case INSTR_XORROR:
        case INSTR_ADDROR:
        case INSTR_SUBROR:
        case INSTR_XORASR:
        case INSTR_ADDASR:
        case INSTR_SUBASR:
        case INSTR_XORLSR:
        case INSTR_ADDLSR:
        case INSTR_SUBLSR:
        {
            // 20 bytes
            EMIT_BYTE(pos, OP_GET); /* local.get $rd */
            EMIT_BYTE(pos, LOC_R0 + instr->dst);
            EMIT_BYTE(pos, OP_CONST_64); /* i64.const imm */
            EMIT_BYTE(pos, instr->imm);
            EMIT_BYTE(pos, lookup_pre[instr->opcode]); /* i64.rotr/shr_s/shr_u */
            pos = emit_mem_src(pos, instr->src);
            EMIT_BYTE(pos, lookup_post[instr->opcode]); /* i64.sub/add/xor */
            EMIT_BYTE(pos, OP_SET); /* local.set $rd */
            EMIT_BYTE(pos, LOC_R0 + instr->dst);
            break;
        }
        case INSTR_BRANCH:
        {
            EMIT(pos, code_branch);
            break;
        }
        case INSTR_HALT:
            break;
        default:
            UNREACHABLE;
        }
    }
    EMIT_BYTE(pos, OP_END);
    assert(pos - code == WASM_MEM_PROGRAM_SIZE);
    return pos;
}

void hashwx_compile_wasm(uint8_t* code, const hashwx_program_list* program_list) {
    uint8_t* pos = code;
    EMIT(pos, code_prologue);

    for (uint32_t i = 0; i < HASHWX_NUM_PROGRAMS; ++i) {
        pos = compile_program_reg(&program_list->prog[i], pos);
    }

    EMIT(pos, code_clear_bc);

    for (uint32_t i = 0; i < HASHWX_NUM_PROGRAMS; ++i) {
        pos = compile_program_mem(&program_list->prog[i], pos);
    }

    EMIT(pos, code_epilogue);
    assert(pos - code == HASHWX_CODE_SIZE);
}

#endif

/* Copyright (c) 2020-2026 tevador <tevador@gmail.com> */
/* See LICENSE for licensing information */

#include "compiler.h"

#ifdef HASHWX_COMPILER_A64

#include <string.h>
#include <assert.h>

#include "program.h"
#include "platform.h"
#include "virtual_memory.h"

#define EMIT(p,x) do {           \
        memcpy(p, &x, sizeof(x)); \
        p += sizeof(x);          \
    } while (0)
#define EMIT_ISN(p,x) do {       \
        uint32_t a = x;          \
        EMIT(p, a);              \
    } while (0)

/*
    aarch64 achitectural register allocation:
        x0-x7   = R0-R7
        x8      = in/out ptr
        x9      = 32-BC
        x10     = 5
        x11     = 17
        x12     = 65
        x13     = R8
        x14-x17 = temporary
*/

static const uint8_t code_prologue[] = {
    0x0d, 0x20, 0x40, 0xf9, /* ldr x13, [x0, #64] */
    0xe8, 0x03, 0x00, 0xaa, /* mov x8, x0 */
    0x06, 0x1c, 0x43, 0xa9, /* ldp x6, x7, [x0, #48] */
    0x04, 0x14, 0x42, 0xa9, /* ldp x4, x5, [x0, #32] */
    0x02, 0x0c, 0x41, 0xa9, /* ldp x2, x3, [x0, #16] */
    0x00, 0x04, 0x40, 0xa9, /* ldp x0, x1, [x0, #0] */
    0x09, 0x00, 0x80, 0xd2, /* mov x9, 0 */
    0xaa, 0x00, 0x80, 0xd2, /* mov x10, 5 */
    0x2b, 0x02, 0x80, 0xd2, /* mov x11, 17 */
    0x2c, 0x08, 0x80, 0xd2, /* mov x12, 65 */
};

static const uint8_t code_epilogue[] = {
    0x06, 0x1d, 0x03, 0xa9, /* stp x6, x7, [x8, #48] */
    0xff, 0x03, 0x20, 0x91, /* add sp, sp, 2048 */
    0x04, 0x15, 0x02, 0xa9, /* stp x4, x5, [x8, #32] */
    0x02, 0x0d, 0x01, 0xa9, /* stp x2, x3, [x8, #16] */
    0x00, 0x05, 0x00, 0xa9, /* stp x0, x1, [x8, #0] */
    0xc0, 0x03, 0x5f, 0xd6, /* ret */
};

static const uint8_t code_branch[] = {
    0xdf, 0x01, 0x7b, 0xf2, /* tst x14, 32 */
    0x29, 0x15, 0x89, 0x9a, /* cinc x9, x9, eq */
};

static const uint8_t code_clear_bc[] = {
    0x09, 0x00, 0x80, 0xd2, /* mov x9, 0 */
};

static uint8_t* emit_mul(uint8_t* pos, uint32_t dst, uint32_t src) {
    EMIT_ISN(pos, 0x9b007c00 | (src << 16) | (dst << 5) | (dst));
    return pos;
}

static uint8_t* emit_ror(uint8_t* pos, uint32_t dst, uint32_t count) {
    EMIT_ISN(pos, 0x93c00000 | (dst << 16) | (dst << 5) | (dst) | (count << 10));
    return pos;
}

static uint8_t* emit_lsr(uint8_t* pos, uint32_t dst, uint32_t count) {
    EMIT_ISN(pos, 0xd340fc00 | (count << 16) | (dst << 5) | (dst));
    return pos;
}

static uint8_t* emit_asr(uint8_t* pos, uint32_t dst, uint32_t count) {
    EMIT_ISN(pos, 0x9340fc00 | (count << 16) | (dst << 5) | (dst));
    return pos;
}

static uint8_t* emit_mov(uint8_t* pos, uint32_t dst, uint32_t src) {
    EMIT_ISN(pos, 0xaa0003e0 | (src << 16) | (dst));
    return pos;
}

static uint8_t* emit_sub(uint8_t* pos, uint32_t dst, uint32_t src) {
    EMIT_ISN(pos, 0xcb000000 | (src << 16) | (dst << 5) | (dst));
    return pos;
}

static uint8_t* emit_add(uint8_t* pos, uint32_t dst, uint32_t src) {
    EMIT_ISN(pos, 0x8b000000 | (src << 16) | (dst << 5) | (dst));
    return pos;
}

static uint8_t* emit_eor(uint8_t* pos, uint32_t dst, uint32_t src) {
    EMIT_ISN(pos, 0xca000000 | (src << 16) | (dst << 5) | (dst));
    return pos;
}

static uint8_t* emit_orr(uint8_t* pos, uint32_t dst, uint32_t src1, uint32_t src2) {
    EMIT_ISN(pos, 0xaa000000 | (src2 << 16) | (src1 << 5) | (dst));
    return pos;
}

 /* converts [1, 5, 17, 65] (divided by 4) to [0, 1, 2, 3] */
static const uint8_t mul_imm_inv[17] = {
    0, 1, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3
};

static const uint32_t premul_tpls[4][4] = {
    { 0xd1000400, 0xd1001400, 0xd1004400, 0xd1010400 }, /* sub */
    { 0x91000400, 0x91001400, 0x91004400, 0x91010400 }, /* add */
    { 0xd2400000, 0xca0a0000, 0xca0b0000, 0xca0c0000 }, /* eor */
    { 0xb2400000, 0xaa0a0000, 0xaa0b0000, 0xaa0c0000 }, /* orr */
};

static const uint32_t store_pair[7] = {
    0xa9020be3, /* stp x3, x2, [sp, #32] */
    0xa90113e5, /* stp x5, x4, [sp, #16] */
    0xa9001be7, /* stp x7, x6, [sp, #0] */
    0xa90303e1, /* stp x1, x0, [sp, #48] */
    0xa9020be3, /* stp x3, x2, [sp, #32] */
    0xa90113e5, /* stp x5, x4, [sp, #16] */
    0xa9001be7, /* stp x7, x6, [sp, #0] */
};

static uint8_t* emit_premul(uint8_t* pos, const instruction* isn) {
    uint32_t imm = mul_imm_inv[isn->imm / 4];
    uint32_t tpl = premul_tpls[isn->opcode][imm];
    uint32_t dst = isn->dst;
    EMIT_ISN(pos, tpl | (dst << 5) | (dst));
    return pos;
}

static uint8_t* emit_prearx(uint8_t* pos, const instruction* isn) {
    switch (isn->opcode) {
    case INSTR_ARXSUB:
    case INSTR_ARXADD:
    case INSTR_ARXROR:
        return emit_ror(pos, isn->dst, isn->imm);
    case INSTR_ARXASR:
        return emit_asr(pos, isn->dst, isn->imm);
    case INSTR_ARXLSR:
        return emit_lsr(pos, isn->dst, isn->imm);
    default:
        UNREACHABLE;
    }
}

static uint8_t* emit_arx(uint8_t* pos, const instruction* isn, uint32_t src) {
    switch (isn->opcode) {
    case INSTR_ARXSUB:
        return emit_sub(pos, isn->dst, src);
    case INSTR_ARXADD:
        return emit_add(pos, isn->dst, src);
    case INSTR_ARXROR:
    case INSTR_ARXASR:
    case INSTR_ARXLSR:
        return emit_eor(pos, isn->dst, src);
    default:
        UNREACHABLE;
    }
}

static uint8_t* emit_beq(uint8_t* pos, uint8_t* target) {
    uint32_t offset = (uint32_t)(target - pos);
    offset &= 0x1ffffc;
    EMIT_ISN(pos, 0x54000000 | (offset << 3));
    return pos;
}

static uint8_t* emit_ldr_sp(uint8_t* pos, uint32_t dst) {
    /* ldr dst, [sp, dst] */
    EMIT_ISN(pos, 0xf8606be0 | (dst << 16) | (dst));
    return pos;
}

static uint8_t* emit_and_2040(uint8_t* pos, uint32_t dst, uint32_t src) {
    /* and dst, src, 2040 */
    EMIT_ISN(pos, 0x927d1c00 | (src << 5) | (dst));
    return pos;
}

static uint8_t* compile_program_reg(const hashwx_program* program, uint8_t* pos) {
    /* sub sp, sp, 64 */
    EMIT_ISN(pos, 0xd10103ff);
    uint8_t* target = pos;
    /* mul dst0, dst0, x13 */
    pos = emit_mul(pos, program->code[0].dst, 13);
    /* ror/lsr/asr dst1, dst1, imm1 */
    pos = emit_prearx(pos, &program->code[1]);
    /* mov x14, src1 */
    pos = emit_mov(pos, 14, program->code[1].src);
    /* sub/add/eor/orr dst2, dst2, imm2 */
    pos = emit_premul(pos, &program->code[2]);
    /* sub/add/eor dst1, dst1, x14 */
    pos = emit_arx(pos, &program->code[1], 14);
    /* mul dst2, dst2, src2 */
    pos = emit_mul(pos, program->code[2].dst, program->code[2].src);
    /* ror/lsr/asr dst3, dst3, imm3 */
    pos = emit_prearx(pos, &program->code[3]);
    /* ror dst0, dst0, imm0 */
    pos = emit_ror(pos, program->code[0].dst, program->code[0].imm);
    /* sub/add/eor dst3, dst3, src3 */
    pos = emit_arx(pos, &program->code[3], program->code[3].src);
    /* sub/add/eor/orr dst4, dst4, imm4 */
    pos = emit_premul(pos, &program->code[4]);
    /* mul dst4, dst4, src4 */
    pos = emit_mul(pos, program->code[4].dst, program->code[4].src);
    /* ror/lsr/asr dst5, dst5, imm5 */
    pos = emit_prearx(pos, &program->code[5]);
    /* orr x14, dst0, x9 */
    pos = emit_orr(pos, 14, program->code[0].dst, 9);
    /* sub/add/eor dst5, dst5, src5 */
    pos = emit_arx(pos, &program->code[5], program->code[5].src);
    /* sub/add/eor/orr dst6, dst6, imm6 */
    pos = emit_premul(pos, &program->code[6]);
    /*
        tst x14, 32
        cinc x9, x9, eq
    */
    EMIT(pos, code_branch);
    /* mul dst6, dst6, src6 */
    pos = emit_mul(pos, program->code[6].dst, program->code[6].src);
    /* b.eq */
    pos = emit_beq(pos, target);
    uint32_t pair_idx = program->code[8].dst / 2;
    /* stp reg0, reg1, [sp, #pos0] */
    EMIT_ISN(pos, store_pair[pair_idx]);
    /* ror/lsr/asr dst8, dst8, imm8 */
    pos = emit_prearx(pos, &program->code[8]);
    /* stp reg2, reg3, [sp, #pos1] */
    EMIT_ISN(pos, store_pair[pair_idx + 1]);
    /* sub/add/eor dst8, dst8, src8 */
    pos = emit_arx(pos, &program->code[8], program->code[8].src);
    /* stp reg4, reg5, [sp, #pos2] */
    EMIT_ISN(pos, store_pair[pair_idx + 2]);
    /* stp reg6, reg7, [sp, #pos3] */
    EMIT_ISN(pos, store_pair[pair_idx + 3]);
    return pos;
}

static uint8_t* compile_program_mem(const hashwx_program* program, uint8_t* pos) {
    uint8_t* target = pos;
    /* and x15, src1, 2040 */
    pos = emit_and_2040(pos, 15, program->code[1].src);
    /* ldr x15, [sp, x15] */
    pos = emit_ldr_sp(pos, 15);
    /* ror/lsr/asr dst1, dst1, imm1 */
    pos = emit_prearx(pos, &program->code[1]);
    /* mul dst0, dst0, x13 */
    pos = emit_mul(pos, program->code[0].dst, 13);
    /* sub/add/eor dst1, dst1, x15 */
    pos = emit_arx(pos, &program->code[1], 15);
    /* and x16, src2, 2040 */
    pos = emit_and_2040(pos, 16, program->code[2].src);
    /* ldr x16, [sp, x16] */
    pos = emit_ldr_sp(pos, 16);
    /* and x17, src3, 2040 */
    pos = emit_and_2040(pos, 17, program->code[3].src);
    /* ldr x17, [sp, x17] */
    pos = emit_ldr_sp(pos, 17);
    /* sub/add/eor/orr dst2, dst2, imm2 */
    pos = emit_premul(pos, &program->code[2]);
    /* ror/lsr/asr dst3, dst3, imm3 */
    pos = emit_prearx(pos, &program->code[3]);
    /* ror dst0, dst0, imm0 */
    pos = emit_ror(pos, program->code[0].dst, program->code[0].imm);
    /* mul dst2, dst2, x16 */
    pos = emit_mul(pos, program->code[2].dst, 16);
    /* sub/add/eor dst3, dst3, x17 */
    pos = emit_arx(pos, &program->code[3], 17);
    /* and x16, src4, 2040 */
    pos = emit_and_2040(pos, 16, program->code[4].src);
    /* ldr x16, [sp, x16] */
    pos = emit_ldr_sp(pos, 16);
    /* and x17, src5, 2040 */
    pos = emit_and_2040(pos, 17, program->code[5].src);
    /* ldr x17, [sp, x17] */
    pos = emit_ldr_sp(pos, 17);
    /* sub/add/eor/orr dst4, dst4, imm4 */
    pos = emit_premul(pos, &program->code[4]);
    /* ror/lsr/asr dst5, dst5, imm5 */
    pos = emit_prearx(pos, &program->code[5]);
    /* orr x14, dst0, x9 */
    pos = emit_orr(pos, 14, program->code[0].dst, 9);
    /* mul dst4, dst4, x16 */
    pos = emit_mul(pos, program->code[4].dst, 16);
    /* sub/add/eor dst5, dst5, x17 */
    pos = emit_arx(pos, &program->code[5], 17);
    /* and x16, src6, 2040 */
    pos = emit_and_2040(pos, 16, program->code[6].src);
    /* ldr x16, [sp, x16] */
    pos = emit_ldr_sp(pos, 16);
    /* and x17, src8, 2040 */
    pos = emit_and_2040(pos, 17, program->code[8].src);
    /* ldr x17, [sp, x17] */
    pos = emit_ldr_sp(pos, 17);
    /* sub/add/eor/orr dst6, dst6, imm6 */
    pos = emit_premul(pos, &program->code[6]);
    /*
        tst x14, 32
        cinc x9, x9, eq
    */
    EMIT(pos, code_branch);
    /* mul dst6, dst6, x16 */
    pos = emit_mul(pos, program->code[6].dst, 16);
    /* b.eq */
    pos = emit_beq(pos, target);
    /* ror/lsr/asr dst8, dst8, imm8 */
    pos = emit_prearx(pos, &program->code[8]);
    /* sub/add/eor dst8, dst8, x17 */
    pos = emit_arx(pos, &program->code[8], 17);
    return pos;
}


void hashwx_compile_a64(uint8_t* code, const hashwx_program_list* program_list) {
    hashwx_vm_rw(code, HASHWX_CODE_SIZE);
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
    hashwx_vm_rx(code, HASHWX_CODE_SIZE);
#ifdef __GNUC__
    __builtin___clear_cache((char*)code, (char*)pos);
#endif
}

#endif

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
        x10     = 9
        x11     = 33
        x12     = R8
        x13     = repeat counter
        x14-x17 = temporary

    Note: The emitted aarch64 code is optimized for the A53 dual-issue
    pipeline. Some instructions are scheduled in a way that depends on
    the table of permitted source permutations to avoid hazards.
    The code may break if the source permutation list in the HashWX specs
    gets changed.
*/

static const uint8_t code_prologue[] = {
    0x0c, 0x20, 0x40, 0xf9, /* ldr x12, [x0, #64] */
    0xe8, 0x03, 0x00, 0xaa, /* mov x8, x0 */
    0x06, 0x1c, 0x43, 0xa9, /* ldp x6, x7, [x0, #48] */
    0x04, 0x14, 0x42, 0xa9, /* ldp x4, x5, [x0, #32] */
    0x02, 0x0c, 0x41, 0xa9, /* ldp x2, x3, [x0, #16] */
    0x00, 0x04, 0x40, 0xa9, /* ldp x0, x1, [x0, #0] */
    0x09, 0x00, 0x80, 0xd2, /* mov x9, 0 */
    0x2a, 0x01, 0x80, 0xd2, /* mov x10, 9 */
    0x2b, 0x04, 0x80, 0xd2, /* mov x11, 33 */
};

static const uint8_t code_epilogue[] = {
    0x06, 0x1d, 0x03, 0xa9, /* stp x6, x7, [x8, #48] */
    0xff, 0x13, 0x40, 0x91, /* add sp, sp, 16384 */
    0x04, 0x15, 0x02, 0xa9, /* stp x4, x5, [x8, #32] */
    0x02, 0x0d, 0x01, 0xa9, /* stp x2, x3, [x8, #16] */
    0x00, 0x05, 0x00, 0xa9, /* stp x0, x1, [x8, #0] */
    0xc0, 0x03, 0x5f, 0xd6, /* ret */
};

static const uint8_t code_clear_bc[] = {
    0x09, 0x00, 0x80, 0xd2, /* mov x9, 0 */
};

static const uint8_t code_set_rc[] = {
    0x8d, 0x00, 0x80, 0xd2, /* mov x13, 4 */
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

 /* converts [1, 9, 33] (divided by 8) to [0, 1, 2] */
static const uint8_t mul_imm_inv[5] = {
    0, 1, 2, 2, 2
};

static const uint32_t premul_tpls[3][3] = {
    { 0xb2400000, 0xaa0a0000, 0xaa0b0000 }, /* orr */
    { 0xd2400000, 0xca0a0000, 0xca0b0000 }, /* eor */
    { 0x91000400, 0x91002400, 0x91008400 }, /* add */
};

static const uint8_t store_all[] = {
    0xe1, 0x03, 0x03, 0xa9, /* stp x1, x0, [sp, #48] */
    0xe3, 0x0b, 0x02, 0xa9, /* stp x3, x2, [sp, #32] */
    0xe5, 0x13, 0x01, 0xa9, /* stp x5, x4, [sp, #16] */
    0xe7, 0x1b, 0x00, 0xa9, /* stp x7, x6, [sp, #0] */
};

static uint8_t* emit_premul(uint8_t* pos, const instruction* isn) {
    uint32_t imm = mul_imm_inv[isn->imm / 8];
    uint32_t tpl = premul_tpls[isn->opcode][imm];
    uint32_t dst = isn->dst;
    EMIT_ISN(pos, tpl | (dst << 5) | (dst));
    return pos;
}

static uint8_t* emit_pre_xas(uint8_t* pos, const instruction* isn) {
    switch ((isn->opcode - 4) / 3) {
    case 0:
        return emit_ror(pos, isn->dst, isn->imm);
    case 1:
        return emit_asr(pos, isn->dst, isn->imm);
    case 2:
        return emit_lsr(pos, isn->dst, isn->imm);
    default:
        UNREACHABLE;
    }
}

static uint8_t* emit_xas(uint8_t* pos, const instruction* isn, uint32_t src) {
    switch ((isn->opcode - 4) % 3) {
    case 0:
        return emit_eor(pos, isn->dst, src);
    case 1:
        return emit_add(pos, isn->dst, src);
    case 2:
        return emit_sub(pos, isn->dst, src);
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

static uint8_t* emit_and_16376(uint8_t* pos, uint32_t dst, uint32_t src) {
    /* and dst, src, 16376 */
    EMIT_ISN(pos, 0x927d2800 | (src << 5) | (dst));
    return pos;
}

static uint8_t* emit_bne(uint8_t* pos, uint8_t* target) {
    uint32_t offset = (uint32_t)(target - pos);
    offset &= 0x1ffffc;
    EMIT_ISN(pos, 0x54000001 | (offset << 3));
    return pos;
}

static uint8_t* emit_repeat_loop(uint8_t* pos, uint8_t* target) {
    /* subs x13, x13, 1 */
    EMIT_ISN(pos, 0xf10005ad);
    return emit_bne(pos, target);
}

static uint8_t* emit_branch(uint8_t* pos, const hashwx_program* program, uint8_t* target) {
    uint32_t tst = program->code[9].opcode == INSTR_UBRANCH
        ?
        0xf27b013f  /* tst x9, 32 */
        :
        0xf27b01df; /* tst x14, 32 */
    EMIT_ISN(pos, tst);
    /* cinc x9, x9, eq */
    EMIT_ISN(pos, 0x9a891529);
    /* b.eq target */
    return emit_beq(pos, target);
}

static uint8_t* compile_program_reg(const hashwx_program* program, uint8_t* pos) {
    uint8_t* target = pos;
    /* sub sp, sp, 64 */
    EMIT_ISN(pos, 0xd10103ff);
    /* STORE: save R0-R7 */
    EMIT(pos, store_all);
    /* mul dst1, dst1, src1 */
    pos = emit_mul(pos, program->code[1].dst, program->code[1].src + 4);
    /* ror/asr/lsr dst2, dst2, imm2 */
    pos = emit_pre_xas(pos, &program->code[2]);
    /* mov x14, src2 */
    pos = emit_mov(pos, 14, program->code[2].src);
    /* orr/eor/add dst3, dst3, imm3 */
    pos = emit_premul(pos, &program->code[3]);
    /* eor/add/sub dst2, dst2, x14 */
    pos = emit_xas(pos, &program->code[2], 14);
    /* mul dst3, dst3, src3 */
    pos = emit_mul(pos, program->code[3].dst, program->code[3].src);
    /* ror/asr/lsr dst4, dst4, imm4 */
    pos = emit_pre_xas(pos, &program->code[4]);
    /* ror dst1, dst1, imm1 */
    pos = emit_ror(pos, program->code[1].dst, program->code[1].imm);
    /* eor/add/sub dst4, dst4, src4 */
    pos = emit_xas(pos, &program->code[4], program->code[4].src);
    /* orr/eor/add dst5, dst5, imm5 */
    pos = emit_premul(pos, &program->code[5]);
    /* mul dst5, dst5, src5 */
    pos = emit_mul(pos, program->code[5].dst, program->code[5].src);
    /* ror/asr/lsr dst6, dst6, imm6 */
    pos = emit_pre_xas(pos, &program->code[6]);
    /* orr x14, dst1, x9 */
    pos = emit_orr(pos, 14, program->code[1].dst, 9);
    /* eor/add/sub dst6, dst6, src6 */
    pos = emit_xas(pos, &program->code[6], program->code[6].src);
    /* orr/eor/add dst7, dst7, imm7 */
    pos = emit_premul(pos, &program->code[7]);
    /* mul dst7, dst7, src7 */
    pos = emit_mul(pos, program->code[7].dst, program->code[7].src);
    /* ror/asr/lsr dst8, dst8, imm8 */
    pos = emit_pre_xas(pos, &program->code[8]);
    /* eor/add/sub dst8, dst8, src8 */
    pos = emit_xas(pos, &program->code[8], program->code[8].src);
    /* branch */
    pos = emit_branch(pos, program, target);
    return pos;
}

static uint8_t* compile_program_mem(const hashwx_program* program, uint8_t* pos) {
    uint8_t* target = pos;
    /* and x15, src2, 16376 */
    pos = emit_and_16376(pos, 15, program->code[2].src);
    /* ldr x15, [sp, x15] */
    pos = emit_ldr_sp(pos, 15);
    /* ror/asr/lsr dst2, dst2, imm2 */
    pos = emit_pre_xas(pos, &program->code[2]);
    /* mul dst1, dst1, src1 */
    pos = emit_mul(pos, program->code[1].dst, program->code[1].src + 4);
    /* eor/add/sub dst2, dst2, x15 */
    pos = emit_xas(pos, &program->code[2], 15);
    /* and x16, src3, 16376 */
    pos = emit_and_16376(pos, 16, program->code[3].src);
    /* ldr x16, [sp, x16] */
    pos = emit_ldr_sp(pos, 16);
    /* and x17, src4, 16376 */
    pos = emit_and_16376(pos, 17, program->code[4].src);
    /* ldr x17, [sp, x17] */
    pos = emit_ldr_sp(pos, 17);
    /* orr/eor/add dst3, dst3, imm3 */
    pos = emit_premul(pos, &program->code[3]);
    /* ror/asr/lsr dst4, dst4, imm4 */
    pos = emit_pre_xas(pos, &program->code[4]);
    /* ror dst1, dst1, imm1 */
    pos = emit_ror(pos, program->code[1].dst, program->code[1].imm);
    /* mul dst3, dst3, x16 */
    pos = emit_mul(pos, program->code[3].dst, 16);
    /* eor/add/sub dst4, dst4, x17 */
    pos = emit_xas(pos, &program->code[4], 17);
    /* and x16, src5, 16376 */
    pos = emit_and_16376(pos, 16, program->code[5].src);
    /* ldr x16, [sp, x16] */
    pos = emit_ldr_sp(pos, 16);
    /* and x17, src6, 16376 */
    pos = emit_and_16376(pos, 17, program->code[6].src);
    /* ldr x17, [sp, x17] */
    pos = emit_ldr_sp(pos, 17);
    /* orr/eor/add dst5, dst5, imm5 */
    pos = emit_premul(pos, &program->code[5]);
    /* ror/asr/lsr dst6, dst6, imm6 */
    pos = emit_pre_xas(pos, &program->code[6]);
    /* orr x14, dst1, x9 */
    pos = emit_orr(pos, 14, program->code[1].dst, 9);
    /* mul dst5, dst5, x16 */
    pos = emit_mul(pos, program->code[5].dst, 16);
    /* eor/add/sub dst6, dst6, x17 */
    pos = emit_xas(pos, &program->code[6], 17);
    /* and x16, src7, 16376 */
    pos = emit_and_16376(pos, 16, program->code[7].src);
    /* ldr x16, [sp, x16] */
    pos = emit_ldr_sp(pos, 16);
    /* and x17, src8, 16376 */
    pos = emit_and_16376(pos, 17, program->code[8].src);
    /* ldr x17, [sp, x17] */
    pos = emit_ldr_sp(pos, 17);
    /* orr/eor/add dst7, dst7, imm7 */
    pos = emit_premul(pos, &program->code[7]);
    /* mul dst7, dst7, x16 */
    pos = emit_mul(pos, program->code[7].dst, 16);
    /* ror/asr/lsr dst8, dst8, imm8 */
    pos = emit_pre_xas(pos, &program->code[8]);
    /* eor/add/sub dst8, dst8, x17 */
    pos = emit_xas(pos, &program->code[8], 17);
    /* branch */
    pos = emit_branch(pos, program, target);
    return pos;
}


void hashwx_compile_a64(uint8_t* code, const hashwx_program_list* program_list) {
    hashwx_vm_rw(code, HASHWX_CODE_SIZE);
    uint8_t* pos = code;
    EMIT(pos, code_prologue);

    /* repeat 4x with BC=32 (memory write) */
    EMIT(pos, code_set_rc);
    uint8_t* reg_phase_start = pos;
    EMIT(pos, code_clear_bc);
    for (uint32_t i = 0; i < HASHWX_NUM_PROGRAMS; ++i) {
        pos = compile_program_reg(&program_list->prog[i], pos);
    }
    pos = emit_repeat_loop(pos, reg_phase_start);

    /* repeat 4x with BC=32 (memory read) */
    EMIT(pos, code_set_rc);
    uint8_t* mem_phase_start = pos;
    EMIT(pos, code_clear_bc);
    for (uint32_t i = 0; i < HASHWX_NUM_PROGRAMS; ++i) {
        pos = compile_program_mem(&program_list->prog[i], pos);
    }
    pos = emit_repeat_loop(pos, mem_phase_start);

    EMIT(pos, code_epilogue);
    hashwx_vm_rx(code, HASHWX_CODE_SIZE);
    assert(pos - code <= HASHWX_CODE_SIZE);
#ifdef __GNUC__
    __builtin___clear_cache((char*)code, (char*)pos);
#endif
}

#endif

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
    aarch64 architectural register allocation:
        x0-x7   = R0-R7
        x8      = in/out ptr
        x9      = 32-BC
        x10     = 9
        x11     = 33
        x12     = R8
        x13     = repeat counter
        x14     = BF | BC
        x15     = temporary
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
    0xec, 0x0f, 0x1f, 0xf8, /* str x12, [sp, #-16]! */
};

static const uint8_t code_epilogue[] = {
    0x06, 0x1d, 0x03, 0xa9, /* stp x6, x7, [x8, #48] */
    0xff, 0x13, 0x40, 0x91, /* add sp, sp, 16384 */
    0x04, 0x15, 0x02, 0xa9, /* stp x4, x5, [x8, #32] */
    0xff, 0x43, 0x00, 0x91, /* add sp, sp, 16 */
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

static uint8_t* emit_and_mask(uint8_t* pos, uint32_t dst, uint32_t src) {
    /* and dst, src, 16383 */
    EMIT_ISN(pos, 0x92403400 | (src << 5) | (dst));
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

static uint8_t* emit_branch(uint8_t* pos, bool unconditional, uint8_t* target) {
    EMIT_ISN(pos, unconditional
        ?
        0xf27b013f /* tst x9, 32 */
        :
        0xf27b01df /* tst x14, 32 */);
    /* cinc x9, x9, eq */
    EMIT_ISN(pos, 0x9a891529);
    /* b.eq target */
    return emit_beq(pos, target);
}

static uint8_t* compile_program_reg(const hashwx_program* program, uint8_t* pos) {
    uint8_t* target = NULL;
    for (int i = 0; i < HASHWX_PROGRAM_SIZE; ++i) {
        const instruction* instr = &program->code[i];
        switch (instr->opcode)
        {
        case INSTR_MULOR:
        case INSTR_MULXOR:
        case INSTR_MULADD:
        {
            /* orr/eor/add dst, dst, imm */
            pos = emit_premul(pos, instr);
            /* mul dst, dst, src */
            pos = emit_mul(pos, instr->dst, instr->src);
            break;
        }
        case INSTR_RMCG:
        {
            /* mul dst, dst, x12 */
            pos = emit_mul(pos, instr->dst, 12);
            /* ror dst, dst, imm */
            pos = emit_ror(pos, instr->dst, instr->imm);
            /* orr x14, dst, x9 */
            pos = emit_orr(pos, 14, instr->dst, 9);
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
            /* ror/asr/lsr dst, dst, imm */
            pos = emit_pre_xas(pos, instr);
            /* eor/add/sub dst, dst, src */
            pos = emit_xas(pos, instr, instr->src);
            break;
        }
        case INSTR_CBRANCH:
        {
            pos = emit_branch(pos, false, target);
            break;
        }
        case INSTR_UBRANCH:
        {
            pos = emit_branch(pos, true, target);
            break;
        }
        case INSTR_STORE:
        {
            target = pos;
            /* sub sp, sp, 64 */
            EMIT_ISN(pos, 0xd10103ff);
            /* STORE: save R0-R7 */
            EMIT(pos, store_all);
            break;
        }
        case INSTR_HALT:
            break;
        default:
            UNREACHABLE;
        }
    }
    return pos;
}

static uint8_t* compile_program_mem(const hashwx_program* program, uint8_t* pos) {
    uint8_t* target = NULL;
    for (int i = 0; i < HASHWX_PROGRAM_SIZE; ++i) {
        const instruction* instr = &program->code[i];
        switch (instr->opcode)
        {
        case INSTR_MULOR:
        case INSTR_MULXOR:
        case INSTR_MULADD:
        {
            /* and x15, src, 16383 */
            /* ldr x15, [sp, x15] */
            pos = emit_and_mask(pos, 15, instr->src);
            pos = emit_ldr_sp(pos, 15);
            /* orr/eor/add dst, dst, imm */
            pos = emit_premul(pos, instr);
            /* mul dst, dst, x15 */
            pos = emit_mul(pos, instr->dst, 15);
            break;
        }
        case INSTR_RMCG:
        {
            /* mul dst, dst, x12 */
            pos = emit_mul(pos, instr->dst, instr->src + 4);
            /* ror dst, dst, imm */
            pos = emit_ror(pos, instr->dst, instr->imm);
            /* orr x14, dst, x9 */
            pos = emit_orr(pos, 14, instr->dst, 9);
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
            /* and x15, src, 16383 */
            /* ldr x15, [sp, x15] */
            pos = emit_and_mask(pos, 15, instr->src);
            pos = emit_ldr_sp(pos, 15);
            /* ror/asr/lsr dst, dst, imm */
            pos = emit_pre_xas(pos, instr);
            /* eor/add/sub dst, dst, x(reg) */
            pos = emit_xas(pos, instr, 15);
            break;
        }
        case INSTR_CBRANCH:
        {
            pos = emit_branch(pos, false, target);
            break;
        }
        case INSTR_UBRANCH:
        {
            pos = emit_branch(pos, true, target);
            break;
        }
        case INSTR_STORE:
        {
            target = pos;
            break;
        }
        case INSTR_HALT:
            break;
        default:
            UNREACHABLE;
        }
    }
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

/* Copyright (c) 2020-2026 tevador <tevador@gmail.com> */
/* See LICENSE for licensing information */

#include "compiler.h"

#ifdef HASHWX_COMPILER_X86

#include <string.h>

#include "platform.h"
#include "program.h"
#include "virtual_memory.h"

#if defined(_WIN32) || defined(__CYGWIN__)
#define WINABI
#endif

#define EMIT(p,x) do {           \
        memcpy(p, &x, sizeof(x)); \
        p += sizeof(x);          \
    } while (0)
#define EMIT_BYTE(p,x) *((p)++) = x

/*
    x86 achitectural register allocation:
        rax    = BF, temporary
        rcx    = in/out ptr
        rdx    = temporary
        rbx    = R8
        rbp    = 2040
        rsp    = stack/memory ptr
        rsi    = 32-BC
        rdi    = 32-(BC-1)
        r8-r15 = R0-R7
*/

static const uint8_t code_prologue[] = {
#ifdef WINABI
    0x56, /* push rsi */
    0x57, /* push rdi */
#else
    0x48, 0x89, 0xf9, /* mov rcx, rdi */
#endif
    0x53, /* push rbx */
    0x55, /* push rbp */
    0x41, 0x54, /* push r12 */
    0x41, 0x55, /* push r13 */
    0x41, 0x56, /* push r14 */
    0x41, 0x57, /* push r15 */
    0x4c, 0x8b, 0x01, /* mov r8, qword ptr [rcx] */
    0x4c, 0x8b, 0x49, 0x08, /* mov r9, qword ptr [rcx+8] */
    0x4c, 0x8b, 0x51, 0x10, /* mov r10, qword ptr [rcx+16] */
    0x4c, 0x8b, 0x59, 0x18, /* mov r11, qword ptr [rcx+24] */
    0x4c, 0x8b, 0x61, 0x20, /* mov r12, qword ptr [rcx+32] */
    0x4c, 0x8b, 0x69, 0x28, /* mov r13, qword ptr [rcx+40] */
    0x4c, 0x8b, 0x71, 0x30, /* mov r14, qword ptr [rcx+48] */
    0x4c, 0x8b, 0x79, 0x38, /* mov r15, qword ptr [rcx+56] */
    0x48, 0x8b, 0x59, 0x40, /* mov rbx, qword ptr [rcx+64] */
    0xbd, 0xf8, 0x07, 0x00, 0x00, /* mov ebp, 2040 */
    0x31, 0xf6, /* xor esi, esi */
    0x8d, 0x7e, 0x01 /* lea edi, [rsi+1] */
};

static const uint8_t code_epilogue[] = {
    0x48, 0x81, 0xc4, 0x00, 0x08, 0x00, 0x00, /* add rsp, 2048 */
    0x4c, 0x89, 0x01, /* mov qword ptr [rcx], r8 */
    0x4c, 0x89, 0x49, 0x08, /* mov qword ptr [rcx+8], r9 */
    0x4c, 0x89, 0x51, 0x10, /* mov qword ptr [rcx+16], r10 */
    0x4c, 0x89, 0x59, 0x18, /* mov qword ptr [rcx+24], r11 */
    0x4c, 0x89, 0x61, 0x20, /* mov qword ptr [rcx+32], r12 */
    0x4c, 0x89, 0x69, 0x28, /* mov qword ptr [rcx+40], r13 */
    0x4c, 0x89, 0x71, 0x30, /* mov qword ptr [rcx+48], r14 */
    0x4c, 0x89, 0x79, 0x38, /* mov qword ptr [rcx+56], r15 */
    0x41, 0x5f, /* pop r15 */
    0x41, 0x5e, /* pop r14 */
    0x41, 0x5d, /* pop r13 */
    0x41, 0x5c, /* pop r12 */
    0x5d, /* pop rbp */
    0x5b, /* pop rbx */
#ifdef WINABI
    0x5f, /* pop rdi */
    0x5e, /* pop rsi */
#endif
    0xc3 /* ret */
};

static const uint8_t code_target[] = {
    0x85, 0xff, /* test edi, edi */
    0x0f, 0x44, 0xf7 /* cmovz esi, edi */
};

static const uint8_t code_branch[] = {
    0x09, 0xf0, /* or eax, esi */
    0x8d, 0x7e, 0x01, /* lea edi, [rsi+1] */
    0xa8, 0x20 /* test al, 32 */
};

static const uint8_t code_store[] = {
    0x41, 0x50, /* push r8 */
    0x41, 0x51, /* push r9 */
    0x41, 0x52, /* push r10 */
    0x41, 0x53, /* push r11 */
    0x41, 0x54, /* push r12 */
    0x41, 0x55, /* push r13 */
    0x41, 0x56, /* push r14 */
    0x41, 0x57  /* push r15 */
};

static const uint8_t code_address[] = {
    0x21, 0xea /* and edx, ebp */
};

static const uint8_t code_clear_bc[] = {
    0x31, 0xf6 /* xor esi, esi */
};

static const uint32_t tpl_mul[] = {
    0x00c88349, /* or */
    0x00f08349, /* xor */
    0x00c08349  /* add */
};

static const uint32_t tpl_pre_xas[] = {
    0x00c8c149, /* ror */
    0x00f8c149, /* sar */
    0x00e8c149  /* shr */
};

static const uint16_t tpl_xas_reg[] = {
    0xc031, /* xor */
    0xc001, /* add */
    0xc029  /* sub */
};

static const uint32_t tpl_xas_mem[] = {
    0x1404334c, /* xor */
    0x1404034c, /* add */
    0x14042b4c  /* sub */
};

static inline uint8_t* emit_imul_reg_4c(uint8_t* pos, uint32_t dst, uint32_t src) {
    uint32_t tpl = 0xc0af0f4c;
    tpl |= src << 24;
    tpl |= dst << 27;
    EMIT(pos, tpl);
    return pos;
}

static inline uint8_t* emit_imul_reg_4d(uint8_t* pos, uint32_t dst, uint32_t src) {
    uint32_t tpl = 0xc0af0f4d;
    tpl |= src << 24;
    tpl |= dst << 27;
    EMIT(pos, tpl);
    return pos;
}

static inline uint8_t* emit_imul_mem(uint8_t* pos, uint32_t dst) {
    EMIT_BYTE(pos, 0x4c);
    uint32_t tpl = 0x1404af0f;
    tpl |= dst << 19;
    EMIT(pos, tpl);
    return pos;
}

static inline uint8_t* emit_op_imm(uint8_t* pos, uint32_t tpl, uint32_t dst, uint32_t imm) {
    tpl |= dst << 16;
    tpl |= imm << 24;
    EMIT(pos, tpl);
    return pos;
}

static inline uint8_t* emit_op_reg_4d(uint8_t* pos, uint16_t tpl, uint32_t dst, uint32_t src) {
    EMIT_BYTE(pos, 0x4d);
    tpl |= dst << 8;
    tpl |= src << 11;
    EMIT(pos, tpl);
    return pos;
}

static inline uint8_t* emit_op_reg_4c(uint8_t* pos, uint16_t tpl, uint32_t dst, uint32_t src) {
    EMIT_BYTE(pos, 0x4c);
    tpl |= dst << 8;
    tpl |= src << 11;
    EMIT(pos, tpl);
    return pos;
}

static inline uint8_t* emit_op_mem(uint8_t* pos, uint32_t tpl, uint32_t dst) {
    tpl |= dst << 19;
    EMIT(pos, tpl);
    return pos;
}

static inline uint8_t* emit_jz(uint8_t* pos, uint8_t* targetp2) {
    uint32_t offset = (uint32_t)(targetp2 - pos);
    uint16_t isn;
    if (offset >= (uint32_t)-128) {
        isn = 0x0074;
        isn |= offset << 8;
        EMIT(pos, isn);
    }
    else {
        offset -= 4;
        isn = 0x840f;
        EMIT(pos, isn);
        EMIT(pos, offset);
    }
    return pos;
}

static uint8_t* compile_program_reg(const hashwx_program * program, uint8_t* pos) {
    uint8_t* target = NULL;
    for (int i = 0; i < HASHWX_PROGRAM_SIZE; ++i) {
        const instruction* instr = &program->code[i];
        instr_type opcode = instr->opcode;
        switch (opcode)
        {
        case INSTR_MULOR:
        case INSTR_MULXOR:
        case INSTR_MULADD:
        {
            /* or/xor/add dst, imm */
            pos = emit_op_imm(pos, tpl_mul[opcode], instr->dst, instr->imm);
            /* imul dst, src */
            pos = emit_imul_reg_4d(pos, instr->dst, instr->src);
            break;
        }
        case INSTR_RMCG:
        {
            target = pos; /* +2 */
            EMIT(pos, code_target);
            /* imul dst, rbx */
            pos = emit_imul_reg_4c(pos, instr->dst, 3);
            /* ror dst, imm */
            pos = emit_op_imm(pos, 0x00c8c149, instr->dst, instr->imm);
            /* mov rax, dst */
            pos = emit_op_reg_4c(pos, 0xc089, 0, instr->dst);
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
            opcode -= 4;
            /* ror/sar/shr dst, imm */
            pos = emit_op_imm(pos, tpl_pre_xas[opcode / 3], instr->dst, instr->imm);
            /* xor/add/sub dst, src */
            pos = emit_op_reg_4d(pos, tpl_xas_reg[opcode % 3], instr->dst, instr->src);
            break;
        }
        case INSTR_BRANCH:
        {
            EMIT(pos, code_branch);
            /* jz target */
            pos = emit_jz(pos, target);
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
        instr_type opcode = instr->opcode;
        switch (opcode)
        {
        case INSTR_MULOR:
        case INSTR_MULXOR:
        case INSTR_MULADD:
        {
            /* mov rdx, src */
            pos = emit_op_reg_4c(pos, 0xc089, 2, instr->src);
            /* or/xor/add dst, imm */
            pos = emit_op_imm(pos, tpl_mul[opcode], instr->dst, instr->imm);
            /* and edx, ebp */
            EMIT(pos, code_address);
            /* imul dst, qword ptr [rsp+rdx] */
            pos = emit_imul_mem(pos, instr->dst);
            break;
        }
        case INSTR_RMCG:
        {
            target = pos; /* +2 */
            EMIT(pos, code_target);
            /* imul dst, rbx */
            pos = emit_imul_reg_4c(pos, instr->dst, 3);
            /* ror dst, imm */
            pos = emit_op_imm(pos, 0x00c8c149, instr->dst, instr->imm);
            /* mov rax, dst */
            pos = emit_op_reg_4c(pos, 0xc089, 0, instr->dst);
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
            opcode -= 4;
            /* mov rdx, src */
            pos = emit_op_reg_4c(pos, 0xc089, 2, instr->src);
            /* ror/sar/shr dst, imm */
            pos = emit_op_imm(pos, tpl_pre_xas[opcode / 3], instr->dst, instr->imm);
            /* and edx, ebp */
            EMIT(pos, code_address);
            /* xor/add/sub dst, qword ptr [rsp+rdx] */
            pos = emit_op_mem(pos, tpl_xas_mem[opcode % 3], instr->dst);
            break;
        }
        case INSTR_BRANCH:
        {
            EMIT(pos, code_branch);
            /* jz target */
            pos = emit_jz(pos, target);
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

void hashwx_compile_x86(uint8_t* code, const hashwx_program_list* program_list) {
    hashwx_vm_rw(code, HASHWX_CODE_SIZE);
    uint8_t* pos = code;
    EMIT(pos, code_prologue);

    for (uint32_t i = 0; i < HASHWX_NUM_PROGRAMS; ++i) {
        pos = compile_program_reg(&program_list->prog[i], pos);
        EMIT(pos, code_store);
    }

    EMIT(pos, code_clear_bc);

    for (uint32_t i = 0; i < HASHWX_NUM_PROGRAMS; ++i) {
        pos = compile_program_mem(&program_list->prog[i], pos);
    }

    EMIT(pos, code_epilogue);
    hashwx_vm_rx(code, HASHWX_CODE_SIZE);
}

#endif

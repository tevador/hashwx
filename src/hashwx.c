/* Copyright (c) 2020-2026 tevador <tevador@gmail.com> */
/* See LICENSE for licensing information */

#include <hashwx.h>
#include <stdio.h>

#include "platform.h"
#include "program.h"
#include "siphash_rng.h"
#include "context.h"
#include "compiler.h"

static void initialize_program(hashwx_ctx* ctx, hashwx_program_list* program_list,
    siphash_key keys[2]) {
    hashwx_program_list_generate(&keys[0], program_list);
    ctx->key = keys[1];
#ifndef NDEBUG
    ctx->has_program = true;
#endif
}

void hashwx_make(hashwx_ctx* ctx, const uint8_t seed[HASHWX_SEED_SIZE]) {
    assert(ctx != NULL && ctx != HASHWX_NOTSUPP);
    assert(seed != NULL);
    siphash_key keys[2];
    keys[0].k0 = platform_load64(&seed[0]);
    keys[0].k1 = platform_load64(&seed[8]);
    keys[1].k0 = platform_load64(&seed[16]);
    keys[1].k1 = platform_load64(&seed[24]);
    if (ctx->type & HASHWX_COMPILED) {
        hashwx_program_list program_list;
        initialize_program(ctx, &program_list, keys);
        hashwx_compile(ctx->code, &program_list);
    }
    else {
        initialize_program(ctx, ctx->program_list, keys);
    }
}

uint64_t hashwx_exec(const hashwx_ctx* ctx, uint64_t input) {
    assert(ctx != NULL && ctx != HASHWX_NOTSUPP);
    assert(ctx->has_program);
    uint64_t r[HASHWX_REG_SIZE];
    //init registers
    siphash_rng gen;
    hashwx_rng_init(&gen, &ctx->key, input);
    for (uint64_t i = 0; i < 8; ++i) {
        r[i] = hashwx_rng_next(&gen);
    }
    //adjust R8 to be 3 mod 8
    r[8] = (r[4] & -8) | 3;
    //adjust R9 to be 5 mod 8
    r[9] = (r[7] & -8) | 5;
    //execute
#ifndef HASHWX_COMPILER_WASM
    if (ctx->type & HASHWX_COMPILED) {
        ctx->func(r);
    }
    else
#endif
    {
        hashwx_program_list_execute(ctx->program_list, r);
    }
    //finalize
    SIPROUND(r[0], r[1], r[2], r[3]);
    SIPROUND(r[4], r[5], r[6], r[7]);
    return r[3] ^ r[7] ^ r[9];
}

#ifdef HASHWX_COMPILER_WASM

/* WASM-only helper functions */

uint8_t* hashwx_seed(hashwx_ctx* ctx) {
    return ctx->seed;
}

uint64_t* hashwx_registers(hashwx_ctx* ctx) {
    return ctx->reg;
}

uint64_t* hashwx_memory(hashwx_ctx* ctx) {
    return ctx->mem;
}

const uint8_t* hashwx_module(const hashwx_ctx* ctx) {
    return ctx->code;
}

uint32_t hashwx_module_size(const hashwx_ctx* ctx) {
    (void)ctx;
    return HASHWX_CODE_SIZE;
}

void hashwx_exec_begin(hashwx_ctx* ctx, uint64_t input) {
    assert(ctx != NULL && ctx != HASHWX_NOTSUPP);
    assert(ctx->has_program);
    //init registers
    siphash_rng gen;
    hashwx_rng_init(&gen, &ctx->key, input);
    uint64_t* r = ctx->reg;
    for (uint64_t i = 0; i < 8; ++i) {
        r[i] = hashwx_rng_next(&gen);
    }
    //adjust R8 to be 3 mod 8
    r[8] = (r[4] & -8) | 3;
    //adjust R9 to be 5 mod 8
    r[9] = (r[7] & -8) | 5;
}

uint64_t hashwx_exec_final(const hashwx_ctx* ctx) {
    const uint64_t* r = ctx->reg;
    uint64_t r0 = r[0];
    uint64_t r1 = r[1];
    uint64_t r2 = r[2];
    uint64_t r3 = r[3];
    uint64_t r4 = r[4];
    uint64_t r5 = r[5];
    uint64_t r6 = r[6];
    uint64_t r7 = r[7];
    uint64_t r9 = r[9];
    //finalize
    SIPROUND(r0, r1, r2, r3);
    SIPROUND(r4, r5, r6, r7);
    return r3 ^ r7 ^ r9;
}

#endif

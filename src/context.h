/* Copyright (c) 2020-2026 tevador <tevador@gmail.com> */
/* See LICENSE for licensing information */

#ifndef CONTEXT_H
#define CONTEXT_H

#include <stdbool.h>

#include "hashwx.h"
#include "siphash_rng.h"
#include "program.h"

typedef void program_func(uint64_t r[]);

typedef struct hashwx_program_list hashwx_program_list;

typedef struct hashwx_ctx {
    union {
        uint8_t* code;
        program_func* func;
        hashwx_program_list* program_list;
    };
    hashwx_type type;
    siphash_key key;
#ifndef NDEBUG
    bool has_program;
#endif
#ifdef __wasm__
    uint8_t seed[HASHWX_SEED_SIZE];
    uint64_t reg[HASHWX_REG_SIZE];
    uint64_t mem[HASHWX_MEM_SIZE];
#endif
} hashwx_ctx;

#endif

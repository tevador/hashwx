/* Copyright (c) 2020-2026 tevador <tevador@gmail.com> */
/* See LICENSE for licensing information */

#ifndef PROGRAM_H
#define PROGRAM_H

#include <stdint.h>
#include <stdbool.h>
#include "hashwx.h"
#include "platform.h"
#include "instruction.h"
#include "siphash_rng.h"

#define HASHWX_PROGRAM_SIZE 10
#define HASHWX_NUM_PROGRAMS 32
#define HASHWX_REG_SIZE 10
#define HASHWX_MEM_SIZE 256

typedef struct hashwx_program {
    instruction code[HASHWX_PROGRAM_SIZE];
} hashwx_program;

typedef struct hashwx_program_list {
    hashwx_program prog[HASHWX_NUM_PROGRAMS];
} hashwx_program_list;

#ifdef __cplusplus
extern "C" {
#endif

HASHWX_PRIVATE void hashwx_program_list_generate(const siphash_key* key, hashwx_program_list* program_list);

HASHWX_PRIVATE void hashwx_program_list_execute(const hashwx_program_list* program_list, uint64_t r[]);

#ifdef __cplusplus
}
#endif

#endif

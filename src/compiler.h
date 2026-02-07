/* Copyright (c) 2020-2026 tevador <tevador@gmail.com> */
/* See LICENSE for licensing information */

#ifndef COMPILER_H
#define COMPILER_H

#include <stdint.h>
#include <stdbool.h>
#include <hashwx.h>

typedef struct hashwx_program_list hashwx_program_list;

HASHWX_PRIVATE void hashwx_compile_x86(uint8_t* code, const hashwx_program_list* program_list);

HASHWX_PRIVATE void hashwx_compile_a64(uint8_t* code, const hashwx_program_list* program_list);

HASHWX_PRIVATE void hashwx_compile_wasm(uint8_t* code, const hashwx_program_list* program_list);

#if defined(_M_X64) || defined(__x86_64__)
#define HASHWX_COMPILER 1
#define HASHWX_COMPILER_X86
#define hashwx_compile hashwx_compile_x86
#define HASHWX_CODE_SIZE 8192
#elif defined(__aarch64__)
#define HASHWX_COMPILER 1
#define HASHWX_COMPILER_A64
#define hashwx_compile hashwx_compile_a64
#define HASHWX_CODE_SIZE 8192
#elif defined(__riscv_xlen) && __riscv_xlen == 64 && defined(__riscv_zbb)
#define HASHWX_COMPILER 0
#define HASHWX_COMPILER_RV64
#define hashwx_compile
#define HASHWX_CODE_SIZE 8192
#elif defined(__wasm__)
#define HASHWX_COMPILER 1
#define HASHWX_COMPILER_WASM
#define hashwx_compile hashwx_compile_wasm
#define HASHWX_CODE_SIZE 11278
#else
#define HASHWX_COMPILER 0
#define hashwx_compile(code, program_list)
#define HASHWX_CODE_SIZE 0
#endif

HASHWX_PRIVATE bool hashwx_compiler_init(hashwx_ctx* compiler);
HASHWX_PRIVATE void hashwx_compiler_destroy(hashwx_ctx* compiler);

#endif

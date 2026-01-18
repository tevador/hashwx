/* Copyright (c) 2020-2026 tevador <tevador@gmail.com> */
/* See LICENSE for licensing information */

#include <stdbool.h>

#include "compiler.h"
#include "context.h"
#ifdef HASHWX_COMPILER_WASM
#include <stdlib.h>
#else
#include "virtual_memory.h"
#endif

bool hashwx_compiler_init(hashwx_ctx* ctx) {
#ifdef HASHWX_COMPILER_WASM
    ctx->code = malloc(HASHWX_CODE_SIZE);
#else
    ctx->code = hashwx_vm_alloc(HASHWX_CODE_SIZE);
#endif
    return ctx->code != NULL;
}

void hashwx_compiler_destroy(hashwx_ctx* ctx) {
#ifdef HASHWX_COMPILER_WASM
    free(ctx->code);
#else
    hashwx_vm_free(ctx->code, HASHWX_CODE_SIZE);
#endif
}

/* Copyright (c) 2020-2026 tevador <tevador@gmail.com> */
/* See LICENSE for licensing information */

#include "context.h"
#include "program.h"
#include "compiler.h"

#include <stdlib.h>

hashwx_ctx* hashwx_alloc(hashwx_type type) {
    if (!HASHWX_COMPILER && (type & HASHWX_COMPILED)) {
        return HASHWX_NOTSUPP;
    }
    hashwx_ctx* ctx = malloc(sizeof(hashwx_ctx));
    if (ctx == NULL) {
        goto failure;
    }
    ctx->code = NULL;
    if (type & HASHWX_COMPILED) {
        if (!hashwx_compiler_init(ctx)) {
            goto failure;
        }
        ctx->type = HASHWX_COMPILED;
    }
    else {
        ctx->program_list = malloc(sizeof(hashwx_program_list));
        if (ctx->program_list == NULL) {
            goto failure;
        }
        ctx->type = HASHWX_INTERPRETED;
    }
#ifndef NDEBUG
    ctx->has_program = false;
#endif
    return ctx;
failure:
    hashwx_free(ctx);
    return NULL;
}

void hashwx_free(hashwx_ctx* ctx) {
    if (ctx != NULL && ctx != HASHWX_NOTSUPP) {
        if (ctx->code != NULL) {
            if (ctx->type & HASHWX_COMPILED) {
                hashwx_compiler_destroy(ctx);
            }
            else {
                free(ctx->program_list);
            }
        }
        free(ctx);
    }
}

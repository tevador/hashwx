/* Copyright (c) 2020-2026 tevador <tevador@gmail.com> */
/* See LICENSE for licensing information */

#include <hashwx.h>
#include <unif01.h>
#include <bbattery.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>

static unsigned long hashwx_gen_bits_hi(void *param, void *state) {
    uint64_t* nonce_ptr = (uint64_t*)state;
    hashwx_ctx* ctx = (hashwx_ctx*)param;
    uint64_t nonce = *nonce_ptr;
    uint32_t next = (uint32_t)(hashwx_exec(ctx, nonce) >> 32);
    *nonce_ptr = nonce + 1;
    return next;
}

static unsigned long hashwx_gen_bits_lo(void *param, void *state) {
    uint64_t* nonce_ptr = (uint64_t*)state;
    hashwx_ctx* ctx = (hashwx_ctx*)param;
    uint64_t nonce = *nonce_ptr;
    uint32_t next = (uint32_t)hashwx_exec(ctx, nonce);
    *nonce_ptr = nonce + 1;
    return next;
}

static double hashwx_gen_double_hi(void *param, void *state) {
    return hashwx_gen_bits_hi(param, state) / unif01_NORM32;
}

static double hashwx_gen_double_lo(void *param, void *state) {
    return hashwx_gen_bits_lo(param, state) / unif01_NORM32;
}

static void hashwx_write_state(void* state) {
    uint64_t* nonce_ptr = (uint64_t*)state;
    printf("Nonce: %016" PRIx64 "\n", *nonce_ptr);
}

int main(int argc, char** argv) {
    if (argc != 3) {
        printf("Usage: %s <seed> <lo|hi>\n", argv[0]);
        return 1;
    }
    int seed = atoi(argv[1]);
    if (seed == 0) {
        printf("Invalid seed\n");
        return 1;
    }
    hashwx_ctx* ctx = hashwx_alloc(HASHWX_COMPILED);
    if (ctx == HASHWX_NOTSUPP) {
        ctx = hashwx_alloc(HASHWX_INTERPRETED);
    }
    if (ctx == NULL) {
        printf("hashwx_alloc failed\n");
        return 1;
    }
    uint64_t nonce = 0;
    uint8_t seed_buf[HASHWX_SEED_SIZE] = "0000-TestU01-hashwx-crush-seed1";
    memcpy(seed_buf, &seed, sizeof(seed));
    hashwx_make(ctx, seed_buf);
    char name[] = "HashWX";

    unif01_Gen gen;
    gen.state = &nonce;
    gen.param = ctx;
    gen.Write = &hashwx_write_state;
    if (strcmp(argv[2], "hi") == 0) {
        gen.GetU01 = &hashwx_gen_double_hi;
        gen.GetBits = &hashwx_gen_bits_hi;
    }
    else {
        gen.GetU01 = &hashwx_gen_double_lo;
        gen.GetBits = &hashwx_gen_bits_lo;
    }
    gen.name = name;

    bbattery_SmallCrush(&gen);
    return 0;
}

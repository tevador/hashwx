/* Copyright (c) 2020-2026 tevador <tevador@gmail.com> */
/* See LICENSE for licensing information */

#ifndef SIPHASH_GENERATOR_H
#define SIPHASH_GENERATOR_H

#include <stdint.h>
#include <hashwx.h>

#define ROTL(x, b) (((x) << (b)) | ((x) >> (64 - (b))))
#define SIPROUND(v0, v1, v2, v3) \
  do { \
    v0 += v1; v2 += v3; v1 = ROTL(v1, 13);   \
    v3 = ROTL(v3, 16); v1 ^= v0; v3 ^= v2;   \
    v0 = ROTL(v0, 32); v2 += v1; v0 += v3;   \
    v1 = ROTL(v1, 17);  v3 = ROTL(v3, 21);   \
    v1 ^= v2; v3 ^= v0; v2 = ROTL(v2, 32);   \
  } while (0)

typedef struct siphash_key {
    uint64_t k0, k1;
} siphash_key;

typedef struct siphash_rng {
    siphash_key key;
    uint64_t state[4];
    uint32_t count;
} siphash_rng;

#ifdef __cplusplus
extern "C" {
#endif

HASHWX_PRIVATE void hashwx_rng_init(siphash_rng* gen, const siphash_key* key, const uint64_t salt);
HASHWX_PRIVATE void hashwx_rng_mix(siphash_rng* gen);

static inline uint64_t hashwx_rng_next(siphash_rng* gen) {
    uint32_t count = gen->count;
    if (count == 0) {
        hashwx_rng_mix(gen);
        count = 4;
    }
    count--;
    gen->count = count;
    return gen->state[count];
}

#ifdef __cplusplus
}
#endif

#endif

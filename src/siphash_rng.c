/* Copyright (c) 2020-2026 tevador <tevador@gmail.com> */
/* See LICENSE for licensing information */

#include "siphash_rng.h"

void hashwx_rng_init(siphash_rng* gen, const siphash_key* key, const uint64_t salt) {
    uint64_t k0 = key->k0;
    uint64_t k1 = key->k1;
    uint64_t v0 = UINT64_C(0x736f6d6570736575) ^ k0;
    uint64_t v1 = UINT64_C(0x646f72616e646f6d) ^ k1;
    uint64_t v2 = UINT64_C(0x6c7967656e657261) ^ k0;
    uint64_t v3 = UINT64_C(0x7465646279746573) ^ k1;

    v3 ^= salt;

    SIPROUND(v0, v1, v2, v3);

    v0 ^= salt;
    v2 ^= 0xbb;

    SIPROUND(v0, v1, v2, v3);
    SIPROUND(v0, v1, v2, v3);
    SIPROUND(v0, v1, v2, v3);

    gen->key = *key;
    gen->state[0] = v0;
    gen->state[1] = v1;
    gen->state[2] = v2;
    gen->state[3] = v3;
    gen->count = 4;
}

void hashwx_rng_mix(siphash_rng* gen) {
    uint64_t k0 = gen->key.k0;
    uint64_t k1 = gen->key.k1;
    uint64_t v0 = gen->state[0] ^ k0;
    uint64_t v1 = gen->state[1] ^ k1;
    uint64_t v2 = gen->state[2] ^ k0;
    uint64_t v3 = gen->state[3] ^ k1;

    SIPROUND(v0, v1, v2, v3);
    SIPROUND(v0, v1, v2, v3);
    SIPROUND(v0, v1, v2, v3);
    SIPROUND(v0, v1, v2, v3);

    gen->state[0] = v0;
    gen->state[1] = v1;
    gen->state[2] = v2;
    gen->state[3] = v3;
}

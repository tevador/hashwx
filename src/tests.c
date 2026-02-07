/* Copyright (c) 2020-2026 tevador <tevador@gmail.com> */
/* See LICENSE for licensing information */

#ifdef NDEBUG
#undef NDEBUG
#endif

#include <hashwx.h>
#include <assert.h>
#include <stdbool.h>
#include <inttypes.h>
#include <stdio.h>

typedef bool test_func(void);

static int test_no = 0;

static hashwx_ctx* ctx_int = NULL;
static hashwx_ctx* ctx_cmp = NULL;

static const uint8_t seed1[32] = "This is a test seed for hashwx";
static const uint8_t seed2[32] = "Lorem ipsum dolor sit amet";

static const uint64_t counter1 = 0;
static const uint64_t counter2 = 123456;
static const uint64_t counter3 = 987654321123456789;

static const uint64_t hash1 = 0x06b638075f29d804;
static const uint64_t hash2 = 0xb4489a882aac21d3;
static const uint64_t hash3 = 0xe3b8e01e61aa0289;
static const uint64_t hash4 = 0x776c7320c85a7842;

#define RUN_TEST(x) run_test(#x, &x)

static void run_test(const char* name, test_func* func) {
    printf("[%2i] %-40s ... ", ++test_no, name);
    printf(func() ? "PASSED\n" : "SKIPPED\n");
}

static bool test_alloc(void) {
    ctx_int = hashwx_alloc(HASHWX_INTERPRETED);
    assert(ctx_int != NULL && ctx_int != HASHWX_NOTSUPP);
    return true;
}

static bool test_make1(void) {
    hashwx_make(ctx_int, seed1);
    return true;
}

static bool test_hash1(void) {
    uint64_t hash = hashwx_exec(ctx_int, counter1);
    assert(hash == hash1);
    return true;
}

static bool test_hash2(void) {
    uint64_t hash = hashwx_exec(ctx_int, counter2);
    assert(hash == hash2);
    return true;
}

static bool test_make2(void) {
    hashwx_make(ctx_int, seed2);
    return true;
}

static bool test_hash3(void) {
    uint64_t hash = hashwx_exec(ctx_int, counter2);
    assert(hash == hash3);
    return true;
}

static bool test_hash4(void) {
    uint64_t hash = hashwx_exec(ctx_int, counter3);
    assert(hash == hash4);
    return true;
}

static bool test_compiler_alloc(void) {
    ctx_cmp = hashwx_alloc(HASHWX_COMPILED);
    assert(ctx_cmp != NULL);
    return ctx_cmp != HASHWX_NOTSUPP;
}

static bool test_compiler_make1(void) {
    if (ctx_cmp == HASHWX_NOTSUPP)
        return false;

    hashwx_make(ctx_cmp, seed1);
    return true;
}

static bool test_compiler_hash1(void) {
    if (ctx_cmp == HASHWX_NOTSUPP)
        return false;

    uint64_t hash = hashwx_exec(ctx_cmp, counter1);
    assert(hash == hash1);
    return true;
}

static bool test_compiler_hash2(void) {
    if (ctx_cmp == HASHWX_NOTSUPP)
        return false;

    uint64_t hash = hashwx_exec(ctx_cmp, counter2);
    assert(hash == hash2);
    return true;
}

static bool test_compiler_make2(void) {
    if (ctx_cmp == HASHWX_NOTSUPP)
        return false;

    hashwx_make(ctx_cmp, seed2);
    return true;
}

static bool test_compiler_hash3(void) {
    if (ctx_cmp == HASHWX_NOTSUPP)
        return false;

    uint64_t hash = hashwx_exec(ctx_cmp, counter2);
    assert(hash == hash3);
    return true;
}

static bool test_compiler_hash4(void) {
    if (ctx_cmp == HASHWX_NOTSUPP)
        return false;

    uint64_t hash = hashwx_exec(ctx_cmp, counter3);
    assert(hash == hash4);
    return true;
}

static bool test_free(void) {
    hashwx_free(ctx_int);
    hashwx_free(ctx_cmp);
    return true;
}

int main(void) {
    RUN_TEST(test_alloc);
    RUN_TEST(test_make1);
    RUN_TEST(test_hash1);
    RUN_TEST(test_hash2);
    RUN_TEST(test_make2);
    RUN_TEST(test_hash3);
    RUN_TEST(test_hash4);
    RUN_TEST(test_compiler_alloc);
    RUN_TEST(test_compiler_make1);
    RUN_TEST(test_compiler_hash1);
    RUN_TEST(test_compiler_hash2);
    RUN_TEST(test_compiler_make2);
    RUN_TEST(test_compiler_hash3);
    RUN_TEST(test_compiler_hash4);
    RUN_TEST(test_free);

    printf("\nAll tests were successful\n");
    return 0;
}

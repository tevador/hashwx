#include <hashwx.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

int main() {
    uint8_t seed[HASHWX_SEED_SIZE] = "this seed will generate a hash";
    hashwx_ctx* ctx = hashwx_alloc(HASHWX_COMPILED);
    if (ctx == HASHWX_NOTSUPP)
        ctx = hashwx_alloc(HASHWX_INTERPRETED);
    if (ctx == NULL)
        return 1;
    hashwx_make(ctx, seed); /* generate a hash function */
    uint64_t hash = hashwx_exec(ctx, 123456789); /* calculate the hash of a nonce value */
    hashwx_free(ctx);
    printf("%016" PRIx64 "\n", hash);
    return 0;
}

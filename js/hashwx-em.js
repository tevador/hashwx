mergeInto(LibraryManager.library, {
    hashwx_alloc: function(type) {
        return HWX_LIB.hashwx_alloc(type);
    },
    hashwx_make: function(ctx, seed) {
        let seed_src = HEAPU8.slice(seed, seed + 32);
        HWX_LIB.hashwx_make(ctx, seed_src);
    },
    hashwx_exec: function(ctx, nonce) {
        return HWX_LIB.hashwx_exec(ctx, nonce);
    },
    hashwx_free: function(ctx) {
        HWX_LIB.hashwx_free(ctx);
    }
});

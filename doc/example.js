let ctx = HWX_LIB.hashwx_alloc(1);
console.assert(ctx > 0);
let encoder = new TextEncoder();
let seed = encoder.encode("this seed will generate a hash\0\0");
HWX_LIB.hashwx_make(ctx, seed);
let hash = HWX_LIB.hashwx_exec(ctx, BigInt(123456789));
HWX_LIB.hashwx_free(ctx);
console.log(hash);

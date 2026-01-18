let lib = new hashwx();
let ctx = lib.hashwx_alloc(1);
console.assert(ctx > 0);
let encoder = new TextEncoder();
let seed - encoder.encode("this seed will generate a hash\0\0");
lib.hashwx_make(ctx, seed);
let hash = lib.hashwx_exec(ctx, BigInt(123456789));
lib.hashwx_free(ctx);
console.log(hash);

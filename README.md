# HashWX

HashWX is an algorithm designed for client puzzles and proof-of-work schemes.
While traditional cryptographic hash functions use a fixed one-way compression
function, each HashWX instance represents a unique pseudorandomly generated
one-way function.

HashWX functions are generated as a carefully crafted sequence of integer
operations and branches. Extra care is taken to avoid optimizations
and to ensure that each function takes on average the same number of CPU cycles.

## Design and specification

See [documentation](doc).

## API

The API consists of 4 functions and is documented in the public header file
[hashwx.h](include/hashwx.h).

Example of usage:

```c
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
```

## Build

A C11-compatible compiler and `cmake` are required.

```
git clone https://github.com/tevador/hashwx.git
cd hashwx
mkdir build
cd build
cmake ..
make
```

## Performance

HashWX was designed for fast verification. Generating a hash function from a seed
takes about 30 000 cycles and a 64-bit nonce can be hashed in under 2400 cycles in compiled
mode (interpreted mode is about 10x slower). These performance numbers were measured on an AMD Zen 2 CPU.

A benchmark executable is included:
```
./hashwx-bench --seeds 20000 --threads 16
```

## WebAssembly

WebAssembly offers about 70% of native performance thanks to the built-in compiler that builds a dynamic module for each generated hash function.

To build a WASM module, install Emscripten and use `emcmake cmake ..` instead of `cmake ..`.

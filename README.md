# HashWX

HashWX is an algorithm designed for client puzzles and proof-of-work schemes.
While traditional cryptographic hash functions use a fixed one-way compression
function, each HashWX instance represents a unique pseudorandomly generated
one-way function.

HashWX functions are generated as a carefully crafted sequence of integer
operations and branches. Extra care is taken to avoid optimizations
and to ensure that each function takes on average the same number of CPU cycles.

## Client puzzle protocols

Client puzzles are protocols designed to protect server resources from abuse. A client requesting a resource from a server may be asked to solve a puzzle before the request is accepted.

One of the first proposed client puzzles is [Hashcash](https://en.wikipedia.org/wiki/Hashcash),
which requires the client to find a partial SHA-1 hash inversion. However,
because of the static nature of cryptographic hash functions, an attacker can
offload hashing to a GPU or FPGA to gain a significant advantage over legitimate
clients equipped only with a CPU.

In a minimal HashWX-based protocol, the client is given a 256-bit challenge bitstring `C` and a 64-bit numeric target `T`. The client's goal is to find a 64-bit nonce `N` such that `H(N) < T`, where `H = hashwx_make(sha256(C || (n & -512)))` is a dynamically constructed hash function. Each hash function can only be used for 512 attempts before it must be discarded. This is recommended for maximum GPU resistance.

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

HashWX was designed for maximum GPU resistance and fast verification. Generating a hash function from a seed
takes about 20 000 CPU cycles and a 64-bit nonce can be hashed in under 2400 cycles in compiled
mode (interpreted mode is about 10x slower). These performance numbers were measured on an AMD Zen 2 CPU.

A benchmark executable is included:
```
./hashwx-bench --seeds 100000 --threads 16
```

## WebAssembly

WebAssembly offers about 70% of native performance thanks to the built-in compiler that builds a dynamic module for each generated hash function. HashWX is therefore well-suited for browser-based CAPTCHA-like client puzzles.

To build a WASM module, install Emscripten and use `emcmake cmake ..` instead of `cmake ..`.

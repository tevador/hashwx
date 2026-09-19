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

In a minimal HashWX-based protocol, the client is given a 256-bit challenge bitstring `C` and a 64-bit numeric target `T`. The client's goal is to find a 64-bit nonce `N` such that `H(N) < T`, where

```
H = hashwx_make(sha256(C || (N / 463)))
```

is a dynamically constructed hash function. Each hash function can only be used for 463 attempts before it must be discarded.

## Design and specification

HashWX was designed for maximum GPU resistance and fast verification.

See [documentation](doc) for details.

## Performance

The current version of HashWX includes JIT compilers for x86-64, ARM64 and WebAssembly. On these architectures, each hash function is compiled into native code for maximum performance.

On an AMD Zen 2 CPU, generating a hash function from a seed takes about 20 000 CPU cycles and a 64-bit nonce can be hashed in under 10 000 cycles in compiled mode (interpreted mode is about 5x slower). On a 3 GHz CPU, a puzzle solution can be verified in under 10 µs.

WebAssembly offers about 60% of native performance. HashWX is therefore well-suited for browser-based CAPTCHA-like client puzzles. For in-browser use, it's recommended to increase the number of attempts per hash function from 463 to 65536 due to higher compilation overhead.

### GPU implementation

During the design of HashWX, a CUDA version of the algorithm was implemented. The code is non-public and was only used to evaluate the GPU resistance of the algorithm.

General details about the optimal GPU implementation are in the [design.md](doc/design.md) document.

### Benchmarks

The benchmark results below compare the performance of AMD Ryzen 3700X with the performance of NVIDIA RTX 5060 Ti. Both were tested at their stock configuration. On the CPU, the native build and the WebAssembly build were benchmarked separately.

| Hardware | Power limit | Build | Threads | `--nonces 463` | `--nonces 65536` |
|----------|-------------|-------|---------|----------------|------------------|
| Ryzen 3700X | 88 W     | native| 16      | 4.5 MH/s       | 4.6 MH/s         |
| Ryzen 3700X | 88 W     | wasm  | 16      | 0.8 MH/s       | 2.8 MH/s         |
| RTX 5060 Ti | 180 W    | cuda  | 63x32   | 5.6 MH/s       | 5.7 MH/s        |

The Ryzen 3700X CPU has a stock power limit of 88 W (despite its advertised TDP of 65 W). The GPU has a stock power limit of 180 W.

The Threads column lists the parallelism that provides the best performance on the hardware. For the CPU, this is limited by the number of hardware threads. For the GPU, the best launch configuration is 63 blocks of 32 threads, limited by the 32 MB L2 cache capacity.

Performance was measured separately for 463 nonces per seed and 65536 nonces per seed. The WebAssembly build shows a large difference between these configurations, which is why using 65536 nonces per seed is recommended for in-browser use.

The CPU benchmarks can be reproduced with the following commands:

```
./hashwx-bench --threads 16 --seeds 100000 --nonces 463
./hashwx-bench --threads 16 --seeds 1000 --nonces 65536
node hashwx-bench.js --threads 16 --seeds 10000 --nonces 463
node hashwx-bench.js --threads 16 --seeds 1000 --nonces 65536
```

## Build

A C11-compatible compiler and `cmake` are required. The build produces the HashWX static and dynamic libraries, a `hashwx-bench` executable and a `hashwx-tests` executable.

```
git clone https://github.com/tevador/hashwx.git
cd hashwx
mkdir build
cd build
cmake ..
make
```

To run HashWX in the browser, install Emscripten and use `emcmake cmake ..` instead of `cmake ..`. The build produces the `hashwx.wasm` module, a `hashwx-bench.js` executable and a `hashwx-tests.js` executable.

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

Browser integration requires both the hashwx.wasm compiled module and the provided Javascript glue code [hashwx.js](js/hashwx.js). With the glue code in scope, a hash can be calculated as follows:

```js
let ctx = HWX_LIB.hashwx_alloc(1);
console.assert(ctx > 0);
let encoder = new TextEncoder();
let seed = encoder.encode("this seed will generate a hash\0\0");
HWX_LIB.hashwx_make(ctx, seed);
let hash = HWX_LIB.hashwx_exec(ctx, BigInt(123456789));
HWX_LIB.hashwx_free(ctx);
console.log(hash);
```

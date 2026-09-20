# HashWX design

## 1. Comparison with HashX

HashWX is a successor of [HashX](https://github.com/tevador/hashx), which was the first attempt to design a GPU resistant client puzzle with fast verification.

However, HashX suffers from the following problems:

1. Repeated multiplications cause an accumulation of trailing zeroes in registers, which produces some "weak" instances that return hashes almost independent of the input (this is even mentioned in the [HashX readme](https://github.com/tevador/hashx?tab=readme-ov-file#security)).
2. A HashX program has 16 branches with a branch rate of 1/16. This is insufficient to cause divergence in JIT compiled GPU implementations.
3. Because HashX uses no memory at all, GPUs can achieve a high interpreter performance with optimal occupancy.
4. The instruction set is tailored too much for x86. For example, the ARM performance suffers due to the need to materialize many 32-bit constants and WebAssembly performance is poor due to the use of 128-bit multiplications.
5. Program generation is unnecessarily complex, simulating a 12+ year old Intel CPU. Additionally, program generation has a chance to fail, which is impractical and makes implementations more complicated and error-prone.

### 1.1 Multiplications

In HashWX, 2 ways are used to prevent a loss of entropy when hashing.

Firstly, multiplications are always fused with a "healing" operation that prevents accumulation of trailing zeroes in registers. In particular, an odd immediate is always ORed, XORed or added to one of the multiplicands, which gives the following 3 MUL* instructions:

|opcode|instruction|operation|dst|src|imm|
|-|-|-|-|-|-|
|0|MULOR|`dst=(dst\|imm)*[src]`|R0-R7|R0-R7|`1,9,33`|
|1|MULXOR|`dst=(dst^imm)*[src]`|R0-R7|R0-R7|`1,9,33`|
|2|MULADD|`dst=(dst+imm)*[src]`|R0-R7|R0-R7|`1,9,33`|

It was empirically tested that even a long sequence of one of these instructions (with random `dst`, `src` and `imm`) will on average accumulate only about 4 trailing zeroes (for MULXOR and MULADD) or 1 trailing zero (for MULOR).

Secondly, HashWX expands the 8 VM registers with an additional read-only register R8, which preserves part of the input state even if some of the registers R0-R7 lose entropy during the execution of the program.

### 1.2 Branches

HashWX greatly expands the number and frequency of branches in the program with the goal of causing significant thread divergence in JIT-compiled GPU implementations.

In particular, each of the 32 programs that make up a HashWX instance has the form of a loop that can branch back to the start of the program with a probability of 1/2. CPUs will have to execute the loop 2x on average for 1 thread to complete. There will be some negative impact caused by branch mispredictions, but pipeline bubbles can be efficiently filled
when running 2 threads per CPU core.

Due to divergence, GPUs (in JIT compiled mode) will have to execute the loop 6x on average for a warp of 32 threads to complete:
* 16 threads won't loop
* 8 threads will loop once
* 4 threads will loop twice
* 2 threads will loop 3x
* 1 thread will loop 4x
* 1 thread will loop 5x or more

The specification is written in a way that causes all HashWX instances to always branch exactly 64 times per hash (32x in the write phase and 32x in the read phase). This is done by a combination of using a branch-counting register (BC) and inserting an unconditional branch at the last program. This property is needed to make the run time roughly constant for all hash functions and nonces.

### 1.3 Memory

While the HashX VM doesn't have any memory (only registers), the HashWX VM was expanded with 16KB of scratchpad memory. Scratchpad loads are unaligned on purpose.

CPUs will store the scratchpad in the L1 cache. The typical load latency of 3-4 cycles can be hidden with reordering. The impact on performance is low. Common CPU architectures such as x86 and ARM64 also support unaligned loads natively with good performance.

GPUs will have to store the scratchpads in local memory. The scratchpads will be cached in the L2 cache, which has a much higher latency (~100 cycles) than shared memory. Shared memory will have to compete for space with the L1 cache (shared memory carveout). Most GPU architectures also don't support unaligned loads natively, so they must be emulated by combining two adjacent loads. Unaligned loads also prevent the use of scratchpad interleaving.

The large latency difference between the CPU and the GPU can be exploited via forced "pointer chasing" in the memory read phase. Source registers for arithmeric instructions are selected from two lists: the "shallow" list produces 3 memory load dependency chains of depth 2 (for example R1 -> R2, R3 -> R4, R5 -> R7), while the "deep" list produces a memory load dependency chain of depth 6 (for example R1 -> R3 -> R4 -> R5 -> R6 -> R7). The lists are interleaved so that the CPU needs to handle on average 2.75 dependent loads per program. However, the GPU interpreter must specialize for the deep list because in ~95% of cases, at least one thread in a warp is running a deep program and not executing the loads sequentially would result in a read-after-write hazard. The GPU therefore gets hit with the full 6 dependent loads for every program.

### 1.4 Instruction set

HashWX uses a simplified instruction set that only includes basic operations that are available in WebAssembly 1.0. This includes 64-bit multiplications, additions, subtractions, bitwise XOR and OR, bit rotations and shifts.

The immediate size is limited to 6 bits to allow for efficient encoding in x86, ARM and RISC-V instruction formats.

HashWX uses a variant of the [multiplicative congruential generator](https://en.wikipedia.org/wiki/Lehmer_random_number_generator) (MCG) to generate pseudorandom numbers for branching. This generator only requires two operations (a multiplication and a rotation) per output. The constant multiplier is stored in the read-only register R8 and is constructed to be 3 (mod 8) in accordance with [literature](https://dl.acm.org/doi/epdf/10.1145/321062.321065). The output of the generator determines if a conditional branch is taken.

### 1.5 Program generation

Program generation in HashWX is greatly simplified compared to HashX. The generator uses a constant number of random numbers and instruction selection is done without backtracking. Destination registers are selected as a permutation, which ensures that all registers are written to in every program. The complex source register selection rules from HashX were replaced with a precomputed list of permutations. Overall, program generation in HashWX is about 5x faster than for HashX, which greatly reduces the time to verify a solution.

## 2. GPU resistance

One of the main goals of HashWX is GPU resistance. For the old HashX algorithm, third parties have implemented GPU solvers that greatly exceed CPU performance.

### 2.1 JIT compiled GPU solvers

In general, GPU kernels can either be statically compiled interpreters or they can use JIT-compiled code to improve performance. JIT compilation is possible for both AMD and NVIDIA GPUs, although the latter is more difficult due to undocumented hardware instruction sets.

HashWX attempts to prevent JIT-compiled GPU kernels with 2 countermeasures:

1. Divergent branching (see above)
2. Limiting the number of nonces per hash function

These countermeasures make JIT-compiled GPU implementations unviable. For the second point, it is specifically recommended to use 463 nonces per hash fuction. This should be taken as a protocol rule. The HashWX library does not enforce this limit. The number 463 was chosen because it maps awkwardly onto 32-thread GPU warps and is just high enough to amortize the native compilation overhead on the CPU.

For the browser-friendly variant of HashWX, it is recommended to use 65536 nonces per hash function due to higher compilation overhead. This makes the protocol somewhat more susceptible to JIT compiled GPU kernels, but divergent branching still makes it more GPU resistant than the old HashX algorithm.

### 2.2 Anti-GPU features

The table below lists the main features of HashWX that make it GPU resistant. For each feature, the table shows if it reduces the GPU/CPU performance ratio :white_check_mark: or not :x:.

| HashWX feature | slower GPU (interpreted) | slower GPU (JIT compiled) |
|-----------------|-------------------|-----------|
| random instructions |:white_check_mark: |   :x:     |
| pointer chasing | :white_check_mark: | :x:     |
| divergent branches |      :x:       | :white_check_mark: |
| unaligned memory reads | :white_check_mark: | :white_check_mark: |
| 16KB scratchpad  | :white_check_mark: | :white_check_mark: |

### 2.3 Optimal GPU kernel

To evaluate the GPU resistance of HashWX, a CUDA implementation was used in the design phase. The code is non-public.

Roughly speaking, the optimal GPU implementation works as follows:

* It's a statically compiled interpreter kernel.
* Programs and VM registers R0-R7 are stored in shared memory in a configuration that avoids bank conflicts.
* HashWX program body is written in a way that minimizes divergence when threads in a warp are executing different programs.
* Each GPU thread has its own independent program counter. Threads in a warp may execute different programs. The specification ensures that they reconverge at the end of the 32-program list.
* The VM scratchpads must fit in L2 cache. Exceeding the L2 cache capacity has a negative impact on performance regardless of occupancy.

The GPU implementation was quite extensively optimized and shows that a mid-range GPU roughly performs on par with a mid-range CPU. This is not a claim that faster GPU implementations cannot be made, but the result provides supporting evidence that HashWX is quite GPU resistant.

## 3. Browser-based proof of work

This section explores the suitability of HashWX as an in-browser proof of work for projects such as [Anubis](https://github.com/TecharoHQ/anubis) or [Cap](https://github.com/tiagozip/cap).

The algorithms that are presently being used by these projects include SHA256 (used by both Anubis and Cap), Argon2id (`m=19456,t=2,p=1`, used by Anubis), HashX (interpreted mode, used by Anubis) and RSW (75000 squarings modulo a 2048-bit semiprime, used by Cap).

The benchmarks below compare the performance of AMD Ryzen 3700X (16 threads) to NVIDIA RTX 5060 Ti GPU (4608 cuda cores).

The CPU results for SHA256, Argon2id and HashX were measured using the WASM backends from Anubis. The result for RSW was measured using the JS backend from Cap. For HashWX, the measurements were made with 65536 nonces per program as recommended.

The GPU results for SHA256 and Argon2id were measured using [Hashcat](https://github.com/hashcat/hashcat). The GPU results for HashX and RSW come from private CUDA solvers. The source code for these solvers cannot be published for security reasons as both HashX and RSW are in active use.

| Algorithm | CPU hashrate (WASM/JS) | GPU hashrate (CUDA) | GPU advantage |
|-----------|---------------------|---------------------|---------------|
| SHA256    |     41 MH/s         |     6150 MH/s       |   ~150x       |
| Argon2id  |     183 H/s         |     3730 H/s        |   ~20x        |
| HashX     |     5.3 MH/s        |     2200 MH/s       |   ~400x       |
| RSW       |     26 H/s          |     4400 H/s        |   ~170x       |
| **HashWX**|     **2.8 MH/s**    |     **5.8 MH/s**    |   **~2x**     |

* Argon2id is moderately GPU resistant (compared to SHA256), but suffers from a high server-side verification cost.
* The results of the old HashX algorithm are particularly bad because the comparison is between an interpreted CPU version and a JIT-compiled GPU version. HashX was simply not designed to be used in the browser.
* RSW fails to be GPU resistant. In fact, Cap's implementation of RSW suffers from a larger GPU advantage than the optimized WASM solver for SHA256. Although the [Cap documentation](https://trycap.dev/guide/rsw.html#why-rsw) lists the latency metric, only the throughput is relevant for DoS protection. The GPU will be able to access 4400 RSW protected resources per second despite its higher latency of 49 µs per squaring.

## 4. ASIC resistance

In general, client puzzles used for DoS protection do not need to be resistant to speed-up by specialized fixed-function hardware (ASIC). This is called "ASIC resistance" and there are two main reasons why it's not strictly required in this case:

1. The cost to design and produce a specialized chip for a given algorithm exceeds USD $10 million and takes up to a few years. This is in contrast with the cost of a DoS attack using rented GPUs, which may be as low as $10 per month.
2. If an ASIC for the client puzzle is developed, the algorithm can be quickly changed with a simple patch, rendering the attacker's investment useless.

Therefore HashWX does not claim ASIC resistance as one of its properties, but it's likely that some ASIC resistance is inherited from its GPU resistant design.

Using HashWX as a cryptocurrency proof-of-work consensus mechanism is not recommended because the above two points typically don't apply for that use-case. Long-term mining rewards may justify the development of specialized hardware and the algorithm cannot be easily changed once implemented.

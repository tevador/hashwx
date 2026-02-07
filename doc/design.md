# HashWX design

HashWX is a successor of [HashX](https://github.com/tevador/hashx), which was the first attempt to design a GPU resistant client puzzle.

However, HashX suffers from the following problems:

1. Repeated multiplications cause an accumulation of trailing zeroes in registers, which produces some "weak" instances that return hashes almost independent of the input (this is even mentioned in the [HashX readme](https://github.com/tevador/hashx?tab=readme-ov-file#security)).
2. A HashX program has 16 branches with a branch rate of 1/16. This is insufficient to cause divergence in compiled GPU implementations.
3. Because HashX uses no memory at all, GPUs can achieve good interpreter performance while storing VM registers in shared memory.
4. The instruction set is tailored too much for x86. For example, the ARM performance suffers due to the need to materialize many 32-bit constants and WebAssembly performance is poor due to the use of 128-bit multiplications.
5. Program generation is unnecessarily complex, simulating a 12+ year old Intel CPU. Additionally, program generation has a chance to fail, which is impractical and makes implementations more complicated and error-prone.

### Multiplications

In HashWX, we use 2 ways to prevent a loss of entropy when hashing.

Firstly, multiplications are always fused with a "healing" operation that prevents accumulation of trailing zeroes in registers. In particular, an odd immediate is always ORed, XORed or added to one of the multiplicands, which gives us the following 3 MUL* instructions:

|opcode|instruction|operation|dst|src|imm|
|-|-|-|-|-|-|
|0|MULOR|`dst=(dst\|imm)*[src]`|R0-R7|R0-R7|`1,5,17,65`|
|1|MULXOR|`dst=(dst^imm)*[src]`|R0-R7|R0-R7|`1,5,17,65`|
|2|MULADD|`dst=(dst+imm)*[src]`|R0-R7|R0-R7|`1,5,17,65`|

We empirically tested that even a long sequence of one of these instructions (with random `dst`, `src` and `imm`) will on average accumulate only about 4 trailing zeroes (for MULXOR and MULADD) or 1 trailing zero (for MULOR).

Secondly, HashWX expands the 8 VM registers with two additional read-only registers R8 and R9, which preserve part of the input state even if some of the registers R0-R7 lose entropy during the execution of the program.

### Branches

HashWX greatly expands the number and frequency of branches in the program with the goal of causing significant thread divergence in JIT-compiled GPU implementations.

In particular, each of the 64 sub-programs that make up a HashWX instance has the form of a loop that can loop back to the start of the program with a probability of 1/2. CPUs will have to execute the loop 2x on average for 1 thread to complete. There will be some negative impact caused by branch mispredictions, but pipeline bubbles can be efficiently filled
when running 2 threads per CPU core.

Due to divergence, GPUs (in compiled mode) will have to execute the loop 6x on average for a warp of 32 threads to complete:
* 16 threads won't loop
* 8 threads will loop once
* 4 threads will loop twice
* 2 threads will loop 3x
* 1 thread will loop 4x
* 1 thread will loop 5x or more

### Memory

While the HashX VM doesn't have any memory (only registers), the HashWX VM was expanded with 2KB of scratchpad memory.

CPUs will store the scratchpad in the L1 cache. The typical load latency of 3-4 cycles can be hidden with reordering or scheduling of VM instructions (this is possible even for in-order CPUs such as the ARM Cortex-A53 by inserting independent instruction after the load). The impact on performance is low. CPUs could even support larger scratchpads (up to 16KB), but run time limitations cap the size at 2KB (larger scratchpads require longer programs).

GPUs will have to store the scratchpad in local memory. The scratchpads will be cached in the L2 cache, which has a much higher latency (~100 cycles) than shared memory. Shared memory will have to compete for space with the L1 cache (lower shared memory carveout).

### Instruction set

HashWX uses a simplified instruction set that only includes basic operations that are available in WebAssembly 1.0. This includes 64-bit multiplications, additions, subtractions, bitwise XOR and OR, bit rotations and shifts.

The immediate size is limited to 7 bits to allow for efficient encoding in x86, ARM and RISC-V instruction formats.

HashWX uses a variant of the [multiplicative congruential generator](https://en.wikipedia.org/wiki/Lehmer_random_number_generator) (MCG) to generate pseudorandom numbers for branching. This generator only requires two operations (a multiplication and a rotation) per output. The constant multipliers are stored in the read-only registers R8-R9 and are constructed to be 3 and 5 (mod 8) in accordance with [literature](https://dl.acm.org/doi/epdf/10.1145/321062.321065).

A HashWX program in register mode consists of roughly the following sequence of x86 opcodes. If multiple opcodes are possible, the probabilities are in parentheses.

```
cmovz
imul
ror
ror (1/3), sar (1/3), shr (1/3)
add (1/3), sub (1/3), xor (1/3)
or  (1/3), xor (1/3), add (1/3)
imul
ror (1/3), sar (1/3), shr (1/3)
add (1/3), sub (1/3), xor (1/3)
or  (1/3), xor (1/3), add (1/3)
imul
ror (1/3), sar (1/3), shr (1/3)
add (1/3), sub (1/3), xor (1/3)
or  (1/3), xor (1/3), add (1/3)
imul
or
lea
test
jz
ror (1/3), sar (1/3), shr (1/3)
add (1/3), sub (1/3), xor (1/3)
```

### Program generation

Program generation in HashWX is greatly simplified compared to HashX. The generator uses a constant number of random numbers and instruction selection is done without backtracking. Destination registers are selected as a permutation, which ensures that all registers are written to in every program. The complex source register selection rules from HashX were replaced with a precomputed list of permutations that satisfy those rules. Overall, program generation in HashWX is about 5x faster than for HashX, which greatly reduces the time to verify a solution.

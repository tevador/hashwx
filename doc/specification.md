# HashWX specification

This document describes the HashWX algorithm. The document is structured as follows:

* Section 1 describes the virtual machine that's internally used by HashWX.
* Section 2 describes how HashWX instances are generated from a seed.
* Section 3 describes how hashes are calculated given a HashWX instance and a nonce value.

## 1. Virtual machine

The HashWX virtual machine (VM) consists of the following parts:

* Program buffer
* Registers
* Memory

### 1.1 Program buffer

Program buffer holds the program to be executed by the VM. Supported instructions are listed in § 1.4.2.

### 1.2 Registers

The VM has a total of 14 registers. The first 9 registers (R0-R8) are arithmetic registers and the remaining 5 registers (PC, BC, BF, MF, SP) are control registers and flags.

#### 1.2.1 Arithmetic registers

The registers R0-R8 are used for arithmetic operations. Their size is 64 bits. Registers R0-R7 are read-write registers, while R8 is a read-only register used by the `RMCG` instruction.

#### 1.2.2 Control registers

##### 1.2.2.1 Program counter (PC)

This register holds the index of the next instruction in the program buffer to be executed. The value of the register is automatically incremented when an instruction starts executing. When the VM is restarted, the value of PC is set to 0.

##### 1.2.2.2 Branch counter (BC)

The branch counter holds the maximum number of branches the VM can take. Whenever a branch instruction writes to the program counter, the BC register is decremented. When the value of BC reaches zero, no further branches can be taken.

##### 1.2.2.3 Branch flag (BF)

This register is a 1-bit flag that determines if a branch will be taken or not taken. The flag is set by the `RMCG` instruction.

##### 1.2.2.4 Memory flag (MF)

This register is a 1-bit flag that determines if the VM executes in the register operand mode (MF=0) or in the memory operand mode (MF=1). The flag is read-only.

##### 1.2.2.5 Store pointer (SP)

This register holds the memory address where the STORE instruction writes to memory.

### 1.3 Memory

The VM has 16392 bytes of memory. The STORE instruction can write to memory if MF=0 and other instructions can read from memory if MF=1.

### 1.4 Instruction set

#### 1.4.1 Instruction format

Every instruction in the program buffer consists of:

1. An opcode that determines what the instruction does.
2. A destination register `dst`, if applicable.
3. A source register `src`, if applicable.
4. An immediate value `imm`, if applicable. The maximum immediate size is 6 bits.

The actual source operand used by an instruction depends on the value of the Memory flag. If MF=0, the register value is used directly as the source operand. If MF=1, the value in the register (modulo 16384) is used to load a 64-bit value from the memory. The load address is 1-byte aligned. Operands are read in little-endian byte order.

#### 1.4.2 Instruction listing

* all arithmetic operations are performed modulo 2<sup>64</sup>
* `>>` denotes an arithmetic (signed) right shift
* `>>>` denotes a logical (unsigned) right shift
* `>>>>` denotes circular right shift (rotation)
* the source operand is evaluated as: `[src] = MF ? load64(src % 16384) : src`
* the RMCG instruction always uses a register source operand

*Table 1.4.2 - HashWX instructions*

|opcode|instruction|operation|dst|src|imm|
|-|-|-|-|-|-|
|0|MULOR|`dst=(dst\|imm)*[src]`|R0-R7|R0-R7|`1,9,33`|
|1|MULXOR|`dst=(dst^imm)*[src]`|R0-R7|R0-R7|`1,9,33`|
|2|MULADD|`dst=(dst+imm)*[src]`|R0-R7|R0-R7|`1,9,33`|
|3|RMCG|`dst=(dst*src)>>>>imm;BF=dst[5];`|R0-R7|R8|`1-63`|
|4|XORROR|`dst=(dst>>>>imm)^[src]`|R0-R7|R0-R7|`1-63`|
|5|ADDROR|`dst=(dst>>>>imm)+[src]`|R0-R7|R0-R7|`1-63`|
|6|SUBROR|`dst=(dst>>>>imm)-[src]`|R0-R7|R0-R7|`1-63`|
|7|XORASR|`dst=(dst>>imm)^[src]`|R0-R7|R0-R7|`1-3`|
|8|ADDASR|`dst=(dst>>imm)+[src]`|R0-R7|R0-R7|`1-3`|
|9|SUBASR|`dst=(dst>>imm)-[src]`|R0-R7|R0-R7|`1-3`|
|10|XORLSR|`dst=(dst>>>imm)^[src]`|R0-R7|R0-R7|`1-3`|
|11|ADDLSR|`dst=(dst>>>imm)+[src]`|R0-R7|R0-R7|`1-3`|
|12|SUBLSR|`dst=(dst>>>imm)-[src]`|R0-R7|R0-R7|`1-3`|
|13|CBRANCH|`if(BC && !BF){BC--;PC=0;}`||||
|14|UBRANCH|`if(BC){BC--;PC=0;}`||||
|15|STORE|`if(!MF){SP=SP-64;store512(SP, [R7..R0]);}`||||
|16|HALT||||||

##### 1.4.2.1 MULOR
This instruction performs a bitwise OR of the destination register with the immediate value, multiplies it by the source operand and stores the result in the destination register.

##### 1.4.2.2 MULXOR
This instruction performs a bitwise XOR of the destination register with the immediate value, multiplies it by the source operand and stores the result in the destination register.

##### 1.4.2.3 MULADD
This instruction adds the immediate value to the destination register, multiplies it by the source operand and stores the result in the destination register.

##### 1.4.2.4 RMCG
This instruction multiplies the destination register by the source register and rotates the result by the immediate count. The result is written to the destination register and bit number 5 (zero-indexed from LSB) of the destination is copied to the Branch flag.

##### 1.4.2.5 XORROR
This instruction rotates the destination register to the right by the immediate count, performs bitwise XOR with the source operand and stores the result in the destination register.

##### 1.4.2.6 ADDROR
This instruction rotates the destination register to the right by the immediate count, adds the source operand and stores the result in the destination register.

##### 1.4.2.7 SUBROR
This instruction rotates the destination register to the right by the immediate count, subtracts the source operand and stores the result in the destination register.

##### 1.4.2.8 XORASR
This instruction performs an arithmetic right shift of the destination register by the immediate count. The source operand is bitwise XORed with the shifted value and the result is stored in the destination register.

##### 1.4.2.9 ADDASR
This instruction performs an arithmetic right shift of the destination register by the immediate count. The source operand is added to the shifted value and the result is stored in the destination register.

##### 1.4.2.10 SUBASR
This instruction performs an arithmetic right shift of the destination register by the immediate count. The source operand is subtracted from the shifted value and the result is stored in the destination register.

##### 1.4.2.11 XORLSR
This instruction performs a logical right shift of the destination register by the immediate count. The source operand is bitwise XORed with the shifted value and the result is stored in the destination register.

##### 1.4.2.12 ADDLSR
This instruction performs a logical right shift of the destination register by the immediate count. The source operand is added to the shifted value and the result is stored in the destination register.

##### 1.4.2.13 SUBLSR
This instruction performs a logical right shift of the destination register by the immediate count. The source operand is subtracted from the shifted value and the result is stored in the destination register.

##### 1.4.2.14 CBRANCH
This instruction performs a conditional branch. The branch is taken if BC is nonzero and BF is zero. If the branch is taken, the BC register is decremented by 1 and the VM jumps to the first instruction in the program buffer.

##### 1.4.2.15 UBRANCH
This instruction performs an unconditional branch. The branch is taken as long as BC is nonzero. If the branch is taken, the BC register is decremented by 1 and the VM jumps to the first instruction in the program buffer.

##### 1.4.2.16 STORE
If MF=0, this instruction decreases the value of SP by 64 and then stores R7 at the address of SP, R6 at the address SP+8, etc. R0 is stored at SP+56. The stores are done in little-endian byte order. If MF=1, this instruction is a no-op.

##### 1.4.2.17 HALT
This instruction stops the VM. No register values are affected.

## 2. HashWX instance generation

The HashWX instance generation procedure takes a 32-byte seed as input and produces a HashWX instance, which consists of 32 random programs for the VM and a 16-byte Siphash key.

### 2.1 Siphash keys

Siphash keys are used to initialize the Siphash generator, which is a pseudorandom number generator described in Appendix A. The 32-byte seed is split into 2 Siphash keys:

1. The first Siphash key (seed bytes 0-15) is used to generate 32 random programs. The generator is initialized with a salt value of `0xffffffffffffffff`.
2. The second Siphash key (seed bytes 16-31) is copied into the HashWX instance and is used during hash calculation.

### 2.2 Program structure

Each HashWX program consists of 11 instructions and has the following structure:

*Table 2.2.1 - HashWX program structure*

|index|opcode|arguments|
|-----|------|---------|
|0|STORE||
|1|RMCG|dst, imm|
|2|(arithmetic)|dst, src, imm|
|3|(arithmetic)|dst, src, imm|
|4|(arithmetic)|dst, src, imm|
|5|(arithmetic)|dst, src, imm|
|6|(arithmetic)|dst, src, imm|
|7|(arithmetic)|dst, src, imm|
|8|(arithmetic)|dst, src, imm|
|9|(branch)||
|10|HALT||

The main program body (indexes 2-8) consists of 7 arithmetic instructions, which include opcodes 0-2 and 4-12 from table 1.4.2. The branch at index 9 is one of the two branch opcodes (13-14) from table 1.4.2.

### 2.3 Program generation

Each random program is generated from 16 pseudorandom 64-bit numbers output from the Siphash generator. In this section, these 16 numbers are referred to as `gen[0]` to `gen[15]`.

#### 2.3.1 Opcode template

Each HashWX program implements one of 35 possible templates of MUL and XAS opcode groups at indexes 2-8. There are always exactly 3 MUL opcodes (0-2) and 4 XAS opcodes (4-12). The opcode template index is calculated as `gen[7] % 35`. The opcode templates are listed in Appendix B.

#### 2.3.2 Opcodes

For each slot from the selected opcode template, an opcode needs to be selected. Opcode selection uses the low 32 bits of `gen[0]` to `gen[6]`.

If the i-th position in the opcode template is a XAS instruction (marked as "X" in Appendix B), it can have one of 9 possible opcodes. The opcode is selected from the lookup table 2.3.2.1 based on the least significant 32 bits of `gen[i]`.

*Table 2.3.2.1 - XAS lookup table*

|`(gen[i] & 0xffffffff) % 9`|opcode|
|-----|------|
|0|XORROR|
|1|ADDROR|
|2|SUBROR|
|3|XORASR|
|4|ADDASR|
|5|SUBASR|
|6|XORLSR|
|7|ADDLSR|
|8|SUBLSR|

If the i-th position in the opcode template is a MUL instruction (marked as "M" in Appendix B), it can have one of 3 possible opcodes. The opcode is selected based on the least significant 32 bits of `gen[i]` as shown in table 2.3.2.2

*Table 2.3.2.2 - MUL lookup table*

|`(gen[i] & 0xffffffff) % 3`|opcode|
|-----|------|
|0|MULOR|
|1|MULXOR|
|2|MULADD|

The opcode for the branch instruction at index 9 is not selected at random. The first 31 programs always use the CBRANCH instruction. The last program uses UBRANCH to ensure that the value of BC is zero at the end.

#### 2.3.3 Destinations

A total of 8 instructions in each program need a destination register (indexes 1-8). Destinations are selected by taking a random permutation of the registers R0-R7. The random permutation is produced using the Fisher-Yates shuffle with the upper 32 bits of `gen[0]` to `gen[6]` used for index selection. The Fisher-Yates shuffle algorithm is described in Appendix C.

#### 2.3.4 Sources

The 7 instructions from the opcode template also need a source register. These sources are selected as one of 128 permitted permutations of the destinations. The source permutation index is calculated as `gen[7] % 128`. The permitted source permutations are listed in Appendix D. 

The source permutation is a permutation of the destinations, so it needs to be combined with the destination permutation to get actual register indexes, i.e. `src[i] = dst[1+perm[i-1]]` for instruction index `i = 2..8` and the selected permutation `perm`.

The RMCG instruction at index 1 always takes R8 as its source operand.

Note: The generator output `gen[7]` is used twice (in § 2.3.1 and here). The resulting random values are independent because `gcd(35,128) = 1`.

#### 2.3.5 Immediates

A total of 8 instructions in each program need an immediate value (indexes 1-8). Immediates are selected from `gen[8]` to `gen[15]` modulo the number of possible immediate values permitted for a given opcode (see Table 2.3.5.1).

*Table 2.3.5.1 - Immediates*

|opcode at index `i`|immediate value|
|-------------------|---------------|
| 0-2               |`{1,9,33}[gen[7+i] % 3]`|
|3-6                |`1 + (gen[7+i] % 63)`|
|7-12               |`1 + (gen[7+i] % 3)`|

## 3. HashWX calculation

Given a HashWX instance and a 64-bit nonce value, the hash value calculation consists of the following phases:

1. Register initialization phase
2. Memory write phase
3. Memory read phase
4. Finalization phase

The whole algorithm in pseudocode is described in Appendix E.

### 3.1 Register initialization

A Siphash generator is initialized using the Siphash key from the HashWX instance and the nonce value as the salt. The first 8 random numbers from the generator are used to initialize the registers R0-R7.

The register R8 is initialized as `R8 = ((R0 ^ R4) & -8) | 3`.

### 3.2 Memory write phase

At the beginning of this phase, the Memory flag is set to 0, the Store pointer is set to 16384 and the value of the R8 register is written at the address of SP in little endian byte order. Then the following is repeated 4 times: The VM Branch counter register is set to 32 and the 32 HashWX programs are executed sequentially. No register values are modified between the VM executions apart from the Program counter, which is reset to 0 at the beginning of each program.

The memory write phase always executes exactly 256 STORE instructions, which exactly fill the remaining 16384 bytes of VM memory. The final value of SP is 0. The value of R8 stored at address 16384 is never overwritten by a STORE instruction and is only partially readable via unaligned loads at addresses 16377-16383.

### 3.3 Memory read phase

At the beginning of this phase, the Memory flag is set to 1. Then the following is repeated 4 times: The VM Branch counter register is set to 32 and the 32 HashWX programs are executed sequentially. No register values are modified between the VM executions apart from the Program counter, which is reset to 0 at the beginning of each program.

### 3.4 Finalization phase

The final hash value is calculated from the values of registers R0-R8 after the memory read phase. First, two SipRounds are executed to mix registers R0-R3 and R4-R7 (SipRound is described in Appendix A.1). The final 64-bit hash value is `R3 ^ R7 ^ R8`. Test vectors can be found in Appendix F.

## Appendix

### A. Siphash generator

Siphash generator is a custom pseudorandom number generator. The internal state of the generator is represented by four 64-bit integers `v0-v3`.

#### A.1 SipRound

SipRound is the basic building block of the generator. It mixes four 64-bit integers as follows:

```
function sipround(v0, v1, v2, v3):
    v0 += v1
    v2 += v3
    v1 = v1 >>>> 51
    v3 = v3 >>>> 48
    v1 ^= v0
    v3 ^= v2
    v0 = v0 >>>> 32
    v2 += v1
    v0 += v3
    v1 = v1 >>>> 47
    v3 = v3 >>>> 43
    v1 ^= v2
    v3 ^= v0
    v2 = v2 >>>> 32
    return (v0, v1, v2, v3)
```

#### A.2 Generator initialization

The Siphash generator is initialized with a 16-byte Siphash key and a 64-bit salt. The key is interpreted as two 64-bit integers `k0` and `k1` (in little endian format). The initial state is generated as follows:

```
function rng_init(k0, k1, salt):
    v0 = 0x736f6d6570736575 ^ k0;
    v1 = 0x646f72616e646f6d ^ k1;
    v2 = 0x6c7967656e657261 ^ k0;
    v3 = 0x7465646279746573 ^ k1;
    v3 ^= salt;
    v0, v1, v2, v3 = sipround(v0, v1, v2, v3);
    v0 ^= salt;
    v2 ^= 0xbb;
    v0, v1, v2, v3 = sipround(v0, v1, v2, v3);
    v0, v1, v2, v3 = sipround(v0, v1, v2, v3);
    v0, v1, v2, v3 = sipround(v0, v1, v2, v3);
    return (v0, v1, v2, v3)
```

#### A.3 Random number generation

The generator outputs its internal state in the order `v3`, `v2`, `v1` and `v0`. When the internal state is exhausted, a mix step is performed to refresh the state using the initial key values:

```
function rng_mix(k0, k1, v0, v1, v2, v3):
    v0 ^= k0
    v1 ^= k1
    v2 ^= k0
    v3 ^= k1
    v0, v1, v2, v3 = sipround(v0, v1, v2, v3);
    v0, v1, v2, v3 = sipround(v0, v1, v2, v3);
    v0, v1, v2, v3 = sipround(v0, v1, v2, v3);
    v0, v1, v2, v3 = sipround(v0, v1, v2, v3);
    return (v0, v1, v2, v3)
```

### B. Opcode templates

Each HashWX program implements one of 35 opcode templates at program indexes 2-8. The templates are listed below (lexicographically sorted). "M" refers to one of the MUL opcodes and "X" refers to one of the XAS opcodes.

```
MMMXXXX
MMXMXXX
MMXXMXX
MMXXXMX
MMXXXXM
MXMMXXX
MXMXMXX
MXMXXMX
MXMXXXM
MXXMMXX
MXXMXMX
MXXMXXM
MXXXMMX
MXXXMXM
MXXXXMM
XMMMXXX
XMMXMXX
XMMXXMX
XMMXXXM
XMXMMXX
XMXMXMX
XMXMXXM
XMXXMMX
XMXXMXM
XMXXXMM
XXMMMXX
XXMMXMX
XXMMXXM
XXMXMMX
XXMXMXM
XXMXXMM
XXXMMMX
XXXMMXM
XXXMXMM
XXXXMMM
```

### C. Fisher-Yates shuffle

Fisher-Yates shuffle generates a random permutation of N elements using N-1 random numbers. In the case of HashWX, we have N=8 and the random numbers are `rnd[0]` to `rnd[6]`, which correspond to the high 32 bits of `gen[0]` to `gen[6]`.

```
function fisher_yates_shuffle(rnd)
    dst[0] = 0
    for i in [1..7]:
        dst[i] = i
        j = rnd[i - 1] % (i + 1)
        dst[i] = dst[j]
        dst[j] = i
    return dst
```

### D. Source permutations

The permitted 128 source permutations are listed below (lexicographically sorted). The first element of the permutation is unused as there are only 7 source registers per program. The source permutation is a permutation of the destinations, so it needs to be combined with the destination permutation to get actual register indexes.

No permutation uses its own destination as its source, i.e. `perm[i] != i` for all `i = 1..7`.

```
02345671 02375614 02451673 02713456
03172456 03456712 03567214 04127356
04567123 04576132 05123746 06123475
06541372 10326745 10576342 10675234
10723456 10742635 10765324 12576304
13056742 14672035 14675230 16057234
16057324 16357042 16705234 17023456
17065234 17506234 17506324 17642053
20675341 20713456 20741653 23457601
23615740 23645701 23657041 23675041
24316705 24675031 25067314 25471603
25640731 26310745 27013456 27450631
27645013 27645031 30172456 30456271
30657142 30741625 34016725 34026715
34057612 34602715 34701625 34702615
35460172 35461270 35462170 36705214
37102456 37506124 37506214 40127356
40752613 42361705 42576130 43012675
43751620 45061273 45062173 45367021
45367102 46075231 46302751 46312705
46315702 47120356 47506213 50123746
52460173 52461073 52461370 52461730
52706314 53017642 53412670 53641270
54062371 54372601 54673120 56317042
56407312 56740123 57123046 60123475
62315704 62450371 62570143 62743015
63517402 63517420 64571230 65740123
65740213 65740312 67123405 67123450
67350142 67540123 72013456 72451306
72451603 73102456 73456021 74120356
74560132 75123046 75316024 75641203
75641230 76123405 76123450 76305124
```

### E. HashWX algorithm pseudocode

```
function hashwx_execute(self, nonce):
    gen = siphash_generator(self.key, nonce)
    vm = hashwx_vm()
    for i in [0..7]:
        vm.r[i] = gen.next()
    vm.r[8] = ((vm.r[0] ^ vm.r[4]) & -8) | 3
    vm.mf = 0
    vm.sp = 16384
    store64(vm.sp, vm.r[8])
    for i in [0..3]:
        vm.bc = 32
        for j in [0..31]:
            vm.program = self.program[j]
            vm.execute()
    vm.mf = 1
    for i in [0..3]:
        vm.bc = 32
        for j in [0..31]:
            vm.program = self.program[j]
            vm.execute()
    vm.r[0], vm.r[1], vm.r[2], vm.r[3] = sipround(vm.r[0], vm.r[1], vm.r[2], vm.r[3])
    vm.r[4], vm.r[5], vm.r[6], vm.r[7] = sipround(vm.r[4], vm.r[5], vm.r[6], vm.r[7])
    return vm.r[3] ^ vm.r[7] ^ vm.r[8]
```

### F. Test vectors

Each test vector consists of a seed value (32 bytes in hex format) to generate a hash function, a nonce value (base 10 number) and the resulting hash value (base 16 number).

#### F.1 Test1

- seed: `5468697320697320612074657374207365656420666f72206861736877780000`
- nonce: `0`
- result: `0x1af53247be2cfe67`

#### F.2 Test2

- seed: `5468697320697320612074657374207365656420666f72206861736877780000`
- nonce: `123456`
- result: `0xe3545019a837ebba`

#### F.3 Test3

- seed: `4c6f72656d20697073756d20646f6c6f722073697420616d6574000000000000`
- nonce: `123456`
- result: `0xc04b99e6001bd636`

#### F.4 Test4

- seed: `4c6f72656d20697073756d20646f6c6f722073697420616d6574000000000000`
- nonce: `987654321123456789`
- result: `0x3f07bf678615b8fa`

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

Each random program is generated from 16 pseudorandom 64-bit numbers output from the Siphash generator. In this section, these 16 numbers are referred to as `gen[0]` to `gen[15]`. Additionally, the generator is parametrized by two boolean flags - `is_last` (if the program is the last one in the list) and `is_deep` (if the program should use a deep source permutation).

#### 2.3.1 Opcode template

Each HashWX program implements one of 35 possible templates of MUL and XAS opcode groups at indexes 2-8. There are always exactly 3 MUL opcodes (0-2) and 4 XAS opcodes (4-12). The opcode template index is calculated as `gen[7] % 35`. The opcode templates are listed in Appendix B.

#### 2.3.2 Opcodes

For each slot from the selected opcode template, an opcode needs to be selected. Opcode selection uses the low 32 bits of `gen[0]` to `gen[6]`.

If the j-th position in the opcode template is a XAS instruction (marked as "X" in Appendix B), it can have one of 9 possible opcodes. The opcode is selected from the lookup table 2.3.2.1 based on the least significant 32 bits of `gen[j]`. The index `j = 0` corresponds to the instruction index 2 from table 2.2.1.

*Table 2.3.2.1 - XAS lookup table*

|`(gen[j] & 0xffffffff) % 9`|opcode|
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

If the j-th position in the opcode template is a MUL instruction (marked as "M" in Appendix B), it can have one of 3 possible opcodes. The opcode is selected based on the least significant 32 bits of `gen[j]` as shown in table 2.3.2.2

*Table 2.3.2.2 - MUL lookup table*

|`(gen[j] & 0xffffffff) % 3`|opcode|
|-----|------|
|0|MULOR|
|1|MULXOR|
|2|MULADD|

The opcode for the branch instruction at index 9 is CBRANCH if `is_last` is false. If `is_last` is true, the program uses UBRANCH to ensure that the value of BC is zero at the end.

#### 2.3.3 Destinations

A total of 8 instructions in each program need a destination register (indexes 1-8). Destinations are selected by taking a random permutation of the registers R0-R7. The random permutation is produced using the Fisher-Yates shuffle with the upper 32 bits of `gen[0]` to `gen[6]` used for index selection. The Fisher-Yates shuffle algorithm is described in Appendix C.

#### 2.3.4 Sources

The 7 instructions from the opcode template also need a source register. These sources are selected as one of the permitted permutations of the destinations.

If `is_deep` is false, the source permutation index is calculated as `gen[7] % 256` and the permutation is selected from the shallow list in Appendix D.1. The 256 shallow permutations produce 3 memory load dependency chains of depth 2.

If `is_deep` is true, the source permutation index is calculated as `gen[7] % 24` and the permutation is selected from the deep list in Appendix D.2. The 24 deep permutations produce a memory load dependency chain of depth 6.

The source permutation is a permutation of the destinations, so it needs to be combined with the destination permutation to get actual register indexes, i.e. `src[j] = dst[perm[j]]` for  index `j = 1..7` and the selected permutation `perm`.

The RMCG instruction at index 1 always takes R8 as its source operand.

Note: The generator output `gen[7]` is used twice (in § 2.3.1 and here). The resulting random values are independent because `gcd(35,256) = 1` and `gcd(35,24) = 1`.

#### 2.3.5 Immediates

A total of 8 instructions in each program need an immediate value (indexes 1-8). Immediates are selected from `gen[8]` to `gen[15]` modulo the number of possible immediate values permitted for a given opcode (see Table 2.3.5.1).

*Table 2.3.5.1 - Immediates*

|opcode at index `i`|immediate value|
|-------------------|---------------|
| 0-2               |`{1,9,33}[gen[7+i] % 3]`|
|3-6                |`1 + (gen[7+i] % 63)`|
|7-12               |`1 + (gen[7+i] % 3)`|

### 2.4 Program list

The 32 programs are generated in sequence (program 0, program 1, ..., program 31) using the same shared instance of Siphash generator.

Only the last program at index 31 is generated with the `is_last` flag set to true.

Only the programs at indexes 0, 6, 12, 18, 24 and 30 are generated with the `is_deep` flag set to true. There are always 6 "deep" programs and 26 "shallow" programs. Deep programs use a different source register selection method (see § 2.3.4).

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

The permitted source permutations are listed below. They are lexicographically sorted and separated into 2 lists. The first element of each permutation is unused as there are only 7 source registers per program. The source permutation is a permutation of the destinations, so it needs to be combined with the destination permutation to get actual register indexes.

No permutation uses its own destination as its source, i.e. `perm[i] != i` for all `i = 1..7`.

#### D.1 Shallow permutations

The 256 shallow permutations produce 3 memory load dependency chains of depth 2.

```
02143675 03416752 04567123 06375214
10452376 13025476 13520476 13627054
13720645 13720654 13725406 13726054
14503276 14572306 14603725 14672035
14672053 14702653 14703625 14752306
14753206 14763052 15670234 15670243
15706234 15706243 15760324 15760423
16472053 16570243 17320654 17326045
17326054 17462035 17463052 17506234
17506243 17560234 17560243 17605243
17642053 17643052 17650234 20463715
23017645 23106745 23170645 23170654
24071653 24173650 24571306 24573106
24671035 24753106 25067341 25167034
25167304 25170634 25176034 25607134
25670134 25760143 25760314 25760341
25760413 26157304 26157403 26157430
26175340 26175430 27105643 27143605
27143650 27150643 27156034 27156304
27156340 27165304 27165403 27310654
27650341 30416725 30426751 30526741
30615472 32016745 32017645 32105476
32106745 32107654 32157406 32160754
32716045 32716054 34071652 34617025
34627051 34672051 34720651 35076421
35607124 35627041 35670124 35670241
35670421 35716042 35726014 35760124
36507124 36705421 36715402 37412650
37516042 37516402 37615042 37615240
37615402 40321675 40326715 42051376
42063715 42063751 42103675 42107635
42153076 42176053 42673015 42703615
42751306 43012675 43016752 43025176
43026715 43027615 43027651 43062715
43527106 43572106 43602715 43620751
43627015 43627051 45061732 45302176
45317206 45327106 45372106 46320715
46327015 46371052 46751230 47301625
47302651 47561203 50326471 50341276
50342176 50461273 50461732 50463721
50473612 50742631 52041376 52043176
52067341 52067413 52106734 52317406
52403176 52743106 52760341 53026741
53402176 53420176 53741206 53742106
54062713 54063172 54302176 54317206
54607123 54607132 54761320 56041723
56043712 56043721 56402713 56403721
56741203 60315472 60315724 60325471
60375412 60451273 60451372 60451723
60451732 60452371 60452731 60475312
60475321 60541273 60542173 60542731
60543172 60543271 60745312 62043751
62157340 62403715 63015472 63527401
64317025 64502371 64571230 64571320
64572130 65042173 65047321 67315204
67315402 67315420 67541230 70452613
70543612 72143650 72165340 72165430
72560134 73415206 73516420 73526410
73615402 73650412 74651023 74651230
75041623 75316240 75316420 75326140
76045213 76045312 76305412 76315420
76325410 76451032 76451230 76451320
76452130 76452310 76453120 76453210
76540312 76541230 76541320 76542130
```

#### D.2 Deep permutations

The 24 deep permutations produce a memory load dependency chain of depth 6.

```
02713456 03172456 04127356 05123746
06123475 10723456 17023456 20713456
27013456 30172456 37102456 40127356
47120356 50123746 57123046 60123475
67123405 67123450 72013456 73102456
74120356 75123046 76123405 76123450
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
- result: `0x973684176f8ee362`

#### F.2 Test2

- seed: `5468697320697320612074657374207365656420666f72206861736877780000`
- nonce: `123456`
- result: `0x401983bb07d69b07`

#### F.3 Test3

- seed: `4c6f72656d20697073756d20646f6c6f722073697420616d6574000000000000`
- nonce: `123456`
- result: `0x4af38d834a9a8d3d`

#### F.4 Test4

- seed: `4c6f72656d20697073756d20646f6c6f722073697420616d6574000000000000`
- nonce: `987654321123456789`
- result: `0x6a8a5514432e17a3`

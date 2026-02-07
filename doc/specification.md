# HashWX

This documents describes the HashWX algorithm. The document is structured as follows:

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

The VM has a total of 14 registers. The first 10 registers (R0-R9) are arithmetic registers and the remaining 4 registers (PC, BC, BF, MF) are control registers and flags.

#### 1.2.1 Arithmetic registers

The registers R0-R9 are used for arithmetic operations. Their size is 64 bits. Registers R0-R7 are read-write registers, while R8 and R9 are read-only registers used by the `RMCG` instruction.

#### 1.2.2 Control registers

##### 1.2.2.1 Program counter (PC)

This register holds the index of the next instruction in the program buffer to be executed. The value of the register is automatically incremented when an instruction starts executing. When the VM is restarted, the value of PC is set to 0.

##### 1.2.2.2 Branch counter (BC)

The branch counter holds the maximum number of branches the VM can take. Whenever a `BRANCH` instruction writes to the program counter, the BC register is decremented. When the value of BC reaches zero, no further branches can be taken.

##### 1.2.2.3 Branch flag (BF)

This register is a 1-bit flag that determines if a branch will be taken or not taken. The flag is set by the `RMCG` instruction.

##### 1.2.2.4 Memory flag (MF)

This register is a 1-bit flag that determines if the VM executes in the register operand mode (MF=0) or in the memory operand mode (MF=1). The flag is read-only.

### 1.3 Memory

The VM has 2048 bytes of memory. The memory is read-only and should be initialized before setting the Memory flag to 1.

### 1.4 Instruction set

#### 1.4.1 Instruction format

Every instruction in the program buffer consists of:

1. An opcode that determines what the instruction does.
2. A destination register `dst`, if applicable.
3. A source register `src`, if applicable.
4. An immediate value `imm`, if applicable. The maximum immediate size is 7 bits.

The actual source operand used by an instruction depends on the value of the Memory flag. If MF=0, the register value is used directly as the source operand. If MF=1, the value in the register (modulo 2048) is used to load an 8-byte aligned value from the memory.

#### 1.4.2 Instruction listing

* all arithmetic operations are performed modulo 2<sup>64</sup>
* `>>` denotes an arithmetic (signed) right shift
* `>>>` denotes a logical (unsigned) right shift
* `>>>>` denotes circular right shift (rotation)
* the source operand is evaluated as: `[src] = MF ? memory[src & 2040] : src`
* the RMCG instruction always uses a register source operand

*Table 1.4.2 - HashWX instructions*

|opcode|instruction|operation|dst|src|imm|
|-|-|-|-|-|-|
|0|MULOR|`dst=(dst\|imm)*[src]`|R0-R7|R0-R7|`1,9,33`|
|1|MULXOR|`dst=(dst^imm)*[src]`|R0-R7|R0-R7|`1,9,33`|
|2|MULADD|`dst=(dst+imm)*[src]`|R0-R7|R0-R7|`1,9,33`|
|3|RMCG|`dst=(dst*src)>>>>imm;BF=dst[5];`|R0-R7|R8-R9|`1-63`|
|4|XORROR|`dst=(dst>>>>imm)^[src]`|R0-R7|R0-R7|`1-63`|
|5|ADDROR|`dst=(dst>>>>imm)+[src]`|R0-R7|R0-R7|`1-63`|
|6|SUBROR|`dst=(dst>>>>imm)-[src]`|R0-R7|R0-R7|`1-63`|
|7|XORASR|`dst=(dst>>imm)^[src]`|R0-R7|R0-R7|`1-3`|
|8|ADDASR|`dst=(dst>>imm)+[src]`|R0-R7|R0-R7|`1-3`|
|9|SUBASR|`dst=(dst>>imm)-[src]`|R0-R7|R0-R7|`1-3`|
|10|XORLSR|`dst=(dst>>>imm)^[src]`|R0-R7|R0-R7|`1-3`|
|11|ADDLSR|`dst=(dst>>>imm)+[src]`|R0-R7|R0-R7|`1-3`|
|12|SUBLSR|`dst=(dst>>>imm)-[src]`|R0-R7|R0-R7|`1-3`|
|13|BRANCH|`if(BC && !BF){BC--;PC=0;}`|||
|14|HALT||||||

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

##### 1.4.2.14 BRANCH
This instruction performs a conditional branch. The branch is taken if BC is nonzero and BF is zero. If the branch is taken, the BC register is decremented by 1 and the VM jumps to the first instruction in the program buffer.

##### 1.4.2.15 HALT
This instruction stops the VM. No register values are affected.

## 2. HashWX instance generation

The HashWX instance generation procedure takes a 32-byte seed as input and produces a HashWX instance, which consists of 32 random programs for the VM and a 16-byte Siphash key.

### 2.1 Siphash keys

Siphash keys are used to initialize the Siphash generator, which is a pseudorandom number generator described in Appendix A. The 32-byte seed is split into 2 Siphash keys:

1. The first Siphash key is used to generate random 32 programs. The generator is initialized with a salt value of -1.
2. The second Siphash key is copied into the HashWX instance and is used during hash calculation.

### 2.2 Program structure

Each HashWX program consists of 10 instructions and has the following structure:

*Table 2.2.1 - HashWX program structure*

|index|opcode|arguments|
|-----|------|---------|
|0|RMCG|dst, src, imm|
|1|XAS*|dst, src, imm|
|2|MUL*|dst, src, imm|
|3|XAS*|dst, src, imm|
|4|MUL*|dst, src, imm|
|5|XAS*|dst, src, imm|
|6|MUL*|dst, src, imm|
|7|BRANCH|
|8|XAS*|dst, src, imm|
|9|HALT|

MUL* refers to one of the 3 MUL opcodes (0-2) and XAS* refers to one of the 9 XOR/ADD/SUB opcodes (4-12) from table 1.4.2.

### 2.3 Program generation

Each random program is generated from 16 pseudorandom 64-bit numbers output from the Siphash generator. In this section, these 16 numbers are referred to as `gen[0]` to `gen[15]`.

#### 2.3.1 Opcodes

The XAS instructions at indexes 1, 3, 5 and 8 can have one of 9 possible opcodes. These opcodes are selected from the lookup table 2.3.1.1 based on the least significant 32 bits of `gen[0]`, `gen[2]`, `gen[4]` and `gen[6]`.

*Table 2.3.1.1 - XAS lookup table*

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

The MUL instructions at indexes 2, 4 and 6 can have one of 3 possible opcodes. These opcodes are selected based on the least significant 32 bits of `gen[1]`, `gen[3]` and `gen[5]` as shown in table 2.3.1.2

*Table 2.3.1.2 - MUL lookup table*

|`(gen[i] & 0xffffffff) % 3`|opcode|
|-----|------|
|0|MULOR|
|1|MULXOR|
|2|MULADD|

#### 2.3.2 Destinations

A total of 8 instructions in each program need a destination register (indexes 0-6 and 8). Destinations are selected by taking a random permutation of the registers R0-R7. The random permutation is produced using the Fisher-Yates shuffle with the upper 32 bits of `gen[0]` to `gen[6]` used for index selection. The Fisher-Yates shuffle algorithm is described in Appendix B.

#### 2.3.3 Sources

A total of 7 instructions in each program need a source register from the range R0-R7 (indexes 1-6 and 8). These sources are selected as one of 625 permitted permutations of the destinations. The source permutation index is calculated as `gen[7] % 625`. The permitted source permutations are listed in Appendix C.

The RMCG instruction at index 0 can take either R8 or R9 as its source operand. This choice is determined by the value `gen[7] % 2` according to Table 2.3.3.1.

*Table 2.3.3.1 - RMCG src register*

|`gen[7] % 2`|register|
|-----|------|
|0|R8|
|1|R9|

#### 2.3.4 Immediates

A total of 8 instructions in each program need an immediate value (indexes 0-6 and 8). Immediates are selected from `gen[8]` to `gen[15]` modulo the number of possible immediate values permitted for a given opcode. 

## 3. HashWX calculation

Given a HashWX instance and a nonce value, the hash value calculation consists of the following phases:

1. Register initialization phase
2. Memory write phase
3. Memory read phase
4. Finalization phase

The whole algorithm in pseudocode is described in Appendix D.

### 3.1 Register initialization

A Siphash generator is initialized using the Siphash key from the HashWX instance and the nonce value as the salt. The first 8 random numbers from the generator are used to initialize the registers R0-R7.

The register R8 is initialized as `R8 = (R4 & -8) | 3` and the register R9 is initialized as `R9 = (R7 & -8) | 5`.

### 3.2 Memory write phase

At the beginning of this phase, the VM Branch counter register is set to 32 and the Memory flag is set to 0. Then the 32 HashWX programs are executed sequentially. After each program halts, the register values R0-R7 are written to the VM memory buffer, filling it from the end (i.e. the value of R0 after the first program halts is written at memory offset 2040 and the value of R7 after the 32nd program halts is written at offset 0). No register values are modified between the 32 VM executions apart from the Program counter, which is reset to 0 at the beginning of each program.

### 3.3 Memory read phase

At the beginning of this phase, the VM Branch counter register is reset to 32 and the Memory flag is set to 1. Then the 32 HashWX programs are again executed sequentially. No register values are modified between the 32 VM executions apart from the Program counter, which is reset to 0 at the beginning of each program.

### 3.4 Finalization phase

The final hash value is calculated from the values of registers R0-R8 after the memory read phase. First, two SipRounds are executed to mix registers R0-R3 and R4-R7 (SipRound is described in Appendix A.1). The final hash value is `R3 ^ R7 ^ R9`.

## Appendix

### A. Siphash generator

Siphash generator is a custom pseudorandom number generator. The internal state of the generator is represented by four 64-bit integers `v0-v3`.

#### A.1 SipRound

SipRound is the basic building block of the generator. It mixes four 64-bit integers as follows:

```
function sipround(v1, v2, v3, v4):
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
    return (v1, v2, v3, v4)
```

#### A.2 Generator initialization

The Siphash generator is initialized with a 16-byte Siphash key and an 64-bit salt. The key is interpreted as two 64-bit integers `k0` and `k1` (in little endian format). The initial state is generated as follows:

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

The generator outputs its internal state in the order `v0`, `v1`, `v2` and `v3`. When the internal state is exhausted, a mix step is performed to refresh the state using the initial key values:

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

### B. Fisher-Yates shuffle

Fisher-Yates shuffle generates a random permutation of N elements using N-1 random numbers. In the case of HashWX, we have N=8 and the random numbers are `gen[0]` to `gen[6]`.

```
function fisher_yates_shuffle(gen)
    dst[0] = 0
    for i in [1..7]:
        dst[i] = i
        j = gen[i - 1] % (i + 1)
        dst[i] = dst[j]
        dst[j] = i
    return dst
```

### C. Source permutations

The permitted 625 source permutations are listed below (lexicographically sorted). The first element of the permutation is unused as there are only 7 source registers per program. The source permutation is a permutation of the destinations, so it needs to be combined with the destination permutation to get actual register indexes.

```
03145672 03146752 03147652 03415672 03416752
03417652 03451672 03456172 03456712 03457612
03461752 03465172 03465712 03467152 03471652
03475612 03476152 03541672 03546172 03546712
03547612 03641752 03645172 03645712 03647152
03741652 03745612 03746152 04153672 04156372
04156732 04157632 04163752 04165372 04165732
04167352 04173652 04175632 04176352 04315672
04316752 04317652 04351672 04356172 04356712
04357612 04361752 04365172 04365712 04367152
04371652 04375612 04376152 04513672 04516372
04516732 04517632 04561372 04561732 04563172
04563712 04567132 04567312 04571632 04573612
04576132 04576312 04613752 04615372 04615732
04617352 04651372 04651732 04653172 04653712
04657132 04657312 04671352 04673152 04675132
04675312 04713652 04715632 04716352 04751632
04753612 04756132 04756312 04761352 04763152
04765132 04765312 05143672 05146372 05146732
05147632 05341672 05346172 05346712 05347612
05413672 05416372 05416732 05417632 05461372
05461732 05463172 05463712 05467132 05467312
05471632 05473612 05476132 05476312 05641372
05641732 05643172 05643712 05647132 05647312
05741632 05743612 05746132 05746312 06143752
06145372 06145732 06147352 06341752 06345172
06345712 06347152 06413752 06415372 06415732
06417352 06451372 06451732 06453172 06453712
06457132 06457312 06471352 06473152 06475132
06475312 06541372 06541732 06543172 06543712
06547132 06547312 06741352 06743152 06745132
06745312 07143652 07145632 07146352 07341652
07345612 07346152 07413652 07415632 07416352
07451632 07453612 07456132 07456312 07461352
07463152 07465132 07465312 07541632 07543612
07546132 07546312 07641352 07643152 07645132
07645312 23145670 23146750 23147650 23415670
23416750 23417650 23451670 23456170 23456710
23457610 23461750 23465170 23465710 23467150
23471650 23475610 23476150 23541670 23546170
23546710 23547610 23641750 23645170 23645710
23647150 23741650 23745610 23746150 24153670
24156370 24156730 24157630 24163750 24165370
24165730 24167350 24173650 24175630 24176350
24315670 24316750 24317650 24351670 24356170
24356710 24357610 24361750 24365170 24365710
24367150 24371650 24375610 24376150 24513670
24516370 24516730 24517630 24561370 24561730
24563170 24563710 24567130 24567310 24571630
24573610 24576130 24576310 24613750 24615370
24615730 24617350 24651370 24651730 24653170
24653710 24657130 24657310 24671350 24673150
24675130 24675310 24713650 24715630 24716350
24751630 24753610 24756130 24756310 24761350
24763150 24765130 24765310 25143670 25146370
25146730 25147630 25341670 25346170 25346710
25347610 25413670 25416370 25416730 25417630
25461370 25461730 25463170 25463710 25467130
25467310 25471630 25473610 25476130 25476310
25641370 25641730 25643170 25643710 25647130
25647310 25741630 25743610 25746130 25746310
26143750 26145370 26145730 26147350 26341750
26345170 26345710 26347150 26413750 26415370
26415730 26417350 26451370 26451730 26453170
26453710 26457130 26457310 26471350 26473150
26475130 26475310 26541370 26541730 26543170
26543710 26547130 26547310 26741350 26743150
26745130 26745310 27143650 27145630 27146350
27341650 27345610 27346150 27413650 27415630
27416350 27451630 27453610 27456130 27456310
27461350 27463150 27465130 27465310 27541630
27543610 27546130 27546310 27641350 27643150
27645130 27645310 42153670 42156370 42156730
42157630 42163750 42165370 42165730 42167350
42173650 42175630 42176350 42315670 42316750
42317650 42351670 42356170 42356710 42357610
42361750 42365170 42365710 42367150 42371650
42375610 42376150 42513670 42516370 42516730
42517630 42561370 42561730 42563170 42563710
42567130 42567310 42571630 42573610 42576130
42576310 42613750 42615370 42615730 42617350
42651370 42651730 42653170 42653710 42657130
42657310 42671350 42673150 42675130 42675310
42713650 42715630 42716350 42751630 42753610
42756130 42756310 42761350 42763150 42765130
42765310 43156702 43157602 43165702 43175602
43516702 43517602 43561702 43567102 43571602
43576102 43615702 43651702 43657102 43675102
43715602 43751602 43756102 43765102 45163702
45167302 45173602 45176302 45316702 45317602
45361702 45367102 45371602 45376102 45613702
45617302 45671302 45673102 45713602 45716302
45761302 45763102 46153702 46157302 46175302
46315702 46351702 46357102 46375102 46513702
46517302 46571302 46573102 46715302 46751302
46753102 47153602 47156302 47165302 47315602
47351602 47356102 47365102 47513602 47516302
47561302 47563102 47615302 47651302 47653102
62143750 62145370 62145730 62147350 62341750
62345170 62345710 62347150 62413750 62415370
62415730 62417350 62451370 62451730 62453170
62453710 62457130 62457310 62471350 62473150
62475130 62475310 62541370 62541730 62543170
62543710 62547130 62547310 62741350 62743150
62745130 62745310 63145702 63415702 63451702
63457102 63475102 63541702 63547102 63745102
64153702 64157302 64175302 64315702 64351702
64357102 64375102 64513702 64517302 64571302
64573102 64715302 64751302 64753102 65143702
65147302 65341702 65347102 65413702 65417302
65471302 65473102 65741302 65743102 67145302
67345102 67415302 67451302 67453102 67541302
67543102 72143650 72145630 72146350 72341650
72345610 72346150 72413650 72415630 72416350
72451630 72453610 72456130 72456310 72461350
72463150 72465130 72465310 72541630 72543610
72546130 72546310 72641350 72643150 72645130
72645310 73145602 73415602 73451602 73456102
73465102 73541602 73546102 73645102 74153602
74156302 74165302 74315602 74351602 74356102
74365102 74513602 74516302 74561302 74563102
74615302 74651302 74653102 75143602 75146302
75341602 75346102 75413602 75416302 75461302
75463102 75641302 75643102 76145302 76345102
76415302 76451302 76453102 76541302 76543102
```

### D. HashWX algorithm pseudocode

```
function hashwx_execute(self, nonce):
    gen = siphash_generator(self.key, nonce)
    vm = hashwx_vm()
    for i in [0..7]:
        vm.r[i] = gen.next()
    vm.r[8] = (vm.r[4] & -8) | 3
    vm.r[9] = (vm.r[7] & -8) | 5
    vm.bc = 32
    vm.mf = 0
    for i in [0..31]:
        vm.program = self.program[i]
        vm.execute()
        for j in [0..7]:
            vm.memory[2040-64*i-8*j] = vm.r[j]
    vm.bc = 32
    vm.mf = 1
    for i in [0..31]:
        vm.program = self.program[i]
        vm.execute()
    vm.r[0], vm.r[1], vm.r[2], vm.r[3] = sipround(vm.r[0], vm.r[1], vm.r[2], vm.r[3])
    vm.r[4], vm.r[5], vm.r[6], vm.r[7] = sipround(vm.r[4], vm.r[5], vm.r[6], vm.r[7])
    return vm.r[3] ^ vm.r[7] ^ vm.r[9]
```

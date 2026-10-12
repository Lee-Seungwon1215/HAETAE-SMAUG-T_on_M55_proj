# HAETAE Cortex-M55 assembly backend

This directory is additive. The preserved intrinsic implementation remains the
default and is selected with `HAETAE_*_BACKEND=intrinsic`. No source below
includes `arm_mve.h` or calls an intrinsic wrapper for vector computation.

Backend selectors are orthogonal per family:

```text
HAETAE_NTT_BACKEND=intrinsic|asm_hand|asm_slothy
HAETAE_FFT_BACKEND=intrinsic|asm_hand|asm_slothy
HAETAE_SAMPLER_BACKEND=intrinsic|asm_hand|asm_slothy
```

Candidate IDs are part of the Make build path. `hand/` is authored assembly,
`candidates/` retains alternative dataflows, and `slothy/output/` is generated
and must not be edited manually. Public assembly symbols deliberately retain
the existing `haetae_*_mve` ABI because exactly one backend is linked.

## ABI policy

- Thumb-2 performs address/control work and MVE performs vector work.
- q0-q3 are caller-saved scratch. Any use of q4-q7 preserves d8-d15 as
  required by AAPCS32.
- SP stays 8-byte aligned at every public call boundary.
- A function's source comment records its register map, frame, table layout,
  access pattern, and constant-time argument.

## Qualified implementations

All public symbols use the existing `haetae_*_mve` ABI. Assembly files record
their register map, frame, table format, and fixed access pattern next to the
implementation. Tables live in their own garbage-collectable sections so an
unselected candidate does not inflate the linked image.

### Sampler and 17-byte loader

- `SAM-H-LOOP`: fixed eight-iteration loop, eight aligned post-increment CDT
  loads, 46-byte function, no frame.
- `SAM-H-UNROLL`: the same accesses fully unrolled, 152-byte function, no
  frame.
- `SAM-H-LOAD17`: a shared 62-byte AAPCS result helper. It performs one exact
  16-byte MVE load and one scalar byte load, and never reads byte 17.
- `SAM-S-MASK-U1/U2`: complete Slothy schedules of an equivalent arithmetic
  comparison. Widening the u16 lanes and computing
  `(threshold - random) >> 31` produces one exactly when `random > threshold`,
  without a predicate comparison.

All candidates use `tables/sampler_cdt.S`, pass direct CDT/loader tests and
the all-mode 1,000-case differential gate. Raw results are in
`measurements/qemu/20260810-haetae-asm-slothy-01/` and
`measurements/qemu/20260811-haetae-sampler-fft-final-slothy-01/`.

The CDT comparison loop was offered to pinned Slothy commit
`241093fa5ddc1e21f75dd9f6588733bc3097872b`; its Arm_v81M parser rejects
the direct `vcmp.u16` form. The arithmetic-mask rewrite is fully generated and
selectable as `mask_u1` or `mask_u2`, but order-balanced STM32N657 measurements
make the complete sampler 4.85--7.94% slower than `SAM-H-UNROLL`. Therefore the
qualified final selection remains the handwritten unrolled sampler.

### NTT and modular arithmetic

- `NTT-H-GENERIC-VLD42`: direct q=64513 transform used as the first semantic
  bridge.
- `NTT-H-PAIRED-VLD4-U1/U2/U4`: paired 12/34/56/78 layers, matching root
  tables and increasingly unrolled lower-layer traversal. U4 is the fastest
  all-handwritten forward transform; U1 is retained for lower code size.
- `NTT-S-PAIRED-VLD4-U1/U2/U4-HIER`: Slothy schedules of the normalized
  handwritten forward loops. The hierarchical candidate uses U4 on the two
  16-iteration upper loops, U2 on the four-iteration `(32,16)` loop, and
  bounded non-SWP scheduling on `(2,1)`. Stage isolation shows that layers-12
  and layers-56 U4 save only four loop-control instructions each while adding
  448/496 function bytes and triggering address-sensitive cache regressions.
  The selected `paired_vld4_u2_l34_hier` keeps the compact U2 footprint and
  takes only the 60-to-56-cycle layers-34 schedule. It improves all three
  parameter sets on STM32N657. U4-HIER remains selectable negative evidence.
- `HAETAE_NTT_INVERSE_VARIANT=slothy_u1`: a complete callable inverse NTT.
  All four reverse layer-pair loops and the fused final scale are present.
  `r1` and `r10` are reserved during scheduling because they carry state
  across loop regions that a loop-local Slothy check cannot see.
- `ARITH-H-D1/D2`: pointwise multiplication, generic/precentered/u16
  accumulation, Barrett tight, reduce2q, freeze2q, and freeze. D2 remains the
  arithmetic proof/dataflow used by the accumulated and reduction interfaces.
- `HAETAE_NTT_POINTWISE_VARIANT=slothy_lowmem`: the complete plain pointwise
  public leaf scheduled by Slothy. It reserves q4-q7, uses only caller-saved
  MVE registers, and was selected over the software-pipelined U1 output to
  avoid 72 bytes of extra stack read/write traffic per call.
- `HAETAE_NTT_POINTWISE_ACC_VARIANT=slothy_block2`: complete Slothy-generated
  generic, precentered, and compact-u16 accumulated-product functions. The
  handwritten D2 bodies remain selectable with `hand`. The selected block2
  input processes eight adjacent coefficients per outer iteration so every
  32-byte cache line is consumed before switching to the next 1 KiB-spaced
  term. It also keeps public `terms` in `r11`, removing one stack load per
  outer iteration. Its local frame is 104 bytes versus 72 for hand/lowmem.
  The attempted software-pipelined block2 output is retained as rejected
  evidence: it stalled before `SELFTEST` on QEMU and is not a build selector.
- `HAETAE_NTT_BARRETT_VARIANT=slothy_u2_nostack`: complete two-vector
  software-pipelined Barrett leaf. It preserves the return address in unused
  caller-saved `r2`, so its 256-coefficient call has exactly 64 vector loads,
  64 vector stores, and no stack traffic. `hand`, `hand_dls`, `slothy_u1`, and
  the first stack-using `slothy_u2` remain selectable.
- `HAETAE_NTT_CENTERED_VARIANT=slothy_mask_u2`: complete U2 schedules for
  `reduce2q`, `freeze2q`, and `freeze`. The arithmetic-mask input is
  constant-time and equivalent to the handwritten predicate corrections. It
  was necessary because the pinned Slothy parser rejects direct `vcmp.s32`.
  The predicate-based hand candidates and U1 mask schedule remain selectable.

The roots in `tables/ntt_roots.S` are generated from the HAETAE reference
constants by `tools/slothy_haetae/generate_ntt_tables.py`. They are not
Dilithium constants.

### FFT, Q16 helpers, and squared magnitude

`fft/hand/fft_soa_reuse.S` is a handwritten exact-Q16 implementation. It
converts interleaved complex input to SoA once, fuses the first two unit-root
layers, reuses each twiddle pair across blocks, and provides both standalone
FFT and fused FFT+squared-magnitude symbols. The fused path consumes its last
layer directly and avoids the compiler path's scratch round trip. It also owns
the wide-scalar and `vqrdmulh` Q16 test helpers.

`FFT-S-SOA-REUSE-U2` schedules the standalone and fused boundary-conversion
loops with `constraints.st_ld_hazard=True` and is retained as the previous
baseline. `FFT-S-SOA-REUSE-VECSQ-U1` replaces the final scalar-lane extraction
with exact vector Q16 squaring: low and signed-high products reconstruct
`(x*x + 32768) >> 16` in each lane, and vector loads/stores accumulate the
four squares. Its 65-instruction loop schedules in 68 modeled M55 cycles and
contains no final `vmov r0,sN`. The original bounded U2 searches timed out,
but hierarchical split/halving scheduling solves the 130-instruction kernel.
Because the post-initialization trip count is 31, the selected input schedules
an even 30-iteration U2 prefix and retains one fixed public tail. The resulting
two-iteration kernel models at 136 cycles and passes all-mode runtime testing.
It replaces U1 after order-balanced board wins of 1.023%, 0.033%, and 0.081%
for HAETAE2/3/5. A correct head-peeled U2 structure remains unselected because
its board result was not uniform. U4 layout and scheduled first-two-layer
experiments are retained because they passed Slothy self-check but failed
runtime preflight.

The U4/U2 recovery solver and QEMU evidence is under
`measurements/qemu/20260811-haetae-u4-u2-recovery-01/`; order-balanced board
results and final gates are under
`measurements/stm32n657/20260811-haetae-u4-u2-recovery-01/`.

Twiddles in `tables/fft_twiddles.S` are generated from the HAETAE reference by
`tools/slothy_haetae/generate_fft_tables.py` and include the exact +65536 lane
repair required by the Q16 contract.

## Selected QEMU and STM32N657 configurations

The updated qualified winner uses Slothy for forward NTT, the complete inverse
NTT, plain pointwise, all three pointwise-accumulation leaves, Barrett,
centered reductions, FFT boundary loops, and the complete final FFT squared
magnitude. Only the sampler retains its qualified handwritten body because its
correct Slothy mask schedules were slower on hardware:

```sh
make ALGORITHM=haetae2 \
  HAETAE_NTT_BACKEND=asm_slothy \
  HAETAE_NTT_ASM_VARIANT=paired_vld4_u2_l34_hier \
  HAETAE_NTT_ARITH_VARIANT=d2 \
  HAETAE_NTT_INVERSE_VARIANT=slothy_u1 \
  HAETAE_NTT_POINTWISE_VARIANT=slothy_lowmem \
  HAETAE_NTT_POINTWISE_ACC_VARIANT=slothy_block2 \
  HAETAE_NTT_BARRETT_VARIANT=slothy_u2_nostack \
  HAETAE_NTT_CENTERED_VARIANT=slothy_mask_u2 \
  HAETAE_FFT_BACKEND=asm_slothy \
  HAETAE_FFT_ASM_VARIANT=soa_reuse_vecsq_u2_hier \
  HAETAE_SAMPLER_BACKEND=asm_hand \
  HAETAE_SAMPLER_ASM_VARIANT=unroll diff
```

The independently authored all-handwritten configuration is:

```sh
make ALGORITHM=haetae2 \
  HAETAE_NTT_BACKEND=asm_hand \
  HAETAE_NTT_ASM_VARIANT=paired_vld4_u4 \
  HAETAE_NTT_ARITH_VARIANT=d2 \
  HAETAE_FFT_BACKEND=asm_hand \
  HAETAE_FFT_ASM_VARIANT=soa_reuse_sep \
  HAETAE_SAMPLER_BACKEND=asm_hand \
  HAETAE_SAMPLER_ASM_VARIANT=unroll diff
```

Replace `haetae2` with `haetae3` or `haetae5` for the other parameter sets.
The lower-size hardware candidate substitutes handwritten NTT U1 and sampler
`loop`; it is not a third generated implementation.

Example:

```sh
make ALGORITHM=haetae2 HAETAE_SAMPLER_BACKEND=asm_hand \
  HAETAE_SAMPLER_ASM_VARIANT=unroll diff
```

QEMU SysTick/icount numbers are relative screening values, not physical
Cortex-M55 cycles.

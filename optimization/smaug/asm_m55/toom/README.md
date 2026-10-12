# SMAUG-T Cortex-M55 Toom-Cook assembly

This directory preserves three independently selectable implementations of
the active `toom4_outer32` polynomial-multiplication path:

- `intrinsic`: the pre-existing MVE intrinsic implementation in
  `optimization/smaug/common/smaugt_toom_mve.c`.
- `hand/`: handwritten M55/MVE assembly. This is a new implementation, not a
  compiler listing or a line-by-line transcription of the intrinsics.
- `slothy/`: the handwritten assembly expressed as bounded scheduling regions,
  plus the generated Cortex-M55 schedule and solver logs.

The handwritten design evaluates the seven Toom points with MVE, decomposes
each 64x64 leaf into three 32x32 products, keeps 32 right-hand coefficients in
`q4`--`q7` throughout each convolution, and fuses cached matrix accumulation
into the leaf composition. The fused path removes the temporary 127-element
product and its load/store round trip. Interpolation uses widened 32-bit MVE
arithmetic and the final reduction returns to 16-bit ring coefficients.

Slothy now covers all 15 marked regions. The local Armv8.1-M frontend parses
`vpst`, zeroing tail-predicated `vldrht`, tail-predicated `vstrht`, and
width-suffixed scalar immediate additions. The Cortex-M55 target assigns their
execution units, throughput, and latency, and adds an architectural constraint
that keeps every `vpst` immediately adjacent to its original predicated
load/store. Parser/model unit tests cover operand semantics, memory
classification, target timing lookup, and the adjacency constraint.

The final `interp_store` contains 42 instructions. Slothy proves the exact
42-cycle bound with zero stalls, so the scheduled instruction stream is the
same as the handwritten stream. `slothy_fused_tailpred_stable_u1` applies that
last pass on top of the measured 14-region winner. Its 1,560-byte `.text` is
byte-identical to `slothy_fused_u1`, while its two reports cumulatively record
15/15 successful regions. This stable 15/15 candidate is the default.

`slothy_fused_tailpred_u1` is a separately preserved full regeneration from
the handwritten input. It also reports 15/15 success and passes all QEMU
correctness gates, but equal-cost solver choices change other region schedules.
It remains non-default until a new physical-board comparison is available.

The local target model also adds the missing contiguous `vstrh` forms.
Replacing them with `vstrw` is not valid for the two-byte-sliding convolution:
odd iterations are only halfword-aligned and fault on M55/QEMU when expressed
as word stores.

## Build selections

The measured STM32N657 winner is the default:

```sh
make run ALGORITHM=smaugt1 IMPLEMENTATION=optimization ITERATIONS=10
```

The preserved alternatives are:

```sh
make run ALGORITHM=smaugt1 IMPLEMENTATION=optimization ITERATIONS=10 \
  SMAUGT_TOOM_BACKEND=intrinsic \
  KECCAK_X4_BACKEND=intrinsic

make run ALGORITHM=smaugt1 IMPLEMENTATION=optimization ITERATIONS=10 \
  SMAUGT_TOOM_BACKEND=asm_hand SMAUGT_TOOM_ASM_VARIANT=hand_fused \
  KECCAK_X4_BACKEND=intrinsic

make run ALGORITHM=smaugt1 IMPLEMENTATION=optimization ITERATIONS=10 \
  SMAUGT_TOOM_BACKEND=asm_slothy \
  SMAUGT_TOOM_ASM_VARIANT=slothy_fused_tailpred_u1
```

Regenerate the complete 15-region candidate with:

```sh
.venv-slothy/bin/python tools/slothy_haetae/schedule_smaugt_toom.py \
  optimization/smaug/asm_m55/toom/slothy/input/toom_outer32_fused.s \
  optimization/smaug/asm_m55/toom/slothy/output/toom_outer32_fused_tailpred_u1.S \
  optimization/smaug/asm_m55/toom/slothy/logs_tailpred \
  optimization/smaug/asm_m55/toom/slothy/report_tailpred.json --timeout 60
```

Regenerate only the stable final pass with:

```sh
.venv-slothy/bin/python tools/slothy_haetae/schedule_smaugt_toom.py \
  optimization/smaug/asm_m55/toom/slothy/output/toom_outer32_fused_u1.S \
  optimization/smaug/asm_m55/toom/slothy/output/toom_outer32_fused_tailpred_stable_u1.S \
  optimization/smaug/asm_m55/toom/slothy/logs_tailpred_stable \
  optimization/smaug/asm_m55/toom/slothy/report_tailpred_stable.json \
  --timeout 60 --regions interp_store
```

`SMAUGT_TOOM_ASM_COMPONENT` can select `evaluate`, `multiply`, `fused`,
`interpolate`, or `reduce` for fault isolation. It is a diagnostic switch;
released builds use `all`.

Correctness and performance evidence is stored under:

- `measurements/qemu/20260812-smaugt-toom-asm-slothy-01/`
- `measurements/qemu/20260813-smaugt-slothy-tailpred-01/`
- `measurements/stm32n657/20260812-smaugt-toom-asm-slothy-01/`

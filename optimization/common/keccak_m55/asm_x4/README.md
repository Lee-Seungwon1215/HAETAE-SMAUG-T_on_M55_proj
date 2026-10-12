# HAETAE Keccak/SHAKE x4 Cortex-M55 backend

This directory preserves the handwritten and Slothy-generated replacements for
the shared MVE-intrinsic Keccak/SHAKE x4 dependency used by HAETAE2, HAETAE3,
and HAETAE5. It does not overwrite the incumbent intrinsic sources.

## Implementations

| Build selector | Permutation | SHAKE128 x4 wrapper | Status |
|---|---|---|---|
| `intrinsic` | MVE intrinsic C | MVE intrinsic C | reference baseline |
| `asm_hand/hand_v1` | handwritten M55 assembly | handwritten scalar pack/unpack | qualified candidate |
| `asm_hand/hand_gather_v2` | handwritten M55 assembly | handwritten MVE gather/scatter | qualified candidate |
| `asm_slothy/slothy_gather_u2` | handwritten M55 assembly | Slothy U2 gather/scatter | qualified full-assembly candidate |
| `hybrid_slothy/slothy_gather_u2` | MVE intrinsic C | Slothy U2 gather/scatter | previous hardware winner |
| `asm_slothy/slothy_gather_u2_fullperm_u1` | full-stage Slothy, Chi U1 | Slothy U2 gather/scatter | qualified experiment |
| `asm_slothy/slothy_gather_u2_fullperm_u2` | full-stage Slothy, Chi U2 | Slothy U2 gather/scatter | qualified experiment |
| `asm_slothy/slothy_gather_u2_fullperm_handchi` | Slothy Theta/Rho-Pi/Iota + handwritten Chi | Slothy U2 gather/scatter | qualified physical baseline |
| `asm_slothy/slothy_gather_u2_fullperm_chi2straight` | Slothy full stages + straight Chi, 2 rows/region | Slothy U2 gather/scatter | qualified physical runner-up |
| `asm_slothy/slothy_gather_u2_fullperm_chi5straight` | Slothy full stages + straight Chi, 5 rows/region | Slothy U2 gather/scatter | previous HAETAE2/3/5 hardware winner |
| `asm_slothy/slothy_gather_u2_fullperm_chi5_allpaths` | selected full permutation + Slothy zero/import/export | Slothy bulk, unaligned, padding, and byte tails | selected complete-coverage HAETAE2/3/5 winner |

The handwritten permutation implements Theta, Rho/Pi, Chi, and Iota directly;
it is not compiler output or a line-by-line intrinsic disassembly. Its public
state remains the existing 800-byte structure-of-arrays layout: 25 vectors of
low 32-bit halves followed by 25 vectors of high halves, with one independent
Keccak instance in each vector lane.

The SHAKE wrapper keeps the existing ten-argument ABI, handles exact public
byte lengths, and retains unaligned-safe fallback paths. For aligned full
words, its gather/scatter path loads or stores all four streams with one MVE
memory instruction. For the representative 34-byte input and 168-byte output,
the bulk-path memory-instruction count falls from 258 to 108 (58.1%). The same
number of data bytes is transferred; the reduction is in issued memory and
address-generation instructions.

## Slothy layout and rules

- `hand/`: handwritten source of truth.
- `slothy/input/`: loop-shaped inputs prepared for the pinned M55 model.
- `slothy/output/`: generated candidates. Do not hand-edit these files.
- `measurements/qemu/20260811-haetae-keccak-x4-asm-slothy-01/solver/`:
  complete commands, solver output, and exit status.

The selected wrapper is generated in two passes from
`slothy/input/fips202x4_gather_u2_even.s`: first
`m55_keccakx4_absorb_gather_loop`, then
`m55_keccakx4_squeeze_scatter_loop`. Both passes enable software-pipelining U2
and retain `constraints.st_ld_hazard=True`. The SHAKE128 rate contains 21
64-bit lanes, so the input consumes one odd lane before the even U2 prefix.
Without that explicit public tail shape, a syntactically valid U2 schedule
silently omitted lane 21 and failed the aligned boundary preflight.

The preserved U1 wrapper is intentionally not a Makefile selector. Its
software-pipeline preamble is invalid for a one-iteration full-word loop and
fails the 7-byte-input/8-byte-output aligned boundary case. It is retained only
as negative evidence.

Chi-row U1 and U2 outputs are selectable through
`slothy_gather_u2_chi_u1` and `slothy_gather_u2_chi_u2`. They pass correctness,
but QEMU measures 642 and 639 ticks per x4 permutation versus 624 for the
handwritten permutation, so neither is selected.

The follow-up full-round campaign schedules Theta as seven straight-line
regions, Rho/Pi as six four-step regions, Iota as one region, and Chi as
complete U1/U2 loops. Every Rho/Pi boundary explicitly preserves the carried
`q0/q1` pair, and Chi reserves `r14` because LR owns the low-overhead-loop
state. Both complete U1/U2 functions pass HAETAE2/3/5 qualification. Their
looped Chi remains slower, so the generated
`slothy_gather_u2_fullperm_handchi` candidate mechanically selects the
handwritten unrolled Chi while keeping the other generated schedules. It is
fully assembly and wins the QEMU screen at 621 permutation ticks versus 624
handwritten and 709 intrinsic. Physical-board selection is pending because no
debug probe was visible during the campaign. Full evidence is in
`measurements/qemu/20260812-keccak-x4-full-slothy-01/REPORT.md`.

The follow-up removes the remaining handwritten scheduling decision from Chi.
`expand_keccakx4_chi.py` emits all ten concrete rows as either five 2-row
regions or two 5-row regions. Slothy then renames registers and interleaves
independent rows without adding a runtime loop. The 2-row form models at
5x43=215 cycles; the overlapping split-heuristic 5-row form models at
2x107=214. Both measure 621 permutation and 641 SHAKE ticks in QEMU, exactly
tying the handwritten-Chi mixture, and the 5-row representative passes the
HAETAE2/3/5 differential gates. Four order-balanced STM32N657 sets then select
the 5-row form: physical permutation medians improve 4.17--4.20% over the
handwritten-Chi mixture and 12.84--13.83% over the previous hybrid. Full board
evidence is in
`measurements/stm32n657/20260812-keccak-x4-chi-straight-01/`.

The final all-path pass closes the remaining data-flow gaps around that
winner. Six zero/import/export regions and 22 SHAKE wrapper regions are now
Slothy-generated. In particular, the 1--7 byte absorb tail uses direct MVE
byte gathers instead of scalar stack packing; this is both schedulable and
0.55--0.69% faster for the isolated SHAKE x4 call on STM32N657. Calls,
branches, labels, and ABI `push`/`pop` instructions remain fixed structural
boundaries: the pinned frontend rejects them as region instructions, so they
are not unscheduled arithmetic or memory kernels. Rejected control-boundary
probes and the complete board comparison are preserved under
`measurements/{qemu,stm32n657}/20260812-keccak-x4-all-slothy-*/`.

## Selected build and reproduction

The final hardware-selected Keccak configuration is:

```sh
make IMPLEMENTATION=optimization ALGORITHM=haetae2 \
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
  HAETAE_SAMPLER_ASM_VARIANT=unroll \
  KECCAK_X4_BACKEND=asm_slothy \
  KECCAK_X4_ASM_VARIANT=slothy_gather_u2_fullperm_chi5_allpaths build
```

Change `ALGORITHM` to `haetae3` or `haetae5` for the other parameter sets.
Add `DIFF_TEST=1 run` to execute the 1,000-case reference differential gate.
This physical selection is qualified for HAETAE2/3/5; keep explicit selectors
for algorithms outside that measured scope.

The complete result and exact log paths are in
`measurements/stm32n657/20260812-keccak-x4-all-slothy-bytegather-01/REPORT.md`.

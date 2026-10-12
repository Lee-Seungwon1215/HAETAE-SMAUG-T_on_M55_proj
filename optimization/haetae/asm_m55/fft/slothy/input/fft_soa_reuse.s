/* SPDX-License-Identifier: MIT */

/*
 * FFT-H-SOA-REUSE
 *
 * Handwritten fixed-size HAETAE Q16 FFT for Cortex-M55.  The input/output ABI
 * remains 256 interleaved complex int32 values.  A 2 KiB stack workspace
 * converts that boundary once to SoA.  Layers 1+2 are fused without rounded
 * products.  Layers 3..8 traverse twiddle groups outside coefficient blocks,
 * so each pair of packed vector twiddles is loaded once per layer instead of
 * once per block.  The table stores y<<15 for the exact VQRDMULH Q16 form.
 *
 * Register map in the layer kernels:
 *   r8/r9   real/imaginary SoA bases
 *   r10     sequential layer twiddle stream
 *   r11     public k byte offset
 *   r12     public block stride in bytes
 *   r4-r7   even-real/even-imag/odd-real/odd-imag pointers
 *   q0-q3   input/output values
 *   q4/q5   shifted real/imaginary twiddles
 *   q6/q7   complex-product temporaries
 *
 * q4-q7 are saved in full.  The core frame is 40 GPR + 64 MVE + 2176
 * workspace bytes.  Every branch, address, and loop count depends only on
 * public N=256 and the fixed layer.  The roots argument remains in the public
 * ABI; the audited common HAETAE2/3/5 roots are represented by the generated
 * packed table.
 */

.syntax unified
.thumb
.arch armv8.1-m.main
.arch_extension mve

/* ABI macros inlined for Slothy normalization. */
.macro HAETAE_MVE_FUNCTION name
  .section .text.\name, "ax", %progbits
  .align 2
  .global \name
  .type \name, %function
  .thumb_func
\name:
.endm
.macro HAETAE_MVE_END name
  .size \name, .-\name
.endm

.macro HAETAE_FFT_VECTOR_COMPUTE special
  vldrw.u32 q0, [r4]
  vldrw.u32 q1, [r5]
  vldrw.u32 q2, [r6]
  vldrw.u32 q3, [r7]
.if \special
  /* The +65536 real unit root cannot be represented by y<<15. */
  vmov r0, s8
  vmov r1, s12
.endif
  vqrdmulh.s32 q6, q2, q4
  vqrdmulh.s32 q7, q3, q5
  vsub.u32 q6, q6, q7
  vqrdmulh.s32 q7, q2, q5
  vqrdmulh.s32 q2, q3, q4
  vadd.u32 q7, q7, q2
.if \special
  vmov s24, r0
  vmov s28, r1
.endif
  vsub.u32 q2, q0, q6
  vadd.u32 q0, q0, q6
  vsub.u32 q3, q1, q7
  vadd.u32 q1, q1, q7
.endm

.macro HAETAE_FFT_VECTOR_BUTTERFLY special
  HAETAE_FFT_VECTOR_COMPUTE \special
  vstrw.32 q0, [r4]
  vstrw.32 q1, [r5]
  vstrw.32 q2, [r6]
  vstrw.32 q3, [r7]
.endm

/* Exact Q16 squared magnitude for two outputs, moved directly from Q lanes. */
.macro HAETAE_FFT_SQABS_LANE lane, left_r, left_i, right_r, right_i
  vmov r0, \left_r
  smull r2, r3, r0, r0
  adds r2, r2, #32768
  adc r3, r3, #0
  asrl r2, r3, #16
  vmov r0, \left_i
  smull r4, r5, r0, r0
  adds r4, r4, #32768
  adc r5, r5, #0
  asrl r4, r5, #16
  ldr r1, [r11, #(\lane * 4)]
  add r2, r2, r4
  add r2, r2, r1
  str r2, [r11, #(\lane * 4)]

  vmov r0, \right_r
  smull r2, r3, r0, r0
  adds r2, r2, #32768
  adc r3, r3, #0
  asrl r2, r3, #16
  vmov r0, \right_i
  smull r4, r5, r0, r0
  adds r4, r4, #32768
  adc r5, r5, #0
  asrl r4, r5, #16
  ldr r1, [r12, #(\lane * 4)]
  add r2, r2, r4
  add r2, r2, r1
  str r2, [r12, #(\lane * 4)]
.endm

.macro HAETAE_FFT_LAYER layer, half_bytes, stride_bytes, blocks, groups
  ldr r10, =haetae_asm_fft_layer\layer\()_twiddles
  movs r11, #0
  movw r12, #\stride_bytes

  /* First k-vector: repair the public unit-root lane in every block. */
  vldrw.u32 q4, [r10], #16
  vldrw.u32 q5, [r10], #16
  mov r4, r8
  mov r5, r9
  add r6, r4, #\half_bytes
  add r7, r5, #\half_bytes
  mov lr, #\blocks
  dls lr, lr
.Lfft_layer\layer\()_first_\@:
  HAETAE_FFT_VECTOR_BUTTERFLY 1
  add r4, r4, r12
  add r5, r5, r12
  add r6, r6, r12
  add r7, r7, r12
  le lr, .Lfft_layer\layer\()_first_\@

.if \groups > 1
  movs r11, #16
  mov r2, #(\groups - 1)
.Lfft_layer\layer\()_groups_\@:
  vldrw.u32 q4, [r10], #16
  vldrw.u32 q5, [r10], #16
  add r4, r8, r11
  add r5, r9, r11
  add r6, r4, #\half_bytes
  add r7, r5, #\half_bytes
  mov lr, #\blocks
  dls lr, lr
.Lfft_layer\layer\()_blocks_\@:
  HAETAE_FFT_VECTOR_BUTTERFLY 0
  add r4, r4, r12
  add r5, r5, r12
  add r6, r6, r12
  add r7, r7, r12
  le lr, .Lfft_layer\layer\()_blocks_\@
  adds r11, r11, #16
  subs r2, r2, #1
  bne .Lfft_layer\layer\()_groups_\@
.endif
.endm

HAETAE_MVE_FUNCTION haetae_mulrnd16_wide4
  push {r4-r8, lr}
  movs r7, #4
.Lmulrnd16_wide4_loop:
  ldr r3, [r1], #4
  ldr r6, [r2], #4
  smull r4, r5, r3, r6
  adds r4, r4, #32768
  adc r5, r5, #0
  asrl r4, r5, #16
  str r4, [r0], #4
  subs r7, r7, #1
  bne .Lmulrnd16_wide4_loop
  pop {r4-r8, pc}
HAETAE_MVE_END haetae_mulrnd16_wide4

HAETAE_MVE_FUNCTION haetae_mulrnd16_qrd4
  vldrw.u32 q0, [r1]
  vldrw.u32 q1, [r2]
  vshl.i32 q1, q1, #15
  vqrdmulh.s32 q0, q0, q1
  vstrw.32 q0, [r0]
  bx lr
HAETAE_MVE_END haetae_mulrnd16_qrd4

HAETAE_MVE_FUNCTION haetae_fft_mve
  push {r4-r12, lr}
  vpush {d8-d15}
  subw sp, sp, #2176
  mov r10, r0
  mov r8, sp
  add r9, r8, #1024

  /* Interleaved boundary -> SoA once. */
  mov r3, r10
  mov r2, r8
  mov r0, r9
  mov lr, #64
  dls lr, lr
haetae_fft_to_soa_loop:
  vld20.u32 {q0, q1}, [r3]
  vld21.u32 {q0, q1}, [r3]!
  vstrw.32 q0, [r2], #16
  vstrw.32 q1, [r0], #16
  le lr, haetae_fft_to_soa_loop
  /* r3 is otherwise unused by the layer macros; retain the output base. */
  mov r3, r10

  /*
   * Fused layers (1,2), four radix-4 blocks per iteration.  The 128-byte
   * scratch holds vector intermediates so q4-q7 can be reused for canonical
   * VST4 stores without a transpose.
   */
  mov r0, r8
  mov r1, r9
  add r2, sp, #2048
  mov lr, #16
  dls lr, lr
haetae_fft_first2_loop:
  vld40.u32 {q0, q1, q2, q3}, [r0]
  vld41.u32 {q0, q1, q2, q3}, [r0]
  vld42.u32 {q0, q1, q2, q3}, [r0]
  vld43.u32 {q0, q1, q2, q3}, [r0]
  vadd.u32 q4, q0, q1
  vsub.u32 q5, q0, q1
  vadd.u32 q6, q2, q3
  vsub.u32 q7, q2, q3
  vstrw.32 q4, [r2, #0]
  vstrw.32 q5, [r2, #16]
  vstrw.32 q6, [r2, #32]
  vstrw.32 q7, [r2, #48]

  vld40.u32 {q0, q1, q2, q3}, [r1]
  vld41.u32 {q0, q1, q2, q3}, [r1]
  vld42.u32 {q0, q1, q2, q3}, [r1]
  vld43.u32 {q0, q1, q2, q3}, [r1]
  vadd.u32 q4, q0, q1
  vsub.u32 q5, q0, q1
  vadd.u32 q6, q2, q3
  vsub.u32 q7, q2, q3
  vstrw.32 q4, [r2, #64]
  vstrw.32 q5, [r2, #80]
  vstrw.32 q6, [r2, #96]
  vstrw.32 q7, [r2, #112]

  vldrw.u32 q0, [r2, #0]
  vldrw.u32 q1, [r2, #16]
  vldrw.u32 q2, [r2, #32]
  vldrw.u32 q3, [r2, #112]
  vadd.u32 q4, q0, q2
  vadd.u32 q5, q1, q3
  vsub.u32 q6, q0, q2
  vsub.u32 q7, q1, q3
  vst40.u32 {q4, q5, q6, q7}, [r0]
  vst41.u32 {q4, q5, q6, q7}, [r0]
  vst42.u32 {q4, q5, q6, q7}, [r0]
  vst43.u32 {q4, q5, q6, q7}, [r0]!

  vldrw.u32 q0, [r2, #64]
  vldrw.u32 q1, [r2, #80]
  vldrw.u32 q2, [r2, #96]
  vldrw.u32 q3, [r2, #48]
  vadd.u32 q4, q0, q2
  vsub.u32 q5, q1, q3
  vsub.u32 q6, q0, q2
  vadd.u32 q7, q1, q3
  vst40.u32 {q4, q5, q6, q7}, [r1]
  vst41.u32 {q4, q5, q6, q7}, [r1]
  vst42.u32 {q4, q5, q6, q7}, [r1]
  vst43.u32 {q4, q5, q6, q7}, [r1]!
  le lr, haetae_fft_first2_loop

  /* 32 vector butterflies per layer, with roots reused across all blocks. */
  HAETAE_FFT_LAYER 3, 16, 32, 32, 1
  HAETAE_FFT_LAYER 4, 32, 64, 16, 2
  HAETAE_FFT_LAYER 5, 64, 128, 8, 4
  HAETAE_FFT_LAYER 6, 128, 256, 4, 8
  HAETAE_FFT_LAYER 7, 256, 512, 2, 16
  HAETAE_FFT_LAYER 8, 512, 1024, 1, 32

  /* SoA -> canonical interleaved boundary. */
  mov r2, r8
  mov r0, r9
  mov lr, #64
  dls lr, lr
haetae_fft_from_soa_loop:
  vldrw.u32 q0, [r2], #16
  vldrw.u32 q1, [r0], #16
  vst20.u32 {q0, q1}, [r3]
  vst21.u32 {q0, q1}, [r3]!
  le lr, haetae_fft_from_soa_loop

  addw sp, sp, #2176
  vpop {d8-d15}
  pop {r4-r12, lr}
  bx lr
HAETAE_MVE_END haetae_fft_mve

/*
 * FFT-H-SOA-FUSED-L8
 *
 * Repeat the common FFT prefix through layer 7, then consume layer-8 output
 * directly from q0..q3.  This removes the final SoA stores, the SoA-to-AoS
 * conversion, and the subsequent 512 scalar coefficient loads.  The scalar
 * long products preserve the exact independent +2^15 rounding of the C
 * reference; combining the squares before shifting would change boundary
 * bits.  The 16 bytes above the common workspace retain the sum pointer.
 */
HAETAE_MVE_FUNCTION haetae_fft_sqabs_acc_mve
  push {r4-r12, lr}
  vpush {d8-d15}
  subw sp, sp, #2192
  str r0, [sp, #2176]
  mov r10, r1
  mov r8, sp
  add r9, r8, #1024

  /* Interleaved boundary -> SoA once. */
  mov r3, r10
  mov r2, r8
  mov r0, r9
  mov lr, #64
  dls lr, lr
haetae_fft_fused_to_soa_loop:
  vld20.u32 {q0, q1}, [r3]
  vld21.u32 {q0, q1}, [r3]!
  vstrw.32 q0, [r2], #16
  vstrw.32 q1, [r0], #16
  le lr, haetae_fft_fused_to_soa_loop

  /* Same exact fused radix-4 prefix as haetae_fft_mve. */
  mov r0, r8
  mov r1, r9
  add r2, sp, #2048
  mov lr, #16
  dls lr, lr
haetae_fft_fused_first2_loop:
  vld40.u32 {q0, q1, q2, q3}, [r0]
  vld41.u32 {q0, q1, q2, q3}, [r0]
  vld42.u32 {q0, q1, q2, q3}, [r0]
  vld43.u32 {q0, q1, q2, q3}, [r0]
  vadd.u32 q4, q0, q1
  vsub.u32 q5, q0, q1
  vadd.u32 q6, q2, q3
  vsub.u32 q7, q2, q3
  vstrw.32 q4, [r2, #0]
  vstrw.32 q5, [r2, #16]
  vstrw.32 q6, [r2, #32]
  vstrw.32 q7, [r2, #48]

  vld40.u32 {q0, q1, q2, q3}, [r1]
  vld41.u32 {q0, q1, q2, q3}, [r1]
  vld42.u32 {q0, q1, q2, q3}, [r1]
  vld43.u32 {q0, q1, q2, q3}, [r1]
  vadd.u32 q4, q0, q1
  vsub.u32 q5, q0, q1
  vadd.u32 q6, q2, q3
  vsub.u32 q7, q2, q3
  vstrw.32 q4, [r2, #64]
  vstrw.32 q5, [r2, #80]
  vstrw.32 q6, [r2, #96]
  vstrw.32 q7, [r2, #112]

  vldrw.u32 q0, [r2, #0]
  vldrw.u32 q1, [r2, #16]
  vldrw.u32 q2, [r2, #32]
  vldrw.u32 q3, [r2, #112]
  vadd.u32 q4, q0, q2
  vadd.u32 q5, q1, q3
  vsub.u32 q6, q0, q2
  vsub.u32 q7, q1, q3
  vst40.u32 {q4, q5, q6, q7}, [r0]
  vst41.u32 {q4, q5, q6, q7}, [r0]
  vst42.u32 {q4, q5, q6, q7}, [r0]
  vst43.u32 {q4, q5, q6, q7}, [r0]!

  vldrw.u32 q0, [r2, #64]
  vldrw.u32 q1, [r2, #80]
  vldrw.u32 q2, [r2, #96]
  vldrw.u32 q3, [r2, #48]
  vadd.u32 q4, q0, q2
  vsub.u32 q5, q1, q3
  vsub.u32 q6, q0, q2
  vadd.u32 q7, q1, q3
  vst40.u32 {q4, q5, q6, q7}, [r1]
  vst41.u32 {q4, q5, q6, q7}, [r1]
  vst42.u32 {q4, q5, q6, q7}, [r1]
  vst43.u32 {q4, q5, q6, q7}, [r1]!
  le lr, haetae_fft_fused_first2_loop

  HAETAE_FFT_LAYER 3, 16, 32, 32, 1
  HAETAE_FFT_LAYER 4, 32, 64, 16, 2
  HAETAE_FFT_LAYER 5, 64, 128, 8, 4
  HAETAE_FFT_LAYER 6, 128, 256, 4, 8
  HAETAE_FFT_LAYER 7, 256, 512, 2, 16

  /* Layer 8: one vector per twiddle group, consumed without final stores. */
  ldr r10, =haetae_asm_fft_layer8_twiddles
  ldr r11, [sp, #2176]
  add r12, r11, #512

  vldrw.u32 q4, [r10], #16
  vldrw.u32 q5, [r10], #16
  mov r4, r8
  mov r5, r9
  add r6, r8, #512
  add r7, r9, #512
  HAETAE_FFT_VECTOR_COMPUTE 1
  HAETAE_FFT_SQABS_LANE 0, s0, s4, s8, s12
  HAETAE_FFT_SQABS_LANE 1, s1, s5, s9, s13
  HAETAE_FFT_SQABS_LANE 2, s2, s6, s10, s14
  HAETAE_FFT_SQABS_LANE 3, s3, s7, s11, s15
  add r8, r8, #16
  add r9, r9, #16
  add r11, r11, #16
  add r12, r12, #16
  mov lr, #31
  dls lr, lr

haetae_fft_fused_layer8_loop:
  vldrw.u32 q4, [r10], #16
  vldrw.u32 q5, [r10], #16
  mov r4, r8
  mov r5, r9
  add r6, r8, #512
  add r7, r9, #512
  HAETAE_FFT_VECTOR_COMPUTE 0
  HAETAE_FFT_SQABS_LANE 0, s0, s4, s8, s12
  HAETAE_FFT_SQABS_LANE 1, s1, s5, s9, s13
  HAETAE_FFT_SQABS_LANE 2, s2, s6, s10, s14
  HAETAE_FFT_SQABS_LANE 3, s3, s7, s11, s15
  add r8, r8, #16
  add r9, r9, #16
  add r11, r11, #16
  add r12, r12, #16
  le lr, haetae_fft_fused_layer8_loop

  addw sp, sp, #2192
  vpop {d8-d15}
  pop {r4-r12, lr}
  bx lr
HAETAE_MVE_END haetae_fft_sqabs_acc_mve

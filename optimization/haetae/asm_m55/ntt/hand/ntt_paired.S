/* SPDX-License-Identifier: MIT */

/*
 * NTT-H-PAIRED-VLD4-U{1,2,4}
 *
 * Handwritten q=64513 transform with four coefficient passes.  The forward
 * pass pairs layers (128,64), (32,16), (8,4), and (2,1); the inverse applies
 * the reverse GS pairs and fuses the final 1/256 scale into (64,128).
 * Upper pairs use packed scalar root/twisted loads that are reused across an
 * entire public block.  The lower pair uses a VLD4/VST4 no-transpose layout
 * and six aligned vector-root loads per 16-coefficient chunk.
 *
 * Register map: r10 coefficient base (or final scale root), r11 root stream
 * (or final scale twisted), r12=-q, r4-r9 three packed scalar root pairs,
 * r0-r3 coefficient streams, lr fixed public loop counter.  q0-q3 are live
 * coefficients, q4-q6 arithmetic/root temporaries.  Frame: 40 GPR bytes and
 * 48 MVE bytes.  Branches, addresses, and counts depend only on public N.
 */

.syntax unified
.thumb
.arch armv8.1-m.main
.arch_extension mve

#include "../../common/abi.inc"

#ifndef HAETAE_NTT_PAIRED_UNROLL
#define HAETAE_NTT_PAIRED_UNROLL 1
#endif

.macro HAETAE_CT_SCALAR left, right, root, twisted
  vmul.s32 q4, \right, \root
  vqrdmulh.s32 \right, \right, \twisted
  vmla.s32 q4, \right, r12
  vsub.u32 \right, \left, q4
  vadd.u32 \left, \left, q4
.endm

.macro HAETAE_GS_SCALAR left, right, root, twisted
  vadd.u32 q4, \left, \right
  vsub.u32 \right, \left, \right
  vmul.s32 \left, \right, \root
  vqrdmulh.s32 \right, \right, \twisted
  vmla.s32 \left, \right, r12
  vmov \right, \left
  vmov \left, q4
.endm

.macro HAETAE_MUL_SCALAR value, root, twisted
  vmul.s32 q4, \value, \root
  vqrdmulh.s32 \value, \value, \twisted
  vmla.s32 q4, \value, r12
  vmov \value, q4
.endm

.macro HAETAE_CT_VECTOR left, right
  vmul.s32 q6, \right, q4
  vqrdmulh.s32 \right, \right, q5
  vmla.s32 q6, \right, r12
  vsub.u32 \right, \left, q6
  vadd.u32 \left, \left, q6
.endm

.macro HAETAE_GS_VECTOR left, right
  vadd.u32 q6, \left, \right
  vsub.u32 \right, \left, \right
  vmul.s32 \left, \right, q4
  vqrdmulh.s32 \right, \right, q5
  vmla.s32 \left, \right, r12
  vmov \right, \left
  vmov \left, q6
.endm

.macro HAETAE_FORWARD_UPPER_BODY
  vldrw.u32 q0, [r0]
  vldrw.u32 q1, [r1]
  vldrw.u32 q2, [r2]
  vldrw.u32 q3, [r3]
  HAETAE_CT_SCALAR q0, q2, r4, r5
  HAETAE_CT_SCALAR q1, q3, r4, r5
  HAETAE_CT_SCALAR q0, q1, r6, r7
  HAETAE_CT_SCALAR q2, q3, r8, r9
  vstrw.32 q0, [r0], #16
  vstrw.32 q1, [r1], #16
  vstrw.32 q2, [r2], #16
  vstrw.32 q3, [r3], #16
.endm

.macro HAETAE_INVERSE_UPPER_BODY scale=0
  vldrw.u32 q0, [r0]
  vldrw.u32 q1, [r1]
  vldrw.u32 q2, [r2]
  vldrw.u32 q3, [r3]
  HAETAE_GS_SCALAR q0, q1, r6, r7
  HAETAE_GS_SCALAR q2, q3, r8, r9
  HAETAE_GS_SCALAR q0, q2, r4, r5
  HAETAE_GS_SCALAR q1, q3, r4, r5
  .if \scale
    HAETAE_MUL_SCALAR q0, r10, r11
    HAETAE_MUL_SCALAR q1, r10, r11
    HAETAE_MUL_SCALAR q2, r10, r11
    HAETAE_MUL_SCALAR q3, r10, r11
  .endif
  vstrw.32 q0, [r0], #16
  vstrw.32 q1, [r1], #16
  vstrw.32 q2, [r2], #16
  vstrw.32 q3, [r3], #16
.endm

.macro HAETAE_FORWARD_UPPER_REPEAT
  HAETAE_FORWARD_UPPER_BODY
#if HAETAE_NTT_PAIRED_UNROLL >= 2
  HAETAE_FORWARD_UPPER_BODY
#endif
#if HAETAE_NTT_PAIRED_UNROLL >= 4
  HAETAE_FORWARD_UPPER_BODY
  HAETAE_FORWARD_UPPER_BODY
#endif
.endm

.macro HAETAE_INVERSE_UPPER_REPEAT scale=0
  HAETAE_INVERSE_UPPER_BODY \scale
#if HAETAE_NTT_PAIRED_UNROLL >= 2
  HAETAE_INVERSE_UPPER_BODY \scale
#endif
#if HAETAE_NTT_PAIRED_UNROLL >= 4
  HAETAE_INVERSE_UPPER_BODY \scale
  HAETAE_INVERSE_UPPER_BODY \scale
#endif
.endm

.macro HAETAE_FORWARD_LOWER_BODY
  vld40.u32 {q0, q1, q2, q3}, [r0]
  vld41.u32 {q0, q1, q2, q3}, [r0]
  vld42.u32 {q0, q1, q2, q3}, [r0]
  vld43.u32 {q0, q1, q2, q3}, [r0]
  vldrw.u32 q4, [r11, #0]
  vldrw.u32 q5, [r11, #16]
  HAETAE_CT_VECTOR q0, q2
  HAETAE_CT_VECTOR q1, q3
  vldrw.u32 q4, [r11, #32]
  vldrw.u32 q5, [r11, #48]
  HAETAE_CT_VECTOR q0, q1
  vldrw.u32 q4, [r11, #64]
  vldrw.u32 q5, [r11, #80]
  HAETAE_CT_VECTOR q2, q3
  vst40.u32 {q0, q1, q2, q3}, [r0]
  vst41.u32 {q0, q1, q2, q3}, [r0]
  vst42.u32 {q0, q1, q2, q3}, [r0]
  vst43.u32 {q0, q1, q2, q3}, [r0]!
  add r11, r11, #96
.endm

.macro HAETAE_INVERSE_LOWER_BODY
  vld40.u32 {q0, q1, q2, q3}, [r0]
  vld41.u32 {q0, q1, q2, q3}, [r0]
  vld42.u32 {q0, q1, q2, q3}, [r0]
  vld43.u32 {q0, q1, q2, q3}, [r0]
  vldrw.u32 q4, [r11, #32]
  vldrw.u32 q5, [r11, #48]
  HAETAE_GS_VECTOR q0, q1
  vldrw.u32 q4, [r11, #64]
  vldrw.u32 q5, [r11, #80]
  HAETAE_GS_VECTOR q2, q3
  vldrw.u32 q4, [r11, #0]
  vldrw.u32 q5, [r11, #16]
  HAETAE_GS_VECTOR q0, q2
  HAETAE_GS_VECTOR q1, q3
  vst40.u32 {q0, q1, q2, q3}, [r0]
  vst41.u32 {q0, q1, q2, q3}, [r0]
  vst42.u32 {q0, q1, q2, q3}, [r0]
  vst43.u32 {q0, q1, q2, q3}, [r0]!
  add r11, r11, #96
.endm

.macro HAETAE_FORWARD_LOWER_REPEAT
  HAETAE_FORWARD_LOWER_BODY
#if HAETAE_NTT_PAIRED_UNROLL >= 2
  HAETAE_FORWARD_LOWER_BODY
#endif
#if HAETAE_NTT_PAIRED_UNROLL >= 4
  HAETAE_FORWARD_LOWER_BODY
  HAETAE_FORWARD_LOWER_BODY
#endif
.endm

.macro HAETAE_INVERSE_LOWER_REPEAT
  HAETAE_INVERSE_LOWER_BODY
#if HAETAE_NTT_PAIRED_UNROLL >= 2
  HAETAE_INVERSE_LOWER_BODY
#endif
#if HAETAE_NTT_PAIRED_UNROLL >= 4
  HAETAE_INVERSE_LOWER_BODY
  HAETAE_INVERSE_LOWER_BODY
#endif
.endm

HAETAE_MVE_FUNCTION haetae_ntt_asm_paired
  push {r4-r12, lr}
  vpush {d8-d13}
  mov r10, r0
  ldr r11, =haetae_asm_paired_forward_upper
  movw r12, #64513
  rsb r12, r12, #0

  /* Pair (128,64): one block, 16/U vector iterations. */
  ldrd r4, r5, [r11], #8
  ldrd r6, r7, [r11], #8
  ldrd r8, r9, [r11], #8
  mov r0, r10
  add r1, r10, #256
  add r2, r10, #512
  add r3, r10, #768
  movs lr, #(16 / HAETAE_NTT_PAIRED_UNROLL)
.Lfwd_128_64:
  HAETAE_FORWARD_UPPER_REPEAT
  subs lr, lr, #1
  bne .Lfwd_128_64

  /* Pair (32,16): four 64-coefficient blocks. */
  mov r0, r10
.Lfwd_32_16_outer:
  ldrd r4, r5, [r11], #8
  ldrd r6, r7, [r11], #8
  ldrd r8, r9, [r11], #8
  add r1, r0, #64
  add r2, r0, #128
  add r3, r0, #192
  movs lr, #(4 / HAETAE_NTT_PAIRED_UNROLL)
.Lfwd_32_16_inner:
  HAETAE_FORWARD_UPPER_REPEAT
  subs lr, lr, #1
  bne .Lfwd_32_16_inner
  mov r0, r3
  sub r4, r0, r10
  cmp r4, #1024
  bne .Lfwd_32_16_outer

  /* Pair (8,4): sixteen 16-coefficient blocks. */
  mov r0, r10
.Lfwd_8_4_outer:
  ldrd r4, r5, [r11], #8
  ldrd r6, r7, [r11], #8
  ldrd r8, r9, [r11], #8
  add r1, r0, #16
  add r2, r0, #32
  add r3, r0, #48
  HAETAE_FORWARD_UPPER_BODY
  mov r0, r3
  sub r4, r0, r10
  cmp r4, #1024
  bne .Lfwd_8_4_outer

  /* Pair (2,1): VLD4 no-transpose records, 16/U chunks. */
  mov r0, r10
  ldr r11, =haetae_asm_paired_forward_lower
  movs lr, #(16 / HAETAE_NTT_PAIRED_UNROLL)
.Lfwd_2_1:
  HAETAE_FORWARD_LOWER_REPEAT
  subs lr, lr, #1
  bne .Lfwd_2_1

  vpop {d8-d13}
  pop {r4-r12, lr}
  bx lr
HAETAE_MVE_END haetae_ntt_asm_paired

HAETAE_MVE_FUNCTION haetae_invntt_asm_paired
  push {r4-r12, lr}
  vpush {d8-d13}
  mov r10, r0
  movw r12, #64513
  rsb r12, r12, #0

  /* Reverse pair (1,2). */
  ldr r11, =haetae_asm_paired_inverse_lower
  movs lr, #(16 / HAETAE_NTT_PAIRED_UNROLL)
.Linv_1_2:
  HAETAE_INVERSE_LOWER_REPEAT
  subs lr, lr, #1
  bne .Linv_1_2

  ldr r11, =haetae_asm_paired_inverse_upper

  /* Reverse pair (4,8): sixteen blocks. */
  mov r0, r10
.Linv_4_8_outer:
  ldrd r4, r5, [r11], #8
  ldrd r6, r7, [r11], #8
  ldrd r8, r9, [r11], #8
  add r1, r0, #16
  add r2, r0, #32
  add r3, r0, #48
  HAETAE_INVERSE_UPPER_BODY
  mov r0, r3
  sub r4, r0, r10
  cmp r4, #1024
  bne .Linv_4_8_outer

  /* Reverse pair (16,32): four blocks. */
  mov r0, r10
.Linv_16_32_outer:
  ldrd r4, r5, [r11], #8
  ldrd r6, r7, [r11], #8
  ldrd r8, r9, [r11], #8
  add r1, r0, #64
  add r2, r0, #128
  add r3, r0, #192
  movs lr, #(4 / HAETAE_NTT_PAIRED_UNROLL)
.Linv_16_32_inner:
  HAETAE_INVERSE_UPPER_REPEAT
  subs lr, lr, #1
  bne .Linv_16_32_inner
  mov r0, r3
  sub r4, r0, r10
  cmp r4, #1024
  bne .Linv_16_32_outer

  /* Reverse pair (64,128), with the inverse scale fused before stores. */
  ldrd r4, r5, [r11], #8
  ldrd r6, r7, [r11], #8
  ldrd r8, r9, [r11], #8
  mov r0, r10
  add r1, r10, #256
  add r2, r10, #512
  add r3, r10, #768
  ldr lr, =haetae_asm_inverse_scale
  ldrd r10, r11, [lr]
  movs lr, #(16 / HAETAE_NTT_PAIRED_UNROLL)
.Linv_64_128:
  HAETAE_INVERSE_UPPER_REPEAT 1
  subs lr, lr, #1
  bne .Linv_64_128

  vpop {d8-d13}
  pop {r4-r12, lr}
  bx lr
HAETAE_MVE_END haetae_invntt_asm_paired

/* The six legacy public variant names below are ABI/dispatch aliases.  In an
 * assembly-backend image they all branch to the one paired forward or inverse
 * dataflow selected at build time; they are not six assembly candidates. */
.macro HAETAE_NTT_FORWARD_WRAPPER name
  HAETAE_MVE_FUNCTION \name
#ifdef HAETAE_NTT_SLOTHY_FORWARD
    b.w haetae_ntt_asm_paired_slothy
#else
    b.w haetae_ntt_asm_paired
#endif
  HAETAE_MVE_END \name
.endm
.macro HAETAE_NTT_INVERSE_WRAPPER name
  HAETAE_MVE_FUNCTION \name
#ifdef HAETAE_NTT_SLOTHY_INVERSE
    b.w haetae_invntt_asm_paired_slothy
#else
    b.w haetae_invntt_asm_paired
#endif
  HAETAE_MVE_END \name
.endm

HAETAE_NTT_FORWARD_WRAPPER haetae_ntt_mve_l44
HAETAE_NTT_INVERSE_WRAPPER haetae_invntt_mve_l44
HAETAE_NTT_FORWARD_WRAPPER haetae_ntt_mve_l332
HAETAE_NTT_INVERSE_WRAPPER haetae_invntt_mve_l332
HAETAE_NTT_FORWARD_WRAPPER haetae_ntt_mve_l332_vld2
HAETAE_NTT_INVERSE_WRAPPER haetae_invntt_mve_l332_vld2
HAETAE_NTT_FORWARD_WRAPPER haetae_ntt_mve_l332_vld4
HAETAE_NTT_INVERSE_WRAPPER haetae_invntt_mve_l332_vld4
HAETAE_NTT_FORWARD_WRAPPER haetae_ntt_mve_l332_vld4_m3
HAETAE_NTT_INVERSE_WRAPPER haetae_invntt_mve_l332_vld4_m3
HAETAE_NTT_FORWARD_WRAPPER haetae_ntt_mve_l332_vld4_u2
HAETAE_NTT_INVERSE_WRAPPER haetae_invntt_mve_l332_vld4_u2

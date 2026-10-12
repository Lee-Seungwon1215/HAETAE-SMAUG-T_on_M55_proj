/* SPDX-License-Identifier: MIT */

/*
 * NTT-H-GENERIC-VLD42
 *
 * A direct handwritten implementation of the HAETAE q=64513 CT/GS transform.
 * Layers with len>=4 use contiguous four-lane butterflies. The len=2 layer
 * processes four groups with VLD4/VST4 and the len=1 layer processes four
 * groups with VLD2/VST2. Forward roots are stored in traversal order 1..255;
 * inverse roots are pre-negated and stored in traversal order 255..1, removing
 * reverse table addressing from the hot loops.
 *
 * Register map: r0 input ABI; r10 base; r4 root pointer; r5/r6 scalar root
 * pair; r7=-q; r1=len; r2/r3 data pointers; r8 public start; r9 end.
 * q0-q3 hold ordinary butterflies and q4-q7 hold VLD2/VLD4 roots/temporaries.
 * Frame: 32 GPR bytes + 64 MVE bytes. d8-d15 are preserved. All branches,
 * addresses and iteration counts depend only on public N=256 and layer index.
 */

.syntax unified
.thumb
.arch armv8.1-m.main
.arch_extension mve

#include "../../common/abi.inc"

.macro HAETAE_MULMOD_SCALAR dst, value, quotient, root, twisted, modulus
  vmul.s32 \dst, \value, \root
  vqrdmulh.s32 \quotient, \value, \twisted
  vmla.s32 \dst, \quotient, \modulus
.endm

HAETAE_MVE_FUNCTION haetae_ntt_asm_generic
  push {r4-r10, lr}
  vpush {d8-d15}
  mov r10, r0
  ldr r4, =haetae_asm_roots_forward
  movw r7, #64513
  rsb r7, r7, #0
  mov r1, #128

.Lntt_layer_ge4:
  movs r8, #0
.Lntt_group_ge4:
  ldrd r5, r6, [r4], #8
  add r2, r10, r8, lsl #2
  add r3, r2, r1, lsl #2
  mov r9, r3
.Lntt_inner_ge4:
  vldrw.u32 q0, [r3]
  HAETAE_MULMOD_SCALAR q2, q0, q3, r5, r6, r7
  vldrw.u32 q1, [r2]
  vsub.u32 q0, q1, q2
  vadd.u32 q1, q1, q2
  vstrw.32 q1, [r2], #16
  vstrw.32 q0, [r3], #16
  cmp r2, r9
  bne .Lntt_inner_ge4
  add r8, r8, r1, lsl #1
  cmp r8, #256
  bne .Lntt_group_ge4
  lsrs r1, r1, #1
  cmp r1, #4
  bhs .Lntt_layer_ge4

  /* len=2: [a0,a1,b0,b1] from four groups at a time. */
  mov r2, r10
  add r3, r10, #1024
.Lntt_len2:
  vld40.u32 {q0, q1, q2, q3}, [r2]
  vld41.u32 {q0, q1, q2, q3}, [r2]
  vld42.u32 {q0, q1, q2, q3}, [r2]
  vld43.u32 {q0, q1, q2, q3}, [r2]
  vld20.u32 {q4, q5}, [r4]
  vld21.u32 {q4, q5}, [r4]!
  vmul.s32 q6, q2, q4
  vqrdmulh.s32 q2, q2, q5
  vmla.s32 q6, q2, r7
  vmul.s32 q7, q3, q4
  vqrdmulh.s32 q3, q3, q5
  vmla.s32 q7, q3, r7
  vsub.u32 q2, q0, q6
  vadd.u32 q0, q0, q6
  vsub.u32 q3, q1, q7
  vadd.u32 q1, q1, q7
  vst40.u32 {q0, q1, q2, q3}, [r2]
  vst41.u32 {q0, q1, q2, q3}, [r2]
  vst42.u32 {q0, q1, q2, q3}, [r2]
  vst43.u32 {q0, q1, q2, q3}, [r2]!
  cmp r2, r3
  bne .Lntt_len2

  /* len=1: [a,b] from four groups at a time. */
  mov r2, r10
.Lntt_len1:
  vld20.u32 {q0, q1}, [r2]
  vld21.u32 {q0, q1}, [r2]
  vld20.u32 {q2, q3}, [r4]
  vld21.u32 {q2, q3}, [r4]!
  vmul.s32 q4, q1, q2
  vqrdmulh.s32 q1, q1, q3
  vmla.s32 q4, q1, r7
  vsub.u32 q1, q0, q4
  vadd.u32 q0, q0, q4
  vst20.u32 {q0, q1}, [r2]
  vst21.u32 {q0, q1}, [r2]!
  cmp r2, r3
  bne .Lntt_len1

  vpop {d8-d15}
  pop {r4-r10, lr}
  bx lr
HAETAE_MVE_END haetae_ntt_asm_generic

HAETAE_MVE_FUNCTION haetae_invntt_asm_generic
  push {r4-r10, lr}
  vpush {d8-d15}
  mov r10, r0
  ldr r4, =haetae_asm_roots_inverse
  movw r7, #64513
  rsb r7, r7, #0
  add r3, r10, #1024

  /* len=1, inverse GS butterfly, four groups per iteration. */
  mov r2, r10
.Linv_len1:
  vld20.u32 {q0, q1}, [r2]
  vld21.u32 {q0, q1}, [r2]
  vld20.u32 {q2, q3}, [r4]
  vld21.u32 {q2, q3}, [r4]!
  vadd.u32 q4, q0, q1
  vsub.u32 q1, q0, q1
  vmul.s32 q5, q1, q2
  vqrdmulh.s32 q1, q1, q3
  vmla.s32 q5, q1, r7
  vmov q0, q4
  vmov q1, q5
  vst20.u32 {q0, q1}, [r2]
  vst21.u32 {q0, q1}, [r2]!
  cmp r2, r3
  bne .Linv_len1

  /* len=2, four groups per iteration. */
  mov r2, r10
.Linv_len2:
  vld40.u32 {q0, q1, q2, q3}, [r2]
  vld41.u32 {q0, q1, q2, q3}, [r2]
  vld42.u32 {q0, q1, q2, q3}, [r2]
  vld43.u32 {q0, q1, q2, q3}, [r2]
  vld20.u32 {q4, q5}, [r4]
  vld21.u32 {q4, q5}, [r4]!
  vadd.u32 q6, q0, q2
  vsub.u32 q0, q0, q2
  vadd.u32 q7, q1, q3
  vsub.u32 q1, q1, q3
  vmul.s32 q2, q0, q4
  vqrdmulh.s32 q0, q0, q5
  vmla.s32 q2, q0, r7
  vmul.s32 q3, q1, q4
  vqrdmulh.s32 q1, q1, q5
  vmla.s32 q3, q1, r7
  vmov q0, q6
  vmov q1, q7
  vst40.u32 {q0, q1, q2, q3}, [r2]
  vst41.u32 {q0, q1, q2, q3}, [r2]
  vst42.u32 {q0, q1, q2, q3}, [r2]
  vst43.u32 {q0, q1, q2, q3}, [r2]!
  cmp r2, r3
  bne .Linv_len2

  movs r1, #4
.Linv_layer_ge4:
  movs r8, #0
.Linv_group_ge4:
  ldrd r5, r6, [r4], #8
  add r2, r10, r8, lsl #2
  add r3, r2, r1, lsl #2
  mov r9, r3
.Linv_inner_ge4:
  vldrw.u32 q0, [r2]
  vldrw.u32 q1, [r3]
  vadd.u32 q2, q0, q1
  vsub.u32 q0, q0, q1
  HAETAE_MULMOD_SCALAR q3, q0, q1, r5, r6, r7
  vstrw.32 q2, [r2], #16
  vstrw.32 q3, [r3], #16
  cmp r2, r9
  bne .Linv_inner_ge4
  add r8, r8, r1, lsl #1
  cmp r8, #256
  bne .Linv_group_ge4
  lsls r1, r1, #1
  cmp r1, #128
  bls .Linv_layer_ge4

  /* Multiply every coefficient by 1/256 in the standard root representation. */
  ldr r4, =haetae_asm_inverse_scale
  ldrd r5, r6, [r4]
  mov r2, r10
  add r3, r10, #1024
.Linv_scale:
  vldrw.u32 q0, [r2]
  HAETAE_MULMOD_SCALAR q1, q0, q2, r5, r6, r7
  vstrw.32 q1, [r2], #16
  cmp r2, r3
  bne .Linv_scale

  vpop {d8-d15}
  pop {r4-r10, lr}
  bx lr
HAETAE_MVE_END haetae_invntt_asm_generic

/* Every preserved public variant has an assembly implementation. For this
 * first correctness-oriented candidate they deliberately share the same
 * layer-by-layer core; later layout candidates own distinct symbols. */
.macro HAETAE_NTT_FORWARD_WRAPPER name
  HAETAE_MVE_FUNCTION \name
    b.w haetae_ntt_asm_generic
  HAETAE_MVE_END \name
.endm
.macro HAETAE_NTT_INVERSE_WRAPPER name
  HAETAE_MVE_FUNCTION \name
    b.w haetae_invntt_asm_generic
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

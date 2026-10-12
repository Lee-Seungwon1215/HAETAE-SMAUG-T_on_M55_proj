/* SPDX-License-Identifier: MIT */

/*
 * NTT-H-ARITH-D1
 *
 * Handwritten q=64513 pointwise, accumulation and normalization leaves.
 * Accumulation reduces every term before adding (the conservative d1
 * candidate); terms is public and at most 7 in HAETAE2/3/5. Coefficient data
 * never controls a branch, address or trip count. q0-q3 are the only vector
 * registers used, so no callee-saved MVE state is touched.
 */

.syntax unified
.thumb
.arch armv8.1-m.main
.arch_extension mve

#include "../../common/abi.inc"

.macro HAETAE_BARRETT_Q value, quotient, reciprocal, negq
  vqrdmulh.s32 \quotient, \value, \reciprocal
  vmla.s32 \value, \quotient, \negq
.endm

.macro HAETAE_RINV_Q dst, value, quotient, root, twisted, negq
  vmul.s32 \dst, \value, \root
  vqrdmulh.s32 \quotient, \value, \twisted
  vmla.s32 \dst, \quotient, \negq
.endm

#if !defined(HAETAE_NTT_SLOTHY_POINTWISE)
HAETAE_MVE_FUNCTION haetae_pointwise_mve
  push {r4-r7}
  add r3, r0, #1024
  movw r4, #33288
  movw r5, #64513
  rsb r5, r5, #0
  ldr r6, =-14127
  ldr r7, =-470254080
.Lpointwise_loop:
  vldrw.u32 q0, [r1], #16
  vldrw.u32 q1, [r2], #16
  HAETAE_BARRETT_Q q0, q2, r4, r5
  HAETAE_BARRETT_Q q1, q2, r4, r5
  vmul.s32 q0, q0, q1
  HAETAE_BARRETT_Q q0, q2, r4, r5
  HAETAE_RINV_Q q1, q0, q2, r6, r7, r5
  vstrw.32 q1, [r0], #16
  cmp r0, r3
  bne .Lpointwise_loop
  pop {r4-r7}
  bx lr
HAETAE_MVE_END haetae_pointwise_mve
#endif

#if !defined(HAETAE_NTT_SLOTHY_POINTWISE_ACC)
/* r0=c, r1=a, r2=b, r3=public terms; mode=0 normal, mode=1 b centered. */
.macro HAETAE_ACC_BODY_D1 name, precentered=0
HAETAE_MVE_FUNCTION \name
  push {r4-r10, lr}
  sub sp, sp, #8
  str r3, [sp]
  add r10, r0, #1024
  movw r4, #33288
  movw r5, #64513
  rsb r5, r5, #0
  ldr r6, =-14127
  ldr r7, =-470254080
.Lacc_outer_\@:
  vmov.i32 q0, #0
  mov r8, r1
  mov r9, r2
  ldr r3, [sp]
  cmp r3, #0
  beq .Lacc_store_\@
.Lacc_term_\@:
  vldrw.u32 q1, [r8]
  HAETAE_BARRETT_Q q1, q2, r4, r5
  vldrw.u32 q3, [r9]
  .if !\precentered
    HAETAE_BARRETT_Q q3, q2, r4, r5
  .endif
  vmul.s32 q1, q1, q3
  HAETAE_BARRETT_Q q1, q2, r4, r5
  HAETAE_RINV_Q q3, q1, q2, r6, r7, r5
  vadd.i32 q0, q0, q3
  add r8, r8, #1024
  add r9, r9, #1024
  subs r3, r3, #1
  bne .Lacc_term_\@
.Lacc_store_\@:
  vstrw.32 q0, [r0], #16
  add r1, r1, #16
  add r2, r2, #16
  cmp r0, r10
  bne .Lacc_outer_\@
  add sp, sp, #8
  pop {r4-r10, lr}
  bx lr
HAETAE_MVE_END \name
.endm

.macro HAETAE_ACC_BODY_D2 name, precentered=0
HAETAE_MVE_FUNCTION \name
  push {r4-r10, lr}
  sub sp, sp, #8
  vpush {d8-d11}
  str r3, [sp, #32]
  add r10, r0, #1024
  movw r4, #33288
  movw r5, #64513
  rsb r5, r5, #0
  ldr r6, =-14127
  ldr r7, =-470254080
.Lacc_d2_outer_\@:
  vmov.i32 q0, #0
  mov r8, r1
  mov r9, r2
  ldr r3, [sp, #32]
.Lacc_d2_pair_test_\@:
  cmp r3, #2
  blo .Lacc_d2_remainder_\@
  vldrw.u32 q1, [r8]
  HAETAE_BARRETT_Q q1, q5, r4, r5
  vldrw.u32 q2, [r9]
  .if !\precentered
    HAETAE_BARRETT_Q q2, q5, r4, r5
  .endif
  vmul.s32 q1, q1, q2
  add r12, r8, #1024
  vldrw.u32 q3, [r12]
  HAETAE_BARRETT_Q q3, q5, r4, r5
  add r12, r9, #1024
  vldrw.u32 q4, [r12]
  .if !\precentered
    HAETAE_BARRETT_Q q4, q5, r4, r5
  .endif
  vmul.s32 q3, q3, q4
  vadd.i32 q1, q1, q3
  HAETAE_BARRETT_Q q1, q5, r4, r5
  vadd.i32 q0, q0, q1
  add r8, r8, #2048
  add r9, r9, #2048
  subs r3, r3, #2
  b .Lacc_d2_pair_test_\@
.Lacc_d2_remainder_\@:
  cmp r3, #0
  beq .Lacc_d2_finish_\@
  vldrw.u32 q1, [r8]
  HAETAE_BARRETT_Q q1, q5, r4, r5
  vldrw.u32 q2, [r9]
  .if !\precentered
    HAETAE_BARRETT_Q q2, q5, r4, r5
  .endif
  vmul.s32 q1, q1, q2
  HAETAE_BARRETT_Q q1, q5, r4, r5
  vadd.i32 q0, q0, q1
.Lacc_d2_finish_\@:
  HAETAE_RINV_Q q1, q0, q2, r6, r7, r5
  vstrw.32 q1, [r0], #16
  add r1, r1, #16
  add r2, r2, #16
  cmp r0, r10
  bne .Lacc_d2_outer_\@
  vpop {d8-d11}
  add sp, sp, #8
  pop {r4-r10, lr}
  bx lr
HAETAE_MVE_END \name
.endm

#if defined(HAETAE_NTT_ARITH_D2)
HAETAE_ACC_BODY_D2 haetae_pointwise_acc_mve, 0
HAETAE_ACC_BODY_D2 haetae_pointwise_acc_precentered_mve, 1
#else
HAETAE_ACC_BODY_D1 haetae_pointwise_acc_mve, 0
HAETAE_ACC_BODY_D1 haetae_pointwise_acc_precentered_mve, 1
#endif

HAETAE_MVE_FUNCTION haetae_pointwise_acc_u16_precentered_mve
  push {r4-r10, lr}
  sub sp, sp, #8
#if defined(HAETAE_NTT_ARITH_D2)
  vpush {d8-d11}
  str r3, [sp, #32]
#else
  str r3, [sp]
#endif
  add r10, r0, #1024
  movw r4, #33288
  movw r5, #64513
  rsb r5, r5, #0
  ldr r6, =-14127
  ldr r7, =-470254080
.Lacc_u16_outer:
  vmov.i32 q0, #0
  mov r8, r1
  mov r9, r2
#if defined(HAETAE_NTT_ARITH_D2)
  ldr r3, [sp, #32]
.Lacc_u16_pair_test:
  cmp r3, #2
  blo .Lacc_u16_remainder
  mov r12, r3
  mov r3, r8
  vldrh.u32 q1, [r3]
  add r3, r3, #512
  vldrh.u32 q3, [r3]
  mov r3, r12
  HAETAE_BARRETT_Q q1, q5, r4, r5
  HAETAE_BARRETT_Q q3, q5, r4, r5
  vldrw.u32 q2, [r9]
  add r12, r9, #1024
  vldrw.u32 q4, [r12]
  vmul.s32 q1, q1, q2
  vmul.s32 q3, q3, q4
  vadd.i32 q1, q1, q3
  HAETAE_BARRETT_Q q1, q5, r4, r5
  vadd.i32 q0, q0, q1
  add r8, r8, #1024
  add r9, r9, #2048
  subs r3, r3, #2
  b .Lacc_u16_pair_test
.Lacc_u16_remainder:
  cmp r3, #0
  beq .Lacc_u16_finish
  mov r12, r3
  mov r3, r8
  vldrh.u32 q1, [r3]
  mov r3, r12
  HAETAE_BARRETT_Q q1, q2, r4, r5
  vldrw.u32 q3, [r9]
  vmul.s32 q1, q1, q3
  HAETAE_BARRETT_Q q1, q2, r4, r5
  vadd.i32 q0, q0, q1
.Lacc_u16_finish:
  HAETAE_RINV_Q q1, q0, q2, r6, r7, r5
  vstrw.32 q1, [r0], #16
#else
  ldr r3, [sp]
  cmp r3, #0
  beq .Lacc_u16_store
.Lacc_u16_term:
  /* VLD(R)H requires a low GPR base in the architectural encoding. */
  mov r12, r3
  mov r3, r8
  vldrh.u32 q1, [r3]
  mov r3, r12
  HAETAE_BARRETT_Q q1, q2, r4, r5
  vldrw.u32 q3, [r9]
  vmul.s32 q1, q1, q3
  HAETAE_BARRETT_Q q1, q2, r4, r5
  HAETAE_RINV_Q q3, q1, q2, r6, r7, r5
  vadd.i32 q0, q0, q3
  add r8, r8, #512
  add r9, r9, #1024
  subs r3, r3, #1
  bne .Lacc_u16_term
.Lacc_u16_store:
  vstrw.32 q0, [r0], #16
#endif
  add r1, r1, #8
  add r2, r2, #16
  cmp r0, r10
  bne .Lacc_u16_outer
#if defined(HAETAE_NTT_ARITH_D2)
  vpop {d8-d11}
#endif
  add sp, sp, #8
  pop {r4-r10, lr}
  bx lr
HAETAE_MVE_END haetae_pointwise_acc_u16_precentered_mve
#endif

#if !defined(HAETAE_NTT_EXTERNAL_BARRETT)
HAETAE_MVE_FUNCTION haetae_barrett_tight_mve
  add r2, r0, #1024
  movw r3, #33288
  movw r12, #64513
  rsb r12, r12, #0
.Lbarrett_tight_loop:
  vldrw.u32 q0, [r1], #16
  HAETAE_BARRETT_Q q0, q1, r3, r12
  vstrw.32 q0, [r0], #16
  cmp r0, r2
  bne .Lbarrett_tight_loop
  bx lr
HAETAE_MVE_END haetae_barrett_tight_mve
#endif

#if !defined(HAETAE_NTT_EXTERNAL_CENTERED)
.macro HAETAE_CENTERED_FUNCTION name, modulus, reciprocal, half, freeze=0
HAETAE_MVE_FUNCTION \name
  .if \freeze
    push {r4-r6}
  .else
    push {r4-r5}
  .endif
  add r1, r0, #1024
  ldr r12, =\modulus
  movw r2, #\reciprocal
  rsb r3, r12, #0
  /* r4 is a loop invariant and must retain half for every upper correction. */
  ldr r4, =\half
  rsb r5, r4, #0
  .if \freeze
    movs r6, #0
  .endif
.Lcentered_loop_\@:
  vldrw.u32 q0, [r0]
  vqrdmulh.s32 q1, q0, r2
  vmla.s32 q0, q1, r3
  /* Inclusive upper boundary matches reference reduce32_2q's -q choice;
   * freeze functions map it back to the same canonical positive value. */
  vcmp.s32 ge, q0, r4
  vpst
  vsubt.i32 q0, q0, r12
  vcmp.s32 lt, q0, r5
  vpst
  vaddt.i32 q0, q0, r12
  .if \freeze
    vcmp.s32 lt, q0, r6
    vpst
    vaddt.i32 q0, q0, r12
  .endif
  vstrw.32 q0, [r0], #16
  cmp r0, r1
  bne .Lcentered_loop_\@
  .if \freeze
    pop {r4-r6}
  .else
    pop {r4-r5}
  .endif
  bx lr
HAETAE_MVE_END \name
.endm

HAETAE_CENTERED_FUNCTION haetae_reduce2q_mve, 129026, 16644, 64513, 0
HAETAE_CENTERED_FUNCTION haetae_freeze2q_mve, 129026, 16644, 64513, 1
HAETAE_CENTERED_FUNCTION haetae_freeze_mve, 64513, 33288, 32256, 1
#endif

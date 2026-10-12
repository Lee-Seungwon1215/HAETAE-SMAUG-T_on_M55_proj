/* SPDX-License-Identifier: MIT */

/*
 * SAM-H-LOOP
 *
 * Register map: r0=random/result, r1=table, r2=public loop count;
 * q0=random, q1=per-lane count, q2=one, q3=thresholds.
 * Memory: eight aligned sequential 16-byte loads, post-incremented; no stores.
 * Control flow and addresses are fixed and independent of random16.
 */

.syntax unified
.thumb
.arch armv8.1-m.main
.arch_extension mve

#include "../../common/abi.inc"

HAETAE_MVE_FUNCTION haetae_sample_gauss16_cdt_mve
  vdup.16 q0, r0
  vmov.i32 q1, #0
  vmov.i16 q2, #1
  ldr r1, =haetae_asm_cdt16
  movs r2, #8
.Lsam_loop:
  vldrh.u16 q3, [r1], #16
  vcmp.u16 hi, q0, q3
  vpst
  vaddt.i16 q1, q1, q2
  subs r2, r2, #1
  bne .Lsam_loop
  movs r0, #0
  vaddva.u16 r0, q1
  movs r1, #0
  bx lr
HAETAE_MVE_END haetae_sample_gauss16_cdt_mve

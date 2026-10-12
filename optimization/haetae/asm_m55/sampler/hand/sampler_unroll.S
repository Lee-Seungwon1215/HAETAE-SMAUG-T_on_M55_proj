/* SPDX-License-Identifier: MIT */

/*
 * SAM-H-UNROLL
 *
 * Same q0-q3 register map and eight aligned sequential loads as SAM-H-LOOP,
 * with the public eight-iteration control completely unrolled.  No
 * callee-saved MVE register is touched.
 */

.syntax unified
.thumb
.arch armv8.1-m.main
.arch_extension mve

#include "../../common/abi.inc"

.macro HAETAE_CDT_GROUP
  vldrh.u16 q3, [r1], #16
  vcmp.u16 hi, q0, q3
  vpst
  vaddt.i16 q1, q1, q2
.endm

HAETAE_MVE_FUNCTION haetae_sample_gauss16_cdt_mve
  vdup.16 q0, r0
  vmov.i32 q1, #0
  vmov.i16 q2, #1
  ldr r1, =haetae_asm_cdt16
  HAETAE_CDT_GROUP
  HAETAE_CDT_GROUP
  HAETAE_CDT_GROUP
  HAETAE_CDT_GROUP
  HAETAE_CDT_GROUP
  HAETAE_CDT_GROUP
  HAETAE_CDT_GROUP
  HAETAE_CDT_GROUP
  movs r0, #0
  vaddva.u16 r0, q1
  movs r1, #0
  bx lr
HAETAE_MVE_END haetae_sample_gauss16_cdt_mve

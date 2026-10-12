/* SPDX-License-Identifier: MIT */

/*
 * SAM-H-LOAD17
 *
 * ABI: a 32-byte struct result uses the AAPCS hidden result pointer in r0;
 * r1 points to exactly 17 readable bytes.  q0 receives bytes 0..15 in one MVE
 * load and byte 16 is fetched once with LDRB.  q0 is caller-saved.  r4-r5 are
 * preserved.  The implementation never reads byte 17 or later.
 */

.syntax unified
.thumb
.arch armv8.1-m.main
.arch_extension mve

#include "../../common/abi.inc"

HAETAE_MVE_FUNCTION haetae_load_gauss17_mve
  push {r4, r5}
  vldrb.u8 q0, [r1]
  vmov r2, s0
  vmov.32 r3, d0[1]
  vmov r4, s2
  vmov.32 r5, d1[1]

  strh r2, [r0, #0]
  lsr r12, r2, #16
  orr r12, r12, r3, lsl #16
  lsr r3, r3, #16
  strd r12, r3, [r0, #8]

  str r4, [r0, #16]
  uxth r4, r5
  str r4, [r0, #20]
  ldrb r1, [r1, #16]
  lsr r5, r5, #16
  orr r5, r5, r1, lsl #16
  str r5, [r0, #24]
  pop {r4, r5}
  bx lr
HAETAE_MVE_END haetae_load_gauss17_mve

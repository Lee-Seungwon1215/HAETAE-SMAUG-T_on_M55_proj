/* SPDX-License-Identifier: MIT */

/*
 * AAPCS32/MVE policy for the HAETAE handwritten backend.
 *
 * q0-q3 (s0-s15) are caller-saved and are the default scratch vectors.
 * q4-q7 (d8-d15) are callee-saved and must be preserved when used.
 * All public functions keep SP 8-byte aligned and return uint64_t in r0:r1.
 */

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

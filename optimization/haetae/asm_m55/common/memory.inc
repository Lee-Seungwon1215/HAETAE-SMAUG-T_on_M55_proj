/* SPDX-License-Identifier: MIT */

/*
 * Table and coefficient bases are at least 16-byte aligned when owned here.
 * Sequential loops prefer post-increment addressing. The Cortex-M55 Slothy
 * model's normal store/load hazard remains enabled; DTCM bank bits are [3:2].
 */
.macro HAETAE_ALIGN_VECTOR
  .balign 16
.endm

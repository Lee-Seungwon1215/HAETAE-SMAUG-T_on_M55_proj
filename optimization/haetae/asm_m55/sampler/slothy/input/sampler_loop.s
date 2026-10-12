// SPDX-License-Identifier: MIT
// Symbolic normalization of SAM-H-LOOP for parser capability testing.
// q0=random, q1=count, q2=one, q3=threshold, r1=table, lr=8 iterations.

.syntax unified
sampler_loop:
    vldrh.u16 q3, [r1], #16
    vcmp.u16 hi, q0, q3
    vpst
    vaddt.i16 q1, q1, q2
    le lr, sampler_loop
sampler_loop_end:

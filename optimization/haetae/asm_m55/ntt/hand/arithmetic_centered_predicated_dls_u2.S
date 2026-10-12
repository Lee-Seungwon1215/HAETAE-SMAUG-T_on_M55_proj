// SPDX-License-Identifier: MIT

// Two-vector handwritten predicate schedule. This retains the shortest MVE
// correction dataflow while overlapping the independent Barrett reductions
// and halving low-overhead-loop control relative to the U1 candidate.

.syntax unified
.thumb
.arch armv8.1-m.main
.arch_extension mve

.macro HAETAE_CORRECT_ONE value, modulus, half, neghalf, zero, freeze=0
        vcmp.s32 ge, \value, \half
        vpst
        vsubt.i32 \value, \value, \modulus
        vcmp.s32 lt, \value, \neghalf
        vpst
        vaddt.i32 \value, \value, \modulus
  .if \freeze
        vcmp.s32 lt, \value, \zero
        vpst
        vaddt.i32 \value, \value, \modulus
  .endif
.endm

.macro HAETAE_CENTERED_DLS_U2 name, modulus, reciprocal, half, freeze=0
.section .text.\name, "ax", %progbits
.align 2
.global \name
.type \name, %function
.thumb_func
\name:
        push {r4-r6, lr}
        ldr r1, =\modulus
        movw r2, #\reciprocal
        rsb r3, r1, #0
        ldr r4, =\half
        rsb r5, r4, #0
        movs r6, #0
        mov lr, #32
        dls lr, lr
\name\()_u2_loop:
        vldrw.u32 q0, [r0]
        vldrw.u32 q2, [r0, #16]
        vqrdmulh.s32 q1, q0, r2
        vqrdmulh.s32 q3, q2, r2
        vmla.s32 q0, q1, r3
        vmla.s32 q2, q3, r3
        HAETAE_CORRECT_ONE q0, r1, r4, r5, r6, \freeze
        HAETAE_CORRECT_ONE q2, r1, r4, r5, r6, \freeze
        vstrw.32 q0, [r0], #16
        vstrw.32 q2, [r0], #16
        le lr, \name\()_u2_loop
        pop {r4-r6, lr}
        bx lr
.size \name, .-\name
.endm

HAETAE_CENTERED_DLS_U2 haetae_reduce2q_mve, 129026, 16644, 64513, 0
HAETAE_CENTERED_DLS_U2 haetae_freeze2q_mve, 129026, 16644, 64513, 1
HAETAE_CENTERED_DLS_U2 haetae_freeze_mve, 64513, 33288, 32256, 1

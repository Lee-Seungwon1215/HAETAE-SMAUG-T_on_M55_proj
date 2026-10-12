// SPDX-License-Identifier: MIT

// Low-overhead-loop handwritten control candidates retaining the original
// predicate-based correction dataflow. This file is also the concrete input
// used to test Slothy's vcmp/vpst support.

.syntax unified
.thumb
.arch armv8.1-m.main
.arch_extension mve

.macro HAETAE_CENTERED_DLS name, modulus, reciprocal, half, freeze=0
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
        mov lr, #64
        dls lr, lr
\name\()_loop:
        vldrw.u32 q0, [r0]
        vqrdmulh.s32 q1, q0, r2
        vmla.s32 q0, q1, r3
        vcmp.s32 ge, q0, r4
        vpst
        vsubt.i32 q0, q0, r1
        vcmp.s32 lt, q0, r5
        vpst
        vaddt.i32 q0, q0, r1
  .if \freeze
        vcmp.s32 lt, q0, r6
        vpst
        vaddt.i32 q0, q0, r1
  .endif
        vstrw.32 q0, [r0], #16
        le lr, \name\()_loop
        pop {r4-r6, lr}
        bx lr
.size \name, .-\name
.endm

HAETAE_CENTERED_DLS haetae_reduce2q_mve, 129026, 16644, 64513, 0
HAETAE_CENTERED_DLS haetae_freeze2q_mve, 129026, 16644, 64513, 1
HAETAE_CENTERED_DLS haetae_freeze_mve, 64513, 33288, 32256, 1

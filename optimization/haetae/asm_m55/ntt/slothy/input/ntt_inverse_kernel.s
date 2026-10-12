// Copyright (c) 2022 Arm Limited
// Copyright (c) 2022 Hanno Becker
// Copyright (c) 2023 Amin Abdulrahman, Matthias Kannwischer
// SPDX-License-Identifier: MIT
//
// HAETAE q=64513 inverse GS pair, normalized as a standalone hot-loop
// scheduling input.  Constants, transform semantics and validation are local;
// the loop-shaping method follows the pinned upstream Dilithium examples.

.syntax unified
.thumb
.arch armv8.1-m.main
.arch_extension mve

.macro haetae_gs left, right, root, twisted
        vadd.u32       q4,     \left,  \right
        vsub.u32       \right, \left,  \right
        vmul.s32       \left,  \right, \root
        vqrdmulh.s32   \right, \right, \twisted
        vmla.s32       \left,  \right, r12
        vmov.i32       \right, \left
        vmov.i32       \left,  q4
.endm

.section .text.haetae_invntt_upper_kernel_slothy, "ax", %progbits
.align 2
.global haetae_invntt_upper_kernel_slothy
.type haetae_invntt_upper_kernel_slothy, %function
.thumb_func
haetae_invntt_upper_kernel_slothy:
        push {r4-r12, lr}
        vpush {d8-d15}
        movw r12, #64513
        rsb r12, r12, #0
        mov lr, #16
        dls lr, lr
haetae_invntt_upper_loop:
        vldrw.u32 q0, [r0]
        vldrw.u32 q1, [r1]
        vldrw.u32 q2, [r2]
        vldrw.u32 q3, [r3]
        haetae_gs q0, q1, r6, r7
        haetae_gs q2, q3, r8, r9
        haetae_gs q0, q2, r4, r5
        haetae_gs q1, q3, r4, r5
        vstrw.32 q0, [r0], #16
        vstrw.32 q1, [r1], #16
        vstrw.32 q2, [r2], #16
        vstrw.32 q3, [r3], #16
        le lr, haetae_invntt_upper_loop
        vpop {d8-d15}
        pop {r4-r12, lr}
        bx lr
.size haetae_invntt_upper_kernel_slothy, .-haetae_invntt_upper_kernel_slothy

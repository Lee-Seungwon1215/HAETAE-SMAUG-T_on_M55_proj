// SPDX-License-Identifier: MIT

// Complete centered reductions rewritten without MVE predicate state so that
// Slothy can model every instruction. For reduced x and positive half h:
//   ((h - 1 - x) >> 31) is -1 exactly when x >= h;
//   ((x + h) >> 31) is -1 exactly when x < -h.
// Multiplication of those masks by +/-modulus performs the same corrections as
// the predicate-based hand implementation without data-dependent control.

.syntax unified
.thumb
.arch armv8.1-m.main
.arch_extension mve

.section .text.haetae_reduce2q_mve, "ax", %progbits
.align 2
.global haetae_reduce2q_mve
.type haetae_reduce2q_mve, %function
.thumb_func
haetae_reduce2q_mve:
        push {lr}
        ldr r1, =129026
        movw r2, #16644
        rsb r3, r1, #0
        ldr r12, =64513
        sub r2, r12, #1
        vdup.32 q2, r2
        movw r2, #16644
        mov lr, #64
        dls lr, lr
haetae_reduce2q_mask_loop:
        vldrw.u32 q0, [r0]
        vqrdmulh.s32 q1, q0, r2
        vmla.s32 q0, q1, r3
        vsub.i32 q1, q2, q0
        vshr.s32 q1, q1, #31
        vmla.s32 q0, q1, r1
        vadd.i32 q1, q0, r12
        vshr.s32 q1, q1, #31
        vmla.s32 q0, q1, r3
        vstrw.32 q0, [r0], #16
        le lr, haetae_reduce2q_mask_loop
        pop {lr}
        bx lr
.size haetae_reduce2q_mve, .-haetae_reduce2q_mve

.section .text.haetae_freeze2q_mve, "ax", %progbits
.align 2
.global haetae_freeze2q_mve
.type haetae_freeze2q_mve, %function
.thumb_func
haetae_freeze2q_mve:
        push {lr}
        ldr r1, =129026
        movw r2, #16644
        rsb r3, r1, #0
        ldr r12, =64513
        sub r2, r12, #1
        vdup.32 q2, r2
        movw r2, #16644
        mov lr, #64
        dls lr, lr
haetae_freeze2q_mask_loop:
        vldrw.u32 q0, [r0]
        vqrdmulh.s32 q1, q0, r2
        vmla.s32 q0, q1, r3
        vsub.i32 q1, q2, q0
        vshr.s32 q1, q1, #31
        vmla.s32 q0, q1, r1
        vadd.i32 q1, q0, r12
        vshr.s32 q1, q1, #31
        vmla.s32 q0, q1, r3
        vshr.s32 q1, q0, #31
        vmla.s32 q0, q1, r3
        vstrw.32 q0, [r0], #16
        le lr, haetae_freeze2q_mask_loop
        pop {lr}
        bx lr
.size haetae_freeze2q_mve, .-haetae_freeze2q_mve

.section .text.haetae_freeze_mve, "ax", %progbits
.align 2
.global haetae_freeze_mve
.type haetae_freeze_mve, %function
.thumb_func
haetae_freeze_mve:
        push {lr}
        ldr r1, =64513
        movw r2, #33288
        rsb r3, r1, #0
        ldr r12, =32256
        sub r2, r12, #1
        vdup.32 q2, r2
        movw r2, #33288
        mov lr, #64
        dls lr, lr
haetae_freeze_mask_loop:
        vldrw.u32 q0, [r0]
        vqrdmulh.s32 q1, q0, r2
        vmla.s32 q0, q1, r3
        vsub.i32 q1, q2, q0
        vshr.s32 q1, q1, #31
        vmla.s32 q0, q1, r1
        vadd.i32 q1, q0, r12
        vshr.s32 q1, q1, #31
        vmla.s32 q0, q1, r3
        vshr.s32 q1, q0, #31
        vmla.s32 q0, q1, r3
        vstrw.32 q0, [r0], #16
        le lr, haetae_freeze_mask_loop
        pop {lr}
        bx lr
.size haetae_freeze_mve, .-haetae_freeze_mve

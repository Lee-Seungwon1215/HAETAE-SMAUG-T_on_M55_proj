// SPDX-License-Identifier: MIT
//
// Complete HAETAE q=64513 pointwise/R-inverse public leaf for a low-memory
// Slothy schedule. q4-q7 are reserved when this input is scheduled, so the
// public function uses caller-saved q0-q3 only and needs no MVE stack frame.

.syntax unified
.thumb
.arch armv8.1-m.main
.arch_extension mve

.macro haetae_barrett value, quotient
        vqrdmulh.s32 \quotient, \value, r4
        vmla.s32     \value,    \quotient, r5
.endm

.section .text.haetae_pointwise_mve, "ax", %progbits
.align 2
.global haetae_pointwise_mve
.type haetae_pointwise_mve, %function
.thumb_func
haetae_pointwise_mve:
        push {r4-r7, lr}
        movw r4, #33288
        movw r5, #64513
        rsb r5, r5, #0
        ldr r6, =-14127
        ldr r7, =-470254080
        mov lr, #64
        dls lr, lr
haetae_pointwise_full_lowmem_loop:
        vldrw.u32 q0, [r1], #16
        vldrw.u32 q1, [r2], #16
        haetae_barrett q0, q2
        haetae_barrett q1, q2
        vmul.s32 q0, q0, q1
        haetae_barrett q0, q2
        vmul.s32 q1, q0, r6
        vqrdmulh.s32 q2, q0, r7
        vmla.s32 q1, q2, r5
        vstrw.32 q1, [r0], #16
        le lr, haetae_pointwise_full_lowmem_loop
        pop {r4-r7, lr}
        bx lr
.size haetae_pointwise_mve, .-haetae_pointwise_mve

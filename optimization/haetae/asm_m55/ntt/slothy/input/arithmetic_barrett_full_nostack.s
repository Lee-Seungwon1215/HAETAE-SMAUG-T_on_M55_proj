// SPDX-License-Identifier: MIT

// Complete q=64513 Barrett tightening function. r2 is otherwise unused, so it
// retains the return address across the low-overhead loop and removes the
// stack write/read present in the first Slothy input.

.syntax unified
.thumb
.arch armv8.1-m.main
.arch_extension mve

.section .text.haetae_barrett_tight_mve, "ax", %progbits
.align 2
.global haetae_barrett_tight_mve
.type haetae_barrett_tight_mve, %function
.thumb_func
haetae_barrett_tight_mve:
        mov r2, lr
        movw r3, #33288
        movw r12, #64513
        rsb r12, r12, #0
        mov lr, #64
        dls lr, lr
haetae_barrett_nostack_loop:
        vldrw.u32 q0, [r1], #16
        vqrdmulh.s32 q1, q0, r3
        vmla.s32 q0, q1, r12
        vstrw.32 q0, [r0], #16
        le lr, haetae_barrett_nostack_loop
        bx r2
.size haetae_barrett_tight_mve, .-haetae_barrett_tight_mve

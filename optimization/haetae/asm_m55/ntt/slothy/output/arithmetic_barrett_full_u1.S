// SPDX-License-Identifier: MIT

// Complete q=64513 one-pass Barrett tightening function normalized for Slothy.

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
        push {lr}
        movw r3, #33288
        movw r12, #64513
        rsb r12, r12, #0
        mov lr, #64
        dls lr, lr
.p2align 2
haetae_barrett_full_loop:
                                       // Instructions:    4
                                       // Expected cycles: 5
                                       // Expected IPC:    0.80
                                       //
                                       // Cycle bound:     5.0
                                       // IPC bound:       0.80
                                       //
                                       // Wall time:     0.04s
                                       // User time:     0.04s
                                       //
                                       // ----- cycle (expected) ------>
                                       // 0                        25
                                       // |------------------------|----
        vldrw.U32 q1, [r1], #16        // *.............................
        vqrdmulh.S32 q2, q1, r3        // .*............................
        vmla.S32 q1, q2, r12           // ...*..........................
        vstrw.32 q1, [r0], #16         // ....*.........................

                                        // ------ cycle (expected) ------>
                                        // 0                        25
                                        // |------------------------|-----
        // vldrw.u32 q0, [r1], #16      // *..............................
        // vqrdmulh.s32 q1, q0, r3      // .*.............................
        // vmla.s32 q0, q1, r12         // ...*...........................
        // vstrw.32 q0, [r0], #16       // ....*..........................

        le lr, haetae_barrett_full_loop
        pop {lr}
        bx lr
.size haetae_barrett_tight_mve, .-haetae_barrett_tight_mve

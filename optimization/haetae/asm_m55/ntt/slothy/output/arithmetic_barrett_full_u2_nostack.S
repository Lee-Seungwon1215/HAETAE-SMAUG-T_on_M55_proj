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
                                       // Instructions:    3
                                       // Expected cycles: 3
                                       // Expected IPC:    1.00
                                       //
                                       // Cycle bound:     3.0
                                       // IPC bound:       1.00
                                       //
                                       // Wall time:     0.00s
                                       // User time:     0.00s
                                       //
                                       // ----- cycle (expected) ------>
                                       // 0                        25
                                       // |------------------------|----
        vldrw.U32 q3, [r1], #16        // *.............................
        vqrdmulh.S32 q2, q3, r3        // .*............................
        vldrw.U32 q0, [r1], #16        // ..*...........................

                                        // ------ cycle (expected) ------>
                                        // 0                        25
                                        // |------------------------|-----
        // vldrw.U32 q3, [r1], #16      // *..............................
        // vqrdmulh.S32 q2, q3, r3      // .*.............................
        // vldrw.U32 q0, [r1], #16      // ..*............................

        lsr lr, lr, #1
        sub lr, lr, #1
.p2align 2
haetae_barrett_nostack_loop:
                                       // Instructions:    8
                                       // Expected cycles: 10
                                       // Expected IPC:    0.80
                                       //
                                       // Cycle bound:     10.0
                                       // IPC bound:       0.80
                                       //
                                       // Wall time:     0.06s
                                       // User time:     0.06s
                                       //
                                       // ----- cycle (expected) ------>
                                       // 0                        25
                                       // |------------------------|----
        vmla.S32 q3, q2, r12           // *.............................
        vstrw.32 q3, [r0], #16         // .*............................
        vqrdmulh.S32 q2, q0, r3        // ..*...........................
        vldrw.U32 q3, [r1], #16        // ....e.........................
        vmla.S32 q0, q2, r12           // .....*........................
        vstrw.32 q0, [r0], #16         // ......*.......................
        vqrdmulh.S32 q2, q3, r3        // .......e......................
        vldrw.U32 q0, [r1], #16        // .........e....................

                                        // ------ cycle (expected) ------>
                                        // 0                        25
                                        // |------------------------|-----
        // vldrw.u32 q0, [r1], #16      // e.....'...~.....'...~.....'....
        // vqrdmulh.s32 q1, q0, r3      // ...e..'......~..'......~..'....
        // vmla.s32 q0, q1, r12         // ......*.........~.........~....
        // vstrw.32 q0, [r0], #16       // ......'*........'~........'~...
        // vldrw.u32 q0, [r1], #16      // .....e'........~'........~'....
        // vqrdmulh.s32 q1, q0, r3      // ......'.*.......'.~.......'.~..
        // vmla.s32 q0, q1, r12         // .~....'....*....'....~....'....
        // vstrw.32 q0, [r0], #16       // ..~...'.....*...'.....~...'....

        le lr, haetae_barrett_nostack_loop
                                       // Instructions:    5
                                       // Expected cycles: 6
                                       // Expected IPC:    0.83
                                       //
                                       // Cycle bound:     6.0
                                       // IPC bound:       0.83
                                       //
                                       // Wall time:     0.00s
                                       // User time:     0.00s
                                       //
                                       // ----- cycle (expected) ------>
                                       // 0                        25
                                       // |------------------------|----
        vmla.S32 q3, q2, r12           // *.............................
        vstrw.32 q3, [r0], #16         // .*............................
        vqrdmulh.S32 q2, q0, r3        // ..*...........................
        vmla.S32 q0, q2, r12           // ....*.........................
        vstrw.32 q0, [r0], #16         // .....*........................

                                        // ------ cycle (expected) ------>
                                        // 0                        25
                                        // |------------------------|-----
        // vmla.S32 q3, q2, r12         // *..............................
        // vstrw.32 q3, [r0], #16       // .*.............................
        // vqrdmulh.S32 q2, q0, r3      // ..*............................
        // vmla.S32 q0, q2, r12         // ....*..........................
        // vstrw.32 q0, [r0], #16       // .....*.........................

        bx r2
.size haetae_barrett_tight_mve, .-haetae_barrett_tight_mve

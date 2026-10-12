// SPDX-License-Identifier: MIT
//
// Complete two-vector-block HAETAE q=64513 pointwise accumulators.
// Two adjacent 16-byte coefficient vectors are consumed for every public
// term pair before moving to the next 1 KiB-spaced term.  The arithmetic is
// the same proven dmax=2 dataflow as the low-memory candidate; only the data
// traversal is changed to consume a complete 32-byte cache line at once.

.syntax unified
.thumb
.arch armv8.1-m.main
.arch_extension mve

.macro haetae_barrett value, quotient
        vqrdmulh.s32 \quotient, \value, r4
        vmla.s32     \value,    \quotient, r5
.endm

.macro haetae_rinv dst, value, quotient
        vmul.s32     \dst,      \value, r6
        vqrdmulh.s32 \quotient, \value, r7
        vmla.s32     \dst,      \quotient, r5
.endm

// r0=c, r1=a, r2=b, r3=public terms.  Both inputs require Barrett reduction.
.section .text.haetae_pointwise_acc_mve, "ax", %progbits
.align 2
.global haetae_pointwise_acc_mve
.type haetae_pointwise_acc_mve, %function
.thumb_func
haetae_pointwise_acc_mve:
        push {r4-r11, lr}
        sub sp, sp, #4
        vpush {d8-d15}
        mov r11, r3
        add r10, r0, #1024
        movw r4, #33288
        movw r5, #64513
        rsb r5, r5, #0
        ldr r6, =-14127
        ldr r7, =-470254080
haetae_acc_block2_outer:
        vmov.i32 q0, #0
        vmov.i32 q1, #0
        mov r8, r1
        mov r9, r2
        lsrs r12, r11, #1
        beq haetae_acc_block2_remainder
        dls lr, r12
haetae_acc_block2_pair_loop:
        // First four coefficients from both terms.
        vldrw.u32 q2, [r8]
        haetae_barrett q2, q6
        vldrw.u32 q3, [r9]
        haetae_barrett q3, q6
        vmul.s32 q2, q2, q3
        add r12, r8, #1024
        vldrw.u32 q4, [r12]
        haetae_barrett q4, q6
        add r12, r9, #1024
        vldrw.u32 q5, [r12]
        haetae_barrett q5, q6
        vmul.s32 q4, q4, q5
        vadd.i32 q2, q2, q4
        haetae_barrett q2, q6
        vadd.i32 q0, q0, q2

        // Adjacent four coefficients complete the same 32-byte cache lines.
        add r12, r8, #16
        vldrw.u32 q2, [r12]
        haetae_barrett q2, q6
        add r12, r9, #16
        vldrw.u32 q3, [r12]
        haetae_barrett q3, q6
        vmul.s32 q2, q2, q3
        add r12, r8, #1040
        vldrw.u32 q4, [r12]
        haetae_barrett q4, q6
        add r12, r9, #1040
        vldrw.u32 q5, [r12]
        haetae_barrett q5, q6
        vmul.s32 q4, q4, q5
        vadd.i32 q2, q2, q4
        haetae_barrett q2, q6
        vadd.i32 q1, q1, q2

        add r8, r8, #2048
        add r9, r9, #2048
        le lr, haetae_acc_block2_pair_loop
haetae_acc_block2_remainder:
        tst r11, #1
        beq haetae_acc_block2_finish
haetae_acc_block2_remainder_start:
        vldrw.u32 q2, [r8]
        haetae_barrett q2, q6
        vldrw.u32 q3, [r9]
        haetae_barrett q3, q6
        vmul.s32 q2, q2, q3
        haetae_barrett q2, q6
        vadd.i32 q0, q0, q2
        add r12, r8, #16
        vldrw.u32 q2, [r12]
        haetae_barrett q2, q6
        add r12, r9, #16
        vldrw.u32 q3, [r12]
        haetae_barrett q3, q6
        vmul.s32 q2, q2, q3
        haetae_barrett q2, q6
        vadd.i32 q1, q1, q2
haetae_acc_block2_remainder_end:
haetae_acc_block2_finish:
        haetae_rinv q2, q0, q6
        haetae_rinv q3, q1, q7
        vstrw.32 q2, [r0], #16
        vstrw.32 q3, [r0], #16
        add r1, r1, #32
        add r2, r2, #32
haetae_acc_block2_final_end:
        cmp r0, r10
        bne haetae_acc_block2_outer
        vpop {d8-d15}
        add sp, sp, #4
        pop {r4-r11, lr}
        bx lr
.size haetae_pointwise_acc_mve, .-haetae_pointwise_acc_mve

// r0=c, r1=a, r2=b, r3=public terms.  b is already tightly centered.
.section .text.haetae_pointwise_acc_precentered_mve, "ax", %progbits
.align 2
.global haetae_pointwise_acc_precentered_mve
.type haetae_pointwise_acc_precentered_mve, %function
.thumb_func
haetae_pointwise_acc_precentered_mve:
        push {r4-r11, lr}
        sub sp, sp, #4
        vpush {d8-d15}
        mov r11, r3
        add r10, r0, #1024
        movw r4, #33288
        movw r5, #64513
        rsb r5, r5, #0
        ldr r6, =-14127
        ldr r7, =-470254080
haetae_acc_pre_block2_outer:
        vmov.i32 q0, #0
        vmov.i32 q1, #0
        mov r8, r1
        mov r9, r2
        lsrs r12, r11, #1
        beq haetae_acc_pre_block2_remainder
        dls lr, r12
haetae_acc_pre_block2_pair_loop:
        vldrw.u32 q2, [r8]
        haetae_barrett q2, q6
        vldrw.u32 q3, [r9]
        vmul.s32 q2, q2, q3
        add r12, r8, #1024
        vldrw.u32 q4, [r12]
        haetae_barrett q4, q6
        add r12, r9, #1024
        vldrw.u32 q5, [r12]
        vmul.s32 q4, q4, q5
        vadd.i32 q2, q2, q4
        haetae_barrett q2, q6
        vadd.i32 q0, q0, q2

        add r12, r8, #16
        vldrw.u32 q2, [r12]
        haetae_barrett q2, q6
        add r12, r9, #16
        vldrw.u32 q3, [r12]
        vmul.s32 q2, q2, q3
        add r12, r8, #1040
        vldrw.u32 q4, [r12]
        haetae_barrett q4, q6
        add r12, r9, #1040
        vldrw.u32 q5, [r12]
        vmul.s32 q4, q4, q5
        vadd.i32 q2, q2, q4
        haetae_barrett q2, q6
        vadd.i32 q1, q1, q2

        add r8, r8, #2048
        add r9, r9, #2048
        le lr, haetae_acc_pre_block2_pair_loop
haetae_acc_pre_block2_remainder:
        tst r11, #1
        beq haetae_acc_pre_block2_finish
haetae_acc_pre_block2_remainder_start:
        vldrw.u32 q2, [r8]
        haetae_barrett q2, q6
        vldrw.u32 q3, [r9]
        vmul.s32 q2, q2, q3
        haetae_barrett q2, q6
        vadd.i32 q0, q0, q2
        add r12, r8, #16
        vldrw.u32 q2, [r12]
        haetae_barrett q2, q6
        add r12, r9, #16
        vldrw.u32 q3, [r12]
        vmul.s32 q2, q2, q3
        haetae_barrett q2, q6
        vadd.i32 q1, q1, q2
haetae_acc_pre_block2_remainder_end:
haetae_acc_pre_block2_finish:
        haetae_rinv q2, q0, q6
        haetae_rinv q3, q1, q7
        vstrw.32 q2, [r0], #16
        vstrw.32 q3, [r0], #16
        add r1, r1, #32
        add r2, r2, #32
haetae_acc_pre_block2_final_end:
        cmp r0, r10
        bne haetae_acc_pre_block2_outer
        vpop {d8-d15}
        add sp, sp, #4
        pop {r4-r11, lr}
        bx lr
.size haetae_pointwise_acc_precentered_mve, .-haetae_pointwise_acc_precentered_mve

// r0=c, r1=uint16_t a, r2=b, r3=public terms.  b is tightly centered.
.section .text.haetae_pointwise_acc_u16_precentered_mve, "ax", %progbits
.align 2
.global haetae_pointwise_acc_u16_precentered_mve
.type haetae_pointwise_acc_u16_precentered_mve, %function
.thumb_func
haetae_pointwise_acc_u16_precentered_mve:
        push {r4-r11, lr}
        sub sp, sp, #4
        vpush {d8-d15}
        mov r11, r3
        add r10, r0, #1024
        movw r4, #33288
        movw r5, #64513
        rsb r5, r5, #0
        ldr r6, =-14127
        ldr r7, =-470254080
haetae_acc_u16_block2_outer:
        vmov.i32 q0, #0
        vmov.i32 q1, #0
        mov r8, r1
        mov r9, r2
        lsrs r12, r11, #1
        beq haetae_acc_u16_block2_remainder
        dls lr, r12
haetae_acc_u16_block2_pair_loop:
        mov r3, r8
        vldrh.u32 q2, [r3]
        add r3, r3, #512
        vldrh.u32 q4, [r3]
        haetae_barrett q2, q6
        haetae_barrett q4, q6
        vldrw.u32 q3, [r9]
        add r12, r9, #1024
        vldrw.u32 q5, [r12]
        vmul.s32 q2, q2, q3
        vmul.s32 q4, q4, q5
        vadd.i32 q2, q2, q4
        haetae_barrett q2, q6
        vadd.i32 q0, q0, q2

        add r3, r8, #8
        vldrh.u32 q2, [r3]
        add r3, r3, #512
        vldrh.u32 q4, [r3]
        haetae_barrett q2, q6
        haetae_barrett q4, q6
        add r12, r9, #16
        vldrw.u32 q3, [r12]
        add r12, r9, #1040
        vldrw.u32 q5, [r12]
        vmul.s32 q2, q2, q3
        vmul.s32 q4, q4, q5
        vadd.i32 q2, q2, q4
        haetae_barrett q2, q6
        vadd.i32 q1, q1, q2

        add r8, r8, #1024
        add r9, r9, #2048
        le lr, haetae_acc_u16_block2_pair_loop
haetae_acc_u16_block2_remainder:
        tst r11, #1
        beq haetae_acc_u16_block2_finish
haetae_acc_u16_block2_remainder_start:
        mov r3, r8
        vldrh.u32 q2, [r3]
        haetae_barrett q2, q6
        vldrw.u32 q3, [r9]
        vmul.s32 q2, q2, q3
        haetae_barrett q2, q6
        vadd.i32 q0, q0, q2
        add r3, r8, #8
        vldrh.u32 q2, [r3]
        haetae_barrett q2, q6
        add r12, r9, #16
        vldrw.u32 q3, [r12]
        vmul.s32 q2, q2, q3
        haetae_barrett q2, q6
        vadd.i32 q1, q1, q2
haetae_acc_u16_block2_remainder_end:
haetae_acc_u16_block2_finish:
        haetae_rinv q2, q0, q6
        haetae_rinv q3, q1, q7
        vstrw.32 q2, [r0], #16
        vstrw.32 q3, [r0], #16
        add r1, r1, #16
        add r2, r2, #32
haetae_acc_u16_block2_final_end:
        cmp r0, r10
        bne haetae_acc_u16_block2_outer
        vpop {d8-d15}
        add sp, sp, #4
        pop {r4-r11, lr}
        bx lr
.size haetae_pointwise_acc_u16_precentered_mve, .-haetae_pointwise_acc_u16_precentered_mve

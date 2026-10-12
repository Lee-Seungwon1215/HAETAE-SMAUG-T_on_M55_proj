// SPDX-License-Identifier: MIT
//
// Complete HAETAE q=64513 pointwise-accumulation leaves prepared for Slothy.
// This is the proven dmax=2 dataflow: two products are combined before one
// Barrett reduction, and the accumulated groups receive one final R^-1.
// `terms` is public (0..7 for the supported HAETAE parameter sets).
//
// The three real public entry points are present here; none is a stand-alone
// representative kernel.  q6-q7 remain reserved so the ABI frame is limited
// to the callee-saved q4-q5 registers actually used by the dataflow.

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
        vpush {d8-d11}
        mov r11, r3
        add r10, r0, #1024
        movw r4, #33288
        movw r5, #64513
        rsb r5, r5, #0
        ldr r6, =-14127
        ldr r7, =-470254080
haetae_acc_full_outer:
        vmov.i32 q0, #0
        mov r8, r1
        mov r9, r2
        lsrs r12, r11, #1
        beq haetae_acc_full_remainder
        dls lr, r12
haetae_acc_full_pair_loop:
        vldrw.u32 q1, [r8]
        haetae_barrett q1, q5
        vldrw.u32 q2, [r9]
        haetae_barrett q2, q5
        vmul.s32 q1, q1, q2
        add r12, r8, #1024
        vldrw.u32 q3, [r12]
        haetae_barrett q3, q5
        add r12, r9, #1024
        vldrw.u32 q4, [r12]
        haetae_barrett q4, q5
        vmul.s32 q3, q3, q4
        vadd.i32 q1, q1, q3
        haetae_barrett q1, q5
        vadd.i32 q0, q0, q1
        add r8, r8, #2048
        add r9, r9, #2048
        le lr, haetae_acc_full_pair_loop
haetae_acc_full_remainder:
        tst r11, #1
        beq haetae_acc_full_finish
haetae_acc_full_remainder_start:
        vldrw.u32 q1, [r8]
        haetae_barrett q1, q5
        vldrw.u32 q2, [r9]
        haetae_barrett q2, q5
        vmul.s32 q1, q1, q2
        haetae_barrett q1, q5
        vadd.i32 q0, q0, q1
haetae_acc_full_remainder_end:
haetae_acc_full_finish:
        haetae_rinv q1, q0, q2
        vstrw.32 q1, [r0], #16
        add r1, r1, #16
        add r2, r2, #16
haetae_acc_full_final_end:
        cmp r0, r10
        bne haetae_acc_full_outer
        vpop {d8-d11}
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
        vpush {d8-d11}
        mov r11, r3
        add r10, r0, #1024
        movw r4, #33288
        movw r5, #64513
        rsb r5, r5, #0
        ldr r6, =-14127
        ldr r7, =-470254080
haetae_acc_pre_full_outer:
        vmov.i32 q0, #0
        mov r8, r1
        mov r9, r2
        lsrs r12, r11, #1
        beq haetae_acc_pre_full_remainder
        dls lr, r12
haetae_acc_pre_full_pair_loop:
        vldrw.u32 q1, [r8]
        haetae_barrett q1, q5
        vldrw.u32 q2, [r9]
        vmul.s32 q1, q1, q2
        add r12, r8, #1024
        vldrw.u32 q3, [r12]
        haetae_barrett q3, q5
        add r12, r9, #1024
        vldrw.u32 q4, [r12]
        vmul.s32 q3, q3, q4
        vadd.i32 q1, q1, q3
        haetae_barrett q1, q5
        vadd.i32 q0, q0, q1
        add r8, r8, #2048
        add r9, r9, #2048
        le lr, haetae_acc_pre_full_pair_loop
haetae_acc_pre_full_remainder:
        tst r11, #1
        beq haetae_acc_pre_full_finish
haetae_acc_pre_full_remainder_start:
        vldrw.u32 q1, [r8]
        haetae_barrett q1, q5
        vldrw.u32 q2, [r9]
        vmul.s32 q1, q1, q2
        haetae_barrett q1, q5
        vadd.i32 q0, q0, q1
haetae_acc_pre_full_remainder_end:
haetae_acc_pre_full_finish:
        haetae_rinv q1, q0, q2
        vstrw.32 q1, [r0], #16
        add r1, r1, #16
        add r2, r2, #16
haetae_acc_pre_full_final_end:
        cmp r0, r10
        bne haetae_acc_pre_full_outer
        vpop {d8-d11}
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
        vpush {d8-d11}
        mov r11, r3
        add r10, r0, #1024
        movw r4, #33288
        movw r5, #64513
        rsb r5, r5, #0
        ldr r6, =-14127
        ldr r7, =-470254080
haetae_acc_u16_full_outer:
        vmov.i32 q0, #0
        mov r8, r1
        mov r9, r2
        lsrs r12, r11, #1
        beq haetae_acc_u16_full_remainder
        dls lr, r12
haetae_acc_u16_full_pair_loop:
        // VLD(R)H requires a low GPR base in the architectural encoding.
        mov r3, r8
        vldrh.u32 q1, [r3]
        add r3, r3, #512
        vldrh.u32 q3, [r3]
        haetae_barrett q1, q5
        haetae_barrett q3, q5
        vldrw.u32 q2, [r9]
        add r12, r9, #1024
        vldrw.u32 q4, [r12]
        vmul.s32 q1, q1, q2
        vmul.s32 q3, q3, q4
        vadd.i32 q1, q1, q3
        haetae_barrett q1, q5
        vadd.i32 q0, q0, q1
        add r8, r8, #1024
        add r9, r9, #2048
        le lr, haetae_acc_u16_full_pair_loop
haetae_acc_u16_full_remainder:
        tst r11, #1
        beq haetae_acc_u16_full_finish
haetae_acc_u16_full_remainder_start:
        mov r3, r8
        vldrh.u32 q1, [r3]
        haetae_barrett q1, q2
        vldrw.u32 q3, [r9]
        vmul.s32 q1, q1, q3
        haetae_barrett q1, q2
        vadd.i32 q0, q0, q1
haetae_acc_u16_full_remainder_end:
haetae_acc_u16_full_finish:
        haetae_rinv q1, q0, q2
        vstrw.32 q1, [r0], #16
        add r1, r1, #8
        add r2, r2, #16
haetae_acc_u16_full_final_end:
        cmp r0, r10
        bne haetae_acc_u16_full_outer
        vpop {d8-d11}
        add sp, sp, #4
        pop {r4-r11, lr}
        bx lr
.size haetae_pointwise_acc_u16_precentered_mve, .-haetae_pointwise_acc_u16_precentered_mve

// Copyright (c) 2022 Arm Limited
// Copyright (c) 2022 Hanno Becker
// Copyright (c) 2023 Amin Abdulrahman, Matthias Kannwischer
// SPDX-License-Identifier: MIT
//
// Complete HAETAE q=64513 inverse NTT normalized for Slothy.  The paired
// layer/no-transpose structure follows the pinned upstream Dilithium method;
// roots, scale, ABI, coefficient order and validation are HAETAE-specific.

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

.macro haetae_mul value, root, twisted
        vmul.s32       q4,     \value, \root
        vqrdmulh.s32   \value, \value, \twisted
        vmla.s32       q4,     \value, r12
        vmov.i32       \value, q4
.endm

.section .text.haetae_invntt_asm_paired_slothy, "ax", %progbits
.align 2
.global haetae_invntt_asm_paired_slothy
.type haetae_invntt_asm_paired_slothy, %function
.thumb_func
haetae_invntt_asm_paired_slothy:
        push {r4-r12, lr}
        // Slothy can allocate the complete callee-saved MVE bank.
        vpush {d8-d15}

        mov r10, r0
        movw r12, #64513
        rsb r12, r12, #0

        // Reverse pair (1,2): VLD4 coefficient records and vector roots.
        ldr r11, =haetae_asm_paired_inverse_lower
        mov lr, #16
        dls lr, lr
haetae_inverse_layer12_loop:
        vld40.u32 {q0, q1, q2, q3}, [r0]
        vld41.u32 {q0, q1, q2, q3}, [r0]
        vld42.u32 {q0, q1, q2, q3}, [r0]
        vld43.u32 {q0, q1, q2, q3}, [r0]
        vldrw.u32 q5, [r11, #32]
        vldrw.u32 q6, [r11, #48]
        haetae_gs q0, q1, q5, q6
        vldrw.u32 q5, [r11, #64]
        vldrw.u32 q6, [r11, #80]
        haetae_gs q2, q3, q5, q6
        vldrw.u32 q5, [r11, #0]
        vldrw.u32 q6, [r11, #16]
        haetae_gs q0, q2, q5, q6
        haetae_gs q1, q3, q5, q6
        vst40.u32 {q0, q1, q2, q3}, [r0]
        vst41.u32 {q0, q1, q2, q3}, [r0]
        vst42.u32 {q0, q1, q2, q3}, [r0]
        vst43.u32 {q0, q1, q2, q3}, [r0]!
        add r11, r11, #96
        le lr, haetae_inverse_layer12_loop

        // Reverse pair (4,8): roots change for every 16-coefficient block.
        mov r0, r10
        ldr r11, =haetae_asm_paired_inverse_upper
        mov lr, #16
        dls lr, lr
haetae_inverse_layer34_loop:
        ldrd r4, r5, [r11], #24
        ldrd r6, r7, [r11, #-16]
        ldrd r8, r9, [r11, #-8]
        vldrw.u32 q0, [r0]
        vldrw.u32 q1, [r0, #16]
        vldrw.u32 q2, [r0, #32]
        vldrw.u32 q3, [r0, #48]
        haetae_gs q0, q1, r6, r7
        haetae_gs q2, q3, r8, r9
        haetae_gs q0, q2, r4, r5
        haetae_gs q1, q3, r4, r5
        vstrw.u32 q0, [r0], #64
        vstrw.u32 q1, [r0, #(-64+16)]
        vstrw.u32 q2, [r0, #(-64+32)]
        vstrw.u32 q3, [r0, #(-64+48)]
        le lr, haetae_inverse_layer34_loop

        // Reverse pair (16,32): four root configurations, four vectors each.
        mov r0, r10
        mov r1, #4
haetae_inverse_layer56_outer:
        ldrd r4, r5, [r11], #8
        ldrd r6, r7, [r11], #8
        ldrd r8, r9, [r11], #8
        mov lr, #4
        dls lr, lr
haetae_inverse_layer56_loop:
        vldrw.u32 q0, [r0]
        vldrw.u32 q1, [r0, #64]
        vldrw.u32 q2, [r0, #128]
        vldrw.u32 q3, [r0, #192]
        haetae_gs q0, q1, r6, r7
        haetae_gs q2, q3, r8, r9
        haetae_gs q0, q2, r4, r5
        haetae_gs q1, q3, r4, r5
        vstrw.u32 q0, [r0], #16
        vstrw.u32 q1, [r0, #(64-16)]
        vstrw.u32 q2, [r0, #(128-16)]
        vstrw.u32 q3, [r0, #(192-16)]
        le lr, haetae_inverse_layer56_loop
        add r0, r0, #192
        subs r1, r1, #1
        bne haetae_inverse_layer56_outer

        // Reverse pair (64,128), with the mandatory 1/256 scale fused before
        // each store.  The root stream is dead after the three scalar pairs,
        // so r10/r11 can hold the final scale pair.
        mov r0, r10
        add r1, r10, #256
        add r2, r10, #512
        add r3, r10, #768
        ldrd r4, r5, [r11], #8
        ldrd r6, r7, [r11], #8
        ldrd r8, r9, [r11], #8
        ldr lr, =haetae_asm_inverse_scale
        ldrd r10, r11, [lr]
        mov lr, #16
        dls lr, lr
haetae_inverse_layer78_loop:
        vldrw.u32 q0, [r0]
        vldrw.u32 q1, [r1]
        vldrw.u32 q2, [r2]
        vldrw.u32 q3, [r3]
        haetae_gs q0, q1, r6, r7
        haetae_gs q2, q3, r8, r9
        haetae_gs q0, q2, r4, r5
        haetae_gs q1, q3, r4, r5
        haetae_mul q0, r10, r11
        haetae_mul q1, r10, r11
        haetae_mul q2, r10, r11
        haetae_mul q3, r10, r11
        vstrw.u32 q0, [r0], #16
        vstrw.u32 q1, [r1], #16
        vstrw.u32 q2, [r2], #16
        vstrw.u32 q3, [r3], #16
        le lr, haetae_inverse_layer78_loop

        vpop {d8-d15}
        pop {r4-r12, lr}
        bx lr
.size haetae_invntt_asm_paired_slothy, .-haetae_invntt_asm_paired_slothy

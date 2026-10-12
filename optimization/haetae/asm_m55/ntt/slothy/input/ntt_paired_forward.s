// Copyright (c) 2022 Arm Limited
// Copyright (c) 2022 Hanno Becker
// Copyright (c) 2023 Amin Abdulrahman, Matthias Kannwischer
// SPDX-License-Identifier: MIT
//
// HAETAE adaptation: q=64513 arithmetic and HAETAE-specific reordered roots.
// The paired layer/no-transpose structure follows the cited upstream Slothy
// Dilithium example, while constants, tables, ABI and validation are HAETAE's.

.syntax unified
.thumb
.arch armv8.1-m.main
.arch_extension mve

.macro haetae_mulmod dst, src, root, twisted
        vmul.s32       \dst,  \src, \root
        vqrdmulh.s32   \src,  \src, \twisted
        vmla.s32       \dst,  \src, modulus
.endm

.macro haetae_ct a, b, root, twisted
        haetae_mulmod tmp, \b, \root, \twisted
        vsub.u32       \b,    \a, tmp
        vadd.u32       \a,    \a, tmp
.endm

.section .text.haetae_ntt_asm_paired_slothy, "ax", %progbits
.align 2
.global haetae_ntt_asm_paired_slothy
.type haetae_ntt_asm_paired_slothy, %function
.thumb_func
haetae_ntt_asm_paired_slothy:
        push {r4-r12, lr}
        // Slothy may allocate q7 even though the normalized source uses q0-q6.
        // Preserve the complete MVE callee-saved set that the generated core
        // can occupy; otherwise d14-d15 leak into the caller.
        vpush {d8-d15}

        modulus  .req r12
        root_ptr .req r11
        movw modulus, #64513
        rsb modulus, modulus, #0
        ldr root_ptr, =haetae_asm_paired_forward_upper

        in_low  .req r0
        in_high .req r1
        add in_high, in_low, #(4*128)

        root0         .req r2
        root0_twisted .req r3
        root1         .req r4
        root1_twisted .req r5
        root2         .req r6
        root2_twisted .req r7

        data0 .req q0
        data1 .req q1
        data2 .req q2
        data3 .req q3
        tmp   .req q4

        // Paired layers (128,64).
        ldrd root0, root0_twisted, [root_ptr], #8
        ldrd root1, root1_twisted, [root_ptr], #8
        ldrd root2, root2_twisted, [root_ptr], #8
        mov lr, #16
haetae_layer12_loop:
        vldrw.u32 data0, [in_low]
        vldrw.u32 data1, [in_low, #(4*64)]
        vldrw.u32 data2, [in_high]
        vldrw.u32 data3, [in_high, #(4*64)]
        haetae_ct data0, data2, root0, root0_twisted
        haetae_ct data1, data3, root0, root0_twisted
        haetae_ct data0, data1, root1, root1_twisted
        haetae_ct data2, data3, root2, root2_twisted
        vstrw.u32 data0, [in_low], #16
        vstrw.u32 data1, [in_low, #(4*64 - 16)]
        vstrw.u32 data2, [in_high], #16
        vstrw.u32 data3, [in_high, #(4*64 - 16)]
        le lr, haetae_layer12_loop

        .unreq in_high
        .unreq in_low
        in .req r0

        // Paired layers (32,16): four root configurations.
        sub in, in, #(64*4)
        count .req r1
        mov count, #4
haetae_layer34_outer:
        ldrd root0, root0_twisted, [root_ptr], #8
        ldrd root1, root1_twisted, [root_ptr], #8
        ldrd root2, root2_twisted, [root_ptr], #8
        mov lr, #4
haetae_layer34_loop:
        vldrw.u32 data0, [in]
        vldrw.u32 data1, [in, #(4*16)]
        vldrw.u32 data2, [in, #(4*2*16)]
        vldrw.u32 data3, [in, #(4*3*16)]
        haetae_ct data0, data2, root0, root0_twisted
        haetae_ct data1, data3, root0, root0_twisted
        haetae_ct data0, data1, root1, root1_twisted
        haetae_ct data2, data3, root2, root2_twisted
        vstrw.u32 data0, [in], #16
        vstrw.u32 data1, [in, #(4*16 - 16)]
        vstrw.u32 data2, [in, #(4*2*16 - 16)]
        vstrw.u32 data3, [in, #(4*3*16 - 16)]
        le lr, haetae_layer34_loop
        add in, in, #(4*64 - 4*16)
        subs count, count, #1
        bne haetae_layer34_outer

        // Paired layers (8,4): roots change every vector block.
        sub in, in, #(4*256)
        mov lr, #16
haetae_layer56_loop:
        ldrd root0, root0_twisted, [root_ptr], #24
        ldrd root1, root1_twisted, [root_ptr, #-16]
        ldrd root2, root2_twisted, [root_ptr, #-8]
        vldrw.u32 data0, [in]
        vldrw.u32 data1, [in, #(4*4)]
        vldrw.u32 data2, [in, #(4*2*4)]
        vldrw.u32 data3, [in, #(4*3*4)]
        haetae_ct data0, data2, root0, root0_twisted
        haetae_ct data1, data3, root0, root0_twisted
        haetae_ct data0, data1, root1, root1_twisted
        haetae_ct data2, data3, root2, root2_twisted
        vstrw.u32 data0, [in], #64
        vstrw.u32 data1, [in, #(-64+16)]
        vstrw.u32 data2, [in, #(-64+32)]
        vstrw.u32 data3, [in, #(-64+48)]
        le lr, haetae_layer56_loop

        // Paired layers (2,1): no-transpose VLD4 plus vector roots.
        sub in, in, #(4*256)
        .unreq root0
        .unreq root0_twisted
        .unreq root1
        .unreq root1_twisted
        .unreq root2
        .unreq root2_twisted
        root0         .req q5
        root0_twisted .req q6
        root1         .req q5
        root1_twisted .req q6
        root2         .req q5
        root2_twisted .req q6
        ldr root_ptr, =haetae_asm_paired_forward_lower
        mov lr, #16
haetae_layer78_loop:
        vld40.u32 {data0, data1, data2, data3}, [in]
        vld41.u32 {data0, data1, data2, data3}, [in]
        vld42.u32 {data0, data1, data2, data3}, [in]
        vld43.u32 {data0, data1, data2, data3}, [in]
        vldrw.u32 root0, [root_ptr], #96
        vldrw.u32 root0_twisted, [root_ptr, #(16-96)]
        haetae_ct data0, data2, root0, root0_twisted
        haetae_ct data1, data3, root0, root0_twisted
        vldrw.u32 root1, [root_ptr, #(32-96)]
        vldrw.u32 root1_twisted, [root_ptr, #(48-96)]
        haetae_ct data0, data1, root1, root1_twisted
        vldrw.u32 root2, [root_ptr, #(64-96)]
        vldrw.u32 root2_twisted, [root_ptr, #(80-96)]
        haetae_ct data2, data3, root2, root2_twisted
        // Re-interleave to preserve HAETAE's canonical coefficient order.
        // A contiguous VSTRW sequence would leave the upstream no-transpose
        // layout in memory and is incompatible with our pointwise/inverse ABI.
        vst40.u32 {data0, data1, data2, data3}, [in]
        vst41.u32 {data0, data1, data2, data3}, [in]
        vst42.u32 {data0, data1, data2, data3}, [in]
        vst43.u32 {data0, data1, data2, data3}, [in]!
        le lr, haetae_layer78_loop

        vpop {d8-d15}
        pop {r4-r12, lr}
        bx lr
.size haetae_ntt_asm_paired_slothy, .-haetae_ntt_asm_paired_slothy

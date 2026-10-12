// SPDX-License-Identifier: MIT

// Complete constant-time CDT leaf without MVE predicate instructions.
// Each unsigned 16-bit threshold is widened before subtraction, so
//   (threshold - random) >> 31
// is one exactly when random > threshold and zero otherwise. The low and high
// halves are folded into one four-lane accumulator. q0-q3 are caller-saved;
// r2 retains the return address while lr owns the low-overhead loop.

.syntax unified
.thumb
.arch armv8.1-m.main
.arch_extension mve

.section .text.haetae_sample_gauss16_cdt_mve, "ax", %progbits
.align 2
.global haetae_sample_gauss16_cdt_mve
.type haetae_sample_gauss16_cdt_mve, %function
.thumb_func
haetae_sample_gauss16_cdt_mve:
        mov r2, lr
        vdup.32 q0, r0
        vmov.i32 q1, #0
        ldr r1, =haetae_asm_cdt16
        mov lr, #8
        dls lr, lr
haetae_sampler_mask_loop:
        vldrh.u16 q2, [r1], #16
        vmovlt.u16 q3, q2
        vmovlb.u16 q2, q2
        vsub.i32 q2, q2, q0
        vsub.i32 q3, q3, q0
        vshr.u32 q2, q2, #31
        vshr.u32 q3, q3, #31
        vadd.u32 q2, q2, q3
        vadd.u32 q1, q1, q2
        le lr, haetae_sampler_mask_loop
        movs r0, #0
        vaddva.u32 r0, q1
        movs r1, #0
        bx r2
.size haetae_sample_gauss16_cdt_mve, .-haetae_sample_gauss16_cdt_mve

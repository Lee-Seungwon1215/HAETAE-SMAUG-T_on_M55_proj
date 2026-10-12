// SPDX-License-Identifier: MIT

// Concrete predicate-loop parser test. This is intentionally retained even
// when the pinned Slothy rejects it, so the limitation is reproducible.

.syntax unified
.thumb
.arch armv8.1-m.main
.arch_extension mve

haetae_centered_predicated_loop:
        vldrw.u32 q0, [r0]
        vqrdmulh.s32 q1, q0, r2
        vmla.s32 q0, q1, r3
        vcmp.s32 ge, q0, r4
        vpst
        vsubt.i32 q0, q0, r1
        vcmp.s32 lt, q0, r5
        vpst
        vaddt.i32 q0, q0, r1
        vstrw.32 q0, [r0], #16
        le lr, haetae_centered_predicated_loop

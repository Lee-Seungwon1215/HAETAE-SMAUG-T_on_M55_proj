.syntax unified
.thumb
.arch armv8.1-m.main
.arch_extension mve
.macro M55_X4_FUNCTION name
  .section .text.\name, "ax", %progbits
  .align 2
  .global \name
  .type \name, %function
  .thumb_func
\name:
.endm
.macro M55_X4_END name
  .size \name, .-\name
.endm
M55_X4_FUNCTION m55_shake128x4
  push {r4-r12, lr}
  sub sp, sp, #880
  str r0, [sp, #0]
  str r1, [sp, #4]
  str r2, [sp, #8]
  str r3, [sp, #12]
  ldr r4, [sp, #920]
  ldr r5, [sp, #924]
  ldr r6, [sp, #928]
  ldr r7, [sp, #932]
  ldr r8, [sp, #936]
  ldr r9, [sp, #940]
  str r4, [sp, #16]
  str r5, [sp, #20]
  str r6, [sp, #24]
  str r7, [sp, #28]
  str r8, [sp, #32]
  str r9, [sp, #36]
  add r11, sp, #79
  bic r11, r11, #15
  str r11, [sp, #40]
  mov r0, r11
  bl m55_keccakx4_zero
.Lshake128x4_absorb_full_test:
  ldr r6, [sp, #36]
  cmp r6, #168
  blo .Lshake128x4_absorb_tail
  ldr r0, [sp, #40]
  ldr r1, [sp, #20]
  ldr r2, [sp, #24]
  ldr r3, [sp, #28]
  ldr r4, [sp, #32]
  movs r5, #168
  bl .Lkeccakx4_absorb_block
  ldr r0, [sp, #40]
  bl m55_keccakx4_permute
  ldr r0, [sp, #20]
  ldr r1, [sp, #24]
  ldr r2, [sp, #28]
  ldr r3, [sp, #32]
  adds r0, r0, #168
  adds r1, r1, #168
  adds r2, r2, #168
  adds r3, r3, #168
  str r0, [sp, #20]
  str r1, [sp, #24]
  str r2, [sp, #28]
  str r3, [sp, #32]
  ldr r0, [sp, #36]
  subs r0, r0, #168
  str r0, [sp, #36]
  b .Lshake128x4_absorb_full_test
.Lshake128x4_absorb_tail:
  ldr r0, [sp, #40]
  ldr r1, [sp, #20]
  ldr r2, [sp, #24]
  ldr r3, [sp, #28]
  ldr r4, [sp, #32]
  ldr r5, [sp, #36]
  bl .Lkeccakx4_absorb_block
  ldr r0, [sp, #40]
  ldr r1, [sp, #36]
  movs r2, #0x1f
  bl .Lkeccakx4_xor_broadcast_byte
  ldr r0, [sp, #40]
  movs r1, #167
  movs r2, #0x80
  bl .Lkeccakx4_xor_broadcast_byte
.Lshake128x4_squeeze_test:
  ldr r6, [sp, #16]
  cbz r6, .Lshake128x4_done
  movs r7, #168
  cmp r6, r7
  it lo
  movlo r7, r6
  str r7, [sp, #44]
  ldr r0, [sp, #40]
  bl m55_keccakx4_permute
  ldr r0, [sp, #40]
  ldr r1, [sp, #0]
  ldr r2, [sp, #4]
  ldr r3, [sp, #8]
  ldr r4, [sp, #12]
  ldr r5, [sp, #44]
  bl .Lkeccakx4_squeeze_block
  ldr r7, [sp, #44]
  ldr r0, [sp, #0]
  ldr r1, [sp, #4]
  ldr r2, [sp, #8]
  ldr r3, [sp, #12]
  add r0, r0, r7
  add r1, r1, r7
  add r2, r2, r7
  add r3, r3, r7
  str r0, [sp, #0]
  str r1, [sp, #4]
  str r2, [sp, #8]
  str r3, [sp, #12]
  ldr r0, [sp, #16]
  sub r0, r0, r7
  str r0, [sp, #16]
  b .Lshake128x4_squeeze_test
.Lshake128x4_done:
  add sp, sp, #880
  pop {r4-r12, lr}
  bx lr
M55_X4_END m55_shake128x4
.Lkeccakx4_absorb_block:
  push {r6, lr}
  vpush {d8-d9}
  mov r12, r5
  orr r6, r1, r2
  orr r6, r6, r3
  orr r6, r6, r4
  sub r2, r2, r1
  sub r3, r3, r1
  sub r4, r4, r1
  vmov.i32 q4, #0
  vmov s17, r2
  vmov s18, r3
  vmov s19, r4
  tst r6, #3
  bne .Lkeccakx4_absorb_gather_scalar_test
  lsrs r6, r5, #3
  beq .Lkeccakx4_absorb_gather_partial
  add r7, r1, #4
  dls lr, r6
.p2align 2
m55_keccakx4_absorb_gather_loop:
                                        // Instructions:    11
                                        // Expected cycles: 11
                                        // Expected IPC:    1.00
                                        //
                                        // Cycle bound:     11.0
                                        // IPC bound:       1.00
                                        //
                                        // Wall time:     0.04s
                                        // User time:     0.04s
                                        //
                                        // ----- cycle (expected) ------>
                                        // 0                        25
                                        // |------------------------|----
        vldrw.U32 q0, [r1, q4]          // *.............................
        vldrw.U32 q1, [r7, q4]          // .*............................
        vldrw.U32 q2, [r0]              // ..*...........................
        vldrw.U32 q3, [r0, #400]        // ...*..........................
        veor.U32 q0, q0, q2             // ....*.........................
        veor.U32 q1, q1, q3             // .....*........................
        vstrw.32 q0, [r0]               // ......*.......................
        vstrw.32 q1, [r0, #400]         // .......*......................
        add r0, r0, #16                 // ........*.....................
        add r1, r1, #8                  // .........*....................
        add r7, r7, #8                  // ..........*...................

                                         // ------ cycle (expected) ------>
                                         // 0                        25
                                         // |------------------------|-----
        // vldrw.u32 q0, [r1, q4]        // *..............................
        // vldrw.u32 q1, [r7, q4]        // .*.............................
        // vldrw.u32 q2, [r0]            // ..*............................
        // vldrw.u32 q3, [r0, #400]      // ...*...........................
        // veor.u32 q0, q0, q2           // ....*..........................
        // veor.u32 q1, q1, q3           // .....*.........................
        // vstrw.32 q0, [r0]             // ......*........................
        // vstrw.32 q1, [r0, #400]       // .......*.......................
        // add r0, r0, #16               // ........*......................
        // add r1, r1, #8                // .........*.....................
        // add r7, r7, #8                // ..........*....................

        le lr, m55_keccakx4_absorb_gather_loop
  and r5, r12, #7
  b .Lkeccakx4_absorb_gather_partial
.Lkeccakx4_absorb_gather_scalar_test:
  cmp r5, #8
  blo .Lkeccakx4_absorb_gather_partial
.Lkeccakx4_absorb_gather_scalar_loop:
  ldr r6, [r1]
  ldr r7, [r1, #4]
  vmov s0, r6
  vmov s4, r7
  add r6, r1, r2
  ldr r7, [r6, #4]
  ldr r6, [r6]
  vmov s1, r6
  vmov s5, r7
  add r6, r1, r3
  ldr r7, [r6, #4]
  ldr r6, [r6]
  vmov s2, r6
  vmov s6, r7
  add r6, r1, r4
  ldr r7, [r6, #4]
  ldr r6, [r6]
  vmov s3, r6
  vmov s7, r7
  vldrw.u32 q2, [r0]
  vldrw.u32 q3, [r0, #400]
  veor.u32 q0, q0, q2
  veor.u32 q1, q1, q3
  vstrw.32 q0, [r0]
  vstrw.32 q1, [r0, #400]
  adds r0, r0, #16
  adds r1, r1, #8
  subs r5, r5, #8
  cmp r5, #8
  bhs .Lkeccakx4_absorb_gather_scalar_loop
.Lkeccakx4_absorb_gather_partial:
  cbz r5, .Lkeccakx4_absorb_gather_return
  vmov r2, s17
  vmov r3, s18
  vmov r4, s19
  add r2, r2, r1
  add r3, r3, r1
  add r4, r4, r1
  mov r6, r1
  mov r7, r5
  bl .Lkeccakx4_load_partial
  vmov s0, r8
  vmov s4, r9
  mov r6, r2
  mov r7, r5
  bl .Lkeccakx4_load_partial
  vmov s1, r8
  vmov s5, r9
  mov r6, r3
  mov r7, r5
  bl .Lkeccakx4_load_partial
  vmov s2, r8
  vmov s6, r9
  mov r6, r4
  mov r7, r5
  bl .Lkeccakx4_load_partial
  vmov s3, r8
  vmov s7, r9
  vldrw.u32 q2, [r0]
  vldrw.u32 q3, [r0, #400]
  veor.u32 q0, q0, q2
  veor.u32 q1, q1, q3
  vstrw.32 q0, [r0]
  vstrw.32 q1, [r0, #400]
.Lkeccakx4_absorb_gather_return:
  vpop {d8-d9}
  pop {r6, pc}
.Lkeccakx4_load_partial:
  push {r0-r5, lr}
  movs r8, #0
  movs r9, #0
  movs r10, #0
.Lkeccakx4_load_partial_loop:
  cmp r10, r7
  bhs .Lkeccakx4_load_partial_done
  ldrb r12, [r6, r10]
  cmp r10, #4
  bhs .Lkeccakx4_load_partial_high
  lsl r11, r10, #3
  lsl r12, r12, r11
  orr r8, r8, r12
  b .Lkeccakx4_load_partial_next
.Lkeccakx4_load_partial_high:
  sub r11, r10, #4
  lsl r11, r11, #3
  lsl r12, r12, r11
  orr r9, r9, r12
.Lkeccakx4_load_partial_next:
  adds r10, r10, #1
  b .Lkeccakx4_load_partial_loop
.Lkeccakx4_load_partial_done:
  pop {r0-r5, pc}
.Lkeccakx4_xor_broadcast_byte:
  and r3, r1, #7
  lsrs r1, r1, #3
  add r0, r0, r1, lsl #4
  cmp r3, #4
  blo .Lkeccakx4_xor_broadcast_low
  adds r0, r0, #400
  subs r3, r3, #4
.Lkeccakx4_xor_broadcast_low:
  lsls r3, r3, #3
  lsl r2, r2, r3
  vdup.32 q0, r2
  vldrw.u32 q1, [r0]
  veor.u32 q0, q0, q1
  vstrw.32 q0, [r0]
  bx lr
.Lkeccakx4_squeeze_block:
  push {r6, lr}
  vpush {d8-d9}
  mov r12, r5
  orr r6, r1, r2
  orr r6, r6, r3
  orr r6, r6, r4
  sub r2, r2, r1
  sub r3, r3, r1
  sub r4, r4, r1
  vmov.i32 q4, #0
  vmov s17, r2
  vmov s18, r3
  vmov s19, r4
  tst r6, #3
  bne .Lkeccakx4_squeeze_scatter_scalar_test
  lsrs r6, r5, #3
  beq .Lkeccakx4_squeeze_scatter_partial
  add r7, r1, #4
  dls lr, r6
m55_keccakx4_squeeze_scatter_loop:
  vldrw.u32 q0, [r0]
  vldrw.u32 q1, [r0, #400]
  vstrw.32 q0, [r1, q4]
  vstrw.32 q1, [r7, q4]
  add r0, r0, #16
  add r1, r1, #8
  add r7, r7, #8
  le lr, m55_keccakx4_squeeze_scatter_loop
  and r5, r12, #7
  b .Lkeccakx4_squeeze_scatter_partial
.Lkeccakx4_squeeze_scatter_scalar_test:
  cmp r5, #8
  blo .Lkeccakx4_squeeze_scatter_partial
.Lkeccakx4_squeeze_scatter_scalar_loop:
  vldrw.u32 q0, [r0]
  vldrw.u32 q1, [r0, #400]
  vmov r6, s0
  vmov r7, s4
  str r6, [r1]
  str r7, [r1, #4]
  add r6, r1, r2
  vmov r8, s1
  vmov r9, s5
  str r8, [r6]
  str r9, [r6, #4]
  add r6, r1, r3
  vmov r8, s2
  vmov r9, s6
  str r8, [r6]
  str r9, [r6, #4]
  add r6, r1, r4
  vmov r8, s3
  vmov r9, s7
  str r8, [r6]
  str r9, [r6, #4]
  adds r0, r0, #16
  adds r1, r1, #8
  subs r5, r5, #8
  cmp r5, #8
  bhs .Lkeccakx4_squeeze_scatter_scalar_loop
.Lkeccakx4_squeeze_scatter_partial:
  cbz r5, .Lkeccakx4_squeeze_scatter_return
  vmov r2, s17
  vmov r3, s18
  vmov r4, s19
  add r2, r2, r1
  add r3, r3, r1
  add r4, r4, r1
  vldrw.u32 q0, [r0]
  vldrw.u32 q1, [r0, #400]
  mov r6, r1
  mov r7, r5
  vmov r8, s0
  vmov r9, s4
  bl .Lkeccakx4_store_partial
  mov r6, r2
  mov r7, r5
  vmov r8, s1
  vmov r9, s5
  bl .Lkeccakx4_store_partial
  mov r6, r3
  mov r7, r5
  vmov r8, s2
  vmov r9, s6
  bl .Lkeccakx4_store_partial
  mov r6, r4
  mov r7, r5
  vmov r8, s3
  vmov r9, s7
  bl .Lkeccakx4_store_partial
.Lkeccakx4_squeeze_scatter_return:
  vpop {d8-d9}
  pop {r6, pc}
.Lkeccakx4_store_partial:
  push {r0-r5, lr}
  movs r10, #0
.Lkeccakx4_store_partial_loop:
  cmp r10, r7
  bhs .Lkeccakx4_store_partial_done
  cmp r10, #4
  bhs .Lkeccakx4_store_partial_high
  lsl r11, r10, #3
  lsr r12, r8, r11
  b .Lkeccakx4_store_partial_byte
.Lkeccakx4_store_partial_high:
  sub r11, r10, #4
  lsl r11, r11, #3
  lsr r12, r9, r11
.Lkeccakx4_store_partial_byte:
  strb r12, [r6, r10]
  adds r10, r10, #1
  b .Lkeccakx4_store_partial_loop
.Lkeccakx4_store_partial_done:
  pop {r0-r5, pc}
.section .note.GNU-stack, "", %progbits

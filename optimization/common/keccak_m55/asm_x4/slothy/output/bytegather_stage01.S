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
  vpush {d8-d15}
  sub sp, sp, #880
        m55_shake128x4_main_init_start:
                                         // Instructions:    19
                                         // Expected cycles: 19
                                         // Expected IPC:    1.00
                                         //
                                         // Cycle bound:     19.0
                                         // IPC bound:       1.00
                                         //
                                         // Wall time:     0.04s
                                         // User time:     0.04s
                                         //
                                         // ----- cycle (expected) ------>
                                         // 0                        25
                                         // |------------------------|----
        ldr r9, [r13, #1004]             // *.............................
        str r9, [r13, #36]               // .*............................
        add r11, r13, #79                // ..*...........................
        ldr r8, [r13, #1000]             // ...*..........................
        ldr r7, [r13, #996]              // ....*.........................
        str r7, [r13, #28]               // .....*........................
        and r11, r11, #0xfffffff0        // ......*.......................
        str r8, [r13, #32]               // .......*......................
        ldr r5, [r13, #988]              // ........*.....................
        str r5, [r13, #20]               // .........*....................
        str r11, [r13, #40]              // ..........*...................
        str r1, [r13, #4]                // ...........*..................
        str r0, [r13, #0]                // ............*.................
        ldr r6, [r13, #992]              // .............*................
        str r6, [r13, #24]               // ..............*...............
        str r3, [r13, #12]               // ...............*..............
        str r2, [r13, #8]                // ................*.............
        ldr r4, [r13, #984]              // .................*............
        str r4, [r13, #16]               // ..................*...........

                                          // ------ cycle (expected) ------>
                                          // 0                        25
                                          // |------------------------|-----
        // str r0, [r13, #0]              // ............*..................
        // str r1, [r13, #4]              // ...........*...................
        // str r2, [r13, #8]              // ................*..............
        // str r3, [r13, #12]             // ...............*...............
        // ldr r4, [r13, #984]            // .................*.............
        // ldr r5, [r13, #988]            // ........*......................
        // ldr r6, [r13, #992]            // .............*.................
        // ldr r7, [r13, #996]            // ....*..........................
        // ldr r8, [r13, #1000]           // ...*...........................
        // ldr r9, [r13, #1004]           // *..............................
        // str r4, [r13, #16]             // ..................*............
        // str r5, [r13, #20]             // .........*.....................
        // str r6, [r13, #24]             // ..............*................
        // str r7, [r13, #28]             // .....*.........................
        // str r8, [r13, #32]             // .......*.......................
        // str r9, [r13, #36]             // .*.............................
        // add r11, r13, #79              // ..*............................
        // and r11, r11, #0xfffffff0      // ......*........................
        // str r11, [r13, #40]            // ..........*....................

        m55_shake128x4_main_init_end:

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
m55_shake128x4_absorb_advance_start:
  ldr r0, [sp, #20]
  ldr r1, [sp, #24]
  ldr r2, [sp, #28]
  ldr r3, [sp, #32]
  add r0, r0, #168
  add r1, r1, #168
  add r2, r2, #168
  add r3, r3, #168
  str r0, [sp, #20]
  str r1, [sp, #24]
  str r2, [sp, #28]
  str r3, [sp, #32]
  ldr r0, [sp, #36]
  sub r0, r0, #168
  str r0, [sp, #36]
m55_shake128x4_absorb_advance_end:
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
m55_shake128x4_squeeze_advance_start:
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
m55_shake128x4_squeeze_advance_end:
  b .Lshake128x4_squeeze_test
.Lshake128x4_done:
  add sp, sp, #880
  vpop {d8-d15}
  pop {r4-r12, lr}
  bx lr
M55_X4_END m55_shake128x4
.Lkeccakx4_absorb_block:
  push {r6, lr}
  sub sp, sp, #32
m55_shake128x4_absorb_setup_start:
  mov r12, r5
  orr r6, r1, r2
  orr r6, r6, r3
  orr r6, r6, r4
  sub r2, r2, r1
  sub r3, r3, r1
  sub r4, r4, r1
  mov r7, #0
  str r7, [sp, #0]  // @slothy:writes=offset_lane0
  str r2, [sp, #4]  // @slothy:reads=offset_lane0 // @slothy:writes=offset_lane1
  str r3, [sp, #8]  // @slothy:reads=offset_lane1 // @slothy:writes=offset_lane2
  str r4, [sp, #12] // @slothy:reads=offset_lane2 // @slothy:writes=offset_lane3
  vldrw.u32 q4, [sp, #0] // @slothy:reads=offset_lane3
m55_shake128x4_absorb_setup_end:
  tst r6, #3
  bne .Lkeccakx4_absorb_gather_scalar_test
  lsrs r6, r5, #3
  beq .Lkeccakx4_absorb_gather_partial
  add r7, r1, #4
  tst r6, #1
  beq .Lkeccakx4_absorb_gather_u2_even
m55_shake128x4_absorb_odd_start:
  vldrw.u32 q0, [r1, q4]
  vldrw.u32 q1, [r7, q4]
  vldrw.u32 q2, [r0]
  vldrw.u32 q3, [r0, #400]
  veor.u32 q0, q0, q2
  veor.u32 q1, q1, q3
  vstrw.32 q0, [r0]
  vstrw.32 q1, [r0, #400]
  add r0, r0, #16
  add r1, r1, #8
  add r7, r7, #8
  sub r6, r6, #1
m55_shake128x4_absorb_odd_end:
  cbz r6, .Lkeccakx4_absorb_gather_u2_done
.Lkeccakx4_absorb_gather_u2_even:
  dls lr, r6
  lsr lr, lr, #1
.p2align 2
m55_keccakx4_absorb_gather_loop:
  vldrw.u32 q0, [r1, q4]
  vldrw.u32 q1, [r7, q4]
  vldrw.u32 q2, [r0]
  vldrw.u32 q3, [r0, #400]
  veor.u32 q0, q0, q2
  veor.u32 q1, q1, q3
  vstrw.32 q0, [r0]
  vstrw.32 q1, [r0, #400]
  add r0, r0, #16
  add r1, r1, #8
  add r7, r7, #8
  vldrw.u32 q0, [r1, q4]
  vldrw.u32 q1, [r7, q4]
  vldrw.u32 q2, [r0]
  vldrw.u32 q3, [r0, #400]
  veor.u32 q0, q0, q2
  veor.u32 q1, q1, q3
  vstrw.32 q0, [r0]
  vstrw.32 q1, [r0, #400]
  add r0, r0, #16
  add r1, r1, #8
  add r7, r7, #8
  le lr, m55_keccakx4_absorb_gather_loop
.Lkeccakx4_absorb_gather_u2_done:
  and r5, r12, #7
  b .Lkeccakx4_absorb_gather_partial
.Lkeccakx4_absorb_gather_scalar_test:
  cmp r5, #8
  blo .Lkeccakx4_absorb_gather_partial
.Lkeccakx4_absorb_gather_scalar_loop:
m55_shake128x4_absorb_scalar_start:
  ldr r6, [r1, #0]
  ldr r7, [r1, #4]
  str r6, [sp, #0]  // @slothy:writes=scratch_low0
  str r7, [sp, #16] // @slothy:writes=scratch_high0
  add r6, r1, r2
  ldr r7, [r6, #4]
  ldr r6, [r6, #0]
  str r6, [sp, #4]  // @slothy:reads=scratch_low0 // @slothy:writes=scratch_low1
  str r7, [sp, #20] // @slothy:reads=scratch_high0 // @slothy:writes=scratch_high1
  add r6, r1, r3
  ldr r7, [r6, #4]
  ldr r6, [r6, #0]
  str r6, [sp, #8]  // @slothy:reads=scratch_low1 // @slothy:writes=scratch_low2
  str r7, [sp, #24] // @slothy:reads=scratch_high1 // @slothy:writes=scratch_high2
  add r6, r1, r4
  ldr r7, [r6, #4]
  ldr r6, [r6, #0]
  str r6, [sp, #12] // @slothy:reads=scratch_low2 // @slothy:writes=scratch_low3
  str r7, [sp, #28] // @slothy:reads=scratch_high2 // @slothy:writes=scratch_high3
  vldrw.u32 q0, [sp, #0]  // @slothy:reads=scratch_low3
  vldrw.u32 q1, [sp, #16] // @slothy:reads=scratch_high3
  vldrw.u32 q2, [r0]
  vldrw.u32 q3, [r0, #400]
  veor.u32 q0, q0, q2
  veor.u32 q1, q1, q3
  vstrw.32 q0, [r0]
  vstrw.32 q1, [r0, #400]
  add r0, r0, #16
  add r1, r1, #8
  sub r5, r5, #8
m55_shake128x4_absorb_scalar_end:
  cmp r5, #8
  bhs .Lkeccakx4_absorb_gather_scalar_loop
.Lkeccakx4_absorb_gather_partial:
  cmp r5, #0
  beq .Lkeccakx4_absorb_gather_return
  cmp r5, #1
  beq .Lkeccakx4_absorb_tail1
  cmp r5, #2
  beq .Lkeccakx4_absorb_tail2
  cmp r5, #3
  beq .Lkeccakx4_absorb_tail3
  cmp r5, #4
  beq .Lkeccakx4_absorb_tail4
  cmp r5, #5
  beq .Lkeccakx4_absorb_tail5
  cmp r5, #6
  beq .Lkeccakx4_absorb_tail6
  b .Lkeccakx4_absorb_tail7
.Lkeccakx4_absorb_tail1:
m55_shake128x4_absorb_tail1_start:
  vldrb.u32 q0, [r1, q4]
  vldrw.u32 q2, [r0]
  veor.u32 q0, q0, q2
  vstrw.32 q0, [r0]
m55_shake128x4_absorb_tail1_end:
  b .Lkeccakx4_absorb_gather_return
.Lkeccakx4_absorb_tail2:
m55_shake128x4_absorb_tail2_start:
  vldrb.u32 q0, [r1, q4]
  add r6, r1, #1
  vldrb.u32 q1, [r6, q4]
  vshl.u32 q1, q1, #8
  vorr.u32 q0, q0, q1
  vldrw.u32 q2, [r0]
  veor.u32 q0, q0, q2
  vstrw.32 q0, [r0]
m55_shake128x4_absorb_tail2_end:
  b .Lkeccakx4_absorb_gather_return
.Lkeccakx4_absorb_tail3:
m55_shake128x4_absorb_tail3_start:
  vldrb.u32 q0, [r1, q4]
  add r6, r1, #1
  vldrb.u32 q1, [r6, q4]
  vshl.u32 q1, q1, #8
  vorr.u32 q0, q0, q1
  add r6, r6, #1
  vldrb.u32 q1, [r6, q4]
  vshl.u32 q1, q1, #16
  vorr.u32 q0, q0, q1
  vldrw.u32 q2, [r0]
  veor.u32 q0, q0, q2
  vstrw.32 q0, [r0]
m55_shake128x4_absorb_tail3_end:
  b .Lkeccakx4_absorb_gather_return
.Lkeccakx4_absorb_tail4:
m55_shake128x4_absorb_tail4_start:
  vldrb.u32 q0, [r1, q4]
  add r6, r1, #1
  vldrb.u32 q1, [r6, q4]
  vshl.u32 q1, q1, #8
  vorr.u32 q0, q0, q1
  add r6, r6, #1
  vldrb.u32 q1, [r6, q4]
  vshl.u32 q1, q1, #16
  vorr.u32 q0, q0, q1
  add r6, r6, #1
  vldrb.u32 q1, [r6, q4]
  vshl.u32 q1, q1, #24
  vorr.u32 q0, q0, q1
  vldrw.u32 q2, [r0]
  veor.u32 q0, q0, q2
  vstrw.32 q0, [r0]
m55_shake128x4_absorb_tail4_end:
  b .Lkeccakx4_absorb_gather_return
.Lkeccakx4_absorb_tail5:
m55_shake128x4_absorb_tail5_start:
  vldrb.u32 q0, [r1, q4]
  add r6, r1, #1
  vldrb.u32 q1, [r6, q4]
  vshl.u32 q1, q1, #8
  vorr.u32 q0, q0, q1
  add r6, r6, #1
  vldrb.u32 q1, [r6, q4]
  vshl.u32 q1, q1, #16
  vorr.u32 q0, q0, q1
  add r6, r6, #1
  vldrb.u32 q1, [r6, q4]
  vshl.u32 q1, q1, #24
  vorr.u32 q0, q0, q1
  add r6, r1, #4
  vldrb.u32 q5, [r6, q4]
  vldrw.u32 q2, [r0]
  veor.u32 q0, q0, q2
  vstrw.32 q0, [r0]
  vldrw.u32 q3, [r0, #400]
  veor.u32 q5, q5, q3
  vstrw.32 q5, [r0, #400]
m55_shake128x4_absorb_tail5_end:
  b .Lkeccakx4_absorb_gather_return
.Lkeccakx4_absorb_tail6:
m55_shake128x4_absorb_tail6_start:
  vldrb.u32 q0, [r1, q4]
  add r6, r1, #1
  vldrb.u32 q1, [r6, q4]
  vshl.u32 q1, q1, #8
  vorr.u32 q0, q0, q1
  add r6, r6, #1
  vldrb.u32 q1, [r6, q4]
  vshl.u32 q1, q1, #16
  vorr.u32 q0, q0, q1
  add r6, r6, #1
  vldrb.u32 q1, [r6, q4]
  vshl.u32 q1, q1, #24
  vorr.u32 q0, q0, q1
  add r6, r1, #4
  vldrb.u32 q5, [r6, q4]
  add r6, r6, #1
  vldrb.u32 q1, [r6, q4]
  vshl.u32 q1, q1, #8
  vorr.u32 q5, q5, q1
  vldrw.u32 q2, [r0]
  veor.u32 q0, q0, q2
  vstrw.32 q0, [r0]
  vldrw.u32 q3, [r0, #400]
  veor.u32 q5, q5, q3
  vstrw.32 q5, [r0, #400]
m55_shake128x4_absorb_tail6_end:
  b .Lkeccakx4_absorb_gather_return
.Lkeccakx4_absorb_tail7:
m55_shake128x4_absorb_tail7_start:
  vldrb.u32 q0, [r1, q4]
  add r6, r1, #1
  vldrb.u32 q1, [r6, q4]
  vshl.u32 q1, q1, #8
  vorr.u32 q0, q0, q1
  add r6, r6, #1
  vldrb.u32 q1, [r6, q4]
  vshl.u32 q1, q1, #16
  vorr.u32 q0, q0, q1
  add r6, r6, #1
  vldrb.u32 q1, [r6, q4]
  vshl.u32 q1, q1, #24
  vorr.u32 q0, q0, q1
  add r6, r1, #4
  vldrb.u32 q5, [r6, q4]
  add r6, r6, #1
  vldrb.u32 q1, [r6, q4]
  vshl.u32 q1, q1, #8
  vorr.u32 q5, q5, q1
  add r6, r6, #1
  vldrb.u32 q1, [r6, q4]
  vshl.u32 q1, q1, #16
  vorr.u32 q5, q5, q1
  vldrw.u32 q2, [r0]
  veor.u32 q0, q0, q2
  vstrw.32 q0, [r0]
  vldrw.u32 q3, [r0, #400]
  veor.u32 q5, q5, q3
  vstrw.32 q5, [r0, #400]
m55_shake128x4_absorb_tail7_end:
  b .Lkeccakx4_absorb_gather_return
.Lkeccakx4_absorb_gather_return:
  add sp, sp, #32
  pop {r6, pc}
.Lkeccakx4_xor_broadcast_byte:
m55_shake128x4_padding_address_start:
  and r3, r1, #7
  lsr r1, r1, #3
  add r0, r0, r1, lsl #4
m55_shake128x4_padding_address_end:
  cmp r3, #4
  blo .Lkeccakx4_xor_broadcast_low
m55_shake128x4_padding_high_start:
  add r0, r0, #400
  sub r3, r3, #4
m55_shake128x4_padding_high_end:
.Lkeccakx4_xor_broadcast_low:
m55_shake128x4_padding_xor_start:
  lsl r3, r3, #3
  lsl r2, r2, r3
  vdup.32 q0, r2
  vldrw.u32 q1, [r0]
  veor.u32 q0, q0, q1
  vstrw.32 q0, [r0]
m55_shake128x4_padding_xor_end:
  bx lr
.Lkeccakx4_squeeze_block:
  push {r6, lr}
  sub sp, sp, #32
m55_shake128x4_squeeze_setup_start:
  mov r12, r5
  orr r6, r1, r2
  orr r6, r6, r3
  orr r6, r6, r4
  sub r2, r2, r1
  sub r3, r3, r1
  sub r4, r4, r1
  mov r7, #0
  str r7, [sp, #0]  // @slothy:writes=offset_lane0
  str r2, [sp, #4]  // @slothy:reads=offset_lane0 // @slothy:writes=offset_lane1
  str r3, [sp, #8]  // @slothy:reads=offset_lane1 // @slothy:writes=offset_lane2
  str r4, [sp, #12] // @slothy:reads=offset_lane2 // @slothy:writes=offset_lane3
  vldrw.u32 q4, [sp, #0] // @slothy:reads=offset_lane3
m55_shake128x4_squeeze_setup_end:
  tst r6, #3
  bne .Lkeccakx4_squeeze_scatter_scalar_test
  lsrs r6, r5, #3
  beq .Lkeccakx4_squeeze_scatter_partial
  add r7, r1, #4
  tst r6, #1
  beq .Lkeccakx4_squeeze_scatter_u2_even
m55_shake128x4_squeeze_odd_start:
  vldrw.u32 q0, [r0]
  vldrw.u32 q1, [r0, #400]
  vstrw.32 q0, [r1, q4]
  vstrw.32 q1, [r7, q4]
  add r0, r0, #16
  add r1, r1, #8
  add r7, r7, #8
  sub r6, r6, #1
m55_shake128x4_squeeze_odd_end:
  cbz r6, .Lkeccakx4_squeeze_scatter_u2_done
.Lkeccakx4_squeeze_scatter_u2_even:
  dls lr, r6
  lsr lr, lr, #1
.p2align 2
m55_keccakx4_squeeze_scatter_loop:
  vldrw.u32 q0, [r0]
  vldrw.u32 q1, [r0, #400]
  vstrw.32 q0, [r1, q4]
  vstrw.32 q1, [r7, q4]
  add r0, r0, #16
  add r1, r1, #8
  add r7, r7, #8
  vldrw.u32 q0, [r0]
  vldrw.u32 q1, [r0, #400]
  vstrw.32 q0, [r1, q4]
  vstrw.32 q1, [r7, q4]
  add r0, r0, #16
  add r1, r1, #8
  add r7, r7, #8
  le lr, m55_keccakx4_squeeze_scatter_loop
.Lkeccakx4_squeeze_scatter_u2_done:
  and r5, r12, #7
  b .Lkeccakx4_squeeze_scatter_partial
.Lkeccakx4_squeeze_scatter_scalar_test:
  cmp r5, #8
  blo .Lkeccakx4_squeeze_scatter_partial
.Lkeccakx4_squeeze_scatter_scalar_loop:
m55_shake128x4_squeeze_scalar_start:
  vldrw.u32 q0, [r0]
  vldrw.u32 q1, [r0, #400]
  vstrw.32 q0, [sp, #0]  // @slothy:writes=scratch_low_ready
  vstrw.32 q1, [sp, #16] // @slothy:writes=scratch_high_ready
  ldr r6, [sp, #0]  // @slothy:reads=scratch_low_ready
  ldr r7, [sp, #16] // @slothy:reads=scratch_high_ready
  str r6, [r1, #0]
  str r7, [r1, #4]
  add r6, r1, r2
  ldr r7, [sp, #4]  // @slothy:reads=scratch_low_ready
  ldr r8, [sp, #20] // @slothy:reads=scratch_high_ready
  str r7, [r6, #0]
  str r8, [r6, #4]
  add r6, r1, r3
  ldr r7, [sp, #8]  // @slothy:reads=scratch_low_ready
  ldr r8, [sp, #24] // @slothy:reads=scratch_high_ready
  str r7, [r6, #0]
  str r8, [r6, #4]
  add r6, r1, r4
  ldr r7, [sp, #12] // @slothy:reads=scratch_low_ready
  ldr r8, [sp, #28] // @slothy:reads=scratch_high_ready
  str r7, [r6, #0]
  str r8, [r6, #4]
  add r0, r0, #16
  add r1, r1, #8
  sub r5, r5, #8
m55_shake128x4_squeeze_scalar_end:
  cmp r5, #8
  bhs .Lkeccakx4_squeeze_scatter_scalar_loop
.Lkeccakx4_squeeze_scatter_partial:
  cbz r5, .Lkeccakx4_squeeze_scatter_return
m55_shake128x4_squeeze_partial_setup_start:
  vstrw.32 q4, [sp, #0] // @slothy:writes=offsets_restored
  ldr r2, [sp, #4]  // @slothy:reads=offsets_restored // @slothy:writes=offset_lane1_read
  ldr r3, [sp, #8]  // @slothy:reads=offset_lane1_read // @slothy:writes=offset_lane2_read
  ldr r4, [sp, #12] // @slothy:reads=offset_lane2_read // @slothy:writes=offset_lane3_read
  add r2, r2, r1
  add r3, r3, r1
  add r4, r4, r1
  vldrw.u32 q0, [r0]
  vldrw.u32 q1, [r0, #400]
  vstrw.32 q0, [sp, #0] // @slothy:reads=offset_lane3_read
  vstrw.32 q1, [sp, #16]
m55_shake128x4_squeeze_partial_setup_end:
  mov r6, r1
  mov r7, r5
  ldr r8, [sp, #0]
  ldr r9, [sp, #16]
  bl .Lkeccakx4_store_partial
  mov r6, r2
  mov r7, r5
  ldr r8, [sp, #4]
  ldr r9, [sp, #20]
  bl .Lkeccakx4_store_partial
  mov r6, r3
  mov r7, r5
  ldr r8, [sp, #8]
  ldr r9, [sp, #24]
  bl .Lkeccakx4_store_partial
  mov r6, r4
  mov r7, r5
  ldr r8, [sp, #12]
  ldr r9, [sp, #28]
  bl .Lkeccakx4_store_partial
.Lkeccakx4_squeeze_scatter_return:
  add sp, sp, #32
  pop {r6, pc}
.Lkeccakx4_store_partial:
  push {r0-r5, lr}
  movs r10, #0
.Lkeccakx4_store_partial_loop:
  cmp r10, r7
  bhs .Lkeccakx4_store_partial_done
  cmp r10, #4
  bhs .Lkeccakx4_store_partial_high
m55_shake128x4_store_partial_low_start:
  lsl r11, r10, #3
  lsr r12, r8, r11
  strb r12, [r6, r10]
m55_shake128x4_store_partial_low_end:
  b .Lkeccakx4_store_partial_next
.Lkeccakx4_store_partial_high:
m55_shake128x4_store_partial_high_start:
  sub r11, r10, #4
  lsl r11, r11, #3
  lsr r12, r9, r11
  strb r12, [r6, r10]
m55_shake128x4_store_partial_high_end:
.Lkeccakx4_store_partial_next:
  adds r10, r10, #1
  b .Lkeccakx4_store_partial_loop
.Lkeccakx4_store_partial_done:
  pop {r0-r5, pc}
.section .note.GNU-stack, "", %progbits

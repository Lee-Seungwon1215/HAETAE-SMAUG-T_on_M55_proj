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
.macro KX4_PARITY base, lane, scratch_offset
  vldrw.u32 q0, [\base, #(16 * (\lane))]
  vldrw.u32 q1, [\base, #(16 * ((\lane) + 5))]
  veor.u32 q0, q0, q1
  vldrw.u32 q1, [\base, #(16 * ((\lane) + 10))]
  veor.u32 q0, q0, q1
  vldrw.u32 q1, [\base, #(16 * ((\lane) + 15))]
  veor.u32 q0, q0, q1
  vldrw.u32 q1, [\base, #(16 * ((\lane) + 20))]
  veor.u32 q0, q0, q1
  vstrw.32 q0, [sp, #(\scratch_offset)]
.endm
.macro KX4_THETA_APPLY lane, prev, next
  vldrw.u32 q0, [sp, #(16 * (\prev))]
  vldrw.u32 q1, [sp, #(80 + 16 * (\prev))]
  vldrw.u32 q2, [sp, #(16 * (\next))]
  vldrw.u32 q3, [sp, #(80 + 16 * (\next))]
  vshl.i32 q4, q2, #1
  vshr.u32 q6, q3, #31
  vorr.u32 q4, q4, q6
  vshl.i32 q5, q3, #1
  vshr.u32 q6, q2, #31
  vorr.u32 q5, q5, q6
  veor.u32 q0, q0, q4
  veor.u32 q1, q1, q5
  .irp y, 0, 5, 10, 15, 20
    vldrw.u32 q2, [r10, #(16 * ((\lane) + \y))]
    vldrw.u32 q3, [r11, #(16 * ((\lane) + \y))]
    veor.u32 q2, q2, q0
    veor.u32 q3, q3, q1
    vstrw.32 q2, [r10, #(16 * ((\lane) + \y))]
    vstrw.32 q3, [r11, #(16 * ((\lane) + \y))]
  .endr
.endm
.macro KX4_RHOPI_STEP dst, amount
  vldrw.u32 q2, [r10, #(16 * (\dst))]
  vldrw.u32 q3, [r11, #(16 * (\dst))]
  .if (\amount) < 32
    vshl.i32 q4, q0, #(\amount)
    vshr.u32 q6, q1, #(32 - (\amount))
    vorr.u32 q4, q4, q6
    vshl.i32 q5, q1, #(\amount)
    vshr.u32 q6, q0, #(32 - (\amount))
    vorr.u32 q5, q5, q6
  .else
    vshl.i32 q4, q1, #((\amount) - 32)
    vshr.u32 q6, q0, #(64 - (\amount))
    vorr.u32 q4, q4, q6
    vshl.i32 q5, q0, #((\amount) - 32)
    vshr.u32 q6, q1, #(64 - (\amount))
    vorr.u32 q5, q5, q6
  .endif
  vstrw.32 q4, [r10, #(16 * (\dst))]
  vstrw.32 q5, [r11, #(16 * (\dst))]
  vmov q0, q2
  vmov q1, q3
.endm
.macro KX4_CHI_ROW base, row
  vldrw.u32 q0, [\base, #(16 * ((\row) + 0))]
  vldrw.u32 q1, [\base, #(16 * ((\row) + 1))]
  vldrw.u32 q2, [\base, #(16 * ((\row) + 2))]
  vldrw.u32 q3, [\base, #(16 * ((\row) + 3))]
  vldrw.u32 q4, [\base, #(16 * ((\row) + 4))]
  vbic.u32 q5, q2, q1
  veor.u32 q5, q5, q0
  vstrw.32 q5, [\base, #(16 * ((\row) + 0))]
  vbic.u32 q5, q3, q2
  veor.u32 q5, q5, q1
  vstrw.32 q5, [\base, #(16 * ((\row) + 1))]
  vbic.u32 q5, q4, q3
  veor.u32 q5, q5, q2
  vstrw.32 q5, [\base, #(16 * ((\row) + 2))]
  vbic.u32 q5, q0, q4
  veor.u32 q5, q5, q3
  vstrw.32 q5, [\base, #(16 * ((\row) + 3))]
  vbic.u32 q5, q1, q0
  veor.u32 q5, q5, q4
  vstrw.32 q5, [\base, #(16 * ((\row) + 4))]
.endm
M55_X4_FUNCTION m55_keccakx4_zero
  vmov.i32 q0, #0
  addw r1, r0, #400
  movs r2, #25
.Lkeccakx4_zero_loop:
  vstrw.32 q0, [r0], #16
  vstrw.32 q0, [r1], #16
  subs r2, r2, #1
  bne .Lkeccakx4_zero_loop
  bx lr
M55_X4_END m55_keccakx4_zero
M55_X4_FUNCTION m55_keccakx4_import
  push {r4-r10, lr}
  mov r5, r0
  addw r6, r0, #400
  mov r2, r1
  add r3, r1, #200
  addw r4, r1, #400
  addw r1, r1, #600
  movs r10, #25
.Lkeccakx4_import_loop:
  ldrd r7, r8, [r2]
  vmov s0, r7
  vmov s4, r8
  ldrd r7, r8, [r3]
  vmov s1, r7
  vmov s5, r8
  ldrd r7, r8, [r4]
  vmov s2, r7
  vmov s6, r8
  ldrd r7, r8, [r1]
  vmov s3, r7
  vmov s7, r8
  vstrw.32 q0, [r5], #16
  vstrw.32 q1, [r6], #16
  add r2, r2, #8
  add r3, r3, #8
  add r4, r4, #8
  add r1, r1, #8
  subs r10, r10, #1
  bne .Lkeccakx4_import_loop
  pop {r4-r10, pc}
M55_X4_END m55_keccakx4_import
M55_X4_FUNCTION m55_keccakx4_export
  push {r4-r10, lr}
  mov r2, r0
  add r3, r0, #200
  addw r4, r0, #400
  addw r5, r0, #600
  mov r6, r1
  addw r7, r1, #400
  movs r10, #25
.Lkeccakx4_export_loop:
  vldrw.u32 q0, [r6], #16
  vldrw.u32 q1, [r7], #16
  vmov r8, s0
  vmov r9, s4
  strd r8, r9, [r2], #8
  vmov r8, s1
  vmov r9, s5
  strd r8, r9, [r3], #8
  vmov r8, s2
  vmov r9, s6
  strd r8, r9, [r4], #8
  vmov r8, s3
  vmov r9, s7
  strd r8, r9, [r5], #8
  subs r10, r10, #1
  bne .Lkeccakx4_export_loop
  pop {r4-r10, pc}
M55_X4_END m55_keccakx4_export
M55_X4_FUNCTION m55_keccakx4_permute
  push {r4-r12, lr}
  vpush {d8-d15}
  sub sp, sp, #160
  mov r10, r0
  addw r11, r0, #400
  ldr r9, =m55_keccakx4_round_constants
  movs r8, #24
.Lkeccakx4_round:
  KX4_PARITY r10, 0, 0
  KX4_PARITY r10, 1, 16
  KX4_PARITY r10, 2, 32
  KX4_PARITY r10, 3, 48
  KX4_PARITY r10, 4, 64
  KX4_PARITY r11, 0, 80
  KX4_PARITY r11, 1, 96
  KX4_PARITY r11, 2, 112
  KX4_PARITY r11, 3, 128
  KX4_PARITY r11, 4, 144
  KX4_THETA_APPLY 0, 4, 1
  KX4_THETA_APPLY 1, 0, 2
  KX4_THETA_APPLY 2, 1, 3
  KX4_THETA_APPLY 3, 2, 4
  KX4_THETA_APPLY 4, 3, 0
  vldrw.u32 q0, [r10, #16]
  vldrw.u32 q1, [r11, #16]
  KX4_RHOPI_STEP 10, 1
  KX4_RHOPI_STEP 7, 3
  KX4_RHOPI_STEP 11, 6
  KX4_RHOPI_STEP 17, 10
  KX4_RHOPI_STEP 18, 15
  KX4_RHOPI_STEP 3, 21
  KX4_RHOPI_STEP 5, 28
  KX4_RHOPI_STEP 16, 36
  KX4_RHOPI_STEP 8, 45
  KX4_RHOPI_STEP 21, 55
  KX4_RHOPI_STEP 24, 2
  KX4_RHOPI_STEP 4, 14
  KX4_RHOPI_STEP 15, 27
  KX4_RHOPI_STEP 23, 41
  KX4_RHOPI_STEP 19, 56
  KX4_RHOPI_STEP 13, 8
  KX4_RHOPI_STEP 12, 25
  KX4_RHOPI_STEP 2, 43
  KX4_RHOPI_STEP 20, 62
  KX4_RHOPI_STEP 14, 18
  KX4_RHOPI_STEP 22, 39
  KX4_RHOPI_STEP 9, 61
  KX4_RHOPI_STEP 6, 20
  KX4_RHOPI_STEP 1, 44
  mov r6, r10
  movs r7, #10
  dls lr, r7
                                       // Instructions:    21
                                       // Expected cycles: 24
                                       // Expected IPC:    0.88
                                       //
                                       // Cycle bound:     24.0
                                       // IPC bound:       0.88
                                       //
                                       // Wall time:     0.03s
                                       // User time:     0.03s
                                       //
                                       // ----- cycle (expected) ------>
                                       // 0                        25
                                       // |------------------------|----
        vldrw.U32 q4, [r6, #32]        // *.............................
        vldrw.U32 q1, [r6, #48]        // ..*...........................
        vbic.U32 q6, q1, q4            // ...*..........................
        vldrw.U32 q0, [r6, #64]        // ....*.........................
        vbic.U32 q5, q0, q1            // .....*........................
        vldrw.U32 q2, [r6, #16]        // ......*.......................
        veor.U32 q7, q5, q4            // .......*......................
        vldrw.U32 q5, [r6, #0]         // ........*.....................
        vbic.U32 q3, q4, q2            // .........*....................
        vstrw.32 q7, [r6, #32]         // ..........*...................
        vbic.U32 q4, q5, q0            // ...........*..................
        veor.U32 q7, q4, q1            // .............*................
        vstrw.32 q7, [r6, #48]         // ..............*...............
        vbic.U32 q1, q2, q5            // ...............*..............
        veor.U32 q4, q1, q0            // .................*............
        vstrw.32 q4, [r6, #64]         // ..................*...........
        veor.U32 q6, q6, q2            // ...................*..........
        vstrw.32 q6, [r6, #16]         // ....................*.........
        veor.U32 q5, q3, q5            // .....................*........
        vstrw.32 q5, [r6, #0]          // ......................*.......
        add r6, r6, #80                // .......................*......

                                        // ------ cycle (expected) ------>
                                        // 0                        25
                                        // |------------------------|-----
        // vldrw.U32 q4, [r6, #32]      // *..............................
        // vldrw.U32 q6, [r6, #48]      // ..*............................
        // vbic.U32 q1, q6, q4          // ...*...........................
        // vldrw.U32 q2, [r6, #64]      // ....*..........................
        // vbic.U32 q0, q2, q6          // .....*.........................
        // vldrw.U32 q7, [r6, #16]      // ......*........................
        // veor.U32 q5, q0, q4          // .......*.......................
        // vbic.U32 q0, q4, q7          // .........*.....................
        // vldrw.U32 q4, [r6, #0]       // ........*......................
        // vbic.U32 q3, q4, q2          // ...........*...................
        // vstrw.32 q5, [r6, #32]       // ..........*....................
        // veor.U32 q6, q3, q6          // .............*.................
        // vstrw.32 q6, [r6, #48]       // ..............*................
        // vbic.U32 q3, q7, q4          // ...............*...............
        // veor.U32 q2, q3, q2          // .................*.............
        // vstrw.32 q2, [r6, #64]       // ..................*............
        // veor.U32 q2, q1, q7          // ...................*...........
        // vstrw.32 q2, [r6, #16]       // ....................*..........
        // veor.U32 q0, q0, q4          // .....................*.........
        // vstrw.32 q0, [r6, #0]        // ......................*........
        // add r6, r6, #80              // .......................*.......

        sub lr, lr, #1
.p2align 2
m55_keccakx4_chi_row_loop:
                                       // Instructions:    21
                                       // Expected cycles: 25
                                       // Expected IPC:    0.84
                                       //
                                       // Cycle bound:     25.0
                                       // IPC bound:       0.84
                                       //
                                       // Wall time:     13.30s
                                       // User time:     13.30s
                                       //
                                       // ----- cycle (expected) ------>
                                       // 0                        25
                                       // |------------------------|----
        vldrw.U32 q4, [r6, #32]        // e.............................
        vldrw.U32 q6, [r6, #48]        // ..e...........................
        vbic.U32 q1, q6, q4            // ...e..........................
        vldrw.U32 q2, [r6, #64]        // ....e.........................
        vbic.U32 q0, q2, q6            // .....e........................
        vldrw.U32 q7, [r6, #16]        // ......e.......................
        veor.U32 q5, q0, q4            // .......e......................
        vbic.U32 q0, q4, q7            // .........e....................
        vldrw.U32 q4, [r6, #0]         // ..........e...................
        vbic.U32 q3, q4, q2            // ...........e..................
        vstrw.32 q5, [r6, #32]         // ............e.................
        veor.U32 q6, q3, q6            // .............e................
        vstrw.32 q6, [r6, #48]         // ..............e...............
        vbic.U32 q3, q7, q4            // ...............e..............
        veor.U32 q2, q3, q2            // .................e............
        vstrw.32 q2, [r6, #64]         // ..................e...........
        veor.U32 q2, q1, q7            // ...................e..........
        vstrw.32 q2, [r6, #16]         // ....................e.........
        veor.U32 q0, q0, q4            // .....................e........
        vstrw.32 q0, [r6, #0]          // ......................e.......
        add r6, r6, #80                // .......................e......

                                        // ------ cycle (expected) ------>
                                        // 0                        25
                                        // |------------------------|-----
        // vldrw.u32 q0, [r6, #0]       // ..........e..............'.....
        // vldrw.u32 q1, [r6, #16]      // ......e..................'.....
        // vldrw.u32 q2, [r6, #32]      // e........................~.....
        // vldrw.u32 q3, [r6, #48]      // ..e......................'.~...
        // vldrw.u32 q4, [r6, #64]      // ....e....................'...~.
        // vbic.u32 q5, q2, q1          // .........e...............'.....
        // veor.u32 q5, q5, q0          // .....................e...'.....
        // vstrw.32 q5, [r6, #0]        // ......................e..'.....
        // vbic.u32 q5, q3, q2          // ...e.....................'..~..
        // veor.u32 q5, q5, q1          // ...................e.....'.....
        // vstrw.32 q5, [r6, #16]       // ....................e....'.....
        // vbic.u32 q5, q4, q3          // .....e...................'.....
        // veor.u32 q5, q5, q2          // .......e.................'.....
        // vstrw.32 q5, [r6, #32]       // ............e............'.....
        // vbic.u32 q5, q0, q4          // ...........e.............'.....
        // veor.u32 q5, q5, q3          // .............e...........'.....
        // vstrw.32 q5, [r6, #48]       // ..............e..........'.....
        // vbic.u32 q5, q1, q0          // ...............e.........'.....
        // veor.u32 q5, q5, q4          // .................e.......'.....
        // vstrw.32 q5, [r6, #64]       // ..................e......'.....
        // add r6, r6, #80              // .......................e.'.....

        le lr, m55_keccakx4_chi_row_loop
                // Instructions:    0
                // Expected cycles: 0
                // Expected IPC:    0.00
                //
                // Wall time:     0.00s
                // User time:     0.00s
                //
  ldrd r2, r3, [r9], #8
  vdup.32 q2, r2
  vdup.32 q3, r3
  vldrw.u32 q0, [r10]
  vldrw.u32 q1, [r11]
  veor.u32 q0, q0, q2
  veor.u32 q1, q1, q3
  vstrw.32 q0, [r10]
  vstrw.32 q1, [r11]
  subs r8, r8, #1
  bne .Lkeccakx4_round
  add sp, sp, #160
  vpop {d8-d15}
  pop {r4-r12, lr}
  bx lr
M55_X4_END m55_keccakx4_permute
.section .rodata.m55_keccakx4_round_constants, "a", %progbits
.align 3
.global m55_keccakx4_round_constants
.type m55_keccakx4_round_constants, %object
m55_keccakx4_round_constants:
  .word 0x00000001, 0x00000000
  .word 0x00008082, 0x00000000
  .word 0x0000808a, 0x80000000
  .word 0x80008000, 0x80000000
  .word 0x0000808b, 0x00000000
  .word 0x80000001, 0x00000000
  .word 0x80008081, 0x80000000
  .word 0x00008009, 0x80000000
  .word 0x0000008a, 0x00000000
  .word 0x00000088, 0x00000000
  .word 0x80008009, 0x00000000
  .word 0x8000000a, 0x00000000
  .word 0x8000808b, 0x00000000
  .word 0x0000008b, 0x80000000
  .word 0x00008089, 0x80000000
  .word 0x00008003, 0x80000000
  .word 0x00008002, 0x80000000
  .word 0x00000080, 0x80000000
  .word 0x0000800a, 0x00000000
  .word 0x8000000a, 0x80000000
  .word 0x80008081, 0x80000000
  .word 0x00008080, 0x80000000
  .word 0x80000001, 0x00000000
  .word 0x80008008, 0x80000000
.size m55_keccakx4_round_constants, .-m55_keccakx4_round_constants
.section .note.GNU-stack, "", %progbits

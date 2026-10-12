/* SPDX-License-Identifier: MIT */

/*
 * Handwritten SHAKE128 x4 wrapper for the M55 SoA Keccak state.
 *
 * The public ten-argument ABI matches fips202x4_mve.c. Full words are packed
 * directly into MVE lanes and absorbed four streams at a time. Public partial
 * lengths use exact byte loops, so no input or output byte outside the caller
 * supplied range is touched. The 800-byte state is kept 16-byte aligned in a
 * fixed stack arena. Input/output lengths control loops but are public.
 */

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

/*
 * Wrapper local frame before the aligned state arena:
 *   0..12 outputs, 16 outlen, 20..32 inputs, 36 inlen, 40 state pointer,
 *   44 current squeeze block length.
 */
M55_X4_FUNCTION m55_shake128x4
  push {r4-r12, lr}
#if defined(M55_KECCAK_X4_GATHER)
  vpush {d8-d15}
#endif
  sub sp, sp, #880
  str r0, [sp, #0]
  str r1, [sp, #4]
  str r2, [sp, #8]
  str r3, [sp, #12]
#if defined(M55_KECCAK_X4_GATHER)
  ldr r4, [sp, #984]
  ldr r5, [sp, #988]
  ldr r6, [sp, #992]
  ldr r7, [sp, #996]
  ldr r8, [sp, #1000]
  ldr r9, [sp, #1004]
#else
  ldr r4, [sp, #920]
  ldr r5, [sp, #924]
  ldr r6, [sp, #928]
  ldr r7, [sp, #932]
  ldr r8, [sp, #936]
  ldr r9, [sp, #940]
#endif
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
#if defined(M55_KECCAK_X4_GATHER)
  vpop {d8-d15}
#endif
  pop {r4-r12, lr}
  bx lr
M55_X4_END m55_shake128x4

/* r0=state, r1..r4=input streams, r5=public length (0..168). */
.Lkeccakx4_absorb_block:
#if defined(M55_KECCAK_X4_GATHER)
  /* q4 holds byte offsets from stream 0 to streams 0..3. Keeping one scalar
   * base and gathering four words halves the full-word memory instructions. */
  push {r6, lr}
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
#if defined(M55_KECCAK_X4_U2_EVENIZE)
  /* Slothy U2 executes two 64-bit lanes per loop iteration.  SHAKE128 has
   * 21 lanes per full block, so consume one lane before the even-sized loop
   * instead of silently dropping the final lane. */
  tst r6, #1
  beq .Lkeccakx4_absorb_gather_u2_even
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
  subs r6, r6, #1
  beq .Lkeccakx4_absorb_gather_u2_done
.Lkeccakx4_absorb_gather_u2_even:
#endif
  dls lr, r6
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
  le lr, m55_keccakx4_absorb_gather_loop
#if defined(M55_KECCAK_X4_U2_EVENIZE)
.Lkeccakx4_absorb_gather_u2_done:
#endif
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
  pop {r6, pc}
#else
  push {r6, lr}
.Lkeccakx4_absorb_word_test:
  cmp r5, #8
  blo .Lkeccakx4_absorb_partial
  ldr r6, [r1]
  ldr r7, [r1, #4]
  vmov s0, r6
  vmov s4, r7
  ldr r6, [r2]
  ldr r7, [r2, #4]
  vmov s1, r6
  vmov s5, r7
  ldr r6, [r3]
  ldr r7, [r3, #4]
  vmov s2, r6
  vmov s6, r7
  ldr r6, [r4]
  ldr r7, [r4, #4]
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
  adds r2, r2, #8
  adds r3, r3, #8
  adds r4, r4, #8
  subs r5, r5, #8
  b .Lkeccakx4_absorb_word_test

.Lkeccakx4_absorb_partial:
  cbz r5, .Lkeccakx4_absorb_return
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
.Lkeccakx4_absorb_return:
  pop {r6, pc}
#endif

/* r6=byte pointer, r7=length 1..7; returns r8:r9 little-endian. */
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

/* r0=state, r1=public position, r2=byte value broadcast to four streams. */
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

/* r0=state, r1..r4=output streams, r5=public length (1..168). */
.Lkeccakx4_squeeze_block:
#if defined(M55_KECCAK_X4_GATHER)
  /* The inverse layout conversion uses two four-lane scatter stores instead
   * of eight scalar stores for each complete 64-bit word. */
  push {r6, lr}
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
#if defined(M55_KECCAK_X4_U2_EVENIZE)
  /* Match the absorb-side odd-lane handling before Slothy's U2 loop. */
  tst r6, #1
  beq .Lkeccakx4_squeeze_scatter_u2_even
  vldrw.u32 q0, [r0]
  vldrw.u32 q1, [r0, #400]
  vstrw.32 q0, [r1, q4]
  vstrw.32 q1, [r7, q4]
  add r0, r0, #16
  add r1, r1, #8
  add r7, r7, #8
  subs r6, r6, #1
  beq .Lkeccakx4_squeeze_scatter_u2_done
.Lkeccakx4_squeeze_scatter_u2_even:
#endif
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
#if defined(M55_KECCAK_X4_U2_EVENIZE)
.Lkeccakx4_squeeze_scatter_u2_done:
#endif
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
  pop {r6, pc}
#else
  push {r6, lr}
.Lkeccakx4_squeeze_word_test:
  cmp r5, #8
  blo .Lkeccakx4_squeeze_partial
  vldrw.u32 q0, [r0]
  vldrw.u32 q1, [r0, #400]
  vmov r6, s0
  vmov r7, s4
  str r6, [r1]
  str r7, [r1, #4]
  vmov r6, s1
  vmov r7, s5
  str r6, [r2]
  str r7, [r2, #4]
  vmov r6, s2
  vmov r7, s6
  str r6, [r3]
  str r7, [r3, #4]
  vmov r6, s3
  vmov r7, s7
  str r6, [r4]
  str r7, [r4, #4]
  adds r0, r0, #16
  adds r1, r1, #8
  adds r2, r2, #8
  adds r3, r3, #8
  adds r4, r4, #8
  subs r5, r5, #8
  b .Lkeccakx4_squeeze_word_test

.Lkeccakx4_squeeze_partial:
  cbz r5, .Lkeccakx4_squeeze_return
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
.Lkeccakx4_squeeze_return:
  pop {r6, pc}
#endif

/* r6=byte pointer, r7=length 1..7, r8:r9=little-endian value. */
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

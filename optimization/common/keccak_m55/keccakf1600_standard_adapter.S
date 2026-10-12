@
@ Implementation by the Keccak, Keyak and Ketje Teams, namely, Guido Bertoni,
@ Joan Daemen, Michaël Peeters, Gilles Van Assche and Ronny Van Keer, hereby
@ denoted as "the implementer".
@ Additional optimizations by Alexandre Adomnicai.
@
@ For more information, feedback or questions, please refer to our websites:
@ http://keccak.noekeon.org/
@ http://keyak.noekeon.org/
@ http://ketje.noekeon.org/
@
@ To the extent possible under law, the implementer has waived all copyright
@ and related or neighboring rights to the source code in this file.
@ http://creativecommons.org/publicdomain/zero/1.0/
@

@ WARNING: These functions work only on little endian CPU with@ ARMv7m architecture (ARM Cortex-M3, ...).


	.thumb
	.syntax unified
.text
#include "keccak_bitinterleave.inc"
@----------------------------------------------------------------------------
@
@ void KeccakF1600_StatePermute_adomnicai_m4_standard(uint64_t *state)
@
@ Adapter for the standard little-endian uint64_t lane representation used by
@ KPQClean fips202.c. The imported permutation itself keeps its state in the
@ pqm4 bit-interleaved representation, so convert all 25 lanes before and after
@ it. Constant-time rationale: both conversion loops have exactly 25 iterations
@ and access sequential public offsets; neither control flow nor addresses
@ depend on state data.
@
.align 2
.global KeccakF1600_StatePermute_adomnicai_m4_standard
.type KeccakF1600_StatePermute_adomnicai_m4_standard,%function
KeccakF1600_StatePermute_adomnicai_m4_standard:
	push	{ r4 - r8, lr }
	mov	r8, r0
	movs	r2, #25
KeccakF1600_StatePermute_StandardToInterleaved:
	ldrd	r4, r5, [r0]
	movs	r6, #0
	movs	r7, #0
	toBitInterleaving r4, r5, r6, r7, r3, 0
	strd	r6, r7, [r0], #8
	subs	r2, r2, #1
	bne	KeccakF1600_StatePermute_StandardToInterleaved

	mov	r0, r8
	bl	KeccakF1600_StatePermute_adomnicai_m4

	mov	r0, r8
	movs	r2, #25
KeccakF1600_StatePermute_InterleavedToStandard:
	ldrd	r4, r5, [r0]
	fromBitInterleaving r4, r5, r3
	strd	r4, r5, [r0], #8
	subs	r2, r2, #1
	bne	KeccakF1600_StatePermute_InterleavedToStandard
	pop	{ r4 - r8, pc }

.size KeccakF1600_StatePermute_adomnicai_m4_standard, .-KeccakF1600_StatePermute_adomnicai_m4_standard

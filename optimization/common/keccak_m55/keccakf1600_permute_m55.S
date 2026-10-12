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
#include "keccak_state_backend.inc"
#include "keccak_round_macros.inc"
#include "keccak_round_constants.inc"
@----------------------------------------------------------------------------
@
@ void KeccakF1600_StatePermute( void *state )
@
.align 8
.global   KeccakF1600_StatePermute_adomnicai_m4
.type KeccakF1600_StatePermute_adomnicai_m4,%function
KeccakF1600_StatePermute_adomnicai_m4:
	adr		r1, KeccakF1600_StatePermute_RoundConstantsWithTerminator
	push	{ r4 - r12, lr }
#if defined(M55_KECCAK_FPSCRATCH_WORDS) || defined(M55_KECCAK_RC_FPGROUP)
	@ d8-d15 are callee-saved under the hard-float ABI. The save/restore count
	@ is fixed and independent of all state and message data.
	vpush	{ d8 - d15 }
#endif
#if defined(M55_KECCAK_FPSCRATCH_WORDS)
#if M55_KECCAK_FPSCRATCH_WORDS == 24
	vldmia	r0, { s0 - s23 }
#elif M55_KECCAK_FPSCRATCH_WORDS == 32
	vldmia	r0, { s0 - s31 }
#else
#error "M55_KECCAK_FPSCRATCH_WORDS must be 24 or 32"
#endif
#endif
	sub		sp, #mSize
	str		r1, [sp, #mRC]
KeccakF1600_StatePermute_RoundLoop:
#if defined(M55_KECCAK_RC_FPGROUP)
	ldr		r1, [sp, #mRC]
	vldmia	r1, { s24 - s31 }
#endif
slothy_start:
	KeccakRound0
	KeccakRound1
	KeccakRound2
	KeccakRound3
slothy_end:
	bne		KeccakF1600_StatePermute_RoundLoop
	add		sp, #mSize
#if defined(M55_KECCAK_FPSCRATCH_WORDS)
#if M55_KECCAK_FPSCRATCH_WORDS == 24
	vstmia	r0, { s0 - s23 }
#else
	vstmia	r0, { s0 - s31 }
#endif
#endif
#if defined(M55_KECCAK_FPSCRATCH_WORDS) || defined(M55_KECCAK_RC_FPGROUP)
	vpop	{ d8 - d15 }
#endif
	pop		{ r4 - r12, pc }

.size KeccakF1600_StatePermute_adomnicai_m4, .-KeccakF1600_StatePermute_adomnicai_m4

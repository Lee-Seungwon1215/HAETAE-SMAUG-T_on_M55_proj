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

.align 8
KeccakF1600_StatePermute_RoundConstantsWithTerminator:
	@		0			1
		.long 		0x00000001,	0x00000000
		.long 		0x00000000,	0x00000089
		.long 		0x00000000,	0x8000008b
		.long 		0x00000000,	0x80008080

		.long 		0x00000001,	0x0000008b
		.long 		0x00000001,	0x00008000
		.long 		0x00000001,	0x80008088
		.long 		0x00000001,	0x80000082

		.long 		0x00000000,	0x0000000b
		.long 		0x00000000,	0x0000000a
		.long 		0x00000001,	0x00008082
		.long 		0x00000000,	0x00008003

		.long 		0x00000001,	0x0000808b
		.long 		0x00000001,	0x8000000b
		.long 		0x00000001,	0x8000008a
		.long 		0x00000001,	0x80000081

		.long 		0x00000000,	0x80000081
		.long 		0x00000000,	0x80000008
		.long 		0x00000000,	0x00000083
		.long 		0x00000000,	0x80008003

		.long 		0x00000001,	0x80008088
		.long 		0x00000000,	0x80000088
		.long 		0x00000001,	0x00008000
		.long 		0x00000000,	0x80008082

		.long 		0x000000FF	@terminator

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


	@ Credit: Henry S. Warren, Hacker's Delight, Addison-Wesley, 2002
.macro	toBitInterleaving	x0,x1,s0,s1,t,over

	and		\t,\x0,#0x55555555
	orr		\t,\t,\t, LSR #1
	and		\t,\t,#0x33333333
	orr		\t,\t,\t, LSR #2
	and		\t,\t,#0x0F0F0F0F
	orr		\t,\t,\t, LSR #4
	and		\t,\t,#0x00FF00FF
	bfi		\t,\t,#8, #8
	.if \over != 0
	lsr		\s0,\t, #8
	.else
	eor		\s0,\s0,\t, LSR #8
	.endif

	and		\t,\x1,#0x55555555
	orr		\t,\t,\t, LSR #1
	and		\t,\t,#0x33333333
	orr		\t,\t,\t, LSR #2
	and		\t,\t,#0x0F0F0F0F
	orr		\t,\t,\t, LSR #4
	and		\t,\t,#0x00FF00FF
	orr		\t,\t,\t, LSR #8
	eor		\s0,\s0,\t, LSL #16

	and		\t,\x0,#0xAAAAAAAA
	orr		\t,\t,\t, LSL #1
	and		\t,\t,#0xCCCCCCCC
	orr		\t,\t,\t, LSL #2
	and		\t,\t,#0xF0F0F0F0
	orr		\t,\t,\t, LSL #4
	and		\t,\t,#0xFF00FF00
	orr		\t,\t,\t, LSL #8
	.if \over != 0
	lsr		\s1,\t, #16
	.else
	eor		\s1,\s1,\t, LSR #16
	.endif

	and		\t,\x1,#0xAAAAAAAA
	orr		\t,\t,\t, LSL #1
	and		\t,\t,#0xCCCCCCCC
	orr		\t,\t,\t, LSL #2
	and		\t,\t,#0xF0F0F0F0
	orr		\t,\t,\t, LSL #4
	and		\t,\t,#0xFF00FF00
	orr		\t,\t,\t, LSL #8
	bfc		\t, #0, #16
	eors	\s1,\s1,\t
	.endm

	@ Credit: Henry S. Warren, Hacker's Delight, Addison-Wesley, 2002
.macro	fromBitInterleaving		x0, x1, t

	movs	\t, \x0					@ t = x0@
	bfi		\x0, \x1, #16, #16		@ x0 = (x0 & 0x0000FFFF) | (x1 << 16)@
	bfc		\x1, #0, #16			@	x1 = (t >> 16) | (x1 & 0xFFFF0000)@
	orr		\x1, \x1, \t, LSR #16

    eor		\t, \x0, \x0, LSR #8    @ t = (x0 ^ (x0 >>  8)) & 0x0000FF00UL@  x0 = x0 ^ t ^ (t <<  8)@
	and		\t, #0x0000FF00
    eors	\x0, \x0, \t
    eor		\x0, \x0, \t, LSL #8

    eor		\t, \x0, \x0, LSR #4	@ t = (x0 ^ (x0 >>  4)) & 0x00F000F0UL@  x0 = x0 ^ t ^ (t <<  4)@
	and		\t, #0x00F000F0
    eors	\x0, \x0, \t
    eor		\x0, \x0, \t, LSL #4

    eor		\t, \x0, \x0, LSR #2	@ t = (x0 ^ (x0 >>  2)) & 0x0C0C0C0CUL@  x0 = x0 ^ t ^ (t <<  2)@
	and		\t, #0x0C0C0C0C
    eors	\x0, \x0, \t
    eor		\x0, \x0, \t, LSL #2

    eor		\t, \x0, \x0, LSR #1	@ t = (x0 ^ (x0 >>  1)) & 0x22222222UL@  x0 = x0 ^ t ^ (t <<  1)@
	and		\t, #0x22222222
    eors	\x0, \x0, \t
    eor		\x0, \x0, \t, LSL #1

    eor		\t, \x1, \x1, LSR #8    @ t = (x1 ^ (x1 >>  8)) & 0x0000FF00UL@  x1 = x1 ^ t ^ (t <<  8)@
	and		\t, #0x0000FF00
    eors	\x1, \x1, \t
    eor		\x1, \x1, \t, LSL #8

    eor		\t, \x1, \x1, LSR #4	@ t = (x1 ^ (x1 >>  4)) & 0x00F000F0UL@  x1 = x1 ^ t ^ (t <<  4)@
	and		\t, #0x00F000F0
    eors	\x1, \x1, \t
    eor		\x1, \x1, \t, LSL #4

    eor		\t, \x1, \x1, LSR #2	@ t = (x1 ^ (x1 >>  2)) & 0x0C0C0C0CUL@  x1 = x1 ^ t ^ (t <<  2)@
	and		\t, #0x0C0C0C0C
    eors	\x1, \x1, \t
    eor		\x1, \x1, \t, LSL #2

    eor		\t, \x1, \x1, LSR #1	@ t = (x1 ^ (x1 >>  1)) & 0x22222222UL@  x1 = x1 ^ t ^ (t <<  1)@
	and		\t, #0x22222222
    eors	\x1, \x1, \t
    eor		\x1, \x1, \t, LSL #1
	.endm

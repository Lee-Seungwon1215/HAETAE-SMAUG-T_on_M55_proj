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

@	--- offsets in state
.equ Aba0, 0*4
.equ Aba1, 1*4
.equ Abe0, 2*4
.equ Abe1, 3*4
.equ Abi0, 4*4
.equ Abi1, 5*4
.equ Abo0, 6*4
.equ Abo1, 7*4
.equ Abu0, 8*4
.equ Abu1, 9*4
.equ Aga0, 10*4
.equ Aga1, 11*4
.equ Age0, 12*4
.equ Age1, 13*4
.equ Agi0, 14*4
.equ Agi1, 15*4
.equ Ago0, 16*4
.equ Ago1, 17*4
.equ Agu0, 18*4
.equ Agu1, 19*4
.equ Aka0, 20*4
.equ Aka1, 21*4
.equ Ake0, 22*4
.equ Ake1, 23*4
.equ Aki0, 24*4
.equ Aki1, 25*4
.equ Ako0, 26*4
.equ Ako1, 27*4
.equ Aku0, 28*4
.equ Aku1, 29*4
.equ Ama0, 30*4
.equ Ama1, 31*4
.equ Ame0, 32*4
.equ Ame1, 33*4
.equ Ami0, 34*4
.equ Ami1, 35*4
.equ Amo0, 36*4
.equ Amo1, 37*4
.equ Amu0, 38*4
.equ Amu1, 39*4
.equ Asa0, 40*4
.equ Asa1, 41*4
.equ Ase0, 42*4
.equ Ase1, 43*4
.equ Asi0, 44*4
.equ Asi1, 45*4
.equ Aso0, 46*4
.equ Aso1, 47*4
.equ Asu0, 48*4
.equ Asu1, 49*4

@	--- offsets on stack
.equ mDa0, 0*4
.equ mDa1, 1*4
.equ mDo0, 2*4
.equ mDo1, 3*4
.equ mDi0, 4*4
.equ mRC	, 5*4
.equ mSize, 6*4

@ Phase 4-K2 state access abstraction. With FP scratch enabled, a fixed public
@ prefix of the bit-interleaved state lives in s0-s23 or s0-s31 for the entire
@ 24-round call. GPR/FP moves replace state-memory traffic. The mapping and
@ iteration count are compile-time constants and contain no secret-dependent
@ branch, address, predicate, or loop count.
.macro state_load dst, ofs
#if defined(M55_KECCAK_FPSCRATCH_WORDS)
  .if \ofs < (M55_KECCAK_FPSCRATCH_WORDS * 4)
    .if \ofs == 0
      vmov \dst, s0
    .elseif \ofs == 4
      vmov \dst, s1
    .elseif \ofs == 8
      vmov \dst, s2
    .elseif \ofs == 12
      vmov \dst, s3
    .elseif \ofs == 16
      vmov \dst, s4
    .elseif \ofs == 20
      vmov \dst, s5
    .elseif \ofs == 24
      vmov \dst, s6
    .elseif \ofs == 28
      vmov \dst, s7
    .elseif \ofs == 32
      vmov \dst, s8
    .elseif \ofs == 36
      vmov \dst, s9
    .elseif \ofs == 40
      vmov \dst, s10
    .elseif \ofs == 44
      vmov \dst, s11
    .elseif \ofs == 48
      vmov \dst, s12
    .elseif \ofs == 52
      vmov \dst, s13
    .elseif \ofs == 56
      vmov \dst, s14
    .elseif \ofs == 60
      vmov \dst, s15
    .elseif \ofs == 64
      vmov \dst, s16
    .elseif \ofs == 68
      vmov \dst, s17
    .elseif \ofs == 72
      vmov \dst, s18
    .elseif \ofs == 76
      vmov \dst, s19
    .elseif \ofs == 80
      vmov \dst, s20
    .elseif \ofs == 84
      vmov \dst, s21
    .elseif \ofs == 88
      vmov \dst, s22
    .elseif \ofs == 92
      vmov \dst, s23
    .elseif \ofs == 96
      vmov \dst, s24
    .elseif \ofs == 100
      vmov \dst, s25
    .elseif \ofs == 104
      vmov \dst, s26
    .elseif \ofs == 108
      vmov \dst, s27
    .elseif \ofs == 112
      vmov \dst, s28
    .elseif \ofs == 116
      vmov \dst, s29
    .elseif \ofs == 120
      vmov \dst, s30
    .elseif \ofs == 124
      vmov \dst, s31
    .else
      .error "invalid FP state offset"
    .endif
  .else
    ldr.w \dst, [r0, #\ofs]
  .endif
#else
  ldr.w \dst, [r0, #\ofs]
#endif
.endm

.macro state_store src, ofs
#if defined(M55_KECCAK_FPSCRATCH_WORDS)
  .if \ofs < (M55_KECCAK_FPSCRATCH_WORDS * 4)
    .if \ofs == 0
      vmov s0, \src
    .elseif \ofs == 4
      vmov s1, \src
    .elseif \ofs == 8
      vmov s2, \src
    .elseif \ofs == 12
      vmov s3, \src
    .elseif \ofs == 16
      vmov s4, \src
    .elseif \ofs == 20
      vmov s5, \src
    .elseif \ofs == 24
      vmov s6, \src
    .elseif \ofs == 28
      vmov s7, \src
    .elseif \ofs == 32
      vmov s8, \src
    .elseif \ofs == 36
      vmov s9, \src
    .elseif \ofs == 40
      vmov s10, \src
    .elseif \ofs == 44
      vmov s11, \src
    .elseif \ofs == 48
      vmov s12, \src
    .elseif \ofs == 52
      vmov s13, \src
    .elseif \ofs == 56
      vmov s14, \src
    .elseif \ofs == 60
      vmov s15, \src
    .elseif \ofs == 64
      vmov s16, \src
    .elseif \ofs == 68
      vmov s17, \src
    .elseif \ofs == 72
      vmov s18, \src
    .elseif \ofs == 76
      vmov s19, \src
    .elseif \ofs == 80
      vmov s20, \src
    .elseif \ofs == 84
      vmov s21, \src
    .elseif \ofs == 88
      vmov s22, \src
    .elseif \ofs == 92
      vmov s23, \src
    .elseif \ofs == 96
      vmov s24, \src
    .elseif \ofs == 100
      vmov s25, \src
    .elseif \ofs == 104
      vmov s26, \src
    .elseif \ofs == 108
      vmov s27, \src
    .elseif \ofs == 112
      vmov s28, \src
    .elseif \ofs == 116
      vmov s29, \src
    .elseif \ofs == 120
      vmov s30, \src
    .elseif \ofs == 124
      vmov s31, \src
    .else
      .error "invalid FP state offset"
    .endif
  .else
    str.w \src, [r0, #\ofs]
  .endif
#else
  str.w \src, [r0, #\ofs]
#endif
.endm

@ Optional current-group round-constant cache. s24-s31 hold eight public
@ constants loaded once per fixed four-round group.
.macro rc_load dst, ofs
#if defined(M55_KECCAK_RC_FPGROUP)
  .if \ofs == 0
    vmov \dst, s24
  .elseif \ofs == 4
    vmov \dst, s25
  .elseif \ofs == 8
    vmov \dst, s26
  .elseif \ofs == 12
    vmov \dst, s27
  .elseif \ofs == 16
    vmov \dst, s28
  .elseif \ofs == 20
    vmov \dst, s29
  .elseif \ofs == 24
    vmov \dst, s30
  .elseif \ofs == 28
    vmov \dst, s31
  .else
    .error "invalid FP round-constant offset"
  .endif
#else
  ldr r1, [sp, #mRC]
  ldr \dst, [r1, #\ofs]
#endif
.endm

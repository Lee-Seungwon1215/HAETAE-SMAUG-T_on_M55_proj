CONFIG ?=

# Paper-stage presets.  Keep CONFIG empty for direct access to every historical
# selector below; use one of these names for the controlled comparisons in the
# paper.  Command-line variable assignments still take precedence.
ifneq ($(CONFIG),)
ifeq ($(filter $(CONFIG),reference intrinsic assembly final),)
$(error Unsupported CONFIG='$(CONFIG)'; choose reference, intrinsic, assembly, or final)
endif
endif

ifeq ($(CONFIG),reference)
IMPLEMENTATION ?= reference
endif

ifeq ($(CONFIG),intrinsic)
IMPLEMENTATION ?= optimization
KECCAK_X4_BACKEND ?= intrinsic
HAETAE_NTT_BACKEND ?= intrinsic
HAETAE_FFT_BACKEND ?= intrinsic
HAETAE_SAMPLER_BACKEND ?= intrinsic
SMAUGT_TOOM_BACKEND ?= intrinsic
endif

ifeq ($(CONFIG),assembly)
IMPLEMENTATION ?= optimization
KECCAK_X4_BACKEND ?= asm_hand
KECCAK_X4_ASM_VARIANT ?= hand_gather_v2
HAETAE_NTT_BACKEND ?= asm_hand
HAETAE_NTT_ASM_VARIANT ?= hand_l332_vld4_u2
HAETAE_NTT_ARITH_VARIANT ?= d1
HAETAE_NTT_INVERSE_VARIANT ?= hand
HAETAE_NTT_POINTWISE_VARIANT ?= hand
HAETAE_NTT_POINTWISE_ACC_VARIANT ?= hand
HAETAE_NTT_BARRETT_VARIANT ?= hand
HAETAE_NTT_CENTERED_VARIANT ?= hand
HAETAE_FFT_BACKEND ?= asm_hand
HAETAE_FFT_ASM_VARIANT ?= hand_soa_l2222
HAETAE_SAMPLER_BACKEND ?= asm_hand
HAETAE_SAMPLER_ASM_VARIANT ?= unroll
SMAUGT_TOOM_BACKEND ?= asm_hand
SMAUGT_TOOM_ASM_VARIANT ?= hand_fused
SMAUGT_TOOM_ASM_COMPONENT ?= all
endif

ifeq ($(CONFIG),final)
IMPLEMENTATION ?= optimization
KECCAK_X4_BACKEND ?= asm_slothy
KECCAK_X4_ASM_VARIANT ?= slothy_gather_u2_fullperm_chi5_allpaths
HAETAE_NTT_BACKEND ?= asm_slothy
HAETAE_NTT_ASM_VARIANT ?= paired_vld4_u2_l34_hier
HAETAE_NTT_ARITH_VARIANT ?= d2
HAETAE_NTT_INVERSE_VARIANT ?= slothy_u1
HAETAE_NTT_POINTWISE_VARIANT ?= slothy_lowmem
HAETAE_NTT_POINTWISE_ACC_VARIANT ?= slothy_block2
HAETAE_NTT_BARRETT_VARIANT ?= slothy_u2_nostack
HAETAE_NTT_CENTERED_VARIANT ?= slothy_mask_u2
HAETAE_FFT_BACKEND ?= asm_slothy
HAETAE_FFT_ASM_VARIANT ?= soa_reuse_vecsq_u2_hier
# The generated sampler schedule regressed on hardware; the selected final
# configuration intentionally retains the manually unrolled sampler.
HAETAE_SAMPLER_BACKEND ?= asm_hand
HAETAE_SAMPLER_ASM_VARIANT ?= unroll
SMAUGT_TOOM_BACKEND ?= asm_slothy
SMAUGT_TOOM_ASM_VARIANT ?= slothy_fused_tailpred_stable_u1
SMAUGT_TOOM_ASM_COMPONENT ?= all
endif

ALGORITHM ?= haetae2
ITERATIONS ?= 3
IMPLEMENTATION ?= reference
PROFILE ?= 0
DIFF_TEST ?= 0
REJECTION_COUNT ?= 0
CT_TEST ?= 0
CT_OPERATION ?= none
CT_PAIRS ?= 1000
CT_BACKEND ?= unknown
PLATFORM ?= mps3-an547
STM32_CLOCK ?= 600mhz
# Phase 4-K1 deployment winner: retain the pqm4 bit-interleaved state between
# public SHAKE/SHA3 calls and convert only at absorb/squeeze boundaries.
KECCAK_VARIANT ?= pqm4_persistent
# The final order-balanced STM32N657 campaign selected the M55 scalar schedule
# plus four-stream MVE SHAKE for all released HAETAE/SMAUG parameter sets.
FINAL_KECCAK_ALGORITHMS := haetae2 haetae3 haetae5 smaugt1 smaugt3 smaugt5
ifneq ($(filter $(ALGORITHM),$(FINAL_KECCAK_ALGORITHMS)),)
KECCAK_KERNEL ?= m55_scalar_sched
KECCAK_X4 ?= 1
else
ifeq ($(PLATFORM),stm32n657)
KECCAK_KERNEL ?= m4_scalar
else
KECCAK_KERNEL ?= m55_fpscratch32
endif
KECCAK_X4 ?= 0
endif
KECCAK_RC ?= memory
# Phase 19 selected the complete Slothy permutation/wrapper path for HAETAE;
# Phase 20b reproduced the same win for every SMAUG-T parameter set.
KECCAK_X4_BACKEND ?= asm_slothy
KECCAK_X4_ASM_VARIANT ?= $(if $(filter asm_slothy,$(KECCAK_X4_BACKEND)),slothy_gather_u2_fullperm_chi5_allpaths,$(if $(filter hybrid_slothy,$(KECCAK_X4_BACKEND)),slothy_gather_u2,hand_v1))
# Phase 3.7-1 deployment winner for HAETAE. Clean and SMAUG builds override
# this through ACTIVE_NTT_VARIANT, so their source selection is unchanged.
NTT_VARIANT ?= m32_l332_vld4_u2
# Phase 3.7-2 deployment winner for HAETAE accumulation and single products.
ACC_VARIANT ?= aftersum_dmax_tight_precenter_poly
# Phase 5-A deployment winner: qrd Q16 products, SoA working layout, vector
# twiddles, exact unit-root four-point front end, and fused final sqabs.
FFT_VARIANT ?= qrd_soa_l2222_vtbl_fused_u4
# Phase 5-B deployment winner: fixed full-table 8-lane CDT scan plus fixed
# 16-byte vector load/rearrangement for each Gaussian candidate.
SAMPLER_VARIANT ?= cdt_mve_load
# Orthogonal HAETAE kernel backends.  The incumbent intrinsic implementation
# remains the default; handwritten and Slothy-generated sources are linked only
# when explicitly selected.  Backend/variant IDs are part of BUILD_DIR below so
# candidate objects can never be reused across experiments.
HAETAE_NTT_BACKEND ?= intrinsic
HAETAE_FFT_BACKEND ?= intrinsic
HAETAE_SAMPLER_BACKEND ?= intrinsic
HAETAE_NTT_ASM_VARIANT ?= hand_l332_vld4_u2
HAETAE_NTT_ARITH_VARIANT ?= d1
HAETAE_NTT_INVERSE_VARIANT ?= hand
HAETAE_NTT_POINTWISE_VARIANT ?= hand
HAETAE_NTT_POINTWISE_ACC_VARIANT ?= hand
HAETAE_NTT_BARRETT_VARIANT ?= hand
HAETAE_NTT_CENTERED_VARIANT ?= hand
HAETAE_FFT_ASM_VARIANT ?= hand_soa_l2222
HAETAE_SAMPLER_ASM_VARIANT ?= loop
# Phase 3.7-3 SMAUG deployment winner: 32-coefficient outer leaves plus
# reusable Toom evaluation caching. Clean and HAETAE builds override these
# through ACTIVE_MUL_VARIANT/ACTIVE_MUL_CACHE.
MUL_VARIANT ?= toom4_outer32
# Selecting a preserved non-Toom or non-deployment variant by its one build
# flag must remain sufficient; only the deployment outer32 defaults to B4.
MUL_CACHE ?= $(if $(filter toom4_outer32,$(MUL_VARIANT)),1,0)
SHIFT_VARIANT ?= mask
# The original intrinsic and handwritten implementations remain selectable.
# The stable tail-predicated variant keeps the measured Slothy winner's exact
# machine code while recording successful Slothy coverage of all 15 regions.
SMAUGT_TOOM_BACKEND ?= asm_slothy
SMAUGT_TOOM_ASM_VARIANT ?= $(if $(filter asm_slothy,$(SMAUGT_TOOM_BACKEND)),slothy_fused_tailpred_stable_u1,hand_fused)
SMAUGT_TOOM_ASM_COMPONENT ?= all
# Project direction changed on 2026-08-07: dedicated stack/lifetime
# optimization is no longer part of the deployment target. Historical
# candidates remain explicit build flags for reproducibility, but every
# parameter defaults to the pre-Phase-H/S top-level memory layout.
HAETAE2_STACK_CONFIG ?= baseline
HAETAE3_STACK_CONFIG ?= baseline
HAETAE5_STACK_CONFIG ?= baseline
SMAUGT1_STACK_CONFIG ?= baseline
SMAUGT3_STACK_CONFIG ?= baseline
SMAUGT5_STACK_CONFIG ?= baseline

SUPPORTED_ALGORITHMS := haetae2 haetae3 haetae5 smaugt1 smaugt3 smaugt5
SUPPORTED_IMPLEMENTATIONS := reference optimization
SUPPORTED_PLATFORMS := mps3-an547 stm32n657

ifeq ($(filter $(PROFILE),0 1),)
$(error PROFILE must be 0 or 1)
endif
ifeq ($(filter $(DIFF_TEST),0 1),)
$(error DIFF_TEST must be 0 or 1)
endif
ifeq ($(filter $(REJECTION_COUNT),0 1),)
$(error REJECTION_COUNT must be 0 or 1)
endif
ifeq ($(filter $(CT_TEST),0 1),)
$(error CT_TEST must be 0 or 1)
endif
ifeq ($(CT_TEST),1)
ifeq ($(filter $(CT_OPERATION),ntt invntt pointwise fft sampler poly_mul_acc matrix_vec_mult keccak_x4 shake_x4),)
$(error Unsupported CT_OPERATION='$(CT_OPERATION)')
endif
endif
ifeq ($(filter $(KECCAK_X4),0 1),)
$(error KECCAK_X4 must be 0 or 1)
endif

ifneq ($(filter $(PLATFORM),$(SUPPORTED_PLATFORMS)),$(PLATFORM))
$(error Unsupported PLATFORM='$(PLATFORM)'; supported: $(SUPPORTED_PLATFORMS))
endif

ifeq ($(PLATFORM),stm32n657)
ifeq ($(filter $(STM32_CLOCK),boot 600mhz),)
$(error Unsupported STM32_CLOCK='$(STM32_CLOCK)'; choose boot or 600mhz)
endif
ACTIVE_PLATFORM_CLOCK := $(STM32_CLOCK)
else
ACTIVE_PLATFORM_CLOCK := model
endif

CC      := arm-none-eabi-gcc
SIZE    := arm-none-eabi-size
QEMU    := qemu-system-arm
NM      := arm-none-eabi-nm
OBJCOPY := arm-none-eabi-objcopy
PROBE_RS ?= probe-rs
PROBE_SERIAL ?=
GDB ?= arm-none-eabi-gdb
PYTHON ?= python3
STM32_RUN_TIMEOUT ?= 300
STM32_GDB_CONNECTION ?=

ifeq ($(ALGORITHM),haetae2)
SCHEME_NAME := HAETAE2
ALG_SUBDIR  := haetae/HAETAE2
REQUESTED_STACK_CONFIG := $(HAETAE2_STACK_CONFIG)
BENCH_SRC   := bench/bench_haetae.c
BENCH_FAMILY := haetae
REJECTION_SYMBOL := cryptolab_haetae_mode2_polyfixveclk_sample_hyperball
PROFILE_SCHEME_SYMBOLS := cryptolab_haetae_mode2_ntt \
  cryptolab_haetae_mode2_invntt_tomont \
  cryptolab_haetae_mode2_poly_pointwise_montgomery \
  cryptolab_haetae_mode2_poly_uniform \
  cryptolab_haetae_mode2_poly_uniform_eta \
  cryptolab_haetae_mode2_polyfixveclk_sample_hyperball \
  cryptolab_haetae_mode2_encode_h cryptolab_haetae_mode2_decode_h \
  cryptolab_haetae_mode2_encode_hb_z1 cryptolab_haetae_mode2_decode_hb_z1 \
  cryptolab_haetae_mode2_pack_vk cryptolab_haetae_mode2_unpack_vk \
  cryptolab_haetae_mode2_pack_sk cryptolab_haetae_mode2_unpack_sk \
  cryptolab_haetae_mode2_pack_sig cryptolab_haetae_mode2_unpack_sig \
  cryptolab_haetae_mode2_pack_vec_highbits \
  cryptolab_haetae_mode2_pack_poly_lsb \
  cryptolab_haetae_mode2_fft cryptolab_haetae_mode2_fft_init_and_bitrev \
  cryptolab_haetae_mode2_complex_fp_sqabs \
  cryptolab_haetae_mode2_polyvecmk_sk_singular_value \
  cryptolab_haetae_mode2_fixpoint_square \
  cryptolab_haetae_mode2_fixpoint_newton_invsqrt \
  cryptolab_haetae_mode2_fixpoint_mul_rnd13 \
  cryptolab_haetae_mode2_fixpoint_add
endif
ifeq ($(ALGORITHM),haetae3)
SCHEME_NAME := HAETAE3
ALG_SUBDIR  := haetae/HAETAE3
REQUESTED_STACK_CONFIG := $(HAETAE3_STACK_CONFIG)
BENCH_SRC   := bench/bench_haetae.c
BENCH_FAMILY := haetae
REJECTION_SYMBOL := cryptolab_haetae_mode3_polyfixveclk_sample_hyperball
PROFILE_SCHEME_SYMBOLS := cryptolab_haetae_mode3_ntt \
  cryptolab_haetae_mode3_invntt_tomont \
  cryptolab_haetae_mode3_poly_pointwise_montgomery \
  cryptolab_haetae_mode3_poly_uniform \
  cryptolab_haetae_mode3_poly_uniform_eta \
  cryptolab_haetae_mode3_polyfixveclk_sample_hyperball \
  cryptolab_haetae_mode3_encode_h cryptolab_haetae_mode3_decode_h \
  cryptolab_haetae_mode3_encode_hb_z1 cryptolab_haetae_mode3_decode_hb_z1 \
  cryptolab_haetae_mode3_pack_vk cryptolab_haetae_mode3_unpack_vk \
  cryptolab_haetae_mode3_pack_sk cryptolab_haetae_mode3_unpack_sk \
  cryptolab_haetae_mode3_pack_sig cryptolab_haetae_mode3_unpack_sig \
  cryptolab_haetae_mode3_pack_vec_highbits \
  cryptolab_haetae_mode3_pack_poly_lsb \
  cryptolab_haetae_mode3_fft cryptolab_haetae_mode3_fft_init_and_bitrev \
  cryptolab_haetae_mode3_complex_fp_sqabs \
  cryptolab_haetae_mode3_polyvecmk_sk_singular_value \
  cryptolab_haetae_mode3_fixpoint_square \
  cryptolab_haetae_mode3_fixpoint_newton_invsqrt \
  cryptolab_haetae_mode3_fixpoint_mul_rnd13 \
  cryptolab_haetae_mode3_fixpoint_add
endif
ifeq ($(ALGORITHM),haetae5)
SCHEME_NAME := HAETAE5
ALG_SUBDIR  := haetae/HAETAE5
REQUESTED_STACK_CONFIG := $(HAETAE5_STACK_CONFIG)
BENCH_SRC   := bench/bench_haetae.c
BENCH_FAMILY := haetae
REJECTION_SYMBOL := cryptolab_haetae_mode5_polyfixveclk_sample_hyperball
PROFILE_SCHEME_SYMBOLS := cryptolab_haetae_mode5_ntt \
  cryptolab_haetae_mode5_invntt_tomont \
  cryptolab_haetae_mode5_poly_pointwise_montgomery \
  cryptolab_haetae_mode5_poly_uniform \
  cryptolab_haetae_mode5_poly_uniform_eta \
  cryptolab_haetae_mode5_polyfixveclk_sample_hyperball \
  cryptolab_haetae_mode5_encode_h cryptolab_haetae_mode5_decode_h \
  cryptolab_haetae_mode5_encode_hb_z1 cryptolab_haetae_mode5_decode_hb_z1 \
  cryptolab_haetae_mode5_pack_vk cryptolab_haetae_mode5_unpack_vk \
  cryptolab_haetae_mode5_pack_sk cryptolab_haetae_mode5_unpack_sk \
  cryptolab_haetae_mode5_pack_sig cryptolab_haetae_mode5_unpack_sig \
  cryptolab_haetae_mode5_pack_vec_highbits \
  cryptolab_haetae_mode5_pack_poly_lsb \
  cryptolab_haetae_mode5_fft cryptolab_haetae_mode5_fft_init_and_bitrev \
  cryptolab_haetae_mode5_complex_fp_sqabs \
  cryptolab_haetae_mode5_polyvecmk_sk_singular_value \
  cryptolab_haetae_mode5_fixpoint_square \
  cryptolab_haetae_mode5_fixpoint_newton_invsqrt \
  cryptolab_haetae_mode5_fixpoint_mul_rnd13 \
  cryptolab_haetae_mode5_fixpoint_add
endif
ifeq ($(ALGORITHM),smaugt1)
SCHEME_NAME := SMAUG-T1
ALG_SUBDIR  := smaug/SMAUG-T1
REQUESTED_STACK_CONFIG := $(SMAUGT1_STACK_CONFIG)
BENCH_SRC   := bench/bench_smaugt.c
BENCH_FAMILY := smaugt
PROFILE_SCHEME_SYMBOLS := cryptolab_smaugt_mode1_poly_mul_acc \
  cryptolab_smaugt_mode1_poly_mul_acc_masked \
  cryptolab_smaugt_mode1_vec_vec_mult_add \
  cryptolab_smaugt_mode1_matrix_vec_mult_add \
  cryptolab_smaugt_mode1_matrix_vec_mult_sub \
  cryptolab_smaugt_mode1_hwt cryptolab_smaugt_mode1_sp_cbd \
  cryptolab_smaugt_mode1_d_gaussian \
  cryptolab_smaugt_mode1_pack_enck cryptolab_smaugt_mode1_unpack_enck \
  cryptolab_smaugt_mode1_pack_deck cryptolab_smaugt_mode1_unpack_deck \
  cryptolab_smaugt_mode1_pack_ct cryptolab_smaugt_mode1_unpack_ct \
  cryptolab_smaugt_mode1_pack_R2_10 cryptolab_smaugt_mode1_unpack_R2_10 \
  cryptolab_smaugt_mode1_pack_R2_8 cryptolab_smaugt_mode1_unpack_R2_8 \
  cryptolab_smaugt_mode1_pack_R2_5 cryptolab_smaugt_mode1_unpack_R2_5
endif
ifeq ($(ALGORITHM),smaugt3)
SCHEME_NAME := SMAUG-T3
ALG_SUBDIR  := smaug/SMAUG-T3
REQUESTED_STACK_CONFIG := $(SMAUGT3_STACK_CONFIG)
BENCH_SRC   := bench/bench_smaugt.c
BENCH_FAMILY := smaugt
PROFILE_SCHEME_SYMBOLS := cryptolab_smaugt_mode3_poly_mul_acc \
  cryptolab_smaugt_mode3_poly_mul_acc_masked \
  cryptolab_smaugt_mode3_vec_vec_mult_add \
  cryptolab_smaugt_mode3_matrix_vec_mult_add \
  cryptolab_smaugt_mode3_matrix_vec_mult_sub \
  cryptolab_smaugt_mode3_hwt cryptolab_smaugt_mode3_sp_cbd \
  cryptolab_smaugt_mode3_d_gaussian \
  cryptolab_smaugt_mode3_pack_enck cryptolab_smaugt_mode3_unpack_enck \
  cryptolab_smaugt_mode3_pack_deck cryptolab_smaugt_mode3_unpack_deck \
  cryptolab_smaugt_mode3_pack_ct cryptolab_smaugt_mode3_unpack_ct \
  cryptolab_smaugt_mode3_pack_R2_11 cryptolab_smaugt_mode3_unpack_R2_11 \
  cryptolab_smaugt_mode3_pack_R2_9 cryptolab_smaugt_mode3_unpack_R2_9 \
  cryptolab_smaugt_mode3_pack_R2_4 cryptolab_smaugt_mode3_unpack_R2_4
endif
ifeq ($(ALGORITHM),smaugt5)
SCHEME_NAME := SMAUG-T5
ALG_SUBDIR  := smaug/SMAUG-T5
REQUESTED_STACK_CONFIG := $(SMAUGT5_STACK_CONFIG)
BENCH_SRC   := bench/bench_smaugt.c
BENCH_FAMILY := smaugt
PROFILE_SCHEME_SYMBOLS := cryptolab_smaugt_mode5_poly_mul_acc \
  cryptolab_smaugt_mode5_poly_mul_acc_masked \
  cryptolab_smaugt_mode5_vec_vec_mult_add \
  cryptolab_smaugt_mode5_matrix_vec_mult_add \
  cryptolab_smaugt_mode5_matrix_vec_mult_sub \
  cryptolab_smaugt_mode5_hwt cryptolab_smaugt_mode5_sp_cbd \
  cryptolab_smaugt_mode5_d_gaussian \
  cryptolab_smaugt_mode5_pack_enck cryptolab_smaugt_mode5_unpack_enck \
  cryptolab_smaugt_mode5_pack_deck cryptolab_smaugt_mode5_unpack_deck \
  cryptolab_smaugt_mode5_pack_ct cryptolab_smaugt_mode5_unpack_ct \
  cryptolab_smaugt_mode5_pack_R2_11 cryptolab_smaugt_mode5_unpack_R2_11 \
  cryptolab_smaugt_mode5_pack_R2_9 cryptolab_smaugt_mode5_unpack_R2_9 \
  cryptolab_smaugt_mode5_pack_R2_7 cryptolab_smaugt_mode5_unpack_R2_7
endif

REQUESTED_STACK_CONFIG ?= baseline

ifeq ($(filter $(ALGORITHM),$(SUPPORTED_ALGORITHMS)),)
$(error Unsupported ALGORITHM='$(ALGORITHM)'; choose one of: $(SUPPORTED_ALGORITHMS))
endif

ifeq ($(filter $(IMPLEMENTATION),$(SUPPORTED_IMPLEMENTATIONS)),)
$(error Unsupported IMPLEMENTATION='$(IMPLEMENTATION)'; choose one of: $(SUPPORTED_IMPLEMENTATIONS))
endif

ifeq ($(IMPLEMENTATION),reference)
ACTIVE_KECCAK_VARIANT := c
ALG_DIR := reference/$(ALG_SUBDIR)
COMMON_DIR := reference/common
else
ifeq ($(filter $(KECCAK_VARIANT),c pqm4 pqm4_persistent),)
$(error Unsupported KECCAK_VARIANT='$(KECCAK_VARIANT)'; choose one of: c pqm4 pqm4_persistent)
endif
ACTIVE_KECCAK_VARIANT := $(KECCAK_VARIANT)
ALG_DIR := optimization/$(ALG_SUBDIR)
COMMON_DIR := optimization/common
endif

ACTIVE_KECCAK_KERNEL := m4_scalar
ACTIVE_KECCAK_RC := memory
ACTIVE_KECCAK_X4 := 0
ifeq ($(IMPLEMENTATION),optimization)
ACTIVE_KECCAK_X4 := $(KECCAK_X4)
ifeq ($(filter $(KECCAK_X4_BACKEND),intrinsic asm_hand asm_slothy hybrid_slothy),)
$(error Unsupported KECCAK_X4_BACKEND='$(KECCAK_X4_BACKEND)'; choose intrinsic, asm_hand, asm_slothy, or hybrid_slothy)
endif
ifeq ($(KECCAK_X4_BACKEND),asm_hand)
ifeq ($(filter $(KECCAK_X4_ASM_VARIANT),hand_v1 hand_gather_v2),)
$(error Unsupported handwritten KECCAK_X4_ASM_VARIANT='$(KECCAK_X4_ASM_VARIANT)'; choose hand_v1 or hand_gather_v2)
endif
endif
ifeq ($(KECCAK_X4_BACKEND),asm_slothy)
ifeq ($(filter $(KECCAK_X4_ASM_VARIANT),slothy_gather_u2 slothy_gather_u2_chi_u1 slothy_gather_u2_chi_u2 slothy_gather_u2_fullperm_u1 slothy_gather_u2_fullperm_u2 slothy_gather_u2_fullperm_handchi slothy_gather_u2_fullperm_chi2straight slothy_gather_u2_fullperm_chi5straight slothy_gather_u2_fullperm_chi5_allpaths slothy_gather_u2_fullperm_chi5_allpaths_input preslothy_fullperm_chi5_allpaths),)
$(error Unsupported Slothy KECCAK_X4_ASM_VARIANT='$(KECCAK_X4_ASM_VARIANT)'; selected complete variant is slothy_gather_u2_fullperm_chi5_allpaths)
endif
endif
ifeq ($(KECCAK_X4_BACKEND),hybrid_slothy)
ifneq ($(KECCAK_X4_ASM_VARIANT),slothy_gather_u2)
$(error Unsupported hybrid Slothy KECCAK_X4_ASM_VARIANT='$(KECCAK_X4_ASM_VARIANT)'; choose slothy_gather_u2)
endif
endif
ifneq ($(ACTIVE_KECCAK_VARIANT),c)
ifeq ($(filter $(KECCAK_KERNEL),m4_scalar m55_scalar_sched m55_fpscratch24 m55_fpscratch32),)
$(error Unsupported KECCAK_KERNEL='$(KECCAK_KERNEL)'; choose one of: m4_scalar m55_scalar_sched m55_fpscratch24 m55_fpscratch32)
endif
ifeq ($(filter $(KECCAK_RC),memory fpgroup),)
$(error Unsupported KECCAK_RC='$(KECCAK_RC)'; choose one of: memory fpgroup)
endif
ifeq ($(KECCAK_KERNEL),m55_fpscratch32)
ifeq ($(KECCAK_RC),fpgroup)
$(error KECCAK_KERNEL=m55_fpscratch32 cannot use KECCAK_RC=fpgroup: s0-s31 already hold state)
endif
endif
ACTIVE_KECCAK_KERNEL := $(KECCAK_KERNEL)
ACTIVE_KECCAK_RC := $(KECCAK_RC)
endif
endif

ACTIVE_NTT_VARIANT := scalar
ACTIVE_ACC_VARIANT := none
ACTIVE_MUL_VARIANT := scalar
ACTIVE_MUL_CACHE := 0
ACTIVE_SHIFT_VARIANT := copy
ACTIVE_FFT_VARIANT := scalar
ACTIVE_SAMPLER_VARIANT := scalar
ifeq ($(IMPLEMENTATION),optimization)
ifeq ($(BENCH_FAMILY),haetae)
ifeq ($(filter $(HAETAE_NTT_BACKEND),intrinsic asm_hand asm_slothy),)
$(error Unsupported HAETAE_NTT_BACKEND='$(HAETAE_NTT_BACKEND)'; choose intrinsic, asm_hand, or asm_slothy)
endif
ifeq ($(filter $(HAETAE_NTT_ARITH_VARIANT),d1 d2),)
$(error Unsupported HAETAE_NTT_ARITH_VARIANT='$(HAETAE_NTT_ARITH_VARIANT)'; choose d1 or d2)
endif
ifeq ($(filter $(HAETAE_NTT_INVERSE_VARIANT),hand preslothy slothy_functional slothy_u1),)
$(error Unsupported HAETAE_NTT_INVERSE_VARIANT='$(HAETAE_NTT_INVERSE_VARIANT)'; choose hand, preslothy, slothy_functional, or slothy_u1)
endif
ifeq ($(filter $(HAETAE_NTT_POINTWISE_VARIANT),hand preslothy_lowmem slothy_u1 slothy_lowmem),)
$(error Unsupported HAETAE_NTT_POINTWISE_VARIANT='$(HAETAE_NTT_POINTWISE_VARIANT)'; choose hand, preslothy_lowmem, slothy_u1, or slothy_lowmem)
endif
ifeq ($(filter $(HAETAE_NTT_POINTWISE_ACC_VARIANT),hand preslothy_block2 slothy_lowmem slothy_lowmem_r11 slothy_block2),)
$(error Unsupported HAETAE_NTT_POINTWISE_ACC_VARIANT='$(HAETAE_NTT_POINTWISE_ACC_VARIANT)'; choose hand, preslothy_block2, slothy_lowmem, slothy_lowmem_r11, or slothy_block2)
endif
ifeq ($(filter $(HAETAE_NTT_BARRETT_VARIANT),hand hand_dls preslothy_nostack slothy_u1 slothy_u2 slothy_u2_nostack),)
$(error Unsupported HAETAE_NTT_BARRETT_VARIANT='$(HAETAE_NTT_BARRETT_VARIANT)'; choose hand, hand_dls, preslothy_nostack, slothy_u1, slothy_u2, or slothy_u2_nostack)
endif
ifeq ($(filter $(HAETAE_NTT_CENTERED_VARIANT),hand hand_dls hand_dls_u2 mask_hand preslothy_mask slothy_mask_u1 slothy_mask_u2),)
$(error Unsupported HAETAE_NTT_CENTERED_VARIANT='$(HAETAE_NTT_CENTERED_VARIANT)'; choose hand, hand_dls, hand_dls_u2, mask_hand, preslothy_mask, slothy_mask_u1, or slothy_mask_u2)
endif
ifeq ($(HAETAE_NTT_BACKEND),asm_hand)
ifeq ($(filter $(HAETAE_NTT_ASM_VARIANT),generic_vld42 paired_vld4_u1 paired_vld4_u2 paired_vld4_u4 hand_l332_vld4_u2),)
$(error Unsupported handwritten HAETAE_NTT_ASM_VARIANT='$(HAETAE_NTT_ASM_VARIANT)'; choose generic_vld42, paired_vld4_u1, paired_vld4_u2, or paired_vld4_u4)
endif
endif
ifeq ($(HAETAE_NTT_BACKEND),asm_slothy)
ifeq ($(filter $(HAETAE_NTT_ASM_VARIANT),preslothy paired_vld4_u1 paired_vld4_u2 paired_vld4_u2_l34_hier paired_vld4_u2_pad_l12_layout paired_vld4_u2_l34_hier_pad_l12_layout paired_vld4_u4_l12_l34_hier paired_vld4_u4_l56_l34_hier paired_vld4_u4_hier),)
$(error Unsupported Slothy HAETAE_NTT_ASM_VARIANT='$(HAETAE_NTT_ASM_VARIANT)'; choose paired_vld4_u1, paired_vld4_u2, paired_vld4_u2_l34_hier, paired_vld4_u2_pad_l12_layout, paired_vld4_u2_l34_hier_pad_l12_layout, paired_vld4_u4_l12_l34_hier, paired_vld4_u4_l56_l34_hier, or paired_vld4_u4_hier)
endif
endif
ifeq ($(filter $(HAETAE_FFT_BACKEND),intrinsic asm_hand asm_slothy),)
$(error Unsupported HAETAE_FFT_BACKEND='$(HAETAE_FFT_BACKEND)'; choose intrinsic, asm_hand, or asm_slothy)
endif
ifeq ($(HAETAE_FFT_BACKEND),asm_hand)
ifeq ($(filter $(HAETAE_FFT_ASM_VARIANT),hand_soa_l2222 soa_reuse_sep),)
$(error Unsupported handwritten HAETAE_FFT_ASM_VARIANT='$(HAETAE_FFT_ASM_VARIANT)'; choose soa_reuse_sep)
endif
endif
ifeq ($(HAETAE_FFT_BACKEND),asm_slothy)
ifeq ($(filter $(HAETAE_FFT_ASM_VARIANT),preslothy_vecsq_u2 functional to_soa_u1 to_soa_u2 to_soa_u4 first2_u1 first2_u1_u2 from_soa_u4 fused_to_soa_u4 soa_reuse_u1 soa_reuse_u2 soa_reuse_vecsq_functional soa_reuse_vecsq_u1 soa_reuse_vecsq_u2_hier),)
$(error Unsupported Slothy HAETAE_FFT_ASM_VARIANT='$(HAETAE_FFT_ASM_VARIANT)'; choose functional, a retained stage ID, soa_reuse_u1/u2, or soa_reuse_vecsq_functional/u1/u2_hier)
endif
endif
ifeq ($(filter $(HAETAE_SAMPLER_BACKEND),intrinsic asm_hand asm_slothy),)
$(error Unsupported HAETAE_SAMPLER_BACKEND='$(HAETAE_SAMPLER_BACKEND)'; choose intrinsic, asm_hand, or asm_slothy)
endif
ifeq ($(HAETAE_SAMPLER_BACKEND),asm_hand)
ifeq ($(filter $(HAETAE_SAMPLER_ASM_VARIANT),loop unroll),)
$(error Unsupported handwritten HAETAE_SAMPLER_ASM_VARIANT='$(HAETAE_SAMPLER_ASM_VARIANT)'; choose loop or unroll)
endif
endif
ifeq ($(HAETAE_SAMPLER_BACKEND),asm_slothy)
ifeq ($(filter $(HAETAE_SAMPLER_ASM_VARIANT),preslothy_mask mask_u1 mask_u2),)
$(error Unsupported Slothy HAETAE_SAMPLER_ASM_VARIANT='$(HAETAE_SAMPLER_ASM_VARIANT)'; choose preslothy_mask, mask_u1, or mask_u2)
endif
endif
ifeq ($(filter $(NTT_VARIANT),scalar m32_l44 m32_l332 m32_l332_vld2 m32_l332_vld4 m32_l332_vld4_m3 m32_l332_vld4_u2),)
$(error Unsupported NTT_VARIANT='$(NTT_VARIANT)'; choose one of: scalar m32_l44 m32_l332 m32_l332_vld2 m32_l332_vld4 m32_l332_vld4_m3 m32_l332_vld4_u2)
endif
ACTIVE_NTT_VARIANT := $(NTT_VARIANT)
ifeq ($(filter $(ACC_VARIANT),perterm_d1_centered aftersum_d2_centered aftersum_dmax_tight aftersum_dmax_tight_precenter aftersum_dmax_tight_precenter_poly),)
$(error Unsupported ACC_VARIANT='$(ACC_VARIANT)'; choose one of: perterm_d1_centered aftersum_d2_centered aftersum_dmax_tight aftersum_dmax_tight_precenter aftersum_dmax_tight_precenter_poly; aftersum_d3_tight is proven int32-unsafe)
endif
ACTIVE_ACC_VARIANT := $(ACC_VARIANT)
ifeq ($(filter $(FFT_VARIANT),scalar wide_i_l2222_gpr_sep qrd_i_l2222_gpr_sep qrd_soa_l2222_gpr_sep qrd_i_l332_gpr_sep qrd_i_l44_gpr_sep qrd_i_l2222_vtbl_sep qrd_soa_l332_gpr_sep qrd_soa_l44_gpr_sep qrd_soa_l2222_vtbl_sep qrd_soa_l332_vtbl_sep qrd_soa_l44_vtbl_sep qrd_soa_l2222_vtbl_fused qrd_soa_l2222_vtbl_sep_u4 qrd_soa_l2222_vtbl_fused_u4),)
$(error Unsupported FFT_VARIANT='$(FFT_VARIANT)'; see Phase 5-A in OPTIMIZATION_LOG.md for preserved variants)
endif
ACTIVE_FFT_VARIANT := $(FFT_VARIANT)
ifeq ($(filter $(SAMPLER_VARIANT),scalar cdt_mve cdt_mve_load),)
$(error Unsupported SAMPLER_VARIANT='$(SAMPLER_VARIANT)'; choose one of: scalar cdt_mve cdt_mve_load)
endif
ACTIVE_SAMPLER_VARIANT := $(SAMPLER_VARIANT)
endif
ifeq ($(BENCH_FAMILY),smaugt)
ifeq ($(filter $(SMAUGT_TOOM_BACKEND),intrinsic asm_hand asm_slothy),)
$(error Unsupported SMAUGT_TOOM_BACKEND='$(SMAUGT_TOOM_BACKEND)'; choose intrinsic, asm_hand, or asm_slothy)
endif
ifeq ($(SMAUGT_TOOM_BACKEND),asm_hand)
ifneq ($(SMAUGT_TOOM_ASM_VARIANT),hand_fused)
$(error Unsupported handwritten SMAUGT_TOOM_ASM_VARIANT='$(SMAUGT_TOOM_ASM_VARIANT)'; choose hand_fused)
endif
endif
ifeq ($(SMAUGT_TOOM_BACKEND),asm_slothy)
ifeq ($(filter $(SMAUGT_TOOM_ASM_VARIANT),preslothy_fused slothy_fused_u1 slothy_fused_tailpred_u1 slothy_fused_tailpred_stable_u1),)
$(error Unsupported Slothy SMAUGT_TOOM_ASM_VARIANT='$(SMAUGT_TOOM_ASM_VARIANT)'; choose preslothy_fused, slothy_fused_u1, slothy_fused_tailpred_u1, or slothy_fused_tailpred_stable_u1)
endif
endif
ifeq ($(filter $(MUL_VARIANT),scalar toom4_k2 toom4_sb64 toom4_k1 toom4_outer64 toom4_outer32 toom4_outer32_leafcall direct256_outer),)
$(error Unsupported MUL_VARIANT='$(MUL_VARIANT)'; choose one of: scalar toom4_k2 toom4_sb64 toom4_k1 toom4_outer64 toom4_outer32 toom4_outer32_leafcall direct256_outer)
endif
ACTIVE_MUL_VARIANT := $(MUL_VARIANT)
ifeq ($(filter $(MUL_CACHE),0 1),)
$(error Unsupported MUL_CACHE='$(MUL_CACHE)'; choose 0 or 1)
endif
ifeq ($(MUL_CACHE),1)
ifneq ($(filter $(MUL_VARIANT),toom4_k2 toom4_sb64 toom4_k1 toom4_outer64 toom4_outer32 toom4_outer32_leafcall),$(MUL_VARIANT))
$(error MUL_CACHE=1 requires a Toom MUL_VARIANT)
endif
endif
ACTIVE_MUL_CACHE := $(MUL_CACHE)
ifeq ($(filter $(SHIFT_VARIANT),copy mask),)
$(error Unsupported SHIFT_VARIANT='$(SHIFT_VARIANT)'; choose one of: copy mask)
endif
ACTIVE_SHIFT_VARIANT := $(SHIFT_VARIANT)
ifneq ($(SMAUGT_TOOM_BACKEND),intrinsic)
ifneq ($(MUL_VARIANT),toom4_outer32)
$(error SMAUGT_TOOM_BACKEND=$(SMAUGT_TOOM_BACKEND) currently requires MUL_VARIANT=toom4_outer32)
endif
CPPFLAGS += -DSMAUGT_TOOM_ASM=1
ifeq ($(filter $(SMAUGT_TOOM_ASM_COMPONENT),all evaluate multiply fused interpolate reduce),)
$(error Unsupported SMAUGT_TOOM_ASM_COMPONENT='$(SMAUGT_TOOM_ASM_COMPONENT)')
endif
ifneq ($(filter $(SMAUGT_TOOM_ASM_COMPONENT),all evaluate),)
CPPFLAGS += -DSMAUGT_TOOM_ASM_EVALUATE=1
endif
ifneq ($(filter $(SMAUGT_TOOM_ASM_COMPONENT),all multiply),)
CPPFLAGS += -DSMAUGT_TOOM_ASM_MULTIPLY=1
endif
ifneq ($(filter $(SMAUGT_TOOM_ASM_COMPONENT),all fused),)
CPPFLAGS += -DSMAUGT_TOOM_ASM_FUSED=1
endif
ifneq ($(filter $(SMAUGT_TOOM_ASM_COMPONENT),all interpolate),)
CPPFLAGS += -DSMAUGT_TOOM_ASM_INTERPOLATE=1
endif
ifneq ($(filter $(SMAUGT_TOOM_ASM_COMPONENT),all reduce),)
CPPFLAGS += -DSMAUGT_TOOM_ASM_REDUCE=1
endif
endif
endif
endif

ACTIVE_STACK_CONFIG := baseline
ifeq ($(IMPLEMENTATION),optimization)
ifeq ($(ALGORITHM),haetae5)
ifeq ($(filter $(REQUESTED_STACK_CONFIG),baseline sign_union01 sign_union02 sign_union03 sign_union03_verify_union01 sign_union03_verify_union01_inplace01 sign_union03_verify_union01_inplace01_view03 sign_union03_verify_union01_inplace01_view03_keygen_union01 sign_union03_verify_union01_inplace01_view03_keygen_union01_stream01 sign_union03_verify_union01_inplace01_view03_keygen_union01_stream01_hyper_outsign01 sign_union03_verify_union01_inplace01_view03_keygen_union01_stream01_hyper_outsign01_gauss_outbuf02 sign_union03_verify_union01_inplace01_view03_keygen_union01_stream01_hyper_outsign01_gauss_outbuf02_hyper_rawscratch03),)
$(error Unsupported HAETAE5_STACK_CONFIG='$(REQUESTED_STACK_CONFIG)'; choose one of: baseline sign_union01 sign_union02 sign_union03 sign_union03_verify_union01 sign_union03_verify_union01_inplace01 sign_union03_verify_union01_inplace01_view03 sign_union03_verify_union01_inplace01_view03_keygen_union01 sign_union03_verify_union01_inplace01_view03_keygen_union01_stream01 sign_union03_verify_union01_inplace01_view03_keygen_union01_stream01_hyper_outsign01 sign_union03_verify_union01_inplace01_view03_keygen_union01_stream01_hyper_outsign01_gauss_outbuf02 sign_union03_verify_union01_inplace01_view03_keygen_union01_stream01_hyper_outsign01_gauss_outbuf02_hyper_rawscratch03)
endif
else ifeq ($(ALGORITHM),haetae3)
ifeq ($(filter $(REQUESTED_STACK_CONFIG),baseline sign_union01 sign_union02_l sign_union02_l_verify_inplace01 sign_union02_l_verify_inplace01_union02 sign_union02_l_verify_inplace01_union02_view03 sign_union02_l_verify_inplace01_union02_view03_stream01 sign_union02_l_verify_inplace01_union02_view03_stream01_keygen_union01 sign_union02_l_verify_inplace01_union02_view03_stream01_keygen_union01_compact02 sign_union02_l_verify_inplace01_union02_view03_stream01_keygen_union01_compact02_hyper_outsign01 sign_union02_l_verify_inplace01_union02_view03_stream01_keygen_union01_compact02_hyper_outsign01_gauss_outbuf02),)
$(error Unsupported HAETAE3_STACK_CONFIG='$(REQUESTED_STACK_CONFIG)'; choose one of: baseline sign_union01 sign_union02_l sign_union02_l_verify_inplace01 sign_union02_l_verify_inplace01_union02 sign_union02_l_verify_inplace01_union02_view03 sign_union02_l_verify_inplace01_union02_view03_stream01 sign_union02_l_verify_inplace01_union02_view03_stream01_keygen_union01 sign_union02_l_verify_inplace01_union02_view03_stream01_keygen_union01_compact02 sign_union02_l_verify_inplace01_union02_view03_stream01_keygen_union01_compact02_hyper_outsign01 sign_union02_l_verify_inplace01_union02_view03_stream01_keygen_union01_compact02_hyper_outsign01_gauss_outbuf02)
endif
else ifeq ($(ALGORITHM),haetae2)
ifeq ($(filter $(REQUESTED_STACK_CONFIG),baseline sign_union01 sign_union02 sign_union02_z1rnd0 sign_union02_z1rnd0_verify_union01 sign_union02_z1rnd0_verify_union01_inplace01 sign_union02_z1rnd0_verify_union01_inplace01_view03 sign_union02_z1rnd0_verify_union01_inplace01_view03_stream01 sign_union02_z1rnd0_verify_union01_inplace01_view03_stream01_keygen_union01 sign_union02_z1rnd0_verify_union01_inplace01_view03_stream01_keygen_union01_compact02 sign_union02_z1rnd0_verify_union01_inplace01_view03_stream01_keygen_union01_compact02_hyper_outsign01 sign_union02_z1rnd0_verify_union01_inplace01_view03_stream01_keygen_union01_compact02_hyper_outsign01_gauss_outbuf03),)
$(error Unsupported HAETAE2_STACK_CONFIG='$(REQUESTED_STACK_CONFIG)'; choose one of: baseline sign_union01 sign_union02 sign_union02_z1rnd0 sign_union02_z1rnd0_verify_union01 sign_union02_z1rnd0_verify_union01_inplace01 sign_union02_z1rnd0_verify_union01_inplace01_view03 sign_union02_z1rnd0_verify_union01_inplace01_view03_stream01 sign_union02_z1rnd0_verify_union01_inplace01_view03_stream01_keygen_union01 sign_union02_z1rnd0_verify_union01_inplace01_view03_stream01_keygen_union01_compact02 sign_union02_z1rnd0_verify_union01_inplace01_view03_stream01_keygen_union01_compact02_hyper_outsign01 sign_union02_z1rnd0_verify_union01_inplace01_view03_stream01_keygen_union01_compact02_hyper_outsign01_gauss_outbuf03)
endif
else ifeq ($(ALGORITHM),smaugt5)
ifeq ($(filter $(REQUESTED_STACK_CONFIG),baseline enc_pk_lifetime01 enc_pk_lifetime01_astream02 enc_pk_lifetime01_astream02_directpack01),)
$(error Unsupported SMAUGT5_STACK_CONFIG='$(REQUESTED_STACK_CONFIG)'; choose one of: baseline enc_pk_lifetime01 enc_pk_lifetime01_astream02 enc_pk_lifetime01_astream02_directpack01)
endif
else
ifneq ($(REQUESTED_STACK_CONFIG),baseline)
$(error Stack candidates are not yet enabled for ALGORITHM='$(ALGORITHM)'; use baseline)
endif
endif
ACTIVE_STACK_CONFIG := $(REQUESTED_STACK_CONFIG)
endif

BUILD_DIR := build/$(PLATFORM)/clock-$(ACTIVE_PLATFORM_CLOCK)/$(IMPLEMENTATION)/$(ALGORITHM)/iter-$(ITERATIONS)/profile-$(PROFILE)/diff-$(DIFF_TEST)/rejection-$(REJECTION_COUNT)/ct-$(CT_TEST)-$(CT_OPERATION)-$(CT_BACKEND)-$(CT_PAIRS)/keccak-$(ACTIVE_KECCAK_VARIANT)/kernel-$(ACTIVE_KECCAK_KERNEL)/rc-$(ACTIVE_KECCAK_RC)/x4-$(ACTIVE_KECCAK_X4)-$(KECCAK_X4_BACKEND)-$(KECCAK_X4_ASM_VARIANT)/ntt-$(ACTIVE_NTT_VARIANT)-$(HAETAE_NTT_BACKEND)-$(HAETAE_NTT_ASM_VARIANT)-arith_$(HAETAE_NTT_ARITH_VARIANT)-inv_$(HAETAE_NTT_INVERSE_VARIANT)-point_$(HAETAE_NTT_POINTWISE_VARIANT)-pointacc_$(HAETAE_NTT_POINTWISE_ACC_VARIANT)-barrett_$(HAETAE_NTT_BARRETT_VARIANT)-centered_$(HAETAE_NTT_CENTERED_VARIANT)/acc-$(ACTIVE_ACC_VARIANT)/fft-$(ACTIVE_FFT_VARIANT)-$(HAETAE_FFT_BACKEND)-$(HAETAE_FFT_ASM_VARIANT)/sampler-$(ACTIVE_SAMPLER_VARIANT)-$(HAETAE_SAMPLER_BACKEND)-$(HAETAE_SAMPLER_ASM_VARIANT)/mul-$(ACTIVE_MUL_VARIANT)-$(SMAUGT_TOOM_BACKEND)-$(SMAUGT_TOOM_ASM_VARIANT)-$(SMAUGT_TOOM_ASM_COMPONENT)/cache-$(ACTIVE_MUL_CACHE)/shift-$(ACTIVE_SHIFT_VARIANT)/stack-$(ACTIVE_STACK_CONFIG)
ELF       := $(BUILD_DIR)/$(ALGORITHM).elf
MAP       := $(BUILD_DIR)/$(ALGORITHM).map

ALG_SOURCES := $(wildcard $(ALG_DIR)/*.c)
ALG_SOURCES := $(filter-out $(ALG_DIR)/kat_sign.c,$(ALG_SOURCES))
COMMON_SOURCES := $(COMMON_DIR)/fips202.c
ifeq ($(IMPLEMENTATION),optimization)
ifeq ($(KECCAK_X4_BACKEND),intrinsic)
COMMON_SOURCES += $(COMMON_DIR)/keccak_m55/keccakx4_mve.c
ifeq ($(ACTIVE_KECCAK_X4),1)
COMMON_SOURCES += $(COMMON_DIR)/keccak_m55/fips202x4_mve.c
CPPFLAGS += -DM55_KECCAK_X4=1
endif
else ifeq ($(KECCAK_X4_BACKEND),hybrid_slothy)
COMMON_SOURCES += $(COMMON_DIR)/keccak_m55/keccakx4_mve.c
ifeq ($(ACTIVE_KECCAK_X4),1)
CPPFLAGS += -DM55_KECCAK_X4=1
endif
else
CPPFLAGS += -DM55_KECCAK_X4_ASM=1
ifeq ($(KECCAK_X4_ASM_VARIANT),hand_gather_v2)
CPPFLAGS += -DM55_KECCAK_X4_GATHER=1
endif
ifeq ($(ACTIVE_KECCAK_X4),1)
CPPFLAGS += -DM55_KECCAK_X4=1
endif
endif
endif
CPPFLAGS += $(ALGORITHM_CPPFLAGS)
ifeq ($(ACTIVE_STACK_CONFIG),sign_union01)
CPPFLAGS += -DHAETAE_STACK_SIGN_UNION01=1
endif
ifneq ($(filter $(ACTIVE_STACK_CONFIG),enc_pk_lifetime01 enc_pk_lifetime01_astream02 enc_pk_lifetime01_astream02_directpack01),)
CPPFLAGS += -DSMAUGT_STACK_ENC_PK_LIFETIME01=1
endif
ifneq ($(filter $(ACTIVE_STACK_CONFIG),enc_pk_lifetime01_astream02 enc_pk_lifetime01_astream02_directpack01),)
CPPFLAGS += -DSMAUGT_STACK_ENC_ASTREAM02=1
endif
ifeq ($(ACTIVE_STACK_CONFIG),enc_pk_lifetime01_astream02_directpack01)
CPPFLAGS += -DSMAUGT_STACK_ENC_DIRECTPACK01=1
endif
ifneq ($(filter $(ACTIVE_STACK_CONFIG),sign_union02 sign_union02_z1rnd0 sign_union02_z1rnd0_verify_union01 sign_union02_z1rnd0_verify_union01_inplace01 sign_union02_z1rnd0_verify_union01_inplace01_view03 sign_union02_z1rnd0_verify_union01_inplace01_view03_stream01 sign_union02_z1rnd0_verify_union01_inplace01_view03_stream01_keygen_union01 sign_union02_z1rnd0_verify_union01_inplace01_view03_stream01_keygen_union01_compact02 sign_union02_z1rnd0_verify_union01_inplace01_view03_stream01_keygen_union01_compact02_hyper_outsign01 sign_union02_z1rnd0_verify_union01_inplace01_view03_stream01_keygen_union01_compact02_hyper_outsign01_gauss_outbuf03),)
CPPFLAGS += -DHAETAE_STACK_SIGN_UNION02=1
endif
ifneq ($(filter $(ACTIVE_STACK_CONFIG),sign_union02_z1rnd0 sign_union02_z1rnd0_verify_union01 sign_union02_z1rnd0_verify_union01_inplace01 sign_union02_z1rnd0_verify_union01_inplace01_view03 sign_union02_z1rnd0_verify_union01_inplace01_view03_stream01 sign_union02_z1rnd0_verify_union01_inplace01_view03_stream01_keygen_union01 sign_union02_z1rnd0_verify_union01_inplace01_view03_stream01_keygen_union01_compact02 sign_union02_z1rnd0_verify_union01_inplace01_view03_stream01_keygen_union01_compact02_hyper_outsign01 sign_union02_z1rnd0_verify_union01_inplace01_view03_stream01_keygen_union01_compact02_hyper_outsign01_gauss_outbuf03),)
CPPFLAGS += -DHAETAE_STACK_SIGN_Z1RND0_01=1
endif
ifneq ($(filter $(ACTIVE_STACK_CONFIG),sign_union02_z1rnd0_verify_union01 sign_union02_z1rnd0_verify_union01_inplace01 sign_union02_z1rnd0_verify_union01_inplace01_view03 sign_union02_z1rnd0_verify_union01_inplace01_view03_stream01 sign_union02_z1rnd0_verify_union01_inplace01_view03_stream01_keygen_union01 sign_union02_z1rnd0_verify_union01_inplace01_view03_stream01_keygen_union01_compact02 sign_union02_z1rnd0_verify_union01_inplace01_view03_stream01_keygen_union01_compact02_hyper_outsign01 sign_union02_z1rnd0_verify_union01_inplace01_view03_stream01_keygen_union01_compact02_hyper_outsign01_gauss_outbuf03),)
CPPFLAGS += -DHAETAE_STACK_VERIFY_UNION01=1
endif
ifneq ($(filter $(ACTIVE_STACK_CONFIG),sign_union02_z1rnd0_verify_union01_inplace01 sign_union02_z1rnd0_verify_union01_inplace01_view03 sign_union02_z1rnd0_verify_union01_inplace01_view03_stream01 sign_union02_z1rnd0_verify_union01_inplace01_view03_stream01_keygen_union01 sign_union02_z1rnd0_verify_union01_inplace01_view03_stream01_keygen_union01_compact02 sign_union02_z1rnd0_verify_union01_inplace01_view03_stream01_keygen_union01_compact02_hyper_outsign01 sign_union02_z1rnd0_verify_union01_inplace01_view03_stream01_keygen_union01_compact02_hyper_outsign01_gauss_outbuf03),)
CPPFLAGS += -DHAETAE_STACK_VERIFY_INPLACE01=1
endif
ifneq ($(filter $(ACTIVE_STACK_CONFIG),sign_union02_z1rnd0_verify_union01_inplace01_view03 sign_union02_z1rnd0_verify_union01_inplace01_view03_stream01 sign_union02_z1rnd0_verify_union01_inplace01_view03_stream01_keygen_union01 sign_union02_z1rnd0_verify_union01_inplace01_view03_stream01_keygen_union01_compact02 sign_union02_z1rnd0_verify_union01_inplace01_view03_stream01_keygen_union01_compact02_hyper_outsign01 sign_union02_z1rnd0_verify_union01_inplace01_view03_stream01_keygen_union01_compact02_hyper_outsign01_gauss_outbuf03),)
CPPFLAGS += -DHAETAE_STACK_VERIFY_VIEW03=1
endif
ifneq ($(filter $(ACTIVE_STACK_CONFIG),sign_union02_z1rnd0_verify_union01_inplace01_view03_stream01 sign_union02_z1rnd0_verify_union01_inplace01_view03_stream01_keygen_union01 sign_union02_z1rnd0_verify_union01_inplace01_view03_stream01_keygen_union01_compact02 sign_union02_z1rnd0_verify_union01_inplace01_view03_stream01_keygen_union01_compact02_hyper_outsign01 sign_union02_z1rnd0_verify_union01_inplace01_view03_stream01_keygen_union01_compact02_hyper_outsign01_gauss_outbuf03),)
CPPFLAGS += -DHAETAE_STACK_VERIFY_STREAM01=1
endif
ifneq ($(filter $(ACTIVE_STACK_CONFIG),sign_union02_z1rnd0_verify_union01_inplace01_view03_stream01_keygen_union01 sign_union02_z1rnd0_verify_union01_inplace01_view03_stream01_keygen_union01_compact02 sign_union02_z1rnd0_verify_union01_inplace01_view03_stream01_keygen_union01_compact02_hyper_outsign01 sign_union02_z1rnd0_verify_union01_inplace01_view03_stream01_keygen_union01_compact02_hyper_outsign01_gauss_outbuf03),)
CPPFLAGS += -DHAETAE_STACK_KEYGEN_UNION01=1
endif
ifneq ($(filter $(ACTIVE_STACK_CONFIG),sign_union02_z1rnd0_verify_union01_inplace01_view03_stream01_keygen_union01_compact02 sign_union02_z1rnd0_verify_union01_inplace01_view03_stream01_keygen_union01_compact02_hyper_outsign01 sign_union02_z1rnd0_verify_union01_inplace01_view03_stream01_keygen_union01_compact02_hyper_outsign01_gauss_outbuf03),)
CPPFLAGS += -DHAETAE_STACK_KEYGEN_COMPACT02=1
endif
ifneq ($(filter $(ACTIVE_STACK_CONFIG),sign_union02_z1rnd0_verify_union01_inplace01_view03_stream01_keygen_union01_compact02_hyper_outsign01 sign_union02_z1rnd0_verify_union01_inplace01_view03_stream01_keygen_union01_compact02_hyper_outsign01_gauss_outbuf03),)
CPPFLAGS += -DHAETAE_STACK_HYPER_OUTSIGN01=1
endif
ifeq ($(ACTIVE_STACK_CONFIG),sign_union02_z1rnd0_verify_union01_inplace01_view03_stream01_keygen_union01_compact02_hyper_outsign01_gauss_outbuf03)
CPPFLAGS += -DHAETAE_STACK_GAUSS_OUTBUF03=1
endif
ifneq ($(filter $(ACTIVE_STACK_CONFIG),sign_union02_l sign_union02_l_verify_inplace01 sign_union02_l_verify_inplace01_union02 sign_union02_l_verify_inplace01_union02_view03 sign_union02_l_verify_inplace01_union02_view03_stream01 sign_union02_l_verify_inplace01_union02_view03_stream01_keygen_union01 sign_union02_l_verify_inplace01_union02_view03_stream01_keygen_union01_compact02 sign_union02_l_verify_inplace01_union02_view03_stream01_keygen_union01_compact02_hyper_outsign01 sign_union02_l_verify_inplace01_union02_view03_stream01_keygen_union01_compact02_hyper_outsign01_gauss_outbuf02),)
CPPFLAGS += -DHAETAE_STACK_SIGN_UNION02_L=1
endif
ifneq ($(filter $(ACTIVE_STACK_CONFIG),sign_union02_l_verify_inplace01 sign_union02_l_verify_inplace01_union02 sign_union02_l_verify_inplace01_union02_view03 sign_union02_l_verify_inplace01_union02_view03_stream01 sign_union02_l_verify_inplace01_union02_view03_stream01_keygen_union01 sign_union02_l_verify_inplace01_union02_view03_stream01_keygen_union01_compact02 sign_union02_l_verify_inplace01_union02_view03_stream01_keygen_union01_compact02_hyper_outsign01 sign_union02_l_verify_inplace01_union02_view03_stream01_keygen_union01_compact02_hyper_outsign01_gauss_outbuf02),)
CPPFLAGS += -DHAETAE_STACK_VERIFY_INPLACE01=1
endif
ifneq ($(filter $(ACTIVE_STACK_CONFIG),sign_union02_l_verify_inplace01_union02 sign_union02_l_verify_inplace01_union02_view03 sign_union02_l_verify_inplace01_union02_view03_stream01 sign_union02_l_verify_inplace01_union02_view03_stream01_keygen_union01 sign_union02_l_verify_inplace01_union02_view03_stream01_keygen_union01_compact02 sign_union02_l_verify_inplace01_union02_view03_stream01_keygen_union01_compact02_hyper_outsign01 sign_union02_l_verify_inplace01_union02_view03_stream01_keygen_union01_compact02_hyper_outsign01_gauss_outbuf02),)
CPPFLAGS += -DHAETAE_STACK_VERIFY_UNION02=1
endif
ifneq ($(filter $(ACTIVE_STACK_CONFIG),sign_union02_l_verify_inplace01_union02_view03 sign_union02_l_verify_inplace01_union02_view03_stream01 sign_union02_l_verify_inplace01_union02_view03_stream01_keygen_union01 sign_union02_l_verify_inplace01_union02_view03_stream01_keygen_union01_compact02 sign_union02_l_verify_inplace01_union02_view03_stream01_keygen_union01_compact02_hyper_outsign01 sign_union02_l_verify_inplace01_union02_view03_stream01_keygen_union01_compact02_hyper_outsign01_gauss_outbuf02),)
CPPFLAGS += -DHAETAE_STACK_VERIFY_VIEW03=1
endif
ifneq ($(filter $(ACTIVE_STACK_CONFIG),sign_union02_l_verify_inplace01_union02_view03_stream01 sign_union02_l_verify_inplace01_union02_view03_stream01_keygen_union01 sign_union02_l_verify_inplace01_union02_view03_stream01_keygen_union01_compact02 sign_union02_l_verify_inplace01_union02_view03_stream01_keygen_union01_compact02_hyper_outsign01 sign_union02_l_verify_inplace01_union02_view03_stream01_keygen_union01_compact02_hyper_outsign01_gauss_outbuf02),)
CPPFLAGS += -DHAETAE_STACK_VERIFY_STREAM01=1
endif
ifneq ($(filter $(ACTIVE_STACK_CONFIG),sign_union02_l_verify_inplace01_union02_view03_stream01_keygen_union01 sign_union02_l_verify_inplace01_union02_view03_stream01_keygen_union01_compact02 sign_union02_l_verify_inplace01_union02_view03_stream01_keygen_union01_compact02_hyper_outsign01 sign_union02_l_verify_inplace01_union02_view03_stream01_keygen_union01_compact02_hyper_outsign01_gauss_outbuf02),)
CPPFLAGS += -DHAETAE_STACK_KEYGEN_UNION01=1
endif
ifneq ($(filter $(ACTIVE_STACK_CONFIG),sign_union02_l_verify_inplace01_union02_view03_stream01_keygen_union01_compact02 sign_union02_l_verify_inplace01_union02_view03_stream01_keygen_union01_compact02_hyper_outsign01 sign_union02_l_verify_inplace01_union02_view03_stream01_keygen_union01_compact02_hyper_outsign01_gauss_outbuf02),)
CPPFLAGS += -DHAETAE_STACK_KEYGEN_COMPACT02=1
endif
ifneq ($(filter $(ACTIVE_STACK_CONFIG),sign_union02_l_verify_inplace01_union02_view03_stream01_keygen_union01_compact02_hyper_outsign01 sign_union02_l_verify_inplace01_union02_view03_stream01_keygen_union01_compact02_hyper_outsign01_gauss_outbuf02),)
CPPFLAGS += -DHAETAE_STACK_HYPER_OUTSIGN01=1
endif
ifeq ($(ACTIVE_STACK_CONFIG),sign_union02_l_verify_inplace01_union02_view03_stream01_keygen_union01_compact02_hyper_outsign01_gauss_outbuf02)
CPPFLAGS += -DHAETAE_STACK_GAUSS_OUTBUF02=1
endif
ifneq ($(filter $(ACTIVE_STACK_CONFIG),sign_union03 sign_union03_verify_union01 sign_union03_verify_union01_inplace01 sign_union03_verify_union01_inplace01_view03 sign_union03_verify_union01_inplace01_view03_keygen_union01 sign_union03_verify_union01_inplace01_view03_keygen_union01_stream01 sign_union03_verify_union01_inplace01_view03_keygen_union01_stream01_hyper_outsign01 sign_union03_verify_union01_inplace01_view03_keygen_union01_stream01_hyper_outsign01_gauss_outbuf02 sign_union03_verify_union01_inplace01_view03_keygen_union01_stream01_hyper_outsign01_gauss_outbuf02_hyper_rawscratch03),)
CPPFLAGS += -DHAETAE_STACK_SIGN_UNION03=1
endif
ifneq ($(filter $(ACTIVE_STACK_CONFIG),sign_union03_verify_union01 sign_union03_verify_union01_inplace01 sign_union03_verify_union01_inplace01_view03 sign_union03_verify_union01_inplace01_view03_keygen_union01 sign_union03_verify_union01_inplace01_view03_keygen_union01_stream01 sign_union03_verify_union01_inplace01_view03_keygen_union01_stream01_hyper_outsign01 sign_union03_verify_union01_inplace01_view03_keygen_union01_stream01_hyper_outsign01_gauss_outbuf02 sign_union03_verify_union01_inplace01_view03_keygen_union01_stream01_hyper_outsign01_gauss_outbuf02_hyper_rawscratch03),)
CPPFLAGS += -DHAETAE_STACK_VERIFY_UNION01=1
endif
ifneq ($(filter $(ACTIVE_STACK_CONFIG),sign_union03_verify_union01_inplace01 sign_union03_verify_union01_inplace01_view03 sign_union03_verify_union01_inplace01_view03_keygen_union01 sign_union03_verify_union01_inplace01_view03_keygen_union01_stream01 sign_union03_verify_union01_inplace01_view03_keygen_union01_stream01_hyper_outsign01 sign_union03_verify_union01_inplace01_view03_keygen_union01_stream01_hyper_outsign01_gauss_outbuf02 sign_union03_verify_union01_inplace01_view03_keygen_union01_stream01_hyper_outsign01_gauss_outbuf02_hyper_rawscratch03),)
CPPFLAGS += -DHAETAE_STACK_VERIFY_INPLACE01=1
endif
ifneq ($(filter $(ACTIVE_STACK_CONFIG),sign_union03_verify_union01_inplace01_view03 sign_union03_verify_union01_inplace01_view03_keygen_union01 sign_union03_verify_union01_inplace01_view03_keygen_union01_stream01 sign_union03_verify_union01_inplace01_view03_keygen_union01_stream01_hyper_outsign01 sign_union03_verify_union01_inplace01_view03_keygen_union01_stream01_hyper_outsign01_gauss_outbuf02 sign_union03_verify_union01_inplace01_view03_keygen_union01_stream01_hyper_outsign01_gauss_outbuf02_hyper_rawscratch03),)
CPPFLAGS += -DHAETAE_STACK_VERIFY_VIEW03=1
endif
ifneq ($(filter $(ACTIVE_STACK_CONFIG),sign_union03_verify_union01_inplace01_view03_keygen_union01 sign_union03_verify_union01_inplace01_view03_keygen_union01_stream01 sign_union03_verify_union01_inplace01_view03_keygen_union01_stream01_hyper_outsign01 sign_union03_verify_union01_inplace01_view03_keygen_union01_stream01_hyper_outsign01_gauss_outbuf02 sign_union03_verify_union01_inplace01_view03_keygen_union01_stream01_hyper_outsign01_gauss_outbuf02_hyper_rawscratch03),)
CPPFLAGS += -DHAETAE_STACK_KEYGEN_UNION01=1
endif
ifneq ($(filter $(ACTIVE_STACK_CONFIG),sign_union03_verify_union01_inplace01_view03_keygen_union01_stream01 sign_union03_verify_union01_inplace01_view03_keygen_union01_stream01_hyper_outsign01 sign_union03_verify_union01_inplace01_view03_keygen_union01_stream01_hyper_outsign01_gauss_outbuf02 sign_union03_verify_union01_inplace01_view03_keygen_union01_stream01_hyper_outsign01_gauss_outbuf02_hyper_rawscratch03),)
CPPFLAGS += -DHAETAE_STACK_KEYGEN_STREAM01=1
endif
ifneq ($(filter $(ACTIVE_STACK_CONFIG),sign_union03_verify_union01_inplace01_view03_keygen_union01_stream01_hyper_outsign01 sign_union03_verify_union01_inplace01_view03_keygen_union01_stream01_hyper_outsign01_gauss_outbuf02 sign_union03_verify_union01_inplace01_view03_keygen_union01_stream01_hyper_outsign01_gauss_outbuf02_hyper_rawscratch03),)
CPPFLAGS += -DHAETAE_STACK_HYPER_OUTSIGN01=1
endif
ifneq ($(filter $(ACTIVE_STACK_CONFIG),sign_union03_verify_union01_inplace01_view03_keygen_union01_stream01_hyper_outsign01_gauss_outbuf02 sign_union03_verify_union01_inplace01_view03_keygen_union01_stream01_hyper_outsign01_gauss_outbuf02_hyper_rawscratch03),)
CPPFLAGS += -DHAETAE_STACK_GAUSS_OUTBUF02=1
endif
ifeq ($(ACTIVE_STACK_CONFIG),sign_union03_verify_union01_inplace01_view03_keygen_union01_stream01_hyper_outsign01_gauss_outbuf02_hyper_rawscratch03)
CPPFLAGS += -DHAETAE_STACK_HYPER_RAWSCRATCH03=1
endif
ifneq ($(filter $(ACTIVE_NTT_VARIANT),m32_l44 m32_l332 m32_l332_vld2 m32_l332_vld4 m32_l332_vld4_m3 m32_l332_vld4_u2),)
ifeq ($(HAETAE_NTT_BACKEND),intrinsic)
COMMON_SOURCES += optimization/haetae/common/haetae_ntt_mve.c
endif
CPPFLAGS += -DHAETAE_MVE_ACC=1
endif
ifneq ($(filter $(ACTIVE_NTT_VARIANT),m32_l44 m32_l332 m32_l332_vld2 m32_l332_vld4 m32_l332_vld4_m3 m32_l332_vld4_u2),)
CPPFLAGS += -DHAETAE_MVE_POLY=1
endif
ifneq ($(HAETAE_NTT_BACKEND),intrinsic)
CPPFLAGS += -DHAETAE_NTT_ASM=1
ifeq ($(HAETAE_NTT_BACKEND),asm_slothy)
CPPFLAGS += -DHAETAE_NTT_SLOTHY_FORWARD=1
endif
ifeq ($(HAETAE_NTT_ARITH_VARIANT),d2)
CPPFLAGS += -DHAETAE_NTT_ARITH_D2=1
endif
ifneq ($(filter $(HAETAE_NTT_INVERSE_VARIANT),preslothy slothy_functional slothy_u1),)
CPPFLAGS += -DHAETAE_NTT_SLOTHY_INVERSE=1
endif
ifneq ($(filter $(HAETAE_NTT_POINTWISE_VARIANT),preslothy_lowmem slothy_u1 slothy_lowmem),)
CPPFLAGS += -DHAETAE_NTT_SLOTHY_POINTWISE=1
endif
ifneq ($(filter $(HAETAE_NTT_POINTWISE_ACC_VARIANT),preslothy_block2 slothy_lowmem slothy_lowmem_r11 slothy_block2),)
CPPFLAGS += -DHAETAE_NTT_SLOTHY_POINTWISE_ACC=1
endif
ifneq ($(HAETAE_NTT_BARRETT_VARIANT),hand)
CPPFLAGS += -DHAETAE_NTT_EXTERNAL_BARRETT=1
endif
ifneq ($(HAETAE_NTT_CENTERED_VARIANT),hand)
CPPFLAGS += -DHAETAE_NTT_EXTERNAL_CENTERED=1
endif
ifneq ($(filter $(HAETAE_NTT_ASM_VARIANT),paired_vld4_u2 paired_vld4_u2_l34_hier paired_vld4_u2_pad_l12_layout paired_vld4_u2_l34_hier_pad_l12_layout paired_vld4_u4_l12_l34_hier paired_vld4_u4_l56_l34_hier hand_l332_vld4_u2),)
CPPFLAGS += -DHAETAE_NTT_PAIRED_UNROLL=2
endif
ifneq ($(filter $(HAETAE_NTT_ASM_VARIANT),paired_vld4_u4 paired_vld4_u4_hier),)
CPPFLAGS += -DHAETAE_NTT_PAIRED_UNROLL=4
endif
endif
ifeq ($(ACTIVE_NTT_VARIANT),m32_l44)
CPPFLAGS += -DHAETAE_NTT_M32_L44=1
endif
ifeq ($(ACTIVE_NTT_VARIANT),m32_l332)
CPPFLAGS += -DHAETAE_NTT_M32_L332=1
endif
ifeq ($(ACTIVE_NTT_VARIANT),m32_l332_vld2)
CPPFLAGS += -DHAETAE_NTT_M32_L332_VLD2=1
endif
ifeq ($(ACTIVE_NTT_VARIANT),m32_l332_vld4)
CPPFLAGS += -DHAETAE_NTT_M32_L332_VLD4=1
endif
ifeq ($(ACTIVE_NTT_VARIANT),m32_l332_vld4_m3)
CPPFLAGS += -DHAETAE_NTT_M32_L332_VLD4_M3=1
endif
ifeq ($(ACTIVE_NTT_VARIANT),m32_l332_vld4_u2)
CPPFLAGS += -DHAETAE_NTT_M32_L332_VLD4_U2=1
endif
ifeq ($(ACTIVE_ACC_VARIANT),perterm_d1_centered)
CPPFLAGS += -DHAETAE_ACC_PERTERM_D1_CENTERED=1
endif
ifeq ($(ACTIVE_ACC_VARIANT),aftersum_d2_centered)
CPPFLAGS += -DHAETAE_ACC_AFTERSUM_D2_CENTERED=1
endif
ifeq ($(ACTIVE_ACC_VARIANT),aftersum_dmax_tight)
CPPFLAGS += -DHAETAE_ACC_AFTERSUM_DMAX_TIGHT=1
endif
ifneq ($(filter $(ACTIVE_ACC_VARIANT),aftersum_dmax_tight_precenter aftersum_dmax_tight_precenter_poly),)
CPPFLAGS += -DHAETAE_ACC_AFTERSUM_DMAX_TIGHT_PRECENTER=1
endif
ifeq ($(ACTIVE_ACC_VARIANT),aftersum_dmax_tight_precenter_poly)
CPPFLAGS += -DHAETAE_POINTWISE_BARRETT_TIGHT=1
endif
ifneq ($(ACTIVE_FFT_VARIANT),scalar)
ifeq ($(HAETAE_FFT_BACKEND),intrinsic)
COMMON_SOURCES += optimization/haetae/common/haetae_fft_mve.c
endif
CPPFLAGS += -DHAETAE_MVE_FFT=1
endif
ifeq ($(ACTIVE_FFT_VARIANT),wide_i_l2222_gpr_sep)
CPPFLAGS += -DHAETAE_FFT_WIDE=1
endif
ifneq ($(filter $(ACTIVE_FFT_VARIANT),qrd_i_l2222_gpr_sep qrd_soa_l2222_gpr_sep qrd_i_l332_gpr_sep qrd_i_l44_gpr_sep qrd_i_l2222_vtbl_sep qrd_soa_l332_gpr_sep qrd_soa_l44_gpr_sep qrd_soa_l2222_vtbl_sep qrd_soa_l332_vtbl_sep qrd_soa_l44_vtbl_sep qrd_soa_l2222_vtbl_fused qrd_soa_l2222_vtbl_sep_u4 qrd_soa_l2222_vtbl_fused_u4),)
CPPFLAGS += -DHAETAE_FFT_QRD=1
endif
ifneq ($(filter $(ACTIVE_FFT_VARIANT),qrd_soa_l2222_gpr_sep qrd_soa_l332_gpr_sep qrd_soa_l44_gpr_sep qrd_soa_l2222_vtbl_sep qrd_soa_l332_vtbl_sep qrd_soa_l44_vtbl_sep qrd_soa_l2222_vtbl_fused qrd_soa_l2222_vtbl_sep_u4 qrd_soa_l2222_vtbl_fused_u4),)
CPPFLAGS += -DHAETAE_FFT_SOA=1
endif
ifneq ($(filter $(ACTIVE_FFT_VARIANT),qrd_i_l332_gpr_sep qrd_soa_l332_gpr_sep qrd_soa_l332_vtbl_sep),)
CPPFLAGS += -DHAETAE_FFT_L332=1
endif
ifneq ($(filter $(ACTIVE_FFT_VARIANT),qrd_i_l44_gpr_sep qrd_soa_l44_gpr_sep qrd_soa_l44_vtbl_sep),)
CPPFLAGS += -DHAETAE_FFT_L44=1
endif
ifneq ($(filter $(ACTIVE_FFT_VARIANT),qrd_i_l2222_vtbl_sep qrd_soa_l2222_vtbl_sep qrd_soa_l332_vtbl_sep qrd_soa_l44_vtbl_sep qrd_soa_l2222_vtbl_fused qrd_soa_l2222_vtbl_sep_u4 qrd_soa_l2222_vtbl_fused_u4),)
CPPFLAGS += -DHAETAE_FFT_VTBL=1
endif
ifneq ($(filter $(ACTIVE_FFT_VARIANT),qrd_soa_l2222_vtbl_fused qrd_soa_l2222_vtbl_fused_u4),)
CPPFLAGS += -DHAETAE_FFT_FUSED=1
endif
ifneq ($(filter $(ACTIVE_FFT_VARIANT),qrd_soa_l2222_vtbl_sep_u4 qrd_soa_l2222_vtbl_fused_u4),)
CPPFLAGS += -DHAETAE_FFT_UNIT4=1
endif
ifeq ($(ACTIVE_SAMPLER_VARIANT),cdt_mve)
ifeq ($(HAETAE_SAMPLER_BACKEND),intrinsic)
COMMON_SOURCES += optimization/haetae/common/haetae_sampler_mve.c
endif
CPPFLAGS += -DHAETAE_SAMPLER_CDT_MVE=1
endif
ifeq ($(ACTIVE_SAMPLER_VARIANT),cdt_mve_load)
ifeq ($(HAETAE_SAMPLER_BACKEND),intrinsic)
COMMON_SOURCES += optimization/haetae/common/haetae_sampler_mve.c
endif
CPPFLAGS += -DHAETAE_SAMPLER_CDT_MVE=1 -DHAETAE_SAMPLER_LOAD_MVE=1
endif
ifneq ($(HAETAE_SAMPLER_BACKEND),intrinsic)
ifneq ($(filter $(ACTIVE_SAMPLER_VARIANT),cdt_mve cdt_mve_load),)
CPPFLAGS += -DHAETAE_SAMPLER_ASM=1
endif
endif
ifneq ($(filter $(ACTIVE_MUL_VARIANT),toom4_k2 toom4_sb64 toom4_k1 toom4_outer64 toom4_outer32 toom4_outer32_leafcall direct256_outer),)
COMMON_SOURCES += optimization/smaug/common/smaugt_toom_mve.c
endif
ifeq ($(ACTIVE_MUL_VARIANT),toom4_k2)
CPPFLAGS += -DSMAUGT_TOOM_B1=1
endif
ifeq ($(ACTIVE_MUL_VARIANT),toom4_sb64)
CPPFLAGS += -DSMAUGT_TOOM_B2=1
endif
ifeq ($(ACTIVE_MUL_VARIANT),toom4_k1)
CPPFLAGS += -DSMAUGT_TOOM_B3=1
endif
ifeq ($(ACTIVE_MUL_VARIANT),toom4_outer64)
CPPFLAGS += -DSMAUGT_TOOM_B6=1
endif
ifeq ($(ACTIVE_MUL_VARIANT),toom4_outer32)
CPPFLAGS += -DSMAUGT_TOOM_OUTER32=1
endif
ifeq ($(ACTIVE_MUL_VARIANT),toom4_outer32_leafcall)
CPPFLAGS += -DSMAUGT_TOOM_OUTER32_LEAFCALL=1
endif
ifeq ($(ACTIVE_MUL_VARIANT),direct256_outer)
CPPFLAGS += -DSMAUGT_DIRECT256_OUTER=1
endif
ifeq ($(ACTIVE_MUL_CACHE),1)
CPPFLAGS += -DSMAUGT_TOOM_CACHE=1
endif
ifeq ($(ACTIVE_SHIFT_VARIANT),mask)
CPPFLAGS += -DSMAUGT_MASKED_MUL=1
endif
PLATFORM_SOURCES := platform/mps3-an547/minimal_libc.c bench/bench_common.c

ifeq ($(PLATFORM),mps3-an547)
PLATFORM_SOURCES += platform/mps3-an547/platform.c \
  platform/mps3-an547/randombytes.c
PLATFORM_STARTUP := platform/mps3-an547/startup.S
LINKER_SCRIPT := platform/mps3-an547/linker.ld
BENCH_WARMUP_ITERATIONS := 0
else
PLATFORM_SOURCES += platform/stm32n657/platform.c \
  platform/stm32n657/randombytes.c
PLATFORM_STARTUP := platform/stm32n657/startup.S
LINKER_SCRIPT := platform/stm32n657/linker.ld
PLATFORM_LDFLAGS := -Wl,--nmagic
BENCH_WARMUP_ITERATIONS := 1
ifeq ($(STM32_CLOCK),600mhz)
CPPFLAGS += -DSTM32N657_CLOCK_600MHZ=1
endif
endif

ifeq ($(IMPLEMENTATION),optimization)
PLATFORM_SOURCES += bench/bench_keccak.c
CPPFLAGS += -DBENCH_M55_KECCAK_INTERNAL=1
endif

PROFILE_COMMON_SYMBOLS := shake128 shake256 shake128_inc_init \
  shake128_inc_absorb shake128_inc_finalize shake128_inc_squeeze \
  shake256_inc_init shake256_inc_absorb shake256_inc_finalize \
  shake256_inc_squeeze shake256_inc_ctx_release sha3_256 sha3_512
comma := ,
WRAP_PREFIX := -Wl$(comma)--wrap=

ifeq ($(PROFILE),1)
PLATFORM_SOURCES += bench/profile.c bench/profile_wrappers.c
CPPFLAGS += -DBENCH_PROFILE=1 -DBENCH_FAMILY_$(BENCH_FAMILY)=1
LDFLAGS += $(addprefix $(WRAP_PREFIX),$(PROFILE_COMMON_SYMBOLS) $(PROFILE_SCHEME_SYMBOLS))
endif

ifeq ($(REJECTION_COUNT),1)
ifneq ($(BENCH_FAMILY),haetae)
$(error REJECTION_COUNT=1 is available only for HAETAE targets)
endif
ifeq ($(PROFILE),1)
$(error REJECTION_COUNT=1 and PROFILE=1 both wrap the HAETAE sampler; run them separately)
endif
PLATFORM_SOURCES += bench/rejection_counter.c
CPPFLAGS += -DBENCH_REJECTION_COUNT=1
LDFLAGS += $(WRAP_PREFIX)$(REJECTION_SYMBOL)
endif

ifeq ($(DIFF_TEST),1)
ifneq ($(IMPLEMENTATION),optimization)
$(error DIFF_TEST=1 directly links the reference implementation and requires IMPLEMENTATION=optimization)
endif
CPPFLAGS += -DBENCH_DIFF_TEST=1
PLATFORM_SOURCES += bench/diff_fips202.c
REFERENCE_ALG_DIR := reference/$(ALG_SUBDIR)
DIFF_REF_DIR := $(BUILD_DIR)/reference-objects
ifeq ($(BENCH_FAMILY),haetae)
PLATFORM_SOURCES += bench/diff_haetae.c
else ifeq ($(BENCH_FAMILY),smaugt)
PLATFORM_SOURCES += bench/diff_smaugt.c
endif
REFERENCE_ALG_SOURCES := $(filter-out $(REFERENCE_ALG_DIR)/kat_sign.c,$(wildcard $(REFERENCE_ALG_DIR)/*.c))
DIFF_REF_NAMES := $(notdir $(REFERENCE_ALG_SOURCES:.c=)) fips202
DIFF_REF_RAW_OBJECTS := $(addprefix $(DIFF_REF_DIR)/raw/,$(addsuffix .o,$(DIFF_REF_NAMES)))
DIFF_REF_OBJECTS := $(addprefix $(DIFF_REF_DIR)/prefixed/,$(addsuffix .o,$(DIFF_REF_NAMES)))
DIFF_SYMBOL_MAP := $(DIFF_REF_DIR)/symbols.map
endif
ifeq ($(CT_TEST),1)
ifneq ($(PROFILE),0)
$(error CT_TEST=1 cannot be combined with PROFILE=1)
endif
ifneq ($(REJECTION_COUNT),0)
$(error CT_TEST=1 cannot be combined with REJECTION_COUNT=1)
endif
PLATFORM_SOURCES += bench/bench_ct.c
CPPFLAGS += -DBENCH_CT_TEST=1 -DBENCH_CT_PAIRS=$(CT_PAIRS) \
            -DBENCH_CT_BACKEND=\"$(CT_BACKEND)\" \
            -DBENCH_CT_$(shell echo $(CT_OPERATION) | tr a-z A-Z)=1
endif
C_SOURCES := $(ALG_SOURCES) $(COMMON_SOURCES) $(PLATFORM_SOURCES) $(BENCH_SRC)
ASM_SOURCES := $(PLATFORM_STARTUP)
ifeq ($(IMPLEMENTATION),optimization)
ifeq ($(KECCAK_X4_BACKEND),asm_hand)
ASM_SOURCES += $(COMMON_DIR)/keccak_m55/asm_x4/hand/keccakx4_hand.S
ASM_SOURCES += $(COMMON_DIR)/keccak_m55/asm_x4/hand/fips202x4_hand.S
else ifeq ($(KECCAK_X4_BACKEND),asm_slothy)
ifeq ($(KECCAK_X4_ASM_VARIANT),slothy_gather_u2_chi_u1)
ASM_SOURCES += $(COMMON_DIR)/keccak_m55/asm_x4/slothy/output/keccakx4_chi_u1.S
else ifeq ($(KECCAK_X4_ASM_VARIANT),slothy_gather_u2_chi_u2)
ASM_SOURCES += $(COMMON_DIR)/keccak_m55/asm_x4/slothy/output/keccakx4_chi_u2.S
else ifeq ($(KECCAK_X4_ASM_VARIANT),slothy_gather_u2_fullperm_u1)
ASM_SOURCES += $(COMMON_DIR)/keccak_m55/asm_x4/slothy/output/keccakx4_full_slothy_u1.S
else ifeq ($(KECCAK_X4_ASM_VARIANT),slothy_gather_u2_fullperm_u2)
ASM_SOURCES += $(COMMON_DIR)/keccak_m55/asm_x4/slothy/output/keccakx4_full_slothy_u2.S
else ifeq ($(KECCAK_X4_ASM_VARIANT),slothy_gather_u2_fullperm_handchi)
ASM_SOURCES += $(COMMON_DIR)/keccak_m55/asm_x4/slothy/output/keccakx4_full_slothy_handchi.S
else ifeq ($(KECCAK_X4_ASM_VARIANT),slothy_gather_u2_fullperm_chi2straight)
ASM_SOURCES += $(COMMON_DIR)/keccak_m55/asm_x4/slothy/output/keccakx4_full_chi_unrolled2.S
else ifeq ($(KECCAK_X4_ASM_VARIANT),slothy_gather_u2_fullperm_chi5straight)
ASM_SOURCES += $(COMMON_DIR)/keccak_m55/asm_x4/slothy/output/keccakx4_full_chi_unrolled5_split.S
else ifneq ($(filter $(KECCAK_X4_ASM_VARIANT),slothy_gather_u2_fullperm_chi5_allpaths slothy_gather_u2_fullperm_chi5_allpaths_input),)
ASM_SOURCES += $(COMMON_DIR)/keccak_m55/asm_x4/slothy/output/keccakx4_full_chi5_aux_slothy.S
else ifeq ($(KECCAK_X4_ASM_VARIANT),preslothy_fullperm_chi5_allpaths)
ASM_SOURCES += $(COMMON_DIR)/keccak_m55/asm_x4/slothy/input/keccakx4_full_chi5_aux.s
else
ASM_SOURCES += $(COMMON_DIR)/keccak_m55/asm_x4/hand/keccakx4_hand.S
endif
ifeq ($(KECCAK_X4_ASM_VARIANT),slothy_gather_u2_fullperm_chi5_allpaths)
ASM_SOURCES += $(COMMON_DIR)/keccak_m55/asm_x4/slothy/output/fips202x4_slothy_allpaths.S
else ifeq ($(KECCAK_X4_ASM_VARIANT),slothy_gather_u2_fullperm_chi5_allpaths_input)
KECCAK_X4_ALLPATHS_PROBE_WRAPPER ?= $(COMMON_DIR)/keccak_m55/asm_x4/slothy/input/fips202x4_allpaths.s
ASM_SOURCES += $(KECCAK_X4_ALLPATHS_PROBE_WRAPPER)
else ifeq ($(KECCAK_X4_ASM_VARIANT),preslothy_fullperm_chi5_allpaths)
ASM_SOURCES += $(COMMON_DIR)/keccak_m55/asm_x4/slothy/input/fips202x4_allpaths.s
else
ASM_SOURCES += $(COMMON_DIR)/keccak_m55/asm_x4/slothy/output/fips202x4_slothy_gather_u2.S
endif
else ifeq ($(KECCAK_X4_BACKEND),hybrid_slothy)
ASM_SOURCES += $(COMMON_DIR)/keccak_m55/asm_x4/slothy/output/fips202x4_slothy_gather_u2.S
endif
endif
ifeq ($(IMPLEMENTATION),optimization)
ifeq ($(BENCH_FAMILY),haetae)
ifeq ($(HAETAE_NTT_BACKEND),asm_hand)
ASM_SOURCES += optimization/haetae/asm_m55/tables/ntt_roots.S
ifeq ($(HAETAE_NTT_ASM_VARIANT),generic_vld42)
ASM_SOURCES += optimization/haetae/asm_m55/ntt/hand/ntt_generic.S
else
ASM_SOURCES += optimization/haetae/asm_m55/ntt/hand/ntt_paired.S
endif
ASM_SOURCES += optimization/haetae/asm_m55/ntt/hand/arithmetic.S
endif
ifeq ($(HAETAE_NTT_BACKEND),asm_slothy)
ASM_SOURCES += optimization/haetae/asm_m55/tables/ntt_roots.S
ifeq ($(HAETAE_NTT_ASM_VARIANT),preslothy)
ASM_SOURCES += optimization/haetae/asm_m55/ntt/slothy/input/ntt_paired_forward.s
else
ASM_SOURCES += optimization/haetae/asm_m55/ntt/slothy/output/ntt_$(HAETAE_NTT_ASM_VARIANT).S
endif
ASM_SOURCES += optimization/haetae/asm_m55/ntt/hand/ntt_paired.S
ASM_SOURCES += optimization/haetae/asm_m55/ntt/hand/arithmetic.S
endif
ifneq ($(HAETAE_NTT_BACKEND),intrinsic)
ifeq ($(HAETAE_NTT_INVERSE_VARIANT),preslothy)
ASM_SOURCES += optimization/haetae/asm_m55/ntt/slothy/input/ntt_inverse_full.s
else ifeq ($(HAETAE_NTT_INVERSE_VARIANT),slothy_functional)
ASM_SOURCES += optimization/haetae/asm_m55/ntt/slothy/output/ntt_inverse_full_functional_retry1.S
else ifeq ($(HAETAE_NTT_INVERSE_VARIANT),slothy_u1)
ASM_SOURCES += optimization/haetae/asm_m55/ntt/slothy/output/ntt_inverse_full_u1_preserve_outer.S
endif
ifeq ($(HAETAE_NTT_POINTWISE_VARIANT),preslothy_lowmem)
ASM_SOURCES += optimization/haetae/asm_m55/ntt/slothy/input/arithmetic_pointwise_full_lowmem.s
else ifeq ($(HAETAE_NTT_POINTWISE_VARIANT),slothy_u1)
ASM_SOURCES += optimization/haetae/asm_m55/ntt/slothy/output/arithmetic_pointwise_full_u1.S
else ifeq ($(HAETAE_NTT_POINTWISE_VARIANT),slothy_lowmem)
ASM_SOURCES += optimization/haetae/asm_m55/ntt/slothy/output/arithmetic_pointwise_full_lowmem_no_swp_v2.S
endif
ifeq ($(HAETAE_NTT_POINTWISE_ACC_VARIANT),preslothy_block2)
ASM_SOURCES += optimization/haetae/asm_m55/ntt/slothy/input/arithmetic_pointwise_acc_full_block2.s
else ifeq ($(HAETAE_NTT_POINTWISE_ACC_VARIANT),slothy_lowmem)
ASM_SOURCES += optimization/haetae/asm_m55/ntt/slothy/output/arithmetic_pointwise_acc_full_lowmem_complete.S
else ifeq ($(HAETAE_NTT_POINTWISE_ACC_VARIANT),slothy_lowmem_r11)
ASM_SOURCES += optimization/haetae/asm_m55/ntt/slothy/output/arithmetic_pointwise_acc_full_lowmem_r11_complete.S
else ifeq ($(HAETAE_NTT_POINTWISE_ACC_VARIANT),slothy_block2)
ASM_SOURCES += optimization/haetae/asm_m55/ntt/slothy/output/arithmetic_pointwise_acc_full_block2_complete.S
endif
ifeq ($(HAETAE_NTT_BARRETT_VARIANT),hand_dls)
ASM_SOURCES += optimization/haetae/asm_m55/ntt/hand/arithmetic_barrett_dls.S
else ifeq ($(HAETAE_NTT_BARRETT_VARIANT),preslothy_nostack)
ASM_SOURCES += optimization/haetae/asm_m55/ntt/slothy/input/arithmetic_barrett_full_nostack.s
else ifeq ($(HAETAE_NTT_BARRETT_VARIANT),slothy_u1)
ASM_SOURCES += optimization/haetae/asm_m55/ntt/slothy/output/arithmetic_barrett_full_u1.S
else ifeq ($(HAETAE_NTT_BARRETT_VARIANT),slothy_u2)
ASM_SOURCES += optimization/haetae/asm_m55/ntt/slothy/output/arithmetic_barrett_full_u2.S
else ifeq ($(HAETAE_NTT_BARRETT_VARIANT),slothy_u2_nostack)
ASM_SOURCES += optimization/haetae/asm_m55/ntt/slothy/output/arithmetic_barrett_full_u2_nostack.S
endif
ifeq ($(HAETAE_NTT_CENTERED_VARIANT),hand_dls)
ASM_SOURCES += optimization/haetae/asm_m55/ntt/hand/arithmetic_centered_predicated_dls.S
else ifeq ($(HAETAE_NTT_CENTERED_VARIANT),hand_dls_u2)
ASM_SOURCES += optimization/haetae/asm_m55/ntt/hand/arithmetic_centered_predicated_dls_u2.S
else ifeq ($(HAETAE_NTT_CENTERED_VARIANT),mask_hand)
ASM_SOURCES += optimization/haetae/asm_m55/ntt/slothy/input/arithmetic_centered_mask_full.s
else ifeq ($(HAETAE_NTT_CENTERED_VARIANT),preslothy_mask)
ASM_SOURCES += optimization/haetae/asm_m55/ntt/slothy/input/arithmetic_centered_mask_full.s
else ifeq ($(HAETAE_NTT_CENTERED_VARIANT),slothy_mask_u1)
ASM_SOURCES += optimization/haetae/asm_m55/ntt/slothy/output/arithmetic_centered_mask_full_u1.S
else ifeq ($(HAETAE_NTT_CENTERED_VARIANT),slothy_mask_u2)
ASM_SOURCES += optimization/haetae/asm_m55/ntt/slothy/output/arithmetic_centered_mask_full_u2.S
endif
endif
ifeq ($(HAETAE_FFT_BACKEND),asm_hand)
ASM_SOURCES += optimization/haetae/asm_m55/tables/fft_twiddles.S
ASM_SOURCES += optimization/haetae/asm_m55/fft/hand/fft_soa_reuse.S
endif
ifeq ($(HAETAE_FFT_BACKEND),asm_slothy)
ASM_SOURCES += optimization/haetae/asm_m55/tables/fft_twiddles.S
ifeq ($(HAETAE_FFT_ASM_VARIANT),preslothy_vecsq_u2)
ASM_SOURCES += optimization/haetae/asm_m55/fft/slothy/input/fft_soa_reuse_vector_sq_u2_tail.s
else ifeq ($(HAETAE_FFT_ASM_VARIANT),functional)
ASM_SOURCES += optimization/haetae/asm_m55/fft/slothy/output/fft_functional.S
else ifeq ($(HAETAE_FFT_ASM_VARIANT),to_soa_u4)
ASM_SOURCES += optimization/haetae/asm_m55/fft/slothy/output/fft_to_soa_u4.s
else ifeq ($(HAETAE_FFT_ASM_VARIANT),to_soa_u1)
ASM_SOURCES += optimization/haetae/asm_m55/fft/slothy/output/fft_to_soa_u1.s
else ifeq ($(HAETAE_FFT_ASM_VARIANT),to_soa_u2)
ASM_SOURCES += optimization/haetae/asm_m55/fft/slothy/output/fft_to_soa_u2.s
else ifeq ($(HAETAE_FFT_ASM_VARIANT),first2_u1)
ASM_SOURCES += optimization/haetae/asm_m55/fft/slothy/output/fft_to_soa_u4_first2_u1.s
else ifeq ($(HAETAE_FFT_ASM_VARIANT),first2_u1_u2)
ASM_SOURCES += optimization/haetae/asm_m55/fft/slothy/output/fft_u2_first2_u1.s
else ifeq ($(HAETAE_FFT_ASM_VARIANT),from_soa_u4)
ASM_SOURCES += optimization/haetae/asm_m55/fft/slothy/output/fft_stage3_from_soa_u4.s
else ifeq ($(HAETAE_FFT_ASM_VARIANT),fused_to_soa_u4)
ASM_SOURCES += optimization/haetae/asm_m55/fft/slothy/output/fft_stage4_fused_to_soa_u4.s
else ifeq ($(HAETAE_FFT_ASM_VARIANT),soa_reuse_vecsq_u2_hier)
ASM_SOURCES += optimization/haetae/asm_m55/fft/slothy/output/fft_soa_reuse_vecsq_u2_tail_hier.S
else
ASM_SOURCES += optimization/haetae/asm_m55/fft/slothy/output/fft_$(HAETAE_FFT_ASM_VARIANT).S
endif
endif
ifeq ($(HAETAE_SAMPLER_BACKEND),asm_hand)
ASM_SOURCES += optimization/haetae/asm_m55/tables/sampler_cdt.S
ASM_SOURCES += optimization/haetae/asm_m55/sampler/hand/sampler_$(HAETAE_SAMPLER_ASM_VARIANT).S
ASM_SOURCES += optimization/haetae/asm_m55/sampler/hand/load17.S
endif
ifeq ($(HAETAE_SAMPLER_BACKEND),asm_slothy)
ASM_SOURCES += optimization/haetae/asm_m55/tables/sampler_cdt.S
ifeq ($(HAETAE_SAMPLER_ASM_VARIANT),preslothy_mask)
ASM_SOURCES += optimization/haetae/asm_m55/sampler/slothy/input/sampler_mask_full.s
else
ASM_SOURCES += optimization/haetae/asm_m55/sampler/slothy/output/sampler_$(HAETAE_SAMPLER_ASM_VARIANT).S
endif
ASM_SOURCES += optimization/haetae/asm_m55/sampler/hand/load17.S
endif
endif
ifeq ($(BENCH_FAMILY),smaugt)
ifeq ($(SMAUGT_TOOM_BACKEND),asm_hand)
ASM_SOURCES += optimization/smaug/asm_m55/toom/hand/toom_outer32_fused.S
else ifeq ($(SMAUGT_TOOM_BACKEND),asm_slothy)
ifeq ($(SMAUGT_TOOM_ASM_VARIANT),preslothy_fused)
ASM_SOURCES += optimization/smaug/asm_m55/toom/slothy/input/toom_outer32_fused.s
else ifeq ($(SMAUGT_TOOM_ASM_VARIANT),slothy_fused_tailpred_stable_u1)
ASM_SOURCES += optimization/smaug/asm_m55/toom/slothy/output/toom_outer32_fused_tailpred_stable_u1.S
else ifeq ($(SMAUGT_TOOM_ASM_VARIANT),slothy_fused_tailpred_u1)
ASM_SOURCES += optimization/smaug/asm_m55/toom/slothy/output/toom_outer32_fused_tailpred_u1.S
else
ASM_SOURCES += optimization/smaug/asm_m55/toom/slothy/output/toom_outer32_fused_u1.S
endif
endif
endif
endif
ifneq ($(filter $(ACTIVE_KECCAK_VARIANT),pqm4 pqm4_persistent),)
CPPFLAGS += -DM55_KECCAK_PQM4=1
# Keep the original in-file order (state I/O, round constants/permutation,
# standard-state adapter) so the linked hot-code layout remains stable.
ASM_SOURCES += optimization/common/keccak_m55/keccakf1600_state_io.S
ASM_SOURCES += optimization/common/keccak_m55/keccakf1600_permute_m55.S
ASM_SOURCES += optimization/common/keccak_m55/keccakf1600_standard_adapter.S
# The monolithic source kept the compatibility adapter in the same .text
# input section as the live raw permutation. Preserve that linked image while
# the functions live in responsibility-specific translation units.
LDFLAGS += -Wl,--undefined=KeccakF1600_StatePermute_adomnicai_m4_standard
endif
ifeq ($(ACTIVE_KECCAK_VARIANT),pqm4_persistent)
CPPFLAGS += -DM55_KECCAK_PERSISTENT=1
endif
ifeq ($(ACTIVE_KECCAK_KERNEL),m55_fpscratch24)
CPPFLAGS += -DM55_KECCAK_FPSCRATCH_WORDS=24
endif
ifeq ($(ACTIVE_KECCAK_KERNEL),m55_fpscratch32)
CPPFLAGS += -DM55_KECCAK_FPSCRATCH_WORDS=32
endif
ifeq ($(ACTIVE_KECCAK_KERNEL),m55_scalar_sched)
CPPFLAGS += -DM55_KECCAK_SCALAR_SCHEDULE=1
endif
ifeq ($(ACTIVE_KECCAK_RC),fpgroup)
CPPFLAGS += -DM55_KECCAK_RC_FPGROUP=1
endif

ASM_OBJECTS := $(patsubst %.s,%.o,$(patsubst %.S,%.o,$(ASM_SOURCES)))
OBJECTS := $(addprefix $(BUILD_DIR)/,$(C_SOURCES:.c=.o)) \
           $(addprefix $(BUILD_DIR)/,$(ASM_OBJECTS))
ifeq ($(DIFF_TEST),1)
OBJECTS += $(DIFF_REF_OBJECTS)
endif
DEPS := $(OBJECTS:.o=.d)

CPU_FLAGS := -mcpu=cortex-m55 -mthumb -mfloat-abi=hard
CPPFLAGS += -Iplatform/mps3-an547/include \
            -I$(COMMON_DIR) \
            -I$(ALG_DIR) \
            -Ioptimization/haetae/common \
            -Ioptimization/smaug/common \
            -Ibench \
            -DBENCH_ITERATIONS=$(ITERATIONS) \
            -DBENCH_WARMUP_ITERATIONS=$(BENCH_WARMUP_ITERATIONS) \
            -DBENCH_SCHEME_NAME=\"$(SCHEME_NAME)\" \
            -DBENCH_IMPLEMENTATION=\"$(IMPLEMENTATION)\" \
            -DBENCH_PLATFORM_NAME=\"$(PLATFORM)\"
CFLAGS += $(CPU_FLAGS) -std=c11 -O3 -ffreestanding \
          -ffunction-sections -fdata-sections -fno-common \
          -fno-delete-null-pointer-checks -fstack-usage \
          -Wall -Wextra -Wshadow -Wpointer-arith
ASFLAGS += $(CPU_FLAGS) -x assembler-with-cpp
LDFLAGS += $(CPU_FLAGS) -nostdlib -nostartfiles \
           -Wl,--gc-sections \
           -Wl,-Map,$(MAP) \
           -T$(LINKER_SCRIPT) \
           $(PLATFORM_LDFLAGS)
LDLIBS += -lgcc

QEMU_FLAGS := -M mps3-an547 -cpu cortex-m55 \
              -display none -monitor none -serial none \
              -semihosting-config enable=on,target=native \
              -icount shift=0,align=off,sleep=off

PROBE_SELECTOR := $(if $(PROBE_SERIAL),--probe 0483:3754:$(PROBE_SERIAL),)

.PHONY: all build run size build-all run-all compare compare-all diff diff-all \
        print-build-dir print-elf print-map doctor doctor-board clean help

all: build

build: $(ELF)

$(ELF): $(OBJECTS) $(LINKER_SCRIPT)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $(OBJECTS) $(LDLIBS) -o $@
	$(SIZE) $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(ASFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/%.o: %.s
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(ASFLAGS) -MMD -MP -c $< -o $@

ifeq ($(DIFF_TEST),1)
$(DIFF_REF_DIR)/raw/%.o: $(REFERENCE_ALG_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -Iplatform/mps3-an547/include -Ireference/common \
	  -I$(REFERENCE_ALG_DIR) $(ALGORITHM_CPPFLAGS) $(CFLAGS) -c $< -o $@

$(DIFF_REF_DIR)/raw/fips202.o: reference/common/fips202.c
	@mkdir -p $(dir $@)
	$(CC) -Iplatform/mps3-an547/include -Ireference/common \
	  $(CFLAGS) -c $< -o $@

$(DIFF_SYMBOL_MAP): $(DIFF_REF_RAW_OBJECTS)
	@mkdir -p $(dir $@)
	$(NM) -g --defined-only $^ | \
	  awk 'NF == 3 && $$2 ~ /^[TDBR]$$/ { print $$3 " clean_" $$3 }' | \
	  sort -u > $@

$(DIFF_REF_DIR)/prefixed/%.o: $(DIFF_REF_DIR)/raw/%.o $(DIFF_SYMBOL_MAP)
	@mkdir -p $(dir $@)
	$(OBJCOPY) --redefine-syms=$(DIFF_SYMBOL_MAP) $< $@
endif

run: $(ELF)
ifeq ($(PLATFORM),mps3-an547)
	$(QEMU) $(QEMU_FLAGS) -kernel $(ELF)
else
	$(PYTHON) scripts/stm32n657_run.py --probe-rs $(PROBE_RS) --gdb $(GDB) \
	  --probe $(if $(PROBE_SERIAL),0483:3754:$(PROBE_SERIAL),0483:3754) \
	  $(if $(STM32_GDB_CONNECTION),--gdb-connection $(STM32_GDB_CONNECTION)) \
	  --timeout $(STM32_RUN_TIMEOUT) $(ELF)
endif

size: $(ELF)
	$(SIZE) -A $(ELF)

print-build-dir:
	@echo $(BUILD_DIR)

print-elf:
	@echo $(ELF)

print-map:
	@echo $(MAP)

build-all:
	@set -e; for alg in $(SUPPORTED_ALGORITHMS); do \
	  $(MAKE) --no-print-directory IMPLEMENTATION=$(IMPLEMENTATION) \
	    ALGORITHM=$$alg ITERATIONS=$(ITERATIONS) build; \
	done

run-all:
	@set -e; for alg in $(SUPPORTED_ALGORITHMS); do \
	  $(MAKE) --no-print-directory IMPLEMENTATION=$(IMPLEMENTATION) \
	    ALGORITHM=$$alg ITERATIONS=$(ITERATIONS) run; \
	done

compare:
	$(MAKE) --no-print-directory IMPLEMENTATION=reference ALGORITHM=$(ALGORITHM) ITERATIONS=$(ITERATIONS) run
	$(MAKE) --no-print-directory IMPLEMENTATION=optimization ALGORITHM=$(ALGORITHM) ITERATIONS=$(ITERATIONS) run

compare-all:
	$(MAKE) --no-print-directory IMPLEMENTATION=reference ITERATIONS=$(ITERATIONS) run-all
	$(MAKE) --no-print-directory IMPLEMENTATION=optimization ITERATIONS=$(ITERATIONS) run-all

diff:
	$(MAKE) --no-print-directory IMPLEMENTATION=optimization ALGORITHM=$(ALGORITHM) \
	  ITERATIONS=1 PROFILE=0 DIFF_TEST=1 run

diff-all:
	@set -e; for alg in $(SUPPORTED_ALGORITHMS); do \
	  $(MAKE) --no-print-directory ALGORITHM=$$alg diff; \
	done

doctor:
	@set -e; \
	for tool in arm-none-eabi-gcc arm-none-eabi-size arm-none-eabi-objcopy \
	  qemu-system-arm python3 make; do \
	  command -v $$tool >/dev/null || { echo "missing required tool: $$tool"; exit 1; }; \
	done; \
	echo "QEMU toolchain: ok"

doctor-board: doctor
	@set -e; \
	for tool in arm-none-eabi-gdb probe-rs; do \
	  command -v $$tool >/dev/null || { echo "missing board tool: $$tool"; exit 1; }; \
	done; \
	echo "STM32N657 board tools: ok"

clean:
	rm -rf build

help:
	@echo "Paper artifact shortcuts:"
	@echo "  make doctor"
	@echo "  make CONFIG=reference ALGORITHM=haetae2 run"
	@echo "  make CONFIG=intrinsic ALGORITHM=haetae2 run"
	@echo "  make CONFIG=assembly ALGORITHM=haetae2 run"
	@echo "  make CONFIG=final ALGORITHM=haetae2 run"
	@echo "  make CONFIG=final ALGORITHM=haetae2 diff"
	@echo "  make CONFIG=final diff-all"
	@echo "  make PLATFORM=stm32n657 CONFIG=final ALGORITHM=haetae2 run"
	@echo ""
	@echo "Expert selector examples:"
	@echo "  make IMPLEMENTATION=reference ALGORITHM=haetae2 run"
	@echo "  make IMPLEMENTATION=optimization ALGORITHM=haetae2 run"
	@echo "  make PLATFORM=stm32n657 STM32_CLOCK=boot IMPLEMENTATION=optimization ALGORITHM=haetae2 run"
	@echo "make IMPLEMENTATION=optimization ALGORITHM=haetae2 KECCAK_VARIANT=c run"
	@echo "make IMPLEMENTATION=optimization ALGORITHM=haetae2 KECCAK_VARIANT=pqm4 run"
	@echo "make IMPLEMENTATION=optimization ALGORITHM=haetae2 KECCAK_VARIANT=pqm4_persistent run"
	@echo "make IMPLEMENTATION=optimization ALGORITHM=haetae2 KECCAK_KERNEL=m55_fpscratch24 KECCAK_RC=fpgroup run"
	@echo "make IMPLEMENTATION=optimization ALGORITHM=haetae2 KECCAK_KERNEL=m55_fpscratch32 KECCAK_RC=memory run"
	@echo "make IMPLEMENTATION=optimization ALGORITHM=haetae2 NTT_VARIANT=m32_l44 run"
	@echo "make IMPLEMENTATION=optimization ALGORITHM=haetae2 NTT_VARIANT=m32_l332 run"
	@echo "make IMPLEMENTATION=optimization ALGORITHM=haetae2 NTT_VARIANT=m32_l332_vld4 run"
	@echo "make IMPLEMENTATION=optimization ALGORITHM=haetae2 NTT_VARIANT=m32_l332_vld4_u2 run"
	@echo "make IMPLEMENTATION=optimization ALGORITHM=haetae2 ACC_VARIANT=aftersum_d2_centered run"
	@echo "make IMPLEMENTATION=optimization ALGORITHM=haetae2 ACC_VARIANT=aftersum_dmax_tight run"
	@echo "make IMPLEMENTATION=optimization ALGORITHM=haetae2 ACC_VARIANT=aftersum_dmax_tight_precenter run"
	@echo "make IMPLEMENTATION=optimization ALGORITHM=haetae2 ACC_VARIANT=aftersum_dmax_tight_precenter_poly run"
	@echo "make IMPLEMENTATION=optimization ALGORITHM=haetae2 FFT_VARIANT=qrd_i_l2222_gpr_sep run"
	@echo "make IMPLEMENTATION=optimization ALGORITHM=haetae2 SAMPLER_VARIANT=cdt_mve run"
	@echo "make IMPLEMENTATION=optimization ALGORITHM=smaugt1 MUL_VARIANT=scalar SHIFT_VARIANT=mask run"
	@echo "make IMPLEMENTATION=optimization ALGORITHM=smaugt1 MUL_VARIANT=toom4_k2 MUL_CACHE=1 SHIFT_VARIANT=mask run"
	@echo "make IMPLEMENTATION=optimization ALGORITHM=smaugt1 MUL_VARIANT=toom4_outer32 MUL_CACHE=1 SHIFT_VARIANT=mask run"
	@echo "make ALGORITHM=haetae2 compare"
	@echo "make ALGORITHM=haetae2 diff"
	@echo "make diff-all"
	@echo "make IMPLEMENTATION=optimization ITERATIONS=1 run-all"
	@echo "Supported: $(SUPPORTED_ALGORITHMS)"
	@echo "Platforms: $(SUPPORTED_PLATFORMS)"

-include $(DEPS)

#include "api.h"
#include "bench_common.h"
#if defined(BENCH_CT_TEST)
#include "bench_ct.h"
#endif
#include "fft.h"
#include "fixpoint.h"
#include "keccak_bench.h"
#include "ntt.h"
#include "platform.h"
#include "poly.h"
#include "polyvec.h"
#include "sampler.h"

#if defined(HAETAE_ACC_AFTERSUM_DMAX_TIGHT_PRECENTER) ||                    \
    defined(HAETAE_NTT_ASM)
#include "haetae_ntt_mve.h"
#endif
#if defined(HAETAE_SAMPLER_CDT_MVE)
#include "haetae_sampler_mve.h"
#endif
#if defined(BENCH_DIFF_TEST)
#include "diff_test.h"
#endif

#include <stddef.h>
#include <stdint.h>

#ifndef BENCH_SCHEME_NAME
#define BENCH_SCHEME_NAME "HAETAE"
#endif

#ifndef BENCH_IMPLEMENTATION
#define BENCH_IMPLEMENTATION "unknown"
#endif

#define MESSAGE_BYTES 32u

static uint8_t public_key[CRYPTO_PUBLICKEYBYTES];
static uint8_t secret_key[CRYPTO_SECRETKEYBYTES];
static uint8_t signature[CRYPTO_BYTES];
static uint8_t message[MESSAGE_BYTES];
static size_t signature_length;
static poly microbench_input_a;
static poly microbench_input_b;
static poly microbench_work;
static polyvecl microbench_acc_a;
static _Alignas(8) polyvecl microbench_acc_b;
static volatile int32_t microbench_fft_sqabs_sink;
#if defined(HAETAE_SAMPLER_CDT_MVE)
static volatile uint64_t microbench_sampler_cdt_sink;
#endif
static uint8_t microbench_sampler_seed[HAETAE_CRHBYTES];
static fp96_76 microbench_sampler_sqsum;

#if defined(BENCH_CT_TEST)
static uint32_t ct_input_state = 0x243f6a88u;

static uint32_t ct_next_u32(void) {
  uint32_t value = ct_input_state;
  value ^= value << 13;
  value ^= value >> 17;
  value ^= value << 5;
  ct_input_state = value;
  return value;
}

static int32_t ct_coefficient(uint32_t class_id, size_t index,
                              uint32_t salt) {
  uint32_t value;
  if (class_id == 0u) {
    value = (uint32_t)index * 257u + 97u * salt + 17u;
  } else {
    value = ct_next_u32();
  }
  return (int32_t)(value % HAETAE_Q) - HAETAE_Q / 2;
}

static void ct_fill_poly(poly *destination, uint32_t class_id,
                         uint32_t salt) {
  for (size_t i = 0; i < HAETAE_N; i++) {
    destination->coeffs[i] = ct_coefficient(class_id, i, salt);
  }
}
#endif

/* Reuse the first 2 KiB of the accumulator arena after its microbenchmark. */
#define microbench_fft_work                                                   \
  ((complex_fp32_16 *)(void *)&microbench_acc_a)
#define microbench_sampler_values ((uint64_t *)(void *)&microbench_acc_b)
#define microbench_sampler_signs ((uint8_t *)(void *)&microbench_work)

static void prepare_microbench_inputs(void) {
  for (size_t i = 0; i < sizeof(microbench_sampler_seed); i++) {
    microbench_sampler_seed[i] = (uint8_t)(0xA5u ^ (uint8_t)(29u * i));
  }
  for (size_t i = 0; i < HAETAE_N; i++) {
    microbench_input_a.coeffs[i] =
        (int32_t)(((uint32_t)i * 257u + 17u) % HAETAE_Q) - HAETAE_Q / 2;
    microbench_input_b.coeffs[i] =
        (int32_t)(((uint32_t)i * 769u + 29u) % HAETAE_Q) - HAETAE_Q / 2;
    for (size_t term = 0; term < HAETAE_L; term++) {
      microbench_acc_a.vec[term].coeffs[i] =
          (int32_t)(((uint32_t)i * 257u + 97u * term + 17u) % HAETAE_Q) -
          HAETAE_Q / 2;
      microbench_acc_b.vec[term].coeffs[i] =
          (int32_t)(((uint32_t)i * 769u + 193u * term + 29u) % HAETAE_Q) -
          HAETAE_Q / 2;
    }
  }
}

static void prepare_ntt(void) { microbench_work = microbench_input_a; }

static void prepare_fft(void) {
  fft_init_and_bitrev(microbench_fft_work, &microbench_input_a);
}

static int operation_fft_init(void) {
  fft_init_and_bitrev(microbench_fft_work, &microbench_input_a);
  return 0;
}

static int operation_fft(void) {
  fft(microbench_fft_work);
  return 0;
}

static int operation_fft_sqabs(void) {
  int32_t sum = 0;
  for (size_t i = 0; i < FFT_N; i++) {
    sum += complex_fp_sqabs(microbench_fft_work[i]);
  }
  microbench_fft_sqabs_sink = sum;
  return 0;
}

#if defined(HAETAE_FFT_FUSED)
static void prepare_fft_fused(void) {
  fft_init_and_bitrev(microbench_fft_work, &microbench_input_a);
  for (size_t i = 0; i < FFT_N; i++) {
    microbench_input_b.coeffs[i] = 0;
  }
}

static int operation_fft_fused(void) {
  fft_sqabs_acc(microbench_input_b.coeffs, microbench_fft_work);
  return 0;
}
#endif

static void prepare_sampler(void) {
  microbench_sampler_sqsum.limb48[0] = 0;
  microbench_sampler_sqsum.limb48[1] = 0;
}

static int operation_sampler(void) {
  sample_gauss_N(microbench_sampler_values, microbench_sampler_signs,
                 &microbench_sampler_sqsum, microbench_sampler_seed, 0,
                 HAETAE_N);
  return 0;
}

#if defined(HAETAE_SAMPLER_CDT_MVE)
static int operation_sampler_cdt_leaf(void) {
  microbench_sampler_cdt_sink = haetae_sample_gauss16_cdt_mve(0x8000u);
  return 0;
}
#endif

static int operation_ntt(void) {
  ntt(microbench_work.coeffs);
  return 0;
}

static void prepare_invntt(void) { microbench_work = microbench_input_b; }

static int operation_invntt(void) {
  invntt_tomont(microbench_work.coeffs);
  return 0;
}

static int operation_pointwise(void) {
  poly_pointwise_montgomery(&microbench_work, &microbench_input_a,
                            &microbench_input_b);
  return 0;
}

static int operation_pointwise_acc(void) {
  polyvecl_pointwise_acc_montgomery(&microbench_work, &microbench_acc_a,
                                    &microbench_acc_b);
  return 0;
}

#if defined(HAETAE_NTT_ASM)
static void prepare_reduction(void) { microbench_work = microbench_input_a; }

static int operation_barrett_tight(void) {
  haetae_barrett_tight_mve(microbench_work.coeffs,
                           microbench_input_a.coeffs);
  return 0;
}

static int operation_reduce2q(void) {
  haetae_reduce2q_mve(microbench_work.coeffs);
  return 0;
}

static int operation_freeze2q(void) {
  haetae_freeze2q_mve(microbench_work.coeffs);
  return 0;
}

static int operation_freeze(void) {
  haetae_freeze_mve(microbench_work.coeffs);
  return 0;
}
#endif

#if defined(HAETAE_ACC_AFTERSUM_DMAX_TIGHT_PRECENTER)
static void prepare_pointwise_acc_precentered(void) {
  for (size_t i = 0; i < HAETAE_N; i++) {
    for (size_t term = 0; term < HAETAE_L; term++) {
      microbench_acc_b.vec[term].coeffs[i] =
          (int32_t)(((uint32_t)i * 769u + 193u * term + 29u) % HAETAE_Q) -
          HAETAE_Q / 2;
    }
  }
  for (size_t term = 0; term < HAETAE_L; term++) {
    haetae_barrett_tight_mve(microbench_acc_b.vec[term].coeffs,
                             microbench_acc_b.vec[term].coeffs);
  }
}

static int operation_pointwise_acc_precentered(void) {
  haetae_pointwise_acc_precentered_mve(
      microbench_work.coeffs, microbench_acc_a.vec[0].coeffs,
      microbench_acc_b.vec[0].coeffs, HAETAE_L);
  return 0;
}

static void prepare_pointwise_acc_u16_precentered(void) {
  uint16_t *compact_a = (uint16_t *)(void *)&microbench_acc_a;

  prepare_pointwise_acc_precentered();
  for (size_t term = 0; term < HAETAE_L; term++) {
    for (size_t i = 0; i < HAETAE_N; i++) {
      compact_a[term * HAETAE_N + i] =
          (uint16_t)(((uint32_t)i * 257u + 97u * term + 17u) % HAETAE_Q);
    }
  }
}

static int operation_pointwise_acc_u16_precentered(void) {
  haetae_pointwise_acc_u16_precentered_mve(
      microbench_work.coeffs,
      (const uint16_t *)(const void *)&microbench_acc_a,
      microbench_acc_b.vec[0].coeffs, HAETAE_L);
  return 0;
}
#endif

#if defined(BENCH_CT_TEST)
static void prepare_ct_ntt(uint32_t class_id, uint32_t pair_index) {
  (void)pair_index;
  ct_fill_poly(&microbench_work, class_id, 1u);
}

static void prepare_ct_invntt(uint32_t class_id, uint32_t pair_index) {
  (void)pair_index;
  ct_fill_poly(&microbench_work, class_id, 2u);
}

static void prepare_ct_pointwise(uint32_t class_id, uint32_t pair_index) {
  (void)pair_index;
  ct_fill_poly(&microbench_input_a, class_id, 3u);
  ct_fill_poly(&microbench_input_b, class_id, 4u);
  ct_fill_poly(&microbench_work, class_id, 5u);
}

static void prepare_ct_fft(uint32_t class_id, uint32_t pair_index) {
  (void)pair_index;
  ct_fill_poly(&microbench_input_a, class_id, 6u);
  fft_init_and_bitrev(microbench_fft_work, &microbench_input_a);
}

static void prepare_ct_sampler(uint32_t class_id, uint32_t pair_index) {
  (void)pair_index;
  for (size_t i = 0; i < sizeof(microbench_sampler_seed); i++) {
    microbench_sampler_seed[i] =
        class_id == 0u ? (uint8_t)(0xa5u ^ (uint8_t)(29u * i))
                       : (uint8_t)ct_next_u32();
  }
  prepare_sampler();
}

static int run_ct_test(void) {
#if defined(BENCH_CT_NTT)
  return bench_ct_measure(BENCH_SCHEME_NAME, BENCH_CT_BACKEND, "ntt",
                          prepare_ct_ntt, operation_ntt);
#elif defined(BENCH_CT_INVNTT)
  return bench_ct_measure(BENCH_SCHEME_NAME, BENCH_CT_BACKEND, "invntt",
                          prepare_ct_invntt, operation_invntt);
#elif defined(BENCH_CT_POINTWISE)
  return bench_ct_measure(BENCH_SCHEME_NAME, BENCH_CT_BACKEND, "pointwise",
                          prepare_ct_pointwise, operation_pointwise);
#elif defined(BENCH_CT_FFT)
  return bench_ct_measure(BENCH_SCHEME_NAME, BENCH_CT_BACKEND, "fft",
                          prepare_ct_fft, operation_fft);
#elif defined(BENCH_CT_SAMPLER)
  return bench_ct_measure(BENCH_SCHEME_NAME, BENCH_CT_BACKEND, "sampler",
                          prepare_ct_sampler, operation_sampler);
#elif defined(BENCH_CT_KECCAK_X4) && defined(BENCH_M55_KECCAK_INTERNAL)
  return bench_ct_measure(BENCH_SCHEME_NAME, BENCH_CT_BACKEND, "keccak_x4",
                          bench_keccak_ct_prepare_x4,
                          bench_keccak_ct_x4);
#elif defined(BENCH_CT_SHAKE_X4) && defined(BENCH_M55_KECCAK_INTERNAL)
  return bench_ct_measure(BENCH_SCHEME_NAME, BENCH_CT_BACKEND, "shake_x4",
                          bench_shake128_ct_prepare_x4,
                          bench_shake128_ct_x4);
#else
#error "CT_OPERATION is unavailable for the HAETAE benchmark"
#endif
}
#endif

static int operation_keypair(void) {
  return crypto_sign_keypair(public_key, secret_key);
}

static int operation_sign(void) {
  return crypto_sign_signature(signature, &signature_length, message,
                               sizeof(message), NULL, 0u, secret_key);
}

static int operation_verify(void) {
  return crypto_sign_verify(signature, signature_length, message,
                            sizeof(message), NULL, 0u, public_key);
}

static int self_test(void) {
  int result;

  for (size_t i = 0; i < sizeof(message); i++) {
    message[i] = (uint8_t)i;
  }

  platform_heap_reset();
  result = operation_keypair();
  if (result != 0) {
    return 10;
  }
  platform_heap_reset();
  result = operation_sign();
  if (result != 0 || signature_length > sizeof(signature)) {
    return 11;
  }
  platform_heap_reset();
  result = operation_verify();
  if (result != 0) {
    return 12;
  }

  signature[0] ^= 1u;
  platform_heap_reset();
  result = operation_verify();
  signature[0] ^= 1u;
  if (result == 0) {
    return 13;
  }
  return 0;
}

int main(void) {
  int result;

  platform_init();
  platform_stack_watermark_init();
  bench_print_header(BENCH_SCHEME_NAME, BENCH_IMPLEMENTATION);
  bench_print_sizes3("public_key", CRYPTO_PUBLICKEYBYTES, "secret_key",
                     CRYPTO_SECRETKEYBYTES, "signature", CRYPTO_BYTES);

#if defined(BENCH_M55_KECCAK_INTERNAL)
  result = bench_keccak_preflight_x4();
  if (result != 0) {
    platform_write("KECCAK_X4_PREFLIGHT,status=failed,code=");
    platform_write_u32((uint32_t)result);
    platform_write("\n");
    return result;
  }
  platform_write("KECCAK_X4_PREFLIGHT,status=ok\n");
#endif

#if defined(BENCH_DIFF_TEST)
  result = bench_fft_preflight();
  if (result != 0) {
    platform_write("FFT_PREFLIGHT,status=failed,code=");
    platform_write_u32((uint32_t)result);
    platform_write("\n");
    return result;
  }
  platform_write("FFT_PREFLIGHT,status=ok\n");
#endif

  result = self_test();
  if (result != 0) {
    platform_write("SELFTEST,failed,code=");
    platform_write_u32((uint32_t)result);
    platform_write("\n");
    return result;
  }
  platform_write("SELFTEST,ok\n");

#if defined(BENCH_CT_TEST)
  prepare_microbench_inputs();
  result = run_ct_test();
  if (result != 0) {
    return result;
  }
  bench_print_footer();
  return 0;
#endif

#if defined(BENCH_DIFF_TEST)
  result = bench_kat_test();
  if (result != 0) {
    platform_write("KAT,status=failed,code=");
    platform_write_u32((uint32_t)result);
    platform_write("\n");
    return result;
  }
  platform_write("KAT,status=ok\n");

  result = bench_diff_test();
  if (result != 0) {
    platform_write("DIFF,iterations=1000,status=failed,code=");
    platform_write_u32((uint32_t)result);
    platform_write("\n");
    return result;
  }
  platform_write("DIFF,iterations=1000,status=ok\n");
#endif

  prepare_microbench_inputs();

#if defined(BENCH_M55_KECCAK_INTERNAL)
  if (bench_measure_prepared(BENCH_SCHEME_NAME, BENCH_IMPLEMENTATION,
                             "keccak_permutation",
                             bench_keccak_prepare_permutation,
                             bench_keccak_permutation) != 0 ||
      bench_measure_prepared(BENCH_SCHEME_NAME, BENCH_IMPLEMENTATION,
                             "keccak_stream", bench_keccak_prepare_stream,
                             bench_keccak_stream) != 0 ||
      bench_keccak_measure_x4() != 0) {
    return 20;
  }
#endif

  if (bench_measure(BENCH_SCHEME_NAME, BENCH_IMPLEMENTATION, "keypair",
                    operation_keypair) != 0 ||
      bench_measure(BENCH_SCHEME_NAME, BENCH_IMPLEMENTATION, "sign",
                    operation_sign) != 0 ||
      bench_measure(BENCH_SCHEME_NAME, BENCH_IMPLEMENTATION, "verify",
                    operation_verify) != 0 ||
      bench_measure_prepared(BENCH_SCHEME_NAME, BENCH_IMPLEMENTATION, "ntt",
                             prepare_ntt, operation_ntt) != 0 ||
      bench_measure_prepared(BENCH_SCHEME_NAME, BENCH_IMPLEMENTATION,
                             "invntt", prepare_invntt,
                             operation_invntt) != 0 ||
      bench_measure_prepared(BENCH_SCHEME_NAME, BENCH_IMPLEMENTATION,
                             "pointwise", prepare_ntt,
                             operation_pointwise) != 0 ||
      bench_measure_prepared(BENCH_SCHEME_NAME, BENCH_IMPLEMENTATION,
                             "pointwise_acc", prepare_ntt,
                             operation_pointwise_acc) != 0 ||
#if defined(HAETAE_NTT_ASM)
      bench_measure_prepared(BENCH_SCHEME_NAME, BENCH_IMPLEMENTATION,
                             "barrett_tight", prepare_reduction,
                             operation_barrett_tight) != 0 ||
      bench_measure_prepared(BENCH_SCHEME_NAME, BENCH_IMPLEMENTATION,
                             "reduce2q", prepare_reduction,
                             operation_reduce2q) != 0 ||
      bench_measure_prepared(BENCH_SCHEME_NAME, BENCH_IMPLEMENTATION,
                             "freeze2q", prepare_reduction,
                             operation_freeze2q) != 0 ||
      bench_measure_prepared(BENCH_SCHEME_NAME, BENCH_IMPLEMENTATION,
                             "freeze", prepare_reduction,
                             operation_freeze) != 0 ||
#endif
#if defined(HAETAE_ACC_AFTERSUM_DMAX_TIGHT_PRECENTER)
      bench_measure_prepared(BENCH_SCHEME_NAME, BENCH_IMPLEMENTATION,
                             "pointwise_acc_precentered",
                             prepare_pointwise_acc_precentered,
                             operation_pointwise_acc_precentered) != 0 ||
      bench_measure_prepared(BENCH_SCHEME_NAME, BENCH_IMPLEMENTATION,
                             "pointwise_acc_u16_precentered",
                             prepare_pointwise_acc_u16_precentered,
                             operation_pointwise_acc_u16_precentered) != 0 ||
#endif
      bench_measure_prepared(BENCH_SCHEME_NAME, BENCH_IMPLEMENTATION,
                             "fft_init", prepare_fft,
                             operation_fft_init) != 0 ||
      bench_measure_prepared(BENCH_SCHEME_NAME, BENCH_IMPLEMENTATION, "fft",
                             prepare_fft, operation_fft) != 0 ||
      bench_measure_prepared(BENCH_SCHEME_NAME, BENCH_IMPLEMENTATION,
                             "fft_sqabs", prepare_fft,
                             operation_fft_sqabs) != 0 ||
#if defined(HAETAE_FFT_FUSED)
      bench_measure_prepared(BENCH_SCHEME_NAME, BENCH_IMPLEMENTATION,
                             "fft_fused_sqabs", prepare_fft_fused,
                             operation_fft_fused) != 0 ||
#endif
#if defined(HAETAE_SAMPLER_CDT_MVE)
      bench_measure(BENCH_SCHEME_NAME, BENCH_IMPLEMENTATION,
                    "sampler_cdt_leaf", operation_sampler_cdt_leaf) != 0 ||
#endif
      bench_measure_prepared(BENCH_SCHEME_NAME, BENCH_IMPLEMENTATION,
                             "sampler_gauss_N", prepare_sampler,
                             operation_sampler) != 0) {
    return 20;
  }

  bench_print_footer();
  return 0;
}

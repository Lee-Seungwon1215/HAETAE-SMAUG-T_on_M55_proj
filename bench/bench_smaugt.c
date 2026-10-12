#include "api.h"
#include "bench_common.h"
#if defined(BENCH_CT_TEST)
#include "bench_ct.h"
#endif
#include "keccak_bench.h"
#include "platform.h"
#include "poly.h"
#include "string.h"
#include "toomcook.h"

#if defined(BENCH_DIFF_TEST)
#include "diff_test.h"
#endif

#include <stddef.h>
#include <stdint.h>

#ifndef BENCH_SCHEME_NAME
#define BENCH_SCHEME_NAME "SMAUG-T"
#endif

#ifndef BENCH_IMPLEMENTATION
#define BENCH_IMPLEMENTATION "unknown"
#endif

static uint8_t public_key[CRYPTO_PUBLICKEYBYTES];
static uint8_t secret_key[CRYPTO_SECRETKEYBYTES];
static uint8_t ciphertext[CRYPTO_CIPHERTEXTBYTES];
static uint8_t shared_secret_enc[CRYPTO_BYTES];
static uint8_t shared_secret_dec[CRYPTO_BYTES];
static int16_t microbench_a[SMAUGT_N];
static int16_t microbench_b[SMAUGT_N];
static int16_t microbench_product[SMAUGT_N];
static polyvec microbench_matrix[SMAUGT_K];
static polyvec microbench_vector;
static polyvec microbench_matrix_result;

#if defined(BENCH_CT_TEST)
static uint32_t ct_input_state = 0x13198a2eu;

static uint32_t ct_next_u32(void) {
  uint32_t value = ct_input_state;
  value ^= value << 13;
  value ^= value >> 17;
  value ^= value << 5;
  ct_input_state = value;
  return value;
}

static int16_t ct_coefficient(uint32_t class_id, size_t index,
                              uint32_t salt) {
  if (class_id == 0u) {
    return (int16_t)((uint16_t)(index * 257u + salt * 97u + 17u));
  }
  return (int16_t)ct_next_u32();
}
#endif

static void prepare_microbench_inputs(void) {
  for (size_t i = 0; i < SMAUGT_N; i++) {
    microbench_a[i] = (int16_t)((uint16_t)(i * 257u + 17u));
    microbench_b[i] = (int16_t)((uint16_t)(i * 769u + 29u));
  }
  for (size_t row = 0; row < SMAUGT_K; row++) {
    for (size_t column = 0; column < SMAUGT_K; column++) {
      for (size_t i = 0; i < SMAUGT_N; i++) {
        microbench_matrix[row].vec[column].coeffs[i] =
            (int16_t)((uint16_t)(row * 4099u + column * 2053u + i * 17u));
      }
    }
  }
  for (size_t column = 0; column < SMAUGT_K; column++) {
    for (size_t i = 0; i < SMAUGT_N; i++) {
      microbench_vector.vec[column].coeffs[i] =
          (int16_t)((uint16_t)(column * 1237u + i * 31u + 1u));
    }
  }
}

static void prepare_poly_mul_acc(void) {
  memset(microbench_product, 0, sizeof(microbench_product));
}

static int operation_poly_mul_acc(void) {
  poly_mul_acc(microbench_a, microbench_b, microbench_product);
  return 0;
}

static void prepare_matrix_vec_mult(void) {
  memset(&microbench_matrix_result, 0, sizeof(microbench_matrix_result));
}

static int operation_matrix_vec_mult(void) {
  matrix_vec_mult_add(&microbench_matrix_result, microbench_matrix,
                      &microbench_vector);
  return 0;
}

#if defined(BENCH_CT_TEST)
static void prepare_ct_poly_mul_acc(uint32_t class_id, uint32_t pair_index) {
  (void)pair_index;
  for (size_t i = 0; i < SMAUGT_N; i++) {
    microbench_a[i] = ct_coefficient(class_id, i, 1u);
    microbench_b[i] = ct_coefficient(class_id, i, 2u);
  }
  prepare_poly_mul_acc();
}

static void prepare_ct_matrix_vec_mult(uint32_t class_id,
                                       uint32_t pair_index) {
  (void)pair_index;
  for (size_t row = 0; row < SMAUGT_K; row++) {
    for (size_t column = 0; column < SMAUGT_K; column++) {
      for (size_t i = 0; i < SMAUGT_N; i++) {
        microbench_matrix[row].vec[column].coeffs[i] =
            ct_coefficient(class_id, i,
                           (uint32_t)(3u + row * SMAUGT_K + column));
      }
    }
  }
  for (size_t column = 0; column < SMAUGT_K; column++) {
    for (size_t i = 0; i < SMAUGT_N; i++) {
      microbench_vector.vec[column].coeffs[i] =
          ct_coefficient(class_id, i, (uint32_t)(19u + column));
    }
  }
  prepare_matrix_vec_mult();
}

static int run_ct_test(void) {
#if defined(BENCH_CT_POLY_MUL_ACC)
  return bench_ct_measure(BENCH_SCHEME_NAME, BENCH_CT_BACKEND, "poly_mul_acc",
                          prepare_ct_poly_mul_acc, operation_poly_mul_acc);
#elif defined(BENCH_CT_MATRIX_VEC_MULT)
  return bench_ct_measure(BENCH_SCHEME_NAME, BENCH_CT_BACKEND,
                          "matrix_vec_mult", prepare_ct_matrix_vec_mult,
                          operation_matrix_vec_mult);
#elif defined(BENCH_CT_KECCAK_X4) && defined(BENCH_M55_KECCAK_INTERNAL)
  return bench_ct_measure(BENCH_SCHEME_NAME, BENCH_CT_BACKEND, "keccak_x4",
                          bench_keccak_ct_prepare_x4,
                          bench_keccak_ct_x4);
#elif defined(BENCH_CT_SHAKE_X4) && defined(BENCH_M55_KECCAK_INTERNAL)
  return bench_ct_measure(BENCH_SCHEME_NAME, BENCH_CT_BACKEND, "shake_x4",
                          bench_shake128_ct_prepare_x4,
                          bench_shake128_ct_x4);
#else
#error "CT_OPERATION is unavailable for the SMAUG-T benchmark"
#endif
}
#endif

static int operation_keypair(void) {
  return crypto_kem_keypair(public_key, secret_key);
}

static int operation_encapsulate(void) {
  return crypto_kem_enc(ciphertext, shared_secret_enc, public_key);
}

static int operation_decapsulate(void) {
  return crypto_kem_dec(shared_secret_dec, ciphertext, secret_key);
}

static int self_test(void) {
  int result;

  platform_heap_reset();
  result = operation_keypair();
  if (result != 0) {
    return 10;
  }
  platform_heap_reset();
  result = operation_encapsulate();
  if (result != 0) {
    return 11;
  }
  platform_heap_reset();
  result = operation_decapsulate();
  if (result != 0) {
    return 12;
  }
  if (memcmp(shared_secret_enc, shared_secret_dec, sizeof(shared_secret_enc)) !=
      0) {
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
                     CRYPTO_SECRETKEYBYTES, "ciphertext",
                     CRYPTO_CIPHERTEXTBYTES);

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
      bench_measure(BENCH_SCHEME_NAME, BENCH_IMPLEMENTATION, "encapsulate",
                    operation_encapsulate) != 0 ||
      bench_measure(BENCH_SCHEME_NAME, BENCH_IMPLEMENTATION, "decapsulate",
                    operation_decapsulate) != 0 ||
      bench_measure_prepared(BENCH_SCHEME_NAME, BENCH_IMPLEMENTATION,
                             "poly_mul_acc", prepare_poly_mul_acc,
                             operation_poly_mul_acc) != 0 ||
      bench_measure_prepared(BENCH_SCHEME_NAME, BENCH_IMPLEMENTATION,
                             "matrix_vec_mult", prepare_matrix_vec_mult,
                             operation_matrix_vec_mult) != 0) {
    return 20;
  }

  bench_print_footer();
  return 0;
}

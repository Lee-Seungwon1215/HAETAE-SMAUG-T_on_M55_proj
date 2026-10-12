#include "diff_test.h"

#include "api.h"
#include "platform.h"
#include "poly.h"
#include "string.h"
#include "toomcook.h"

#include <stddef.h>
#include <stdint.h>

#define DIFF_CASES 1000u
#define CLEAN_NAME_INNER(name) clean_##name
#define CLEAN_NAME(name) CLEAN_NAME_INNER(name)

extern void CLEAN_NAME(poly_mul_acc)(const int16_t[SMAUGT_N],
                                     const int16_t[SMAUGT_N],
                                     int16_t[SMAUGT_N]);
extern void CLEAN_NAME(matrix_vec_mult_add)(polyvec *,
                                            const polyvec[SMAUGT_K],
                                            const polyvec *);
extern int CLEAN_NAME(crypto_kem_keypair_internal)(
    uint8_t *, uint8_t *, uint8_t[SMAUGT_T_BYTES],
    uint8_t[SMAUGT_CRYPTO_BYTES]);
extern int CLEAN_NAME(crypto_kem_enc_internal)(uint8_t *, uint8_t *,
                                               const uint8_t *,
                                               const uint8_t *);
extern int CLEAN_NAME(crypto_kem_dec_internal)(uint8_t *, const uint8_t *,
                                               const uint8_t *);

static int16_t input_left[SMAUGT_N];
static int16_t input_right[SMAUGT_N];
static int16_t clean_product[SMAUGT_N];
static int16_t m55_product[SMAUGT_N];
static polyvec input_matrix[SMAUGT_K];
static polyvec input_vector;
static polyvec clean_matrix_output;
static polyvec m55_matrix_output;
static uint32_t random_state = 0x243F6A88u;
static uint8_t kat_clean_public_key[CRYPTO_PUBLICKEYBYTES];
static uint8_t kat_m55_public_key[CRYPTO_PUBLICKEYBYTES];
static uint8_t kat_clean_secret_key[CRYPTO_SECRETKEYBYTES];
static uint8_t kat_m55_secret_key[CRYPTO_SECRETKEYBYTES];
static uint8_t kat_clean_ciphertext[CRYPTO_CIPHERTEXTBYTES];
static uint8_t kat_m55_ciphertext[CRYPTO_CIPHERTEXTBYTES];
static uint8_t kat_clean_shared_secret[CRYPTO_BYTES];
static uint8_t kat_m55_shared_secret[CRYPTO_BYTES];

static uint32_t test_random(void) {
  uint32_t value = random_state;
  value ^= value << 13;
  value ^= value >> 17;
  value ^= value << 5;
  random_state = value;
  return value;
}

static int equal_coefficients(const int16_t *left, const int16_t *right,
                              size_t count) {
  for (size_t i = 0; i < count; i++) {
    if (left[i] != right[i]) {
      return 0;
    }
  }
  return 1;
}

static void random_coefficients(int16_t *values, size_t count) {
  for (size_t i = 0; i < count; i++) {
    values[i] = (int16_t)(uint16_t)test_random();
  }
}

int bench_kat_test(void) {
  uint8_t d[SMAUGT_T_BYTES];
  uint8_t seed[SMAUGT_CRYPTO_BYTES];
  uint8_t message[SMAUGT_MSG_BYTES];
  uint8_t clean_decapsulated[CRYPTO_BYTES];
  uint8_t m55_decapsulated[CRYPTO_BYTES];

  for (size_t i = 0; i < sizeof(d); i++) {
    d[i] = (uint8_t)(0x5Au ^ i);
  }
  for (size_t i = 0; i < sizeof(seed); i++) {
    seed[i] = (uint8_t)i;
  }
  for (size_t i = 0; i < sizeof(message); i++) {
    message[i] = (uint8_t)(7u * i + 3u);
  }

  platform_heap_reset();
  if (CLEAN_NAME(crypto_kem_keypair_internal)(
          kat_clean_public_key, kat_clean_secret_key, d, seed) != 0) {
    return 211;
  }
  platform_heap_reset();
  if (crypto_kem_keypair_internal(kat_m55_public_key, kat_m55_secret_key, d,
                                  seed) != 0) {
    return 212;
  }
  if (memcmp(kat_clean_public_key, kat_m55_public_key,
             sizeof(kat_clean_public_key)) != 0 ||
      memcmp(kat_clean_secret_key, kat_m55_secret_key,
             sizeof(kat_clean_secret_key)) != 0) {
    return 213;
  }

  platform_heap_reset();
  if (CLEAN_NAME(crypto_kem_enc_internal)(
          kat_clean_ciphertext, kat_clean_shared_secret, kat_clean_public_key,
          message) != 0) {
    return 214;
  }
  platform_heap_reset();
  if (crypto_kem_enc_internal(kat_m55_ciphertext, kat_m55_shared_secret,
                              kat_m55_public_key, message) != 0) {
    return 215;
  }
  if (memcmp(kat_clean_ciphertext, kat_m55_ciphertext,
             sizeof(kat_clean_ciphertext)) != 0 ||
      memcmp(kat_clean_shared_secret, kat_m55_shared_secret,
             sizeof(kat_clean_shared_secret)) != 0) {
    return 216;
  }

  platform_heap_reset();
  if (CLEAN_NAME(crypto_kem_dec_internal)(
          clean_decapsulated, kat_clean_ciphertext, kat_clean_secret_key) !=
      0) {
    return 217;
  }
  platform_heap_reset();
  if (crypto_kem_dec_internal(m55_decapsulated, kat_m55_ciphertext,
                              kat_m55_secret_key) != 0) {
    return 218;
  }
  if (memcmp(clean_decapsulated, m55_decapsulated,
             sizeof(clean_decapsulated)) != 0 ||
      memcmp(clean_decapsulated, kat_clean_shared_secret,
             sizeof(clean_decapsulated)) != 0) {
    return 219;
  }

  /* Exercise the constant-time FO failure path with the same invalid input. */
  kat_m55_ciphertext[0] ^= 0x01u;
  platform_heap_reset();
  if (CLEAN_NAME(crypto_kem_dec_internal)(
          clean_decapsulated, kat_m55_ciphertext, kat_clean_secret_key) != 0) {
    return 220;
  }
  platform_heap_reset();
  if (crypto_kem_dec_internal(m55_decapsulated, kat_m55_ciphertext,
                              kat_m55_secret_key) != 0) {
    return 221;
  }
  if (memcmp(clean_decapsulated, m55_decapsulated,
             sizeof(clean_decapsulated)) != 0 ||
      memcmp(m55_decapsulated, kat_m55_shared_secret,
             sizeof(m55_decapsulated)) == 0) {
    return 222;
  }
  kat_m55_ciphertext[0] ^= 0x01u;
  return 0;
}

int bench_diff_test(void) {
  random_state = 0x243F6A88u;
  for (size_t test = 0; test < DIFF_CASES; test++) {
    uint8_t d[SMAUGT_T_BYTES];
    uint8_t seed[SMAUGT_CRYPTO_BYTES];
    uint8_t message[SMAUGT_MSG_BYTES];
    uint8_t clean_decapsulated[CRYPTO_BYTES];
    uint8_t m55_decapsulated[CRYPTO_BYTES];

    int hash_result = bench_fips202_diff_case((unsigned int)test);
    if (hash_result != 0) {
      return hash_result;
    }

    for (size_t i = 0; i < sizeof(d); i++) {
      d[i] = (uint8_t)test_random();
    }
    for (size_t i = 0; i < sizeof(seed); i++) {
      seed[i] = (uint8_t)test_random();
    }
    for (size_t i = 0; i < sizeof(message); i++) {
      message[i] = (uint8_t)test_random();
    }

    platform_heap_reset();
    if (CLEAN_NAME(crypto_kem_keypair_internal)(
            kat_clean_public_key, kat_clean_secret_key, d, seed) != 0) {
      return 223;
    }
    platform_heap_reset();
    if (crypto_kem_keypair_internal(kat_m55_public_key, kat_m55_secret_key, d,
                                    seed) != 0) {
      return 224;
    }
    if (memcmp(kat_clean_public_key, kat_m55_public_key,
               sizeof(kat_clean_public_key)) != 0 ||
        memcmp(kat_clean_secret_key, kat_m55_secret_key,
               sizeof(kat_clean_secret_key)) != 0) {
      return 225;
    }

    platform_heap_reset();
    if (CLEAN_NAME(crypto_kem_enc_internal)(
            kat_clean_ciphertext, kat_clean_shared_secret,
            kat_clean_public_key, message) != 0) {
      return 226;
    }
    platform_heap_reset();
    if (crypto_kem_enc_internal(kat_m55_ciphertext, kat_m55_shared_secret,
                                kat_m55_public_key, message) != 0) {
      return 227;
    }
    if (memcmp(kat_clean_ciphertext, kat_m55_ciphertext,
               sizeof(kat_clean_ciphertext)) != 0 ||
        memcmp(kat_clean_shared_secret, kat_m55_shared_secret,
               sizeof(kat_clean_shared_secret)) != 0) {
      return 228;
    }

    platform_heap_reset();
    if (CLEAN_NAME(crypto_kem_dec_internal)(
            clean_decapsulated, kat_clean_ciphertext, kat_clean_secret_key) !=
        0) {
      return 229;
    }
    platform_heap_reset();
    if (crypto_kem_dec_internal(m55_decapsulated, kat_m55_ciphertext,
                                kat_m55_secret_key) != 0) {
      return 230;
    }
    if (memcmp(clean_decapsulated, m55_decapsulated,
               sizeof(clean_decapsulated)) != 0 ||
        memcmp(m55_decapsulated, kat_m55_shared_secret,
               sizeof(m55_decapsulated)) != 0) {
      return 231;
    }

    random_coefficients(input_left, SMAUGT_N);
    random_coefficients(input_right, SMAUGT_N);
    random_coefficients(clean_product, SMAUGT_N);
    for (size_t i = 0; i < SMAUGT_N; i++) {
      m55_product[i] = clean_product[i];
    }
    CLEAN_NAME(poly_mul_acc)(input_left, input_right, clean_product);
    poly_mul_acc(input_left, input_right, m55_product);
    if (!equal_coefficients(clean_product, m55_product, SMAUGT_N)) {
      return 201;
    }


    random_coefficients((int16_t *)input_matrix,
                        SMAUGT_K * SMAUGT_K * SMAUGT_N);
    random_coefficients((int16_t *)&input_vector, SMAUGT_K * SMAUGT_N);
    random_coefficients((int16_t *)&clean_matrix_output,
                        SMAUGT_K * SMAUGT_N);
    m55_matrix_output = clean_matrix_output;
    CLEAN_NAME(matrix_vec_mult_add)(&clean_matrix_output, input_matrix,
                                     &input_vector);
    matrix_vec_mult_add(&m55_matrix_output, input_matrix, &input_vector);
    if (!equal_coefficients((const int16_t *)&clean_matrix_output,
                            (const int16_t *)&m55_matrix_output,
                            SMAUGT_K * SMAUGT_N)) {
      return 202;
    }
  }
  return 0;
}

#include "profile.h"

#include "fips202.h"

#if defined(BENCH_FAMILY_haetae)
#include "encoding.h"
#include "fft.h"
#include "fixpoint.h"
#include "ntt.h"
#include "packing.h"
#include "poly.h"
#include "polyfix.h"
#include "polyvec.h"
#elif defined(BENCH_FAMILY_smaugt)
#include "cbd.h"
#include "dg.h"
#include "hwt.h"
#include "pack.h"
#include "packring.h"
#include "poly.h"
#include "toomcook.h"
#elif defined(BENCH_FAMILY_aimer)
#include "field.h"
#elif defined(BENCH_FAMILY_ntruplus)
#include "poly.h"
#else
#error "A benchmark family must be selected when profiling"
#endif

#define WRAP_NAME_INNER(name) __wrap_##name
#define WRAP_NAME(name) WRAP_NAME_INNER(name)
#define REAL_NAME_INNER(name) __real_##name
#define REAL_NAME(name) REAL_NAME_INNER(name)

#define PROFILE_VOID0(name, category)                                           \
  extern void REAL_NAME(name)(void);                                           \
  void WRAP_NAME(name)(void) {                                                 \
    bench_profile_enter(category);                                             \
    REAL_NAME(name)();                                                         \
    bench_profile_exit();                                                      \
  }

#define PROFILE_VOID1(name, category, type1, arg1)                             \
  extern void REAL_NAME(name)(type1);                                          \
  void WRAP_NAME(name)(type1 arg1) {                                           \
    bench_profile_enter(category);                                             \
    REAL_NAME(name)(arg1);                                                     \
    bench_profile_exit();                                                      \
  }

extern void __real_shake128(uint8_t *, size_t, const uint8_t *, size_t);
void __wrap_shake128(uint8_t *output, size_t outlen, const uint8_t *input,
                     size_t inlen) {
  bench_profile_enter(BENCH_CATEGORY_KECCAK);
  __real_shake128(output, outlen, input, inlen);
  bench_profile_exit();
}

extern void __real_shake256(uint8_t *, size_t, const uint8_t *, size_t);
void __wrap_shake256(uint8_t *output, size_t outlen, const uint8_t *input,
                     size_t inlen) {
  bench_profile_enter(BENCH_CATEGORY_KECCAK);
  __real_shake256(output, outlen, input, inlen);
  bench_profile_exit();
}

#define PROFILE_INC_INIT(name, type)                                           \
  extern void __real_##name(type *);                                           \
  void __wrap_##name(type *state) {                                            \
    bench_profile_enter(BENCH_CATEGORY_KECCAK);                                \
    __real_##name(state);                                                       \
    bench_profile_exit();                                                      \
  }

#define PROFILE_INC_ABSORB(name, type)                                         \
  extern void __real_##name(type *, const uint8_t *, size_t);                   \
  void __wrap_##name(type *state, const uint8_t *input, size_t inlen) {         \
    bench_profile_enter(BENCH_CATEGORY_KECCAK);                                \
    __real_##name(state, input, inlen);                                         \
    bench_profile_exit();                                                      \
  }

#define PROFILE_INC_FINALIZE(name, type)                                       \
  extern void __real_##name(type *);                                           \
  void __wrap_##name(type *state) {                                            \
    bench_profile_enter(BENCH_CATEGORY_KECCAK);                                \
    __real_##name(state);                                                       \
    bench_profile_exit();                                                      \
  }

#define PROFILE_INC_SQUEEZE(name, type)                                        \
  extern void __real_##name(uint8_t *, size_t, type *);                         \
  void __wrap_##name(uint8_t *output, size_t outlen, type *state) {             \
    bench_profile_enter(BENCH_CATEGORY_KECCAK);                                \
    __real_##name(output, outlen, state);                                       \
    bench_profile_exit();                                                      \
  }

PROFILE_INC_INIT(shake128_inc_init, shake128incctx)
PROFILE_INC_ABSORB(shake128_inc_absorb, shake128incctx)
PROFILE_INC_FINALIZE(shake128_inc_finalize, shake128incctx)
PROFILE_INC_SQUEEZE(shake128_inc_squeeze, shake128incctx)
PROFILE_INC_INIT(shake256_inc_init, shake256incctx)
PROFILE_INC_ABSORB(shake256_inc_absorb, shake256incctx)
PROFILE_INC_FINALIZE(shake256_inc_finalize, shake256incctx)
PROFILE_INC_SQUEEZE(shake256_inc_squeeze, shake256incctx)
PROFILE_VOID1(shake256_inc_ctx_release, BENCH_CATEGORY_KECCAK, shake256incctx *,
              state)

extern void __real_sha3_256(uint8_t *, const uint8_t *, size_t);
void __wrap_sha3_256(uint8_t *output, const uint8_t *input, size_t inlen) {
  bench_profile_enter(BENCH_CATEGORY_KECCAK);
  __real_sha3_256(output, input, inlen);
  bench_profile_exit();
}

extern void __real_sha3_512(uint8_t *, const uint8_t *, size_t);
void __wrap_sha3_512(uint8_t *output, const uint8_t *input, size_t inlen) {
  bench_profile_enter(BENCH_CATEGORY_KECCAK);
  __real_sha3_512(output, input, inlen);
  bench_profile_exit();
}

#if defined(BENCH_FAMILY_haetae)
extern void REAL_NAME(ntt)(int32_t[HAETAE_N]);
void WRAP_NAME(ntt)(int32_t values[HAETAE_N]) {
  bench_profile_enter(BENCH_CATEGORY_MULTIPLICATION);
  REAL_NAME(ntt)(values);
  bench_profile_exit();
}

extern void REAL_NAME(invntt_tomont)(int32_t[HAETAE_N]);
void WRAP_NAME(invntt_tomont)(int32_t values[HAETAE_N]) {
  bench_profile_enter(BENCH_CATEGORY_MULTIPLICATION);
  REAL_NAME(invntt_tomont)(values);
  bench_profile_exit();
}

extern void REAL_NAME(poly_pointwise_montgomery)(poly *, const poly *,
                                                  const poly *);
void WRAP_NAME(poly_pointwise_montgomery)(poly *result, const poly *left,
                                          const poly *right) {
  bench_profile_enter(BENCH_CATEGORY_MULTIPLICATION);
  REAL_NAME(poly_pointwise_montgomery)(result, left, right);
  bench_profile_exit();
}

extern void REAL_NAME(poly_uniform)(poly *, const uint8_t[HAETAE_SEEDBYTES],
                                    uint16_t);
void WRAP_NAME(poly_uniform)(poly *result,
                             const uint8_t seed[HAETAE_SEEDBYTES],
                             uint16_t nonce) {
  bench_profile_enter(BENCH_CATEGORY_SAMPLER);
  REAL_NAME(poly_uniform)(result, seed, nonce);
  bench_profile_exit();
}

extern void REAL_NAME(poly_uniform_eta)(poly *,
                                        const uint8_t[HAETAE_CRHBYTES],
                                        uint16_t);
void WRAP_NAME(poly_uniform_eta)(poly *result,
                                 const uint8_t seed[HAETAE_CRHBYTES],
                                 uint16_t nonce) {
  bench_profile_enter(BENCH_CATEGORY_SAMPLER);
  REAL_NAME(poly_uniform_eta)(result, seed, nonce);
  bench_profile_exit();
}

extern uint16_t REAL_NAME(polyfixveclk_sample_hyperball)(
    polyfixvecl *, polyfixveck *, uint8_t *, const uint8_t[HAETAE_CRHBYTES],
    uint16_t);
uint16_t WRAP_NAME(polyfixveclk_sample_hyperball)(
    polyfixvecl *y1, polyfixveck *y2, uint8_t *b,
    const uint8_t seed[HAETAE_CRHBYTES], uint16_t nonce) {
  uint16_t result;
  bench_profile_enter(BENCH_CATEGORY_SAMPLER);
  result = REAL_NAME(polyfixveclk_sample_hyperball)(y1, y2, b, seed, nonce);
  bench_profile_exit();
  return result;
}

extern uint16_t REAL_NAME(encode_h)(uint8_t *, const int32_t *);
uint16_t WRAP_NAME(encode_h)(uint8_t *buffer, const int32_t *values) {
  uint16_t result;
  bench_profile_enter(BENCH_CATEGORY_ENCODING);
  result = REAL_NAME(encode_h)(buffer, values);
  bench_profile_exit();
  return result;
}

extern uint16_t REAL_NAME(decode_h)(int32_t *, const uint8_t *, uint16_t);
uint16_t WRAP_NAME(decode_h)(int32_t *values, const uint8_t *buffer,
                             uint16_t size) {
  uint16_t result;
  bench_profile_enter(BENCH_CATEGORY_ENCODING);
  result = REAL_NAME(decode_h)(values, buffer, size);
  bench_profile_exit();
  return result;
}

extern uint16_t REAL_NAME(encode_hb_z1)(uint8_t *, const int32_t *);
uint16_t WRAP_NAME(encode_hb_z1)(uint8_t *buffer, const int32_t *values) {
  uint16_t result;
  bench_profile_enter(BENCH_CATEGORY_ENCODING);
  result = REAL_NAME(encode_hb_z1)(buffer, values);
  bench_profile_exit();
  return result;
}

extern uint16_t REAL_NAME(decode_hb_z1)(int32_t *, const uint8_t *, uint16_t);
uint16_t WRAP_NAME(decode_hb_z1)(int32_t *values, const uint8_t *buffer,
                                 uint16_t size) {
  uint16_t result;
  bench_profile_enter(BENCH_CATEGORY_ENCODING);
  result = REAL_NAME(decode_hb_z1)(values, buffer, size);
  bench_profile_exit();
  return result;
}

extern void REAL_NAME(pack_vk)(uint8_t[HAETAE_CRYPTO_PUBLICKEYBYTES],
                               polyveck *, const uint8_t[HAETAE_SEEDBYTES]);
void WRAP_NAME(pack_vk)(uint8_t output[HAETAE_CRYPTO_PUBLICKEYBYTES],
                        polyveck *key,
                        const uint8_t seed[HAETAE_SEEDBYTES]) {
  bench_profile_enter(BENCH_CATEGORY_PACKING);
  REAL_NAME(pack_vk)(output, key, seed);
  bench_profile_exit();
}

extern void REAL_NAME(unpack_vk)(polyvecl[HAETAE_K],
                                 const uint8_t[HAETAE_CRYPTO_PUBLICKEYBYTES]);
void WRAP_NAME(unpack_vk)(polyvecl matrix[HAETAE_K],
                          const uint8_t input[HAETAE_CRYPTO_PUBLICKEYBYTES]) {
  bench_profile_enter(BENCH_CATEGORY_PACKING);
  REAL_NAME(unpack_vk)(matrix, input);
  bench_profile_exit();
}

extern void REAL_NAME(pack_sk)(uint8_t[HAETAE_CRYPTO_SECRETKEYBYTES],
                               const uint8_t[HAETAE_CRYPTO_PUBLICKEYBYTES],
                               const polyvecm *, const polyveck *,
                               const uint8_t[HAETAE_SEEDBYTES]);
void WRAP_NAME(pack_sk)(uint8_t output[HAETAE_CRYPTO_SECRETKEYBYTES],
                        const uint8_t key[HAETAE_CRYPTO_PUBLICKEYBYTES],
                        const polyvecm *s0, const polyveck *s1,
                        const uint8_t seed[HAETAE_SEEDBYTES]) {
  bench_profile_enter(BENCH_CATEGORY_PACKING);
  REAL_NAME(pack_sk)(output, key, s0, s1, seed);
  bench_profile_exit();
}

extern void REAL_NAME(unpack_sk)(polyvecl[HAETAE_K], polyvecm *, polyveck *,
                                 uint8_t *,
                                 const uint8_t[HAETAE_CRYPTO_SECRETKEYBYTES]);
void WRAP_NAME(unpack_sk)(polyvecl matrix[HAETAE_K], polyvecm *s0,
                          polyveck *s1, uint8_t *key,
                          const uint8_t input[HAETAE_CRYPTO_SECRETKEYBYTES]) {
  bench_profile_enter(BENCH_CATEGORY_PACKING);
  REAL_NAME(unpack_sk)(matrix, s0, s1, key, input);
  bench_profile_exit();
}

extern int REAL_NAME(pack_sig)(uint8_t[HAETAE_CRYPTO_BYTES], const poly *,
                               const polyvecl *, const polyvecl *,
                               const polyveck *);
int WRAP_NAME(pack_sig)(uint8_t output[HAETAE_CRYPTO_BYTES], const poly *c,
                        const polyvecl *low, const polyvecl *high,
                        const polyveck *hint) {
  int result;
  bench_profile_enter(BENCH_CATEGORY_PACKING);
  result = REAL_NAME(pack_sig)(output, c, low, high, hint);
  bench_profile_exit();
  return result;
}

extern int REAL_NAME(unpack_sig)(poly *, polyvecl *, polyvecl *, polyveck *,
                                 const uint8_t[HAETAE_CRYPTO_BYTES]);
int WRAP_NAME(unpack_sig)(poly *c, polyvecl *low, polyvecl *high,
                          polyveck *hint,
                          const uint8_t input[HAETAE_CRYPTO_BYTES]) {
  int result;
  bench_profile_enter(BENCH_CATEGORY_PACKING);
  result = REAL_NAME(unpack_sig)(c, low, high, hint, input);
  bench_profile_exit();
  return result;
}

extern void REAL_NAME(pack_vec_highbits)(uint8_t *, const polyveck *);
void WRAP_NAME(pack_vec_highbits)(uint8_t *output, const polyveck *values) {
  bench_profile_enter(BENCH_CATEGORY_PACKING);
  REAL_NAME(pack_vec_highbits)(output, values);
  bench_profile_exit();
}

extern void REAL_NAME(pack_poly_lsb)(uint8_t *, const poly *);
void WRAP_NAME(pack_poly_lsb)(uint8_t *output, const poly *values) {
  bench_profile_enter(BENCH_CATEGORY_PACKING);
  REAL_NAME(pack_poly_lsb)(output, values);
  bench_profile_exit();
}

extern void REAL_NAME(fft_init_and_bitrev)(complex_fp32_16[FFT_N],
                                           const poly *);
void WRAP_NAME(fft_init_and_bitrev)(complex_fp32_16 output[FFT_N],
                                    const poly *input) {
  bench_profile_enter(BENCH_CATEGORY_FFT_FIXPOINT);
  REAL_NAME(fft_init_and_bitrev)(output, input);
  bench_profile_exit();
}

PROFILE_VOID1(fft, BENCH_CATEGORY_FFT_FIXPOINT, complex_fp32_16 *, data)

extern int32_t REAL_NAME(complex_fp_sqabs)(complex_fp32_16);
int32_t WRAP_NAME(complex_fp_sqabs)(complex_fp32_16 value) {
  int32_t result;
  bench_profile_enter(BENCH_CATEGORY_FFT_FIXPOINT);
  result = REAL_NAME(complex_fp_sqabs)(value);
  bench_profile_exit();
  return result;
}

extern int64_t REAL_NAME(polyvecmk_sk_singular_value)(const polyvecm *,
                                                      const polyveck *);
int64_t WRAP_NAME(polyvecmk_sk_singular_value)(const polyvecm *s0,
                                               const polyveck *s1) {
  int64_t result;
  bench_profile_enter(BENCH_CATEGORY_FFT_FIXPOINT);
  result = REAL_NAME(polyvecmk_sk_singular_value)(s0, s1);
  bench_profile_exit();
  return result;
}

extern void REAL_NAME(fixpoint_square)(fp96_76 *, const fp96_76 *);
void WRAP_NAME(fixpoint_square)(fp96_76 *output, const fp96_76 *input) {
  bench_profile_enter(BENCH_CATEGORY_FFT_FIXPOINT);
  REAL_NAME(fixpoint_square)(output, input);
  bench_profile_exit();
}

extern void REAL_NAME(fixpoint_newton_invsqrt)(fp96_76 *, const fp96_76 *);
void WRAP_NAME(fixpoint_newton_invsqrt)(fp96_76 *output,
                                        const fp96_76 *input) {
  bench_profile_enter(BENCH_CATEGORY_FFT_FIXPOINT);
  REAL_NAME(fixpoint_newton_invsqrt)(output, input);
  bench_profile_exit();
}

extern int32_t REAL_NAME(fixpoint_mul_rnd13)(uint64_t, const fp96_76 *,
                                             uint8_t);
int32_t WRAP_NAME(fixpoint_mul_rnd13)(uint64_t value, const fp96_76 *factor,
                                      uint8_t sign) {
  int32_t result;
  bench_profile_enter(BENCH_CATEGORY_FFT_FIXPOINT);
  result = REAL_NAME(fixpoint_mul_rnd13)(value, factor, sign);
  bench_profile_exit();
  return result;
}

extern void REAL_NAME(fixpoint_add)(fp96_76 *, const fp96_76 *,
                                    const fp96_76 *);
void WRAP_NAME(fixpoint_add)(fp96_76 *output, const fp96_76 *left,
                             const fp96_76 *right) {
  bench_profile_enter(BENCH_CATEGORY_FFT_FIXPOINT);
  REAL_NAME(fixpoint_add)(output, left, right);
  bench_profile_exit();
}
#elif defined(BENCH_FAMILY_smaugt)
extern void REAL_NAME(poly_mul_acc)(const int16_t[SMAUGT_N],
                                    const int16_t[SMAUGT_N],
                                    int16_t[SMAUGT_N]);
void WRAP_NAME(poly_mul_acc)(const int16_t left[SMAUGT_N],
                             const int16_t right[SMAUGT_N],
                             int16_t result[SMAUGT_N]) {
  bench_profile_enter(BENCH_CATEGORY_MULTIPLICATION);
  REAL_NAME(poly_mul_acc)(left, right, result);
  bench_profile_exit();
}

extern void REAL_NAME(poly_mul_acc_masked)(const int16_t[SMAUGT_N],
                                           const int16_t[SMAUGT_N],
                                           int16_t[SMAUGT_N], uint16_t);
void WRAP_NAME(poly_mul_acc_masked)(const int16_t left[SMAUGT_N],
                                    const int16_t right[SMAUGT_N],
                                    int16_t result[SMAUGT_N], uint16_t mask) {
  bench_profile_enter(BENCH_CATEGORY_MULTIPLICATION);
  REAL_NAME(poly_mul_acc_masked)(left, right, result, mask);
  bench_profile_exit();
}

/*
 * The selected cached SMAUG-T matrix paths call an internal multiplication
 * helper directly, bypassing poly_mul_acc_masked.  Wrap their public
 * fixed-shape entry points as well so the exclusive profiler attributes the
 * complete multiplication path consistently across build variants.
 */
extern void REAL_NAME(vec_vec_mult_add)(poly *, const polyvec *,
                                        const polyvec *, uint8_t);
void WRAP_NAME(vec_vec_mult_add)(poly *result, const polyvec *left,
                                 const polyvec *right, uint8_t mod) {
  bench_profile_enter(BENCH_CATEGORY_MULTIPLICATION);
  REAL_NAME(vec_vec_mult_add)(result, left, right, mod);
  bench_profile_exit();
}

extern void REAL_NAME(matrix_vec_mult_add)(polyvec *,
                                           const polyvec[SMAUGT_K],
                                           const polyvec *);
void WRAP_NAME(matrix_vec_mult_add)(polyvec *result,
                                    const polyvec matrix[SMAUGT_K],
                                    const polyvec *vector) {
  bench_profile_enter(BENCH_CATEGORY_MULTIPLICATION);
  REAL_NAME(matrix_vec_mult_add)(result, matrix, vector);
  bench_profile_exit();
}

extern void REAL_NAME(matrix_vec_mult_sub)(polyvec *,
                                           const polyvec[SMAUGT_K],
                                           const polyvec *);
void WRAP_NAME(matrix_vec_mult_sub)(polyvec *result,
                                    const polyvec matrix[SMAUGT_K],
                                    const polyvec *vector) {
  bench_profile_enter(BENCH_CATEGORY_MULTIPLICATION);
  REAL_NAME(matrix_vec_mult_sub)(result, matrix, vector);
  bench_profile_exit();
}

extern int REAL_NAME(hwt)(int16_t *, const uint8_t *);
int WRAP_NAME(hwt)(int16_t *result, const uint8_t *seed) {
  int status;
  bench_profile_enter(BENCH_CATEGORY_SAMPLER);
  status = REAL_NAME(hwt)(result, seed);
  bench_profile_exit();
  return status;
}

extern void REAL_NAME(sp_cbd)(poly *, const uint8_t[SMAUGT_CBDSEED_BYTES]);
void WRAP_NAME(sp_cbd)(poly *result,
                       const uint8_t seed[SMAUGT_CBDSEED_BYTES]) {
  bench_profile_enter(BENCH_CATEGORY_SAMPLER);
  REAL_NAME(sp_cbd)(result, seed);
  bench_profile_exit();
}

extern void REAL_NAME(d_gaussian)(polyvec *,
                                  const uint8_t[SMAUGT_CRYPTO_BYTES]);
void WRAP_NAME(d_gaussian)(polyvec *result,
                           const uint8_t seed[SMAUGT_CRYPTO_BYTES]) {
  bench_profile_enter(BENCH_CATEGORY_SAMPLER);
  REAL_NAME(d_gaussian)(result, seed);
  bench_profile_exit();
}

extern void REAL_NAME(pack_enck)(uint8_t *, const public_key *);
void WRAP_NAME(pack_enck)(uint8_t *output, const public_key *key) {
  bench_profile_enter(BENCH_CATEGORY_PACKING);
  REAL_NAME(pack_enck)(output, key);
  bench_profile_exit();
}

extern void REAL_NAME(unpack_enck)(public_key *, const uint8_t *);
void WRAP_NAME(unpack_enck)(public_key *key, const uint8_t *input) {
  bench_profile_enter(BENCH_CATEGORY_PACKING);
  REAL_NAME(unpack_enck)(key, input);
  bench_profile_exit();
}

extern void REAL_NAME(pack_deck)(uint8_t *, const secret_key *);
void WRAP_NAME(pack_deck)(uint8_t *output, const secret_key *key) {
  bench_profile_enter(BENCH_CATEGORY_PACKING);
  REAL_NAME(pack_deck)(output, key);
  bench_profile_exit();
}

extern void REAL_NAME(unpack_deck)(secret_key *, const uint8_t *);
void WRAP_NAME(unpack_deck)(secret_key *key, const uint8_t *input) {
  bench_profile_enter(BENCH_CATEGORY_PACKING);
  REAL_NAME(unpack_deck)(key, input);
  bench_profile_exit();
}

extern void REAL_NAME(pack_ct)(uint8_t *, const ciphertext *);
void WRAP_NAME(pack_ct)(uint8_t *output, const ciphertext *ciphertext_value) {
  bench_profile_enter(BENCH_CATEGORY_PACKING);
  REAL_NAME(pack_ct)(output, ciphertext_value);
  bench_profile_exit();
}

extern void REAL_NAME(unpack_ct)(ciphertext *, const uint8_t *);
void WRAP_NAME(unpack_ct)(ciphertext *ciphertext_value, const uint8_t *input) {
  bench_profile_enter(BENCH_CATEGORY_PACKING);
  REAL_NAME(unpack_ct)(ciphertext_value, input);
  bench_profile_exit();
}

#define PROFILE_PACK_RING(name)                                                \
  extern void REAL_NAME(name)(uint8_t *, const poly *);                        \
  void WRAP_NAME(name)(uint8_t *output, const poly *input) {                   \
    bench_profile_enter(BENCH_CATEGORY_PACKING);                               \
    REAL_NAME(name)(output, input);                                            \
    bench_profile_exit();                                                      \
  }

#define PROFILE_UNPACK_RING(name)                                              \
  extern void REAL_NAME(name)(poly *, const uint8_t *);                        \
  void WRAP_NAME(name)(poly *output, const uint8_t *input) {                   \
    bench_profile_enter(BENCH_CATEGORY_PACKING);                               \
    REAL_NAME(name)(output, input);                                            \
    bench_profile_exit();                                                      \
  }

PROFILE_PACK_RING(PACK_RING(SMAUGT_LOG_Q))
PROFILE_UNPACK_RING(UNPACK_RING(SMAUGT_LOG_Q))
PROFILE_PACK_RING(PACK_RING(SMAUGT_LOG_P))
PROFILE_UNPACK_RING(UNPACK_RING(SMAUGT_LOG_P))
PROFILE_PACK_RING(PACK_RING(SMAUGT_LOG_P_PRIME))
PROFILE_UNPACK_RING(UNPACK_RING(SMAUGT_LOG_P_PRIME))
#elif defined(BENCH_FAMILY_aimer)
extern void REAL_NAME(gf_mul)(gf, const gf, const gf);
void WRAP_NAME(gf_mul)(gf output, const gf left, const gf right) {
  bench_profile_enter(BENCH_CATEGORY_MULTIPLICATION);
  REAL_NAME(gf_mul)(output, left, right);
  bench_profile_exit();
}

extern void REAL_NAME(gf_mul_add)(gf, const gf, const gf);
void WRAP_NAME(gf_mul_add)(gf output, const gf left, const gf right) {
  bench_profile_enter(BENCH_CATEGORY_MULTIPLICATION);
  REAL_NAME(gf_mul_add)(output, left, right);
  bench_profile_exit();
}

extern void REAL_NAME(gf_sqr)(gf, const gf);
void WRAP_NAME(gf_sqr)(gf output, const gf input) {
  bench_profile_enter(BENCH_CATEGORY_MULTIPLICATION);
  REAL_NAME(gf_sqr)(output, input);
  bench_profile_exit();
}

extern void REAL_NAME(gf_mat_vec_mul)(
    gf, const gf, const gf[AIM2_NUM_BITS_FIELD]);
void WRAP_NAME(gf_mat_vec_mul)(gf output, const gf input,
                               const gf matrix[AIM2_NUM_BITS_FIELD]) {
  bench_profile_enter(BENCH_CATEGORY_MULTIPLICATION);
  REAL_NAME(gf_mat_vec_mul)(output, input, matrix);
  bench_profile_exit();
}

extern void REAL_NAME(gf_mat_vec_mul_add)(
    gf, const gf, const gf[AIM2_NUM_BITS_FIELD]);
void WRAP_NAME(gf_mat_vec_mul_add)(gf output, const gf input,
                                   const gf matrix[AIM2_NUM_BITS_FIELD]) {
  bench_profile_enter(BENCH_CATEGORY_MULTIPLICATION);
  REAL_NAME(gf_mat_vec_mul_add)(output, input, matrix);
  bench_profile_exit();
}
#elif defined(BENCH_FAMILY_ntruplus)
extern void REAL_NAME(poly_ntt)(poly *);
void WRAP_NAME(poly_ntt)(poly *value) {
  bench_profile_enter(BENCH_CATEGORY_MULTIPLICATION);
  REAL_NAME(poly_ntt)(value);
  bench_profile_exit();
}

extern void REAL_NAME(poly_invntt)(poly *);
void WRAP_NAME(poly_invntt)(poly *value) {
  bench_profile_enter(BENCH_CATEGORY_MULTIPLICATION);
  REAL_NAME(poly_invntt)(value);
  bench_profile_exit();
}

extern int REAL_NAME(poly_baseinv)(poly *, const poly *);
int WRAP_NAME(poly_baseinv)(poly *output, const poly *input) {
  int result;
  bench_profile_enter(BENCH_CATEGORY_MULTIPLICATION);
  result = REAL_NAME(poly_baseinv)(output, input);
  bench_profile_exit();
  return result;
}

extern void REAL_NAME(poly_basemul)(poly *, const poly *, const poly *);
void WRAP_NAME(poly_basemul)(poly *output, const poly *left,
                             const poly *right) {
  bench_profile_enter(BENCH_CATEGORY_MULTIPLICATION);
  REAL_NAME(poly_basemul)(output, left, right);
  bench_profile_exit();
}

extern void REAL_NAME(poly_basemul_add)(poly *, const poly *, const poly *,
                                        const poly *);
void WRAP_NAME(poly_basemul_add)(poly *output, const poly *left,
                                 const poly *right, const poly *addend) {
  bench_profile_enter(BENCH_CATEGORY_MULTIPLICATION);
  REAL_NAME(poly_basemul_add)(output, left, right, addend);
  bench_profile_exit();
}
#endif

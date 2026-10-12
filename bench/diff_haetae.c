#include "diff_test.h"

#include "api.h"
#include "fft.h"
#include "fixpoint.h"
#include "ntt.h"
#include "platform.h"
#include "poly.h"
#include "polyvec.h"
#include "sampler.h"
#include "string.h"

#if defined(HAETAE_ACC_AFTERSUM_DMAX_TIGHT_PRECENTER)
#include "haetae_ntt_mve.h"
#endif
#if defined(HAETAE_MVE_FFT)
#include "haetae_fft_mve.h"
#endif
#if defined(HAETAE_SAMPLER_CDT_MVE) || defined(HAETAE_SAMPLER_LOAD_MVE)
#include "haetae_sampler_mve.h"
#endif

#include <stddef.h>
#include <stdint.h>

#define DIFF_CASES 1000u
#define CLEAN_NAME_INNER(name) clean_##name
#define CLEAN_NAME(name) CLEAN_NAME_INNER(name)

extern void CLEAN_NAME(ntt)(int32_t[HAETAE_N]);
extern void CLEAN_NAME(invntt_tomont)(int32_t[HAETAE_N]);
extern void CLEAN_NAME(poly_pointwise_montgomery)(poly *, const poly *,
                                                   const poly *);
extern void CLEAN_NAME(polyvecl_pointwise_acc_montgomery)(
    poly *, const polyvecl *, const polyvecl *);
extern void CLEAN_NAME(poly_reduce2q)(poly *);
extern void CLEAN_NAME(poly_freeze2q)(poly *);
extern void CLEAN_NAME(poly_freeze)(poly *);
extern void CLEAN_NAME(fft_init_and_bitrev)(complex_fp32_16[FFT_N],
                                            const poly *);
extern void CLEAN_NAME(fft)(complex_fp32_16[FFT_N]);
extern int32_t CLEAN_NAME(complex_fp_sqabs)(complex_fp32_16);
extern void CLEAN_NAME(sample_gauss_N)(
    uint64_t *, uint8_t *, fp96_76 *,
    const uint8_t[HAETAE_CRHBYTES], uint16_t, size_t);
extern int CLEAN_NAME(crypto_sign_keypair_internal)(
    uint8_t *, uint8_t *, uint8_t[HAETAE_SEEDBYTES]);
extern int CLEAN_NAME(crypto_sign_signature_internal)(
    uint8_t *, size_t *, const uint8_t *, size_t, const uint8_t *, size_t,
    const uint8_t[HAETAE_SEEDBYTES], const uint8_t *);
extern int CLEAN_NAME(crypto_sign_verify_internal)(
    const uint8_t *, size_t, const uint8_t *, size_t, const uint8_t *, size_t,
    const uint8_t *);

static poly input_left;
static poly input_right;
static poly clean_output;
static poly m55_output;
static uint32_t random_state = 0x9E3779B9u;
static uint8_t kat_clean_public_key[CRYPTO_PUBLICKEYBYTES];
static uint8_t kat_m55_public_key[CRYPTO_PUBLICKEYBYTES];
static uint8_t kat_clean_secret_key[CRYPTO_SECRETKEYBYTES];
static uint8_t kat_m55_secret_key[CRYPTO_SECRETKEYBYTES];
static uint8_t kat_clean_signature[CRYPTO_BYTES];
static uint8_t kat_m55_signature[CRYPTO_BYTES];
#if defined(HAETAE_MVE_FFT)
static int32_t mul_x[4];
static int32_t mul_y[4];
static int32_t mul_wide[4];
static int32_t mul_qrd[4];
#endif

static uint32_t test_random(void) {
  uint32_t value = random_state;
  value ^= value << 13;
  value ^= value >> 17;
  value ^= value << 5;
  random_state = value;
  return value;
}

static int32_t canonical(int32_t value) {
  value %= HAETAE_Q;
  if (value < 0) {
    value += HAETAE_Q;
  }
  return value;
}

static int equal_mod_q(const poly *left, const poly *right) {
  for (size_t i = 0; i < HAETAE_N; i++) {
    if (canonical(left->coeffs[i]) != canonical(right->coeffs[i])) {
      return 0;
    }
  }
  return 1;
}

static int equal_exact_poly(const poly *left, const poly *right) {
  return memcmp(left, right, sizeof(*left)) == 0;
}

#if defined(HAETAE_NTT_ASM)
static uint64_t hash_exact_poly(uint64_t state, const poly *value) {
  for (size_t i = 0; i < HAETAE_N; i++) {
    state ^= (uint32_t)value->coeffs[i];
    state *= UINT64_C(1099511628211);
  }
  return state;
}
#endif

static void random_poly(poly *value) {
  for (size_t i = 0; i < HAETAE_N; i++) {
    value->coeffs[i] =
        (int32_t)(test_random() % HAETAE_Q) - HAETAE_Q / 2;
  }
}

static void random_fft_poly(poly *value) {
  for (size_t i = 0; i < HAETAE_N; i++) {
    value->coeffs[i] = (int32_t)(test_random() % 7u) - 3;
  }
}

static int32_t mulrnd16_reference(int32_t x, int32_t y) {
  return (int32_t)((((int64_t)x * y) + (1 << 15)) >> 16);
}

int bench_fft_preflight(void) {
  complex_fp32_16 clean_fft_output[FFT_N];
  complex_fp32_16 m55_fft_output[FFT_N];

  for (size_t i = 0; i < HAETAE_N; i++) {
    input_left.coeffs[i] = (int32_t)(i % 7u) - 3;
  }
  CLEAN_NAME(fft_init_and_bitrev)(clean_fft_output, &input_left);
  fft_init_and_bitrev(m55_fft_output, &input_left);
  if (memcmp(clean_fft_output, m55_fft_output, sizeof(clean_fft_output)) != 0) {
    return 108;
  }
  CLEAN_NAME(fft)(clean_fft_output);
  fft(m55_fft_output);
  return memcmp(clean_fft_output, m55_fft_output, sizeof(clean_fft_output)) == 0
             ? 0
             : 109;
}

#if defined(HAETAE_SAMPLER_CDT_MVE)
static uint64_t gauss16_cdt_reference(uint16_t random16) {
  static const uint16_t cdt16[64] = {
      3266,  6520,  9748,  12938, 16079, 19159, 22168, 25096,
      27934, 30674, 33309, 35833, 38241, 40531, 42698, 44742,
      46663, 48460, 50135, 51690, 53128, 54454, 55670, 56781,
      57794, 58712, 59541, 60287, 60956, 61554, 62085, 62556,
      62972, 63337, 63657, 63936, 64178, 64388, 64569, 64724,
      64857, 64970, 65066, 65148, 65216, 65273, 65321, 65361,
      65394, 65422, 65444, 65463, 65478, 65490, 65500, 65508,
      65514, 65519, 65523, 65527, 65529, 65531, 65533, 65534};
  uint64_t result = 0;
  for (size_t i = 0; i < 64; i++) {
    result += random16 > cdt16[i];
  }
  return result;
}


static int gauss16_cdt_exhaustive_test(void) {
  for (uint32_t input = 0; input <= UINT16_MAX; input++) {
    const uint64_t expected = gauss16_cdt_reference((uint16_t)input);
    const uint64_t actual =
        haetae_sample_gauss16_cdt_mve((uint16_t)input);
    if (actual != expected) {
      platform_write("CDT_EXHAUSTIVE,status=failed,input=");
      platform_write_u32(input);
      platform_write(",expected=");
      platform_write_u64(expected);
      platform_write(",actual=");
      platform_write_u64(actual);
      platform_write("\n");
      return 131;
    }
  }
  platform_write("CDT_EXHAUSTIVE,inputs=65536,status=ok\n");
  return 0;
}
#endif

int bench_kat_test(void) {
  uint8_t seed[HAETAE_SEEDBYTES];
  uint8_t randomness[HAETAE_SEEDBYTES];
  uint8_t message[32];
  size_t clean_signature_length = 0u;
  size_t m55_signature_length = 0u;

  for (size_t i = 0; i < sizeof(seed); i++) {
    seed[i] = (uint8_t)i;
    randomness[i] = (uint8_t)(0xA5u ^ i);
  }
  for (size_t i = 0; i < sizeof(message); i++) {
    message[i] = (uint8_t)(3u * i + 1u);
  }

  platform_heap_reset();
  if (CLEAN_NAME(crypto_sign_keypair_internal)(kat_clean_public_key,
                                                kat_clean_secret_key,
                                                seed) != 0) {
    return 111;
  }
  platform_heap_reset();
  if (crypto_sign_keypair_internal(kat_m55_public_key, kat_m55_secret_key,
                                   seed) != 0) {
    return 112;
  }
  if (memcmp(kat_clean_public_key, kat_m55_public_key,
             sizeof(kat_clean_public_key)) != 0 ||
      memcmp(kat_clean_secret_key, kat_m55_secret_key,
             sizeof(kat_clean_secret_key)) != 0) {
    return 113;
  }

  platform_heap_reset();
  if (CLEAN_NAME(crypto_sign_signature_internal)(
          kat_clean_signature, &clean_signature_length, message,
          sizeof(message), NULL, 0u, randomness, kat_clean_secret_key) != 0) {
    return 114;
  }
  platform_heap_reset();
  if (crypto_sign_signature_internal(
          kat_m55_signature, &m55_signature_length, message, sizeof(message),
          NULL, 0u, randomness, kat_m55_secret_key) != 0) {
    return 115;
  }
  if (clean_signature_length != m55_signature_length ||
      memcmp(kat_clean_signature, kat_m55_signature,
             clean_signature_length) != 0) {
    return 116;
  }

  platform_heap_reset();
  if (CLEAN_NAME(crypto_sign_verify_internal)(
          kat_clean_signature, clean_signature_length, message,
          sizeof(message), NULL, 0u, kat_clean_public_key) != 0) {
    return 117;
  }
  platform_heap_reset();
  if (crypto_sign_verify_internal(kat_m55_signature, m55_signature_length,
                                  message, sizeof(message), NULL, 0u,
                                  kat_m55_public_key) != 0) {
    return 118;
  }
  return 0;
}

int bench_diff_test(void) {
  polyvecl input_acc_left;
  polyvecl input_acc_right;
  complex_fp32_16 clean_fft_output[FFT_N];
  complex_fp32_16 m55_fft_output[FFT_N];
  uint64_t clean_gauss[HAETAE_N];
  uint64_t m55_gauss[HAETAE_N];
  uint8_t clean_signs[HAETAE_N / 8];
  uint8_t m55_signs[HAETAE_N / 8];
  uint8_t gauss_seed[HAETAE_CRHBYTES];
  fp96_76 clean_sqsum;
  fp96_76 m55_sqsum;
#if defined(HAETAE_FFT_FUSED)
  int32_t clean_fft_sum[FFT_N];
  int32_t m55_fft_sum[FFT_N];
#endif
#if defined(HAETAE_NTT_ASM)
  uint64_t reduce2q_digest = UINT64_C(1469598103934665603);
  uint64_t freeze2q_digest = UINT64_C(1469598103934665603);
  uint64_t freeze_digest = UINT64_C(1469598103934665603);
#endif

#if defined(HAETAE_SAMPLER_CDT_MVE)
  {
    const int exhaustive_result = gauss16_cdt_exhaustive_test();
    if (exhaustive_result != 0) {
      return exhaustive_result;
    }
  }
#endif

  random_state = 0x9E3779B9u;
  for (size_t test = 0; test < DIFF_CASES; test++) {
    int hash_result = bench_fips202_diff_case((unsigned int)test);
    if (hash_result != 0) {
      return hash_result;
    }

#if defined(HAETAE_SAMPLER_LOAD_MVE)
    {
      uint8_t guarded[19];
      guarded[0] = 0xA5u;
      guarded[18] = 0x5Au;
      for (size_t i = 0; i < 17; i++) {
        guarded[i + 1] = (uint8_t)test_random();
      }
      const haetae_gauss17_fields fields =
          haetae_load_gauss17_mve(guarded + 1);
      const uint32_t w0 = (uint32_t)guarded[1] |
                          ((uint32_t)guarded[2] << 8) |
                          ((uint32_t)guarded[3] << 16) |
                          ((uint32_t)guarded[4] << 24);
      const uint32_t w1 = (uint32_t)guarded[5] |
                          ((uint32_t)guarded[6] << 8) |
                          ((uint32_t)guarded[7] << 16) |
                          ((uint32_t)guarded[8] << 24);
      const uint32_t w2 = (uint32_t)guarded[9] |
                          ((uint32_t)guarded[10] << 8) |
                          ((uint32_t)guarded[11] << 16) |
                          ((uint32_t)guarded[12] << 24);
      const uint32_t w3 = (uint32_t)guarded[13] |
                          ((uint32_t)guarded[14] << 8) |
                          ((uint32_t)guarded[15] << 16) |
                          ((uint32_t)guarded[16] << 24);
      if (guarded[0] != 0xA5u || guarded[18] != 0x5Au ||
          fields.gauss16 != (uint16_t)w0 ||
          fields.rejection48 !=
              ((uint64_t)(w0 >> 16) | ((uint64_t)w1 << 16)) ||
          fields.y_low48 !=
              ((uint64_t)w2 | ((uint64_t)(w3 & 0xFFFFu) << 32)) ||
          fields.y_high24 !=
              ((w3 >> 16) | ((uint32_t)guarded[17] << 16))) {
        return 123;
      }
    }
#endif

#if defined(HAETAE_MVE_FFT)
    for (size_t lane = 0; lane < 4; lane++) {
      mul_x[lane] = (int32_t)(test_random() & 0x003FFFFFu) - 0x00200000;
      mul_y[lane] = (int32_t)(test_random() % 131072u) - 65536;
    }
    if (test == 0u) {
      mul_x[0] = 0;
      mul_x[1] = 1;
      mul_x[2] = -1;
      mul_x[3] = 0x001FFFFF;
      mul_y[0] = -65536;
      mul_y[1] = -65535;
      mul_y[2] = 65535;
      mul_y[3] = 46341;
    }
    haetae_mulrnd16_wide4(mul_wide, mul_x, mul_y);
    haetae_mulrnd16_qrd4(mul_qrd, mul_x, mul_y);
    for (size_t lane = 0; lane < 4; lane++) {
      int32_t expected = mulrnd16_reference(mul_x[lane], mul_y[lane]);
      if (mul_wide[lane] != expected) {
        return 106;
      }
      if (mul_qrd[lane] != expected) {
        return 107;
      }
    }
#endif

    random_fft_poly(&input_left);
    CLEAN_NAME(fft_init_and_bitrev)(clean_fft_output, &input_left);
    fft_init_and_bitrev(m55_fft_output, &input_left);
    if (memcmp(clean_fft_output, m55_fft_output,
               sizeof(clean_fft_output)) != 0) {
      return 108;
    }
    CLEAN_NAME(fft)(clean_fft_output);
    fft(m55_fft_output);
    if (memcmp(clean_fft_output, m55_fft_output,
               sizeof(clean_fft_output)) != 0) {
      return 109;
    }
    for (size_t i = 0; i < FFT_N; i++) {
      if (CLEAN_NAME(complex_fp_sqabs)(clean_fft_output[i]) !=
          complex_fp_sqabs(m55_fft_output[i])) {
        return 110;
      }
    }
#if defined(HAETAE_FFT_FUSED)
    for (size_t i = 0; i < FFT_N; i++) {
      clean_fft_sum[i] = (int32_t)(i & 7u);
      m55_fft_sum[i] = clean_fft_sum[i];
      clean_fft_sum[i] +=
          CLEAN_NAME(complex_fp_sqabs)(clean_fft_output[i]);
    }
    fft_init_and_bitrev(m55_fft_output, &input_left);
    fft_sqabs_acc(m55_fft_sum, m55_fft_output);
    if (memcmp(clean_fft_sum, m55_fft_sum, sizeof(clean_fft_sum)) != 0) {
      return 120;
    }
#endif

    for (size_t i = 0; i < sizeof(gauss_seed); i++) {
      gauss_seed[i] = (uint8_t)test_random();
    }
    memset(&clean_sqsum, 0, sizeof(clean_sqsum));
    memset(&m55_sqsum, 0, sizeof(m55_sqsum));
    platform_heap_reset();
    CLEAN_NAME(sample_gauss_N)(clean_gauss, clean_signs, &clean_sqsum,
                               gauss_seed, (uint16_t)test, HAETAE_N);
    platform_heap_reset();
    sample_gauss_N(m55_gauss, m55_signs, &m55_sqsum, gauss_seed,
                   (uint16_t)test, HAETAE_N);
    if (memcmp(clean_gauss, m55_gauss, sizeof(clean_gauss)) != 0 ||
        memcmp(clean_signs, m55_signs, sizeof(clean_signs)) != 0 ||
        memcmp(&clean_sqsum, &m55_sqsum, sizeof(clean_sqsum)) != 0) {
      return 121;
    }

    random_poly(&input_left);
    clean_output = input_left;
    m55_output = input_left;
    CLEAN_NAME(ntt)(clean_output.coeffs);
    ntt(m55_output.coeffs);
    if (!equal_mod_q(&clean_output, &m55_output)) {
      return 101;
    }

    random_poly(&input_left);
    clean_output = input_left;
    m55_output = input_left;
    CLEAN_NAME(invntt_tomont)(clean_output.coeffs);
    invntt_tomont(m55_output.coeffs);
    if (!equal_mod_q(&clean_output, &m55_output)) {
      return 102;
    }

#if defined(HAETAE_NTT_ASM)
    /* These six public names are ABI/dispatch aliases of one selected paired
     * assembly dataflow, not six independent assembly candidates.  Iterating
     * them checks symbol presence and routing only; it adds no dataflow-level
     * correctness coverage beyond the common implementation. */
    if (test < 16u) {
      static void (*const forward_variants[])(int32_t *) = {
          haetae_ntt_mve_l44,          haetae_ntt_mve_l332,
          haetae_ntt_mve_l332_vld2,    haetae_ntt_mve_l332_vld4,
          haetae_ntt_mve_l332_vld4_m3, haetae_ntt_mve_l332_vld4_u2};
      static void (*const inverse_variants[])(int32_t *) = {
          haetae_invntt_mve_l44,          haetae_invntt_mve_l332,
          haetae_invntt_mve_l332_vld2,    haetae_invntt_mve_l332_vld4,
          haetae_invntt_mve_l332_vld4_m3, haetae_invntt_mve_l332_vld4_u2};
      random_poly(&input_left);
      clean_output = input_left;
      CLEAN_NAME(ntt)(clean_output.coeffs);
      for (size_t variant = 0;
           variant < sizeof(forward_variants) / sizeof(forward_variants[0]);
           variant++) {
        m55_output = input_left;
        forward_variants[variant](m55_output.coeffs);
        if (!equal_mod_q(&clean_output, &m55_output)) {
          return 124;
        }
      }

      random_poly(&input_left);
      clean_output = input_left;
      CLEAN_NAME(invntt_tomont)(clean_output.coeffs);
      for (size_t variant = 0;
           variant < sizeof(inverse_variants) / sizeof(inverse_variants[0]);
           variant++) {
        m55_output = input_left;
        inverse_variants[variant](m55_output.coeffs);
        if (!equal_mod_q(&clean_output, &m55_output)) {
          return 125;
        }
        for (size_t i = 0; i < HAETAE_N; i++) {
          if (m55_output.coeffs[i] <= -HAETAE_Q ||
              m55_output.coeffs[i] >= HAETAE_Q) {
            return 126;
          }
        }
      }
    }

    for (size_t i = 0; i < HAETAE_N; i++) {
      input_left.coeffs[i] = (int32_t)test_random();
    }
    clean_output = input_left;
    m55_output = input_left;
    CLEAN_NAME(poly_reduce2q)(&clean_output);
    haetae_reduce2q_mve(m55_output.coeffs);
    if (!equal_exact_poly(&clean_output, &m55_output)) {
      return 127;
    }
    reduce2q_digest = hash_exact_poly(reduce2q_digest, &m55_output);
    clean_output = input_left;
    m55_output = input_left;
    CLEAN_NAME(poly_freeze2q)(&clean_output);
    haetae_freeze2q_mve(m55_output.coeffs);
    if (!equal_exact_poly(&clean_output, &m55_output)) {
      return 128;
    }
    freeze2q_digest = hash_exact_poly(freeze2q_digest, &m55_output);
    clean_output = input_left;
    m55_output = input_left;
    CLEAN_NAME(poly_freeze)(&clean_output);
    haetae_freeze_mve(m55_output.coeffs);
    if (!equal_exact_poly(&clean_output, &m55_output)) {
      return 129;
    }
    freeze_digest = hash_exact_poly(freeze_digest, &m55_output);
#endif

    random_poly(&input_left);
    random_poly(&input_right);
    CLEAN_NAME(poly_pointwise_montgomery)(&clean_output, &input_left,
                                           &input_right);
    poly_pointwise_montgomery(&m55_output, &input_left, &input_right);
    if (!equal_mod_q(&clean_output, &m55_output)) {
      return 103;
    }

    for (size_t term = 0; term < HAETAE_L; term++) {
      random_poly(&input_acc_left.vec[term]);
      random_poly(&input_acc_right.vec[term]);
    }
    CLEAN_NAME(polyvecl_pointwise_acc_montgomery)(
        &clean_output, &input_acc_left, &input_acc_right);
    polyvecl_pointwise_acc_montgomery(&m55_output, &input_acc_left,
                                      &input_acc_right);
    if (!equal_mod_q(&clean_output, &m55_output)) {
      return 104;
    }
#if defined(HAETAE_ACC_AFTERSUM_DMAX_TIGHT_PRECENTER)
    for (size_t term = 0; term < HAETAE_L; term++)
      haetae_barrett_tight_mve(input_acc_right.vec[term].coeffs,
                               input_acc_right.vec[term].coeffs);
    haetae_pointwise_acc_precentered_mve(
        m55_output.coeffs, input_acc_left.vec[0].coeffs,
        input_acc_right.vec[0].coeffs, HAETAE_L);
    if (!equal_mod_q(&clean_output, &m55_output)) {
      return 105;
    }

    /* The compact-A keygen path is not live in every stack configuration, so
     * keep its public leaf covered directly.  Canonical uint16 coefficients
     * are widened to the same clean polynomial for an independent modulo-q
     * result; only the right stream is converted to the documented one-pass
     * precentered representation before the assembly call. */
    for (size_t term = 0; term < HAETAE_L; term++) {
      for (size_t i = 0; i < HAETAE_N; i++) {
        const uint16_t value = (uint16_t)(test_random() % HAETAE_Q);
        input_acc_left.vec[term].coeffs[i] = (int32_t)value;
      }
      random_poly(&input_acc_right.vec[term]);
    }
    CLEAN_NAME(polyvecl_pointwise_acc_montgomery)(
        &clean_output, &input_acc_left, &input_acc_right);
    /* Pack forward into the now-dead full-width left operand.  Destination
     * byte 2*i is never ahead of unread source byte 4*i, and byte-wise memcpy
     * avoids an aliasing assumption. */
    for (size_t term = 0; term < HAETAE_L; term++) {
      for (size_t i = 0; i < HAETAE_N; i++) {
        const size_t index = term * HAETAE_N + i;
        const uint16_t value =
            (uint16_t)input_acc_left.vec[term].coeffs[i];
        memcpy((uint8_t *)(void *)&input_acc_left + 2 * index, &value,
               sizeof(value));
      }
    }
    for (size_t term = 0; term < HAETAE_L; term++) {
      haetae_barrett_tight_mve(input_acc_right.vec[term].coeffs,
                               input_acc_right.vec[term].coeffs);
    }
    haetae_pointwise_acc_u16_precentered_mve(
        m55_output.coeffs, (const uint16_t *)(const void *)&input_acc_left,
        input_acc_right.vec[0].coeffs, HAETAE_L);
    if (!equal_mod_q(&clean_output, &m55_output)) {
      return 130;
    }
#endif
  }
#if defined(HAETAE_NTT_ASM)
  platform_write("REDUCTION_EQUIV_DIGEST,cases=256000,reduce2q=");
  platform_write_u64(reduce2q_digest);
  platform_write(",freeze2q=");
  platform_write_u64(freeze2q_digest);
  platform_write(",freeze=");
  platform_write_u64(freeze_digest);
  platform_write("\n");
#endif
  return 0;
}

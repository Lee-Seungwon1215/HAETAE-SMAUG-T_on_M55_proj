// SPDX-License-Identifier: MIT

#ifndef HAETAE_HYPERBALL_INLINE_H
#define HAETAE_HYPERBALL_INLINE_H

#include <fixpoint.h>
#include <polyfix.h>
#include <sampler.h>
#include <symmetric.h>

#if !defined(HAETAE_STACK_HYPER_OUTSIGN01) ||                                \
    !defined(HAETAE_STACK_GAUSS_OUTBUF02)
#error "inlined raw-scratch hyperball requires OUTSIGN01 and GAUSS_OUTBUF02"
#endif

static inline __attribute__((always_inline)) uint16_t
polyfixveclk_sample_hyperball_rawscratch_inline(
    polyfixvecl *y1, polyfixveck *y2, uint8_t *b,
    const uint8_t seed[HAETAE_CRHBYTES], const uint16_t nonce,
    uint64_t samples[HAETAE_N * (HAETAE_L + HAETAE_K)]) {
  uint16_t ni = nonce;
  fp96_76 sqsum, invsqrt;
  unsigned int i, j;
  enum {
    HYPER_SIGN_BYTES = HAETAE_N * (HAETAE_L + HAETAE_K) / 8
  };
  uint8_t *const signs = (uint8_t *)y1;
  _Static_assert(sizeof(polyfixvecl) >=
                     HYPER_SIGN_BYTES + HAETAE_GAUSS_N_BUFFER_BYTES,
                 "y1 cannot hold signs and the Gaussian buffer");
  uint8_t *const gauss_buf = signs + HYPER_SIGN_BYTES;

  do {
    sqsum.limb48[0] = 0;
    sqsum.limb48[1] = 0;

    sample_gauss_N_outbuf_inline(&samples[0], &signs[0], &sqsum, seed, ni++,
                                 HAETAE_N + 1, gauss_buf);
    sample_gauss_N_outbuf_inline(&samples[HAETAE_N], &signs[HAETAE_N / 8],
                                 &sqsum, seed, ni++, HAETAE_N + 1, gauss_buf);
    for (i = 2; i < HAETAE_L + HAETAE_K; i++)
      sample_gauss_N_outbuf_inline(&samples[HAETAE_N * i],
                                   &signs[HAETAE_N / 8 * i], &sqsum, seed,
                                   ni++, HAETAE_N, gauss_buf);

    sqsum.limb48[0] += 1;
    sqsum.limb48[0] >>= 1;
    sqsum.limb48[0] += (sqsum.limb48[1] & 1) << 47;
    sqsum.limb48[1] >>= 1;
    sqsum.limb48[1] += sqsum.limb48[0] >> 48;
    sqsum.limb48[0] &= (1ULL << 48) - 1;
    fixpoint_newton_invsqrt(&invsqrt, &sqsum);
    fixpoint_mul_high(&sqsum, &invsqrt,
                      (uint64_t)(HAETAE_B0 * HAETAE_LN + HAETAE_SQNM / 2)
                          << (28 - 13));

    /* Consume sign bytes in reverse order so output writes overwrite only
     * sign bytes that have already been loaded. */
    for (i = HAETAE_K; i-- > 0;) {
      for (j = HAETAE_N / 8; j-- > 0;) {
        const unsigned int sign_index =
            (HAETAE_L + i) * (HAETAE_N / 8) + j;
        const uint8_t sign_byte = signs[sign_index];
        for (unsigned int bit = 8; bit-- > 0;) {
          const unsigned int coefficient = 8 * j + bit;
          const unsigned int sample_index =
              (HAETAE_L + i) * HAETAE_N + coefficient;
          y2->vec[i].coeffs[coefficient] = fixpoint_mul_rnd13(
              samples[sample_index], &sqsum, (sign_byte >> bit) & 1);
        }
      }
    }
    for (i = HAETAE_L; i-- > 0;) {
      for (j = HAETAE_N / 8; j-- > 0;) {
        const unsigned int sign_index = i * (HAETAE_N / 8) + j;
        const uint8_t sign_byte = signs[sign_index];
        for (unsigned int bit = 8; bit-- > 0;) {
          const unsigned int coefficient = 8 * j + bit;
          const unsigned int sample_index = i * HAETAE_N + coefficient;
          y1->vec[i].coeffs[coefficient] = fixpoint_mul_rnd13(
              samples[sample_index], &sqsum, (sign_byte >> bit) & 1);
        }
      }
    }
  } while (polyfixveclk_sqnorm2(y1, y2) >
           HAETAE_B0SQ * HAETAE_LN * HAETAE_LN);

  {
    uint8_t tmp[HAETAE_CRHBYTES + 2];
    for (i = 0; i < HAETAE_CRHBYTES; i++) {
      tmp[i] = seed[i];
    }
    tmp[HAETAE_CRHBYTES + 0] = ni >> 0;
    tmp[HAETAE_CRHBYTES + 1] = ni >> 8;
    shake256(b, 1, tmp, HAETAE_CRHBYTES + 2);
  }

  return ni;
}

#endif /* !HAETAE_HYPERBALL_INLINE_H */

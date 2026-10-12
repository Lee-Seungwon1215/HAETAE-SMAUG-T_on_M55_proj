// SPDX-License-Identifier: MIT

#ifndef SMAUGT_TOOM_MVE_H
#define SMAUGT_TOOM_MVE_H

#include <stdint.h>

#define SMAUGT_MVE_N 256
#define SMAUGT_MVE_MAX_K 4
#define SMAUGT_MVE_TOOM_POINTS 7
#define SMAUGT_MVE_TOOM_PART 64

typedef struct {
  int16_t points[SMAUGT_MVE_MAX_K][SMAUGT_MVE_TOOM_POINTS]
                [SMAUGT_MVE_TOOM_PART];
} smaugt_mve_eval_cache;

void smaugt_poly_mul_acc_b1(const int16_t a[SMAUGT_MVE_N],
                            const int16_t b[SMAUGT_MVE_N],
                            int16_t res[SMAUGT_MVE_N]);
void smaugt_poly_mul_acc_b2(const int16_t a[SMAUGT_MVE_N],
                            const int16_t b[SMAUGT_MVE_N],
                            int16_t res[SMAUGT_MVE_N]);
void smaugt_poly_mul_acc_b3(const int16_t a[SMAUGT_MVE_N],
                            const int16_t b[SMAUGT_MVE_N],
                            int16_t res[SMAUGT_MVE_N]);
void smaugt_poly_mul_acc_b6(const int16_t a[SMAUGT_MVE_N],
                            const int16_t b[SMAUGT_MVE_N],
                            int16_t res[SMAUGT_MVE_N]);
void smaugt_poly_mul_acc_outer32(const int16_t a[SMAUGT_MVE_N],
                                 const int16_t b[SMAUGT_MVE_N],
                                 int16_t res[SMAUGT_MVE_N]);
void smaugt_poly_mul_acc_outer32_leafcall(const int16_t a[SMAUGT_MVE_N],
                                          const int16_t b[SMAUGT_MVE_N],
                                          int16_t res[SMAUGT_MVE_N]);
void smaugt_poly_mul_acc_direct256_outer(const int16_t a[SMAUGT_MVE_N],
                                         const int16_t b[SMAUGT_MVE_N],
                                         int16_t res[SMAUGT_MVE_N]);
void smaugt_poly_mul_acc_masked_b1(const int16_t a[SMAUGT_MVE_N],
                                   const int16_t b[SMAUGT_MVE_N],
                                   int16_t res[SMAUGT_MVE_N], uint16_t mask);
void smaugt_poly_mul_acc_masked_b2(const int16_t a[SMAUGT_MVE_N],
                                   const int16_t b[SMAUGT_MVE_N],
                                   int16_t res[SMAUGT_MVE_N], uint16_t mask);
void smaugt_poly_mul_acc_masked_b3(const int16_t a[SMAUGT_MVE_N],
                                   const int16_t b[SMAUGT_MVE_N],
                                   int16_t res[SMAUGT_MVE_N], uint16_t mask);
void smaugt_poly_mul_acc_masked_b6(const int16_t a[SMAUGT_MVE_N],
                                   const int16_t b[SMAUGT_MVE_N],
                                   int16_t res[SMAUGT_MVE_N], uint16_t mask);
void smaugt_poly_mul_acc_masked_outer32(const int16_t a[SMAUGT_MVE_N],
                                        const int16_t b[SMAUGT_MVE_N],
                                        int16_t res[SMAUGT_MVE_N],
                                        uint16_t mask);
void smaugt_poly_mul_acc_masked_outer32_leafcall(
    const int16_t a[SMAUGT_MVE_N], const int16_t b[SMAUGT_MVE_N],
    int16_t res[SMAUGT_MVE_N], uint16_t mask);
void smaugt_poly_mul_acc_masked_direct256_outer(
    const int16_t a[SMAUGT_MVE_N], const int16_t b[SMAUGT_MVE_N],
    int16_t res[SMAUGT_MVE_N], uint16_t mask);

void smaugt_mve_cache_vector(smaugt_mve_eval_cache *cache,
                             const int16_t *b, unsigned terms);
void smaugt_vec_mul_acc_cached_b1(const int16_t *a,
                                  const smaugt_mve_eval_cache *cache,
                                  unsigned terms, unsigned stride,
                                  uint16_t mask, int16_t res[SMAUGT_MVE_N]);
void smaugt_vec_mul_acc_cached_b2(const int16_t *a,
                                  const smaugt_mve_eval_cache *cache,
                                  unsigned terms, unsigned stride,
                                  uint16_t mask, int16_t res[SMAUGT_MVE_N]);
void smaugt_vec_mul_acc_cached_b3(const int16_t *a,
                                  const smaugt_mve_eval_cache *cache,
                                  unsigned terms, unsigned stride,
                                  uint16_t mask, int16_t res[SMAUGT_MVE_N]);
void smaugt_vec_mul_acc_cached_b6(const int16_t *a,
                                  const smaugt_mve_eval_cache *cache,
                                  unsigned terms, unsigned stride,
                                  uint16_t mask, int16_t res[SMAUGT_MVE_N]);
void smaugt_vec_mul_acc_cached_outer32(const int16_t *a,
                                       const smaugt_mve_eval_cache *cache,
                                       unsigned terms, unsigned stride,
                                       uint16_t mask,
                                       int16_t res[SMAUGT_MVE_N]);
void smaugt_vec_mul_acc_cached_outer32_leafcall(
    const int16_t *a, const smaugt_mve_eval_cache *cache, unsigned terms,
    unsigned stride, uint16_t mask, int16_t res[SMAUGT_MVE_N]);

/*
 * Keep backend selection inside the common MVE interface.  These aliases are
 * compile-time only: callers retain the exact variant-specific call target and
 * no wrapper, branch, or runtime dispatch is introduced.
 */
#if (defined(SMAUGT_TOOM_B1) + defined(SMAUGT_TOOM_B2) +                    \
     defined(SMAUGT_TOOM_B3) + defined(SMAUGT_TOOM_B6) +                    \
     defined(SMAUGT_TOOM_OUTER32) +                                         \
     defined(SMAUGT_TOOM_OUTER32_LEAFCALL) +                                \
     defined(SMAUGT_DIRECT256_OUTER)) > 1
#error "Select exactly one SMAUG-T MVE multiplication backend"
#endif

#if defined(SMAUGT_TOOM_B1)
#define SMAUGT_MVE_BACKEND_SELECTED 1
#define SMAUGT_MVE_CACHED_BACKEND_SELECTED 1
#define smaugt_mve_poly_mul_acc_masked smaugt_poly_mul_acc_masked_b1
#define smaugt_mve_vec_mul_acc_cached smaugt_vec_mul_acc_cached_b1
#elif defined(SMAUGT_TOOM_B2)
#define SMAUGT_MVE_BACKEND_SELECTED 1
#define SMAUGT_MVE_CACHED_BACKEND_SELECTED 1
#define smaugt_mve_poly_mul_acc_masked smaugt_poly_mul_acc_masked_b2
#define smaugt_mve_vec_mul_acc_cached smaugt_vec_mul_acc_cached_b2
#elif defined(SMAUGT_TOOM_B3)
#define SMAUGT_MVE_BACKEND_SELECTED 1
#define SMAUGT_MVE_CACHED_BACKEND_SELECTED 1
#define smaugt_mve_poly_mul_acc_masked smaugt_poly_mul_acc_masked_b3
#define smaugt_mve_vec_mul_acc_cached smaugt_vec_mul_acc_cached_b3
#elif defined(SMAUGT_TOOM_B6)
#define SMAUGT_MVE_BACKEND_SELECTED 1
#define SMAUGT_MVE_CACHED_BACKEND_SELECTED 1
#define smaugt_mve_poly_mul_acc_masked smaugt_poly_mul_acc_masked_b6
#define smaugt_mve_vec_mul_acc_cached smaugt_vec_mul_acc_cached_b6
#elif defined(SMAUGT_TOOM_OUTER32)
#define SMAUGT_MVE_BACKEND_SELECTED 1
#define SMAUGT_MVE_CACHED_BACKEND_SELECTED 1
#define smaugt_mve_poly_mul_acc_masked smaugt_poly_mul_acc_masked_outer32
#define smaugt_mve_vec_mul_acc_cached smaugt_vec_mul_acc_cached_outer32
#elif defined(SMAUGT_TOOM_OUTER32_LEAFCALL)
#define SMAUGT_MVE_BACKEND_SELECTED 1
#define SMAUGT_MVE_CACHED_BACKEND_SELECTED 1
#define smaugt_mve_poly_mul_acc_masked                                      \
  smaugt_poly_mul_acc_masked_outer32_leafcall
#define smaugt_mve_vec_mul_acc_cached                                      \
  smaugt_vec_mul_acc_cached_outer32_leafcall
#elif defined(SMAUGT_DIRECT256_OUTER)
#define SMAUGT_MVE_BACKEND_SELECTED 1
#define smaugt_mve_poly_mul_acc_masked                                     \
  smaugt_poly_mul_acc_masked_direct256_outer
#endif

#if defined(SMAUGT_TOOM_CACHE) &&                                           \
    !defined(SMAUGT_MVE_CACHED_BACKEND_SELECTED)
#error "SMAUGT_TOOM_CACHE requires a supported Toom MVE backend"
#endif

#endif

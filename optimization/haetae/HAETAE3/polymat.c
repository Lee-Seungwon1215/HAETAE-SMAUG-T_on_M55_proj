// SPDX-License-Identifier: MIT

#include "polymat.h"
#include "params.h"
#include "poly.h"
#include "polyvec.h"

#if defined(HAETAE_ACC_AFTERSUM_DMAX_TIGHT_PRECENTER)
#include "haetae_ntt_mve.h"
#endif

#include <stdint.h>

/*************************************************
 * Name:        polymatkl_expand_matA
 *
 * Description: Implementation of ExpandA. Generates matrix A with uniformly
 *              random coefficients a_{i,j} by performing rejection
 *              sampling on the output stream of SHAKE128(rho|j|i)
 *              or AES256CTR(rho,j|i).
 *
 * Arguments:   - polyvecm mat[HAETAE_K]: output matrix k \times m
 *              - const uint8_t rho[]: byte array containing seed rho
 *
 * Specification: Implements @[KS X 123456, Algorithm 4, ExpandMatA]
 **************************************************/
void polymatkl_expand_matA(polyvecl mat[HAETAE_K],
                           const uint8_t rho[HAETAE_SEEDBYTES]) {
#if defined(M55_KECCAK_X4)
  const unsigned int total = HAETAE_K * HAETAE_M;
  unsigned int base = 0;

  for (; base + 4 <= total; base += 4) {
    poly *output[4];
    uint16_t nonce[4];
    for (unsigned int lane = 0; lane < 4; lane++) {
      const unsigned int index = base + lane;
      const unsigned int i = index / HAETAE_M;
      const unsigned int j = index % HAETAE_M;
      output[lane] = &mat[i].vec[j + 1];
      nonce[lane] = (uint16_t)((i << 8) + j);
    }
    poly_uniform_x4(output, rho, nonce);
  }
  for (; base < total; base++) {
    const unsigned int i = base / HAETAE_M;
    const unsigned int j = base % HAETAE_M;
    poly_uniform(&mat[i].vec[j + 1], rho, (uint16_t)((i << 8) + j));
  }
#else
  unsigned int i, j;

  for (i = 0; i < HAETAE_K; ++i)
    for (j = 0; j < HAETAE_M; ++j)
      poly_uniform(&mat[i].vec[j + 1], rho, (i << 8) + j);
#endif
}

/*************************************************
 * Name:        polymatkm_expand_matA
 *
 * Description: Implementation of ExpandA. Generates matrix A with uniformly
 *              random coefficients a_{i,j} by performing rejection
 *              sampling on the output stream of SHAKE128(rho|j|i)
 *              or AES256CTR(rho,j|i).
 *
 * Arguments:   - polyvecm mat[HAETAE_K]: output matrix k \times m
 *              - const uint8_t rho[]: byte array containing seed rho
 *
 * Specification: Implements @[KS X 123456, Algorithm 4, ExpandMatA]
 **************************************************/
void polymatkm_expand_matA(polyvecm mat[HAETAE_K],
                           const uint8_t rho[HAETAE_SEEDBYTES]) {
#if defined(M55_KECCAK_X4)
  const unsigned int total = HAETAE_K * HAETAE_M;
  unsigned int base = 0;

  for (; base + 4 <= total; base += 4) {
    poly *output[4];
    uint16_t nonce[4];
    for (unsigned int lane = 0; lane < 4; lane++) {
      const unsigned int index = base + lane;
      const unsigned int i = index / HAETAE_M;
      const unsigned int j = index % HAETAE_M;
      output[lane] = &mat[i].vec[j];
      nonce[lane] = (uint16_t)((i << 8) + j);
    }
    poly_uniform_x4(output, rho, nonce);
  }
  for (; base < total; base++) {
    const unsigned int i = base / HAETAE_M;
    const unsigned int j = base % HAETAE_M;
    poly_uniform(&mat[i].vec[j], rho, (uint16_t)((i << 8) + j));
  }
#else
  unsigned int i, j;

  for (i = 0; i < HAETAE_K; ++i)
    for (j = 0; j < HAETAE_M; ++j)
      poly_uniform(&mat[i].vec[j], rho, (i << 8) + j);
#endif
}

// doubles k * m sub-matrix of k * l mat
void polymatkl_double(polyvecl mat[HAETAE_K]) {
  unsigned int i, j, k;
  for (i = 0; i < HAETAE_K; ++i) {
    for (j = 1; j < HAETAE_L; ++j) {
      for (k = 0; k < HAETAE_N; ++k) {
        mat[i].vec[j].coeffs[k] *= 2;
      }
    }
  }
}

void polymatkl_pointwise_montgomery(polyveck *t, const polyvecl mat[HAETAE_K],
                                    const polyvecl *v) {
  unsigned int i;

#if defined(HAETAE_ACC_AFTERSUM_DMAX_TIGHT_PRECENTER)
  polyvecl centered_v;
  for (i = 0; i < HAETAE_L; ++i)
    haetae_barrett_tight_mve(centered_v.vec[i].coeffs, v->vec[i].coeffs);
  for (i = 0; i < HAETAE_K; ++i)
    haetae_pointwise_acc_precentered_mve(
        t->vec[i].coeffs, mat[i].vec[0].coeffs,
        centered_v.vec[0].coeffs, HAETAE_L);
#else
  for (i = 0; i < HAETAE_K; ++i) {
    polyvecl_pointwise_acc_montgomery(&t->vec[i], &mat[i], v);
  }
#endif
}

void polymatkm_pointwise_montgomery(polyveck *t, const polyvecm mat[HAETAE_K],
                                    const polyvecm *v) {
  unsigned int i;

#if defined(HAETAE_ACC_AFTERSUM_DMAX_TIGHT_PRECENTER)
  polyvecm centered_v;
  for (i = 0; i < HAETAE_M; ++i)
    haetae_barrett_tight_mve(centered_v.vec[i].coeffs, v->vec[i].coeffs);
  for (i = 0; i < HAETAE_K; ++i)
    haetae_pointwise_acc_precentered_mve(
        t->vec[i].coeffs, mat[i].vec[0].coeffs,
        centered_v.vec[0].coeffs, HAETAE_M);
#else
  for (i = 0; i < HAETAE_K; ++i) {
    polyvecm_pointwise_acc_montgomery(&t->vec[i], &mat[i], v);
  }
#endif
}

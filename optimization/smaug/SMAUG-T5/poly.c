// SPDX-License-Identifier: MIT

#include "poly.h"
#if defined(SMAUGT_STACK_ENC_ASTREAM02)
#include "fips202.h"
#include "pack.h"
#if defined(M55_KECCAK_X4)
#include "keccak_m55/fips202x4_mve.h"
#endif
#endif
#if defined(SMAUGT_STACK_ENC_DIRECTPACK01)
#include "ciphertext.h"
#endif
#include "toomcook.h"
#if defined(SMAUGT_TOOM_CACHE)
#include "smaugt_toom_mve.h"
#endif

#if defined(SMAUGT_STACK_ENC_DIRECTPACK01)
void matrix_seed_vec_mult_pack(uint8_t output[SMAUGT_CTPOLYVEC_BYTES],
                               const uint8_t seed[SMAUGT_PKSEED_BYTES],
                               const polyvec *b,
                               polyvec *column_workspace) {
    const unsigned int shift = SMAUGT_MODULUS_16_LOG_Q;
    const uint16_t mask = (uint16_t)~((1u << shift) - 1u);
    smaugt_mve_eval_cache cache;
    poly c1;
    uint8_t buf[SMAUGT_PKPOLY_BYTES] = {0};
    uint8_t extseed[SMAUGT_PKSEED_BYTES + 2];

    memcpy(extseed, seed, SMAUGT_PKSEED_BYTES);
    smaugt_mve_cache_vector(&cache, b->vec[0].coeffs, SMAUGT_K);
    for (unsigned int column = 0; column < SMAUGT_K; ++column) {
        extseed[SMAUGT_PKSEED_BYTES + 1] = (uint8_t)column;
        for (unsigned int row = 0; row < SMAUGT_K; ++row) {
            extseed[SMAUGT_PKSEED_BYTES] = (uint8_t)row;
            shake128(buf, SMAUGT_PKPOLY_BYTES, extseed,
                     SMAUGT_PKSEED_BYTES + 2);
            unpack_ring(&column_workspace->vec[row], buf);
        }
        memset(&c1, 0, sizeof(c1));
        vec_vec_mult_from_cache(&c1, column_workspace->vec[0].coeffs,
                                SMAUGT_N, mask, &cache);
        for (unsigned int k = 0; k < SMAUGT_N; ++k) {
            c1.coeffs[k] =
                ((c1.coeffs[k] + SMAUGT_RD_ADD) & SMAUGT_RD_AND) >>
                SMAUGT_MODULUS_16_LOG_P;
        }
        pack_ring_p(output + column * SMAUGT_CTPOLY1_BYTES, &c1);
    }
}
#endif
#include <string.h>

#if SMAUGT_MODE == SMAUGT_MODET
/*************************************************
 * Name:        flipabs
 *
 * Description: Computes |(x mod q) - Q/2|
 *
 * Arguments:   - uint16_t x: input coefficient
 *
 * Return: |(x mod q) - Q/2|
 **************************************************/
uint16_t flipabs(uint16_t x) {
    int16_t r, m;
    r = x - (SMAUGT_MODULUS_SCALED_Q_HALF);
    m = r >> 15;
    return (r + m) ^ m;
}

/*************************************************
 * Name:        d2_ecd
 *
 * Description: Convert 16-byte message to polynomial
 *
 * Arguments:   - poly *r: pointer to output polynomial
 *              - const uint8_t msg[SMAUGT_MSG_BYTES]: pointer to input message
 **************************************************/
void d2_ecd(poly *r, const uint8_t msg[SMAUGT_MSG_BYTES]) {
    unsigned int mask;
    for (size_t i = 0; i < SMAUGT_MSG_BYTES; i++) {
        for (size_t j = 0; j < 8; j++) {
            mask = (msg[i] >> j) & 1;
            mask = (mask * SMAUGT_MODULUS_SCALED_Q_HALF) &
                   SMAUGT_MODULUS_SCALED_Q_HALF;
            r->coeffs[8 * i + j] = mask;
            r->coeffs[8 * i + j + 128] = mask;
        }
    }
}

/*************************************************
 * Name:        d2_dcd
 *
 * Description: Convert polynomial to 16-byte message
 *
 * Arguments:   - uint8_t msg[SMAUGT_MSG_BYTES]: pointer to output message
 *              - const poly *x: pointer to input polynomial
 **************************************************/
void d2_dcd(uint8_t msg[SMAUGT_MSG_BYTES], const poly *x) {
    uint16_t t;
    for (size_t i = 0; i < SMAUGT_MSG_BYTES; i++)
        msg[i] = 0;

    for (size_t i = 0; i < SMAUGT_N / 2; i++) {
        t = flipabs(x->coeffs[i]);
        t += flipabs(x->coeffs[i + 128]);
        t = t - SMAUGT_MODULUS_SCALED_Q_HALF;
        t >>= 15;
        msg[i >> 3] |= t << (i & 7);
    }
}
#endif
/*************************************************
 * Name:        poly_add
 *
 * Description: Add two polynomials. no modular reduction is performed
 *
 * Arguments: - poly *r: pointer to output polynomial
 *            - const poly *a: pointer to first input polynomial
 *            - const poly *b: pointer to second input polynomial
 **************************************************/
static inline void poly_add(poly *r, const poly *a, const poly *b) {
    unsigned int i;
    for (i = 0; i < SMAUGT_N; i++)
        r->coeffs[i] = a->coeffs[i] + b->coeffs[i];
}

/*************************************************
 * Name:        poly_sub
 *
 * Description: Subtract two polynomials. no modular reduction is performed
 *
 * Arguments: - poly *r: pointer to output polynomial
 *            - const poly *a: pointer to first input polynomial
 *            - const poly *b: pointer to second input polynomial
 **************************************************/
static inline void poly_sub(poly *r, const poly *a, const poly *b) {
    unsigned int i;
    for (i = 0; i < SMAUGT_N; i++)
        r->coeffs[i] = a->coeffs[i] - b->coeffs[i];
}

#define vec_vec_mult SMAUGT_NAMESPACE(vec_vec_mult)
#if defined(SMAUGT_TOOM_CACHE)
static void vec_vec_mult_from_cache(poly *r, const int16_t *a,
                                    unsigned stride, uint16_t mask,
                                    const smaugt_mve_eval_cache *cache) {
    smaugt_mve_vec_mul_acc_cached(a, cache, SMAUGT_K, stride, mask,
                                  r->coeffs);
}
#endif
/*************************************************
 * Name:        vec_vec_mult
 *
 * Description: Multiply two vector of polynomials.
 *
 * Arguments:   - poly *r: pointer to output polynomial
 *              - const polyvec *a: pointer to input vector of polynomials
 *              - const polyvec *b: pointer to input vector of polynomials
 **************************************************/
void vec_vec_mult(poly *r, const polyvec *a, const polyvec *b) {
#if defined(SMAUGT_TOOM_CACHE)
    smaugt_mve_eval_cache cache;
    smaugt_mve_cache_vector(&cache, b->vec[0].coeffs, SMAUGT_K);
    vec_vec_mult_from_cache(r, a->vec[0].coeffs, SMAUGT_N, 0xffffu, &cache);
#else
    unsigned int i;
    for (i = 0; i < SMAUGT_K; i++)
        poly_mul_acc(a->vec[i].coeffs, b->vec[i].coeffs, r->coeffs);
#endif
}

/*
 * CT for the masked paths in vec_vec_mult_add, matrix_vec_mult_add, and
 * matrix_vec_mult_sub: K, N, mod/shift, masks, loop counts, and all address
 * strides are public. Coefficients affect arithmetic only. The #if choices
 * are compile-time variants rather than runtime data-dependent branches.
 */
/*************************************************
 * Name:        vec_vec_mult_add
 *
 * Description: Multiply two vectors of polynomials and add the result to output
 *              polynomial
 *
 * Arguments:   - poly *r: pointer to output polynomial
 *              - const polyvec *a: pointer to input vector of polynomials
 *              - const polyvec *b: pointer to input vector of polynomials
 *              - uint8_t mod: modulus (16-SMAUGT_LOG_P) or (16-SMAUGT_LOG_Q)
 **************************************************/
void vec_vec_mult_add(poly *r, const polyvec *a, const polyvec *b,
                      uint8_t mod) {
#if defined(SMAUGT_MASKED_MUL)
    const uint16_t mask = (uint16_t)~((1u << mod) - 1u);
    poly res;
    memset(&res, 0, sizeof(res));
#if defined(SMAUGT_TOOM_CACHE)
    smaugt_mve_eval_cache cache;
    smaugt_mve_cache_vector(&cache, b->vec[0].coeffs, SMAUGT_K);
    vec_vec_mult_from_cache(&res, a->vec[0].coeffs, SMAUGT_N, mask, &cache);
#else
    for (unsigned int i = 0; i < SMAUGT_K; ++i)
        poly_mul_acc_masked(a->vec[i].coeffs, b->vec[i].coeffs, res.coeffs,
                            mask);
#endif
    poly_add(r, r, &res);
#else
    unsigned int i, j;
    polyvec al;
    poly res;

    for (i = 0; i < SMAUGT_K; ++i)
        for (j = 0; j < SMAUGT_N; ++j)
            al.vec[i].coeffs[j] = a->vec[i].coeffs[j] >> mod;

    memset(&res, 0, sizeof(poly));
    vec_vec_mult(&res, &al, b);
    for (j = 0; j < SMAUGT_N; ++j)
        res.coeffs[j] <<= mod;

    poly_add(r, r, &res);
#endif
}

/*************************************************
 * Name:        matrix_vec_mult_add
 *
 * Description: Transpose the matrix of polynomial and multiply it with the
 *              vector of polynomials.
 *
 * Arguments:   - polyvec *r: pointer to output vector of polynomials
 *              - const polyvec a[SMAUGT_K]: pointer to input matrix of
 *              polynomials
 *              - const polyvec *b: pointer to input vector of polynomials
 **************************************************/
void matrix_vec_mult_add(polyvec *r, const polyvec a[SMAUGT_K],
                         const polyvec *b) {
#if defined(SMAUGT_MASKED_MUL)
    const unsigned int shift = SMAUGT_MODULUS_16_LOG_Q;
    const uint16_t mask = (uint16_t)~((1u << shift) - 1u);
#if defined(SMAUGT_TOOM_CACHE)
    smaugt_mve_eval_cache cache;
    smaugt_mve_cache_vector(&cache, b->vec[0].coeffs, SMAUGT_K);
#endif
    /*
     * In Z_2^16, ((x >> m) * y) << m == (x & ~(2^m-1)) * y.
     * The old function also shifted an existing output, so retain that once
     * per output and apply the public mask while loading each matrix operand.
     * Bounds, addresses, and loop counts depend only on public K/N/m.
     */
    for (unsigned int i = 0; i < SMAUGT_K; ++i) {
        for (unsigned int k = 0; k < SMAUGT_N; ++k)
            r->vec[i].coeffs[k] =
                (int16_t)((uint16_t)r->vec[i].coeffs[k] << shift);
#if defined(SMAUGT_TOOM_CACHE)
        vec_vec_mult_from_cache(&r->vec[i], a[0].vec[i].coeffs,
                                SMAUGT_K * SMAUGT_N, mask, &cache);
#else
        for (unsigned int j = 0; j < SMAUGT_K; ++j)
            poly_mul_acc_masked(a[j].vec[i].coeffs, b->vec[j].coeffs,
                                r->vec[i].coeffs, mask);
#endif
    }
#else
    unsigned int i, j, k;
    polyvec at;
#if defined(SMAUGT_TOOM_CACHE)
    smaugt_mve_eval_cache cache;
    smaugt_mve_cache_vector(&cache, b->vec[0].coeffs, SMAUGT_K);
#endif

    for (i = 0; i < SMAUGT_K; ++i) {
        for (j = 0; j < SMAUGT_K; ++j)
            for (k = 0; k < SMAUGT_N; ++k)
                at.vec[j].coeffs[k] =
                    a[j].vec[i].coeffs[k] >> SMAUGT_MODULUS_16_LOG_Q;

#if defined(SMAUGT_TOOM_CACHE)
        vec_vec_mult_from_cache(&r->vec[i], at.vec[0].coeffs, SMAUGT_N,
                                0xffffu, &cache);
#else
        vec_vec_mult(&r->vec[i], &at, b);
#endif
        for (j = 0; j < SMAUGT_N; ++j)
            r->vec[i].coeffs[j] <<= SMAUGT_MODULUS_16_LOG_Q;
    }
#endif
}

#if defined(SMAUGT_STACK_ENC_ASTREAM02)
void matrix_seed_vec_mult_add(polyvec *r,
                              const uint8_t seed[SMAUGT_PKSEED_BYTES],
                              const polyvec *b, polyvec *column_workspace) {
    const unsigned int shift = SMAUGT_MODULUS_16_LOG_Q;
    const uint16_t mask = (uint16_t)~((1u << shift) - 1u);
    smaugt_mve_eval_cache cache;
#if defined(M55_KECCAK_X4) && SMAUGT_K == 4
    uint8_t extseed[4][SMAUGT_PKSEED_BYTES + 2];
#else
    uint8_t buf[SMAUGT_PKPOLY_BYTES] = {0};
    uint8_t extseed[SMAUGT_PKSEED_BYTES + 2];
#endif

#if defined(M55_KECCAK_X4) && SMAUGT_K == 4
    for (unsigned int row = 0; row < 4; ++row)
        memcpy(extseed[row], seed, SMAUGT_PKSEED_BYTES);
#else
    memcpy(extseed, seed, SMAUGT_PKSEED_BYTES);
#endif
    smaugt_mve_cache_vector(&cache, b->vec[0].coeffs, SMAUGT_K);
    for (unsigned int column = 0; column < SMAUGT_K; ++column) {
#if defined(M55_KECCAK_X4) && SMAUGT_K == 4
        for (unsigned int row = 0; row < 4; ++row) {
            extseed[row][SMAUGT_PKSEED_BYTES] = (uint8_t)row;
            extseed[row][SMAUGT_PKSEED_BYTES + 1] = (uint8_t)column;
        }
        m55_shake128x4((uint8_t *)&column_workspace->vec[0],
                       (uint8_t *)&column_workspace->vec[1],
                       (uint8_t *)&column_workspace->vec[2],
                       (uint8_t *)&column_workspace->vec[3],
                       SMAUGT_PKPOLY_BYTES, extseed[0], extseed[1], extseed[2],
                       extseed[3], SMAUGT_PKSEED_BYTES + 2);
        for (unsigned int row = 0; row < 4; ++row)
            unpack_ring_inplace(&column_workspace->vec[row]);
#else
        extseed[SMAUGT_PKSEED_BYTES + 1] = (uint8_t)column;
        for (unsigned int row = 0; row < SMAUGT_K; ++row) {
            extseed[SMAUGT_PKSEED_BYTES] = (uint8_t)row;
            shake128(buf, SMAUGT_PKPOLY_BYTES, extseed,
                     SMAUGT_PKSEED_BYTES + 2);
            unpack_ring(&column_workspace->vec[row], buf);
        }
#endif
        for (unsigned int k = 0; k < SMAUGT_N; ++k)
            r->vec[column].coeffs[k] =
                (int16_t)((uint16_t)r->vec[column].coeffs[k] << shift);
        vec_vec_mult_from_cache(&r->vec[column],
                                column_workspace->vec[0].coeffs, SMAUGT_N,
                                mask, &cache);
    }
}
#endif

/*************************************************
 * Name:        matrix_vec_mult_sub
 *
 * Description: Multiply the matrix of polynomial with the vector of polynomial
 *              and subtract the result to output vector of polynomials.
 *
 * Arguments:   - polyvec *r: pointer to input/output vector of polynomials
 *              - const polyvec a[SMAUGT_K]: pointer to input matrix of
 *              polynomials
 *              - const polyvec *b: pointer to input vector of polynomials
 **************************************************/
void matrix_vec_mult_sub(polyvec *r, const polyvec a[SMAUGT_K],
                         const polyvec *b) {
#if defined(SMAUGT_MASKED_MUL)
    const unsigned int shift = SMAUGT_MODULUS_16_LOG_Q;
    const uint16_t mask = (uint16_t)~((1u << shift) - 1u);
#if defined(SMAUGT_TOOM_CACHE)
    smaugt_mve_eval_cache cache;
    smaugt_mve_cache_vector(&cache, b->vec[0].coeffs, SMAUGT_K);
#endif
    for (unsigned int i = 0; i < SMAUGT_K; ++i) {
        poly res;
        memset(&res, 0, sizeof(res));
#if defined(SMAUGT_TOOM_CACHE)
        vec_vec_mult_from_cache(&res, a[i].vec[0].coeffs, SMAUGT_N, mask,
                                &cache);
#else
        for (unsigned int j = 0; j < SMAUGT_K; ++j)
            poly_mul_acc_masked(a[i].vec[j].coeffs, b->vec[j].coeffs,
                                res.coeffs, mask);
#endif
        poly_sub(&r->vec[i], &r->vec[i], &res);
    }
#else
    unsigned int i, j, k;
    polyvec al;
    poly res;
#if defined(SMAUGT_TOOM_CACHE)
    smaugt_mve_eval_cache cache;
    smaugt_mve_cache_vector(&cache, b->vec[0].coeffs, SMAUGT_K);
#endif

    for (i = 0; i < SMAUGT_K; ++i) {
        for (j = 0; j < SMAUGT_K; ++j)
            for (k = 0; k < SMAUGT_N; ++k)
                al.vec[j].coeffs[k] =
                    a[i].vec[j].coeffs[k] >> SMAUGT_MODULUS_16_LOG_Q;

        memset(&res, 0, sizeof(poly));
#if defined(SMAUGT_TOOM_CACHE)
        vec_vec_mult_from_cache(&res, al.vec[0].coeffs, SMAUGT_N, 0xffffu,
                                &cache);
#else
        vec_vec_mult(&res, &al, b);
#endif
        for (j = 0; j < SMAUGT_N; ++j)
            res.coeffs[j] <<= SMAUGT_MODULUS_16_LOG_Q;

        poly_sub(&r->vec[i], &r->vec[i], &res);
    }
#endif
}

// SPDX-License-Identifier: MIT

#include "api.h"
/* NIST 5-arg crypto_sign shim disabled here so the 7-arg definitions below
 * are not mis-parsed as macro invocations by clang. */
#undef crypto_sign
#undef crypto_sign_open
#include "packing.h"
#include "params.h"
#include "poly.h"
#include "polyfix.h"
#include "polymat.h"
#include "polyvec.h"
#include "randombytes.h"
#include "symmetric.h"

#if defined(HAETAE_STACK_VERIFY_STREAM01) ||                                  \
    defined(HAETAE_STACK_KEYGEN_COMPACT02)
#include "haetae_ntt_mve.h"
#endif

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if defined(HAETAE_STACK_VERIFY_STREAM01)
__attribute__((noinline)) static void
verify_matvec_stream(polyveck *t, const uint8_t *vk, const polyvecl *v) {
  polyvecl row, centered_v;
  uint8_t seed[HAETAE_SEEDBYTES];

  memcpy(seed, vk, HAETAE_SEEDBYTES);
  vk += HAETAE_SEEDBYTES;

  for (unsigned int j = 0; j < HAETAE_L; ++j) {
    haetae_barrett_tight_mve(centered_v.vec[j].coeffs, v->vec[j].coeffs);
  }

  for (unsigned int i = 0; i < HAETAE_K; ++i) {
    unpack_poly_q(&row.vec[0], vk + i * HAETAE_POLYQ_PACKEDBYTES);
#if defined(M55_KECCAK_X4)
    /* HAETAE3 has five public matrix polynomials per streamed row. Generate
     * four together and retain one scalar tail. This raises Verify's local
     * peak, but remains well below the scheme-wide Sign peak. */
    {
      poly *output[4] = {&row.vec[1], &row.vec[2], &row.vec[3], &row.vec[4]};
      uint16_t nonce[4];
      for (unsigned int j = 0; j < 4; ++j)
        nonce[j] = (uint16_t)((i << 8) + j);
      poly_uniform_x4(output, seed, nonce);
    }
    for (unsigned int j = 1; j < 5; ++j) {
      for (unsigned int k = 0; k < HAETAE_N; ++k)
        row.vec[j].coeffs[k] *= 2;
    }
    for (unsigned int j = 5; j < HAETAE_L; ++j) {
      poly_uniform(&row.vec[j], seed, (uint16_t)((i << 8) + j - 1));
      for (unsigned int k = 0; k < HAETAE_N; ++k)
        row.vec[j].coeffs[k] *= 2;
    }
#else
    for (unsigned int j = 1; j < HAETAE_L; ++j) {
      poly_uniform(&row.vec[j], seed, (uint16_t)((i << 8) + j - 1));
      for (unsigned int k = 0; k < HAETAE_N; ++k) {
        row.vec[j].coeffs[k] *= 2;
      }
    }
#endif

    poly_uniform(&t->vec[i], seed,
                 (uint16_t)((HAETAE_K << 8) + HAETAE_M + i));
    for (unsigned int k = 0; k < HAETAE_N; ++k) {
      row.vec[0].coeffs[k] *= 2;
    }
    poly_sub(&row.vec[0], &t->vec[i], &row.vec[0]);
    for (unsigned int k = 0; k < HAETAE_N; ++k) {
      row.vec[0].coeffs[k] *= 2;
    }
    poly_ntt(&row.vec[0]);

    haetae_pointwise_acc_precentered_mve(
        t->vec[i].coeffs, row.vec[0].coeffs, centered_v.vec[0].coeffs,
        HAETAE_L);
  }
}
#endif

#if defined(HAETAE_STACK_KEYGEN_COMPACT02)
typedef struct {
  uint16_t coeffs[HAETAE_N];
} keygen_poly_u16;

typedef struct {
  keygen_poly_u16 vec[HAETAE_M];
} keygen_polyvecm_u16;

#define KEYGEN_POLY_UNIFORM_NBLOCKS                                           \
  ((512 + STREAM128_BLOCKBYTES - 1) / STREAM128_BLOCKBYTES)

static unsigned int keygen_rej_uniform_u16(uint16_t *a, unsigned int len,
                                            const uint8_t *buf,
                                            unsigned int buflen) {
  unsigned int ctr = 0, pos = 0;

  while (ctr < len && pos + 2 <= buflen) {
    uint32_t t = buf[pos++];
    t |= (uint32_t)buf[pos++] << 8;
    if (t < HAETAE_Q) {
      a[ctr++] = (uint16_t)t;
    }
  }
  return ctr;
}

static void keygen_poly_uniform_u16(
    keygen_poly_u16 *a, const uint8_t seed[HAETAE_SEEDBYTES],
    uint16_t nonce) {
  unsigned int i, ctr, off;
  unsigned int buflen = KEYGEN_POLY_UNIFORM_NBLOCKS * STREAM128_BLOCKBYTES;
  uint8_t buf[KEYGEN_POLY_UNIFORM_NBLOCKS * STREAM128_BLOCKBYTES + 1];
  stream128_state state;

  stream128_init(&state, seed, nonce);
  stream128_squeezeblocks(buf, KEYGEN_POLY_UNIFORM_NBLOCKS, &state);
  ctr = keygen_rej_uniform_u16(a->coeffs, HAETAE_N, buf, buflen);

  while (ctr < HAETAE_N) {
    off = buflen % 2;
    for (i = 0; i < off; ++i) {
      buf[i] = buf[buflen - off + i];
    }
    stream128_squeezeblocks(buf + off, 1, &state);
    buflen = STREAM128_BLOCKBYTES + off;
    ctr += keygen_rej_uniform_u16(a->coeffs + ctr, HAETAE_N - ctr, buf,
                                  buflen);
  }
}

__attribute__((noinline)) static void keygen_expand_compact_A(
    keygen_polyvecm_u16 A[HAETAE_K],
    const uint8_t rho[HAETAE_SEEDBYTES]) {
#if defined(M55_KECCAK_X4)
  unsigned int base = 0;
  for (; base + 4 <= HAETAE_K * HAETAE_M; base += 4) {
    uint16_t *output[4];
    uint16_t nonce[4];
    for (unsigned int lane = 0; lane < 4; ++lane) {
      const unsigned int index = base + lane;
      const unsigned int i = index / HAETAE_M;
      const unsigned int j = index % HAETAE_M;
      output[lane] = A[i].vec[j].coeffs;
      nonce[lane] = (uint16_t)((i << 8) + j);
    }
    const uint8_t complete = poly_uniform_x4_u16(output, rho, nonce);
    for (unsigned int lane = 0; lane < 4; ++lane) {
      if ((complete & (1u << lane)) == 0) {
        const unsigned int index = base + lane;
        const unsigned int i = index / HAETAE_M;
        const unsigned int j = index % HAETAE_M;
        keygen_poly_uniform_u16(&A[i].vec[j], rho, nonce[lane]);
      }
    }
  }
  for (; base < HAETAE_K * HAETAE_M; ++base) {
    const unsigned int i = base / HAETAE_M;
    const unsigned int j = base % HAETAE_M;
    keygen_poly_uniform_u16(&A[i].vec[j], rho,
                            (uint16_t)((i << 8) + j));
  }
#else
  for (unsigned int i = 0; i < HAETAE_K; ++i) {
    for (unsigned int j = 0; j < HAETAE_M; ++j) {
      keygen_poly_uniform_u16(&A[i].vec[j], rho,
                              (uint16_t)((i << 8) + j));
    }
  }
#endif
}

__attribute__((noinline)) static void keygen_matvec_compact_A(
    polyveck *b, const keygen_polyvecm_u16 A[HAETAE_K],
    const polyvecm *s1hat) {
  polyvecm centered_s1hat;

  for (unsigned int j = 0; j < HAETAE_M; ++j) {
    haetae_barrett_tight_mve(centered_s1hat.vec[j].coeffs,
                             s1hat->vec[j].coeffs);
  }
  for (unsigned int i = 0; i < HAETAE_K; ++i) {
    haetae_pointwise_acc_u16_precentered_mve(
        b->vec[i].coeffs, A[i].vec[0].coeffs,
        centered_s1hat.vec[0].coeffs, HAETAE_M);
  }
}
#endif

/*************************************************
 * Name:        crypto_sign_keypair_internal
 *
 * Description: Generates public and private key.
 *
 * Arguments:   - uint8_t *vk: pointer to output public key (allocated
 *              array of HAETAE_CRYPTO_PUBLICKEYBYTES bytes)
 *              - uint8_t *sk: pointer to output private key (allocated
 *              array of HAETAE_CRYPTO_SECRETKEYBYTES bytes)
 *              - uint8_t seed : input seed
 *
 * Returns 0 (success)
 *
 * Specification: Implements @[KS X 123456, Algorithm 48, KeyGenInternal]
 **************************************************/

int crypto_sign_keypair_internal(uint8_t *vk, uint8_t *sk,
                                 uint8_t seed[HAETAE_SEEDBYTES]) {
  uint8_t seedbuf[2 * HAETAE_SEEDBYTES + HAETAE_CRHBYTES] = {0};
  uint16_t counter = 0;
  const uint8_t *rhoprime, *sigma, *key;
#if defined(HAETAE_STACK_KEYGEN_COMPACT02)
  keygen_polyvecm_u16 A[HAETAE_K];
  polyvecm s1;
#else
  polyvecm A[HAETAE_K], s1;
#endif
  polyveck b, s2;
#if (HAETAE_MODE == HAETAE_MODE2) || (HAETAE_MODE == HAETAE_MODE3)
  polyveck a;
#if defined(HAETAE_STACK_KEYGEN_UNION01)
  union {
    polyvecm s1hat;
    polyveck b0;
  } ws_keygen_round;
#define s1hat ws_keygen_round.s1hat
#define b0 ws_keygen_round.b0
#else
  polyvecm s1hat;
  polyveck b0;
#endif
#elif HAETAE_MODE == HAETAE_MODE5
  polyvecm s1hat;
  polyveck s2hat;
#else
  polyvecm s1hat;
#endif
  xof256_state state;

  // Get seed_A
  memcpy(seedbuf, seed, HAETAE_SEEDBYTES);

  // Sample seeds with entropy \rho
  xof256_absorb_once(&state, seedbuf, HAETAE_SEEDBYTES);
  xof256_squeeze(seedbuf, 2 * HAETAE_SEEDBYTES + HAETAE_CRHBYTES, &state);

  rhoprime = seedbuf;
  sigma = rhoprime + HAETAE_SEEDBYTES;
  key = sigma + HAETAE_CRHBYTES;

  // Expand Matrix A0 and vector a
#if defined(HAETAE_STACK_KEYGEN_COMPACT02)
  keygen_expand_compact_A(A, rhoprime);
#else
  polymatkm_expand_matA(A, rhoprime);
#endif

#if (HAETAE_MODE == HAETAE_MODE2) || (HAETAE_MODE == HAETAE_MODE3)
  /**********************************************
   * If there is rounding (D > 0), we need another polyveck a.
   * Then, b = a + A0 * s1 + s2 and the lower D bits are
   * rounded from b. The lower D bits are subsequently
   * subtracted from s2.
   **********************************************/
  polyveck_expand_vecA(&a, rhoprime);

reject:
  // Sample secret vectors s1 and s2
  polyvecmk_expand_S(&s1, &s2, sigma, counter);
  counter += HAETAE_M + HAETAE_K;

  // b = a + A0 * s1 + s2 mod q
  s1hat = s1;
  polyvecm_ntt(&s1hat);
#if defined(HAETAE_STACK_KEYGEN_COMPACT02)
  keygen_matvec_compact_A(&b, A, &s1hat);
#else
  polymatkm_pointwise_montgomery(&b, A, &s1hat);
#endif
  polyveck_invntt_tomont(&b);
  polyveck_add(&b, &b, &s2);
  polyveck_add(&b, &b, &a);
  polyveck_freeze(&b);

  // round off D bits
  polyveck_decompose_vk(&b0, &b);
  polyveck_sub(&s2, &s2, &b0);

  int64_t squared_singular_value = polyvecmk_sk_singular_value(&s1, &s2);
  if (squared_singular_value > HAETAE_GAMMA * HAETAE_GAMMA * HAETAE_N) {
    goto reject;
  }
#elif HAETAE_MODE == HAETAE_MODE5
  /**********************************************
   * If there is no rounding (D == 0), we store
   * -2b directly in NTT domain into the public key.
   **********************************************/
reject:
  // Sample secret vectors s1 and s2
  polyvecmk_expand_S(&s1, &s2, sigma, counter);
  counter += HAETAE_M + HAETAE_K;
  int64_t squared_singular_value = polyvecmk_sk_singular_value(&s1, &s2);
  if (squared_singular_value > HAETAE_GAMMA * HAETAE_GAMMA * HAETAE_N) {
    goto reject;
  }

  // b = A0 * s1 + s2 mod q
  s1hat = s1;
  s2hat = s2;
  polyvecm_ntt(&s1hat);
  polyveck_ntt(&s2hat);
  polymatkm_pointwise_montgomery(&b, A, &s1hat);
  polyveck_frommont(&b);
  polyveck_add(&b, &b, &s2hat);
  polyveck_double_negate(&b);
  polyveck_caddq(&b); // directly compute and store NTT(-2b)
#endif

  pack_vk(vk, &b, rhoprime);
  pack_sk(sk, vk, &s1, &s2, key);

#if defined(HAETAE_STACK_KEYGEN_UNION01)
#undef s1hat
#undef b0
#endif

  return 0;
}

/*************************************************
 * Name:        crypto_sign_keypair_internal
 *
 * Description: Generates public and private key.
 *
 * Arguments:   - uint8_t *vk: pointer to output public key (allocated
 *              array of HAETAE_CRYPTO_PUBLICKEYBYTES bytes)
 *              - uint8_t *sk: pointer to output private key (allocated
 *              array of HAETAE_CRYPTO_SECRETKEYBYTES bytes)
 *
 * Returns 0 (success)
 *
 * Specification: Implements @[KS X 123456, Algorithm 45, KeyGen]
 **************************************************/
int crypto_sign_keypair(uint8_t *pk, uint8_t *sk) {
  int ret = 0;
  uint8_t seed[HAETAE_SEEDBYTES] = {0};

  randombytes(seed, HAETAE_SEEDBYTES);

  ret = crypto_sign_keypair_internal(pk, sk, seed);

  return ret;
}

/*************************************************
 * Name:        crypto_sign_signature_internal
 *
 * Description: Computes signature. Internal API.
 *
 * Arguments:   - uint8_t *sig: pointer to output signature (of length
 *              HAETAE_CRYPTO_BYTES)
 *              - size_t *siglen: pointer to output length of signature
 *              - uint8_t *m: pointer to message to be signed
 *              - size_t mlen: length of message
 *              - uint8_t *pre: pointer to prefix string
 *              - size_t prelen: length of prefix string
 *              - uint8_t *rnd: pointer to random seed
 *              - uint8_t *sk: pointer to bit-packed secret key
 *
 * Returns 0 (success)
 *
 * Specification: Implements @[KS X 123456, Algorithm 49, SignInternal]
 **************************************************/
int crypto_sign_signature_internal(uint8_t *sig, size_t *siglen,
                                   const uint8_t *m, size_t mlen,
                                   const uint8_t *pre, size_t prelen,
                                   const uint8_t rnd[HAETAE_SEEDBYTES],
                                   const uint8_t *sk) {

  uint8_t buf[HAETAE_POLYVECK_HIGHBITS_PACKEDBYTES +
              HAETAE_POLYC_PACKEDBYTES] = {0};
  uint8_t seedbuf[HAETAE_CRHBYTES] = {0}, key[HAETAE_SEEDBYTES] = {0};
  uint8_t mu[HAETAE_CRHBYTES] = {0};
  uint8_t b = 0; // one bit
  uint16_t counter = 0;
  uint64_t reject1, reject2;

#if defined(HAETAE_STACK_SIGN_UNION02_L)
  polyvecm s1;
  polyvecl A1[HAETAE_K];
  polyveck s2, highbits, Ay, z2rnd;
  polyfixveck z2;
  poly c, chat, z1rnd0, lsb;

  union {
    polyfixvecl y1;
    polyvecl lb_z1;
  } ws_y1_lb;
  union {
    polyfixvecl z1;
    polyvecl hb_z1;
  } ws_z1_hb;
  union {
    polyvecl z1rnd;
    polyvecl cs1;
    polyfixvecl z1tmp;
  } ws_response_l;
  union {
    polyfixveck y2;
    polyveck h;
  } ws_y2_h;
  union {
    polyveck cs2;
    polyfixveck z2tmp;
    polyveck htmp;
  } ws_response_k;

  _Static_assert(sizeof(polyfixvecl) == sizeof(polyvecl),
                 "HAETAE L-vector overlay size mismatch");
  _Static_assert(sizeof(polyfixveck) == sizeof(polyveck),
                 "HAETAE K-vector overlay size mismatch");

#define y1 ws_y1_lb.y1
#define lb_z1 ws_y1_lb.lb_z1
#define z1 ws_z1_hb.z1
#define hb_z1 ws_z1_hb.hb_z1
#define z1rnd ws_response_l.z1rnd
#define cs1 ws_response_l.cs1
#define z1tmp ws_response_l.z1tmp
#define y2 ws_y2_h.y2
#define h ws_y2_h.h
#define cs2 ws_response_k.cs2
#define z2tmp ws_response_k.z2tmp
#define htmp ws_response_k.htmp
#elif defined(HAETAE_STACK_SIGN_UNION01)
  polyvecm s1;
  polyvecl A1[HAETAE_K], z1rnd;
  polyveck s2, highbits, Ay, z2rnd;
  polyfixvecl z1;
  polyfixveck z2;
  poly c, chat, z1rnd0, lsb;

  union {
    polyfixvecl y1;
    polyvecl hb_z1;
  } ws_y1_hb;
  union {
    polyfixveck y2;
    polyveck h;
  } ws_y2_h;
  union {
    polyvecl cs1;
    polyfixvecl z1tmp;
    polyvecl lb_z1;
  } ws_response_l;
  union {
    polyveck cs2;
    polyfixveck z2tmp;
    polyveck htmp;
  } ws_response_k;

  _Static_assert(sizeof(polyfixvecl) == sizeof(polyvecl),
                 "HAETAE L-vector overlay size mismatch");
  _Static_assert(sizeof(polyfixveck) == sizeof(polyveck),
                 "HAETAE K-vector overlay size mismatch");

#define y1 ws_y1_hb.y1
#define hb_z1 ws_y1_hb.hb_z1
#define y2 ws_y2_h.y2
#define h ws_y2_h.h
#define cs1 ws_response_l.cs1
#define z1tmp ws_response_l.z1tmp
#define lb_z1 ws_response_l.lb_z1
#define cs2 ws_response_k.cs2
#define z2tmp ws_response_k.z2tmp
#define htmp ws_response_k.htmp
#else
  polyvecm s1;
  polyvecl A1[HAETAE_K], cs1;
  polyveck s2, cs2, highbits, Ay;
  polyfixvecl y1, z1, z1tmp;
  polyfixveck y2, z2, z2tmp;
  polyvecl z1rnd; // round of z1
  polyvecl hb_z1, lb_z1;
  polyveck z2rnd, h, htmp; // round of z2
  poly c, chat, z1rnd0, lsb;
#endif

  xof256_state state;

  unsigned int i;

  // Unpack secret key
  unpack_sk(A1, &s1, &s2, key, sk);

  xof256_absorb_thrice(&state, sk, HAETAE_CRYPTO_PUBLICKEYBYTES, pre, prelen, m,
                       mlen);
  xof256_squeeze(mu, HAETAE_CRHBYTES, &state);

  xof256_absorb_thrice(&state, key, HAETAE_SEEDBYTES, rnd, HAETAE_SEEDBYTES, mu,
                       HAETAE_CRHBYTES);
  xof256_squeeze(seedbuf, HAETAE_CRHBYTES, &state);

  polyvecm_ntt(&s1);
  polyveck_ntt(&s2);

reject:

  /*------------------ 1. Sample y1 and y2 from hyperball ------------------*/
  counter = polyfixveclk_sample_hyperball(&y1, &y2, &b, seedbuf, counter);

  /*------------------- 2. Compute a chanllenge c --------------------------*/
  // Round y1 and y2
  polyfixvecl_round(&z1rnd, &y1);
  polyfixveck_round(&z2rnd, &y2);

  // A * round(y) mod q = A1 * round(y1) + 2 * round(y2) mod q
  z1rnd0 = z1rnd.vec[0];
  polyvecl_ntt(&z1rnd);
  polymatkl_pointwise_montgomery(&Ay, A1, &z1rnd);
  polyveck_invntt_tomont(&Ay);
  polyveck_double(&z2rnd);
  polyveck_add(&Ay, &Ay, &z2rnd);

  // recover A * round(y) mod 2q
  polyveck_poly_fromcrt(&Ay, &Ay, &z1rnd0);
  polyveck_freeze2q(&Ay);

  // HighBits of (A * round(y) mod 2q)
  polyveck_highbits_hint(&highbits, &Ay);

  // LSB(round(y_0) * j)
  poly_lsb(&lsb, &z1rnd0);

  // Pack HighBits of A * round(y) mod 2q and LSB of round(y0)
  pack_vec_highbits(buf, &highbits);
  pack_poly_lsb(buf + HAETAE_POLYVECK_HIGHBITS_PACKEDBYTES, &lsb);

  // c = challenge(highbits, lsb, mu)
  poly_challenge(&c, buf, mu);

  /*------------------- 3. Compute z = y + (-1)^b c * s --------------------*/
  // cs = c * s = c * (si1 || s2)
  cs1.vec[0] = c;
  chat = c;
  poly_ntt(&chat);

  for (i = 1; i < HAETAE_L; ++i) {
    poly_pointwise_montgomery(&cs1.vec[i], &chat, &s1.vec[i - 1]);
    poly_invntt_tomont(&cs1.vec[i]);
  }
  polyveck_poly_pointwise_montgomery(&cs2, &s2, &chat);
  polyveck_invntt_tomont(&cs2);

  // z = y + (-1)^b cs = z1 + z2
  polyvecl_cneg(&cs1, b & 1);
  polyveck_cneg(&cs2, b & 1);
  polyfixvecl_add(&z1, &y1, &cs1);
  polyfixveck_add(&z2, &y2, &cs2);

  // reject if norm(z) >= B'
  reject1 = ((uint64_t)HAETAE_B1SQ * HAETAE_LN * HAETAE_LN -
             polyfixveclk_sqnorm2(&z1, &z2)) >>
            63;
  reject1 &= 1;

  polyfixvecl_double(&z1tmp, &z1);
  polyfixveck_double(&z2tmp, &z2);

  polyfixfixvecl_sub(&z1tmp, &z1tmp, &y1);
  polyfixfixveck_sub(&z2tmp, &z2tmp, &y2);

  // reject if norm(2z-y) < B and b' = 0
  reject2 = (polyfixveclk_sqnorm2(&z1tmp, &z2tmp) -
             (uint64_t)HAETAE_B0SQ * HAETAE_LN * HAETAE_LN) >>
            63;
  reject2 &= 1;
  reject2 &= (b & 0x2) >> 1;

  if (reject1 | reject2) {
    goto reject;
  }

  /*------------------- 4. Make a hint -------------------------------------*/
  // Round z1 and z2
  polyfixvecl_round(&z1rnd, &z1);
  polyfixveck_round(&z2rnd, &z2);

  // recover A1 * round(z1) - qcj mod 2q
  polyveck_double(&z2rnd);
  polyveck_sub(&htmp, &Ay, &z2rnd);
  polyveck_freeze2q(&htmp);

  // HighBits of (A * round(z) - qcj mod 2q) and (A1 * round(z1) - qcj mod 2q)
  polyveck_highbits_hint(&htmp, &htmp);
  polyveck_sub(&h, &highbits, &htmp);
  polyveck_caddDQ2ALPHA(&h);

  /*------------------ Decompose(z1) and Pack signature -------------------*/
  polyvecl_lowbits(&lb_z1, &z1rnd); // TODO do this in one function together!
  polyvecl_highbits(&hb_z1, &z1rnd);

  if (pack_sig(sig, &c, &lb_z1, &hb_z1,
               &h)) { // reject if signature is too big
    goto reject;
  }
  *siglen = HAETAE_CRYPTO_BYTES;

#if defined(HAETAE_STACK_SIGN_UNION02_L)
#undef y1
#undef lb_z1
#undef z1
#undef hb_z1
#undef z1rnd
#undef cs1
#undef z1tmp
#undef y2
#undef h
#undef cs2
#undef z2tmp
#undef htmp
#elif defined(HAETAE_STACK_SIGN_UNION01)
#undef y1
#undef hb_z1
#undef y2
#undef h
#undef cs1
#undef z1tmp
#undef lb_z1
#undef cs2
#undef z2tmp
#undef htmp
#endif
  return 0;
}

/*************************************************
 * Name:        crypto_sign_signature
 *
 * Description: Computes signature.
 *
 * Arguments:   - uint8_t *sig:   pointer to output signature (of length
 *                                HAETAE_CRYPTO_BYTES)
 *              - size_t *siglen: pointer to output length of signature
 *              - uint8_t *m:     pointer to message to be signed
 *              - size_t mlen:    length of message
 *              - uint8_t *ctx:   pointer of contex string
 *              - size_t ctxlen:  length of contex string
 *              - uint8_t *sk:    pointer to bit-packed secret key
 *
 * Returns 0 (success)
 *
 * Specification: Implements @[KS X 123456, Algorithm 46, Sign]
 **************************************************/
int crypto_sign_signature(uint8_t *sig, size_t *siglen, const uint8_t *m,
                          size_t mlen, const uint8_t *ctx, size_t ctxlen,
                          const uint8_t *sk) {
  uint8_t pre[256];
  uint8_t rnd[HAETAE_SEEDBYTES];

  if (ctxlen > 255)
    return -1;

  /* Prepare pre = (ctxlen, ctx) */
  pre[0] = ctxlen;
  memcpy(pre + 1, ctx, ctxlen);

  randombytes(rnd, HAETAE_SEEDBYTES);

  return crypto_sign_signature_internal(sig, siglen, m, mlen, pre, 1 + ctxlen,
                                        rnd, sk);
}

/*************************************************
 * Name:        crypto_sign
 *
 * Description: Compute signed message.
 *
 * Arguments:   - uint8_t *sm: pointer to output signed message (allocated
 *                             array with HAETAE_CRYPTO_BYTES + mlen bytes),
 *                             can be equal to m
 *              - size_t *smlen: pointer to output length of signed
 *                               message
 *              - const uint8_t *m: pointer to message to be signed
 *              - size_t mlen: length of message
 *              - const uint8_t *ctx: pointer to context string
 *              - size_t ctxlen: length of context string
 *              - const uint8_t *sk: pointer to bit-packed secret key
 *
 * Returns 0 (success) or -1 (context string too long)
 **************************************************/
int crypto_sign(uint8_t *sm, size_t *smlen, const uint8_t *m, size_t mlen,
                const uint8_t *ctx, size_t ctxlen, const uint8_t *sk) {
  size_t i;

  for (i = 0; i < mlen; ++i)
    sm[HAETAE_CRYPTO_BYTES + mlen - 1 - i] = m[mlen - 1 - i];
  int ret = crypto_sign_signature(sm, smlen, sm + HAETAE_CRYPTO_BYTES, mlen,
                                  ctx, ctxlen, sk);
  *smlen += mlen;
  return ret;
}

/*************************************************
 * Name:        crypto_sign_verify_internal
 *
 * Description: Verifies signature.
 *
 * Arguments:   - uint8_t *m: pointer to input signature
 *              - size_t siglen: length of signature
 *              - const uint8_t *m: pointer to message
 *              - size_t mlen: length of message
 *              - const uint8_t *pre: pointer to prefix string
 *              - size_t prelen: length of prefix string
 *              - const uint8_t *vk: pointer to bit-packed public key
 *
 * Returns 0 if signature could be verified correctly and -1 otherwise
 *
 * Specification: Implements @[KS X 123456, Algorithm 50, VerifyInternal]
 **************************************************/
int crypto_sign_verify_internal(const uint8_t *sig, size_t siglen,
                                const uint8_t *m, size_t mlen,
                                const uint8_t *pre, size_t prelen,
                                const uint8_t *vk) {
  unsigned int i;
  uint8_t buf[HAETAE_POLYVECK_HIGHBITS_PACKEDBYTES + HAETAE_POLYC_PACKEDBYTES] =
      {0};
  uint8_t mu[HAETAE_SEEDBYTES];
  uint64_t sqnorm2;
#if defined(HAETAE_STACK_VERIFY_VIEW03)
#if defined(HAETAE_STACK_VERIFY_STREAM01)
  polyvecl z1;
#else
  polyvecl A1[HAETAE_K], z1;
#endif
  poly lowz_row;
#elif defined(HAETAE_STACK_VERIFY_INPLACE01)
  polyvecl A1[HAETAE_K], lowz;
#define z1 lowz
#else
  polyvecl A1[HAETAE_K], lowz, z1;
#endif
  polyveck h, z2, w;
#if defined(HAETAE_STACK_VERIFY_VIEW03)
  polyveck highbits;
#elif defined(HAETAE_STACK_VERIFY_UNION02)
  union {
    polyvecl highz;
    polyveck highbits;
  } ws_decode_product;
#define highz ws_decode_product.highz
#define highbits ws_decode_product.highbits
#else
  polyvecl highz;
  polyveck highbits;
#endif
  poly c, cprime, wprime;

  xof256_state state;

  // Check signature length
  if (siglen != HAETAE_CRYPTO_BYTES) {
    return -1;
  }
  /*------------------- 1. Recover A1 --------------------------------------*/
  // A1 = (-2b + qj || 2 * A0)
  // Unpack public key
#if !defined(HAETAE_STACK_VERIFY_STREAM01)
  unpack_vk(A1, vk);
#endif

  // Unpack signature and Check conditions -- A1 is used only as intermediate
  // buffer for the low bits
#if defined(HAETAE_STACK_VERIFY_VIEW03)
  if (unpack_sig(&c, &z1, &z1, &h, sig)) {
#else
  if (unpack_sig(&c, &lowz, &highz, &h, sig)) {
#endif
    return -1;
  }

  // Compose z1 out of HighBits(z1) and LowBits(z1)
#if defined(HAETAE_STACK_VERIFY_VIEW03)
  for (i = 0; i < HAETAE_L; ++i) {
    poly_decomposed_unpack(&lowz_row,
                           sig + HAETAE_N / 8 + HAETAE_N * i);
    poly_compose(&z1.vec[i], &z1.vec[i], &lowz_row);
  }
#else
  for (i = 0; i < HAETAE_L; ++i) {
    poly_compose(&z1.vec[i], &highz.vec[i], &lowz.vec[i]);
  }
#endif

  /*------------------- 2. Compute \tilde{z}_2 -----------------------------*/
  // compute squared norm of z1 and w' before NTT
  sqnorm2 = polyvecl_sqnorm2(&z1);
  poly_sub(&wprime, &z1.vec[0], &c);
  poly_lsb(&wprime, &wprime);

  // A1 * round(z1) - qcj mod q
  polyvecl_ntt(&z1);
#if defined(HAETAE_STACK_VERIFY_STREAM01)
  verify_matvec_stream(&highbits, vk, &z1);
#else
  polymatkl_pointwise_montgomery(&highbits, A1, &z1);
#endif
  polyveck_invntt_tomont(&highbits);

  // recover A1 * round(z1) - qcj mod 2q
  polyveck_poly_fromcrt(&highbits, &highbits, &wprime);
  polyveck_freeze2q(&highbits);

  // recover w1
  polyveck_highbits_hint(&w, &highbits);
  polyveck_add(&w, &w, &h);
  polyveck_csubDQ2ALPHA(&w);

  // recover \tilde{z}_2 mod q
  polyveck_mul_alpha(&z2, &w);
  polyveck_sub(&z2, &z2, &highbits);
  poly_add(&z2.vec[0], &z2.vec[0], &wprime);
  polyveck_reduce2q(&z2);
  polyveck_div2(&z2);

  // check final norm of \tilde{z}
  if (sqnorm2 + polyveck_sqnorm2(&z2) > HAETAE_B2SQ) {
    return -1;
  }

  /*------------------- 3. Compute c_seed' and Compare ---------------------*/

  // Pack highBits(A * round(z) - qcj mod 2q) and h'
  pack_vec_highbits(buf, &w);
  pack_poly_lsb(buf + HAETAE_POLYVECK_HIGHBITS_PACKEDBYTES, &wprime);

  xof256_absorb_thrice(&state, vk, HAETAE_CRYPTO_PUBLICKEYBYTES, pre, prelen, m,
                       mlen);
  xof256_squeeze(mu, HAETAE_SEEDBYTES, &state);
  // c_seed = H(HighBits(A * y mod 2q), LSB(round(y0) * j), HAETAE_M)
  poly_challenge(&cprime, buf, mu);

  for (i = 0; i < HAETAE_N; ++i) {
    if (c.coeffs[i] != cprime.coeffs[i]) {
      return -1;
    }
  }
#if defined(HAETAE_STACK_VERIFY_INPLACE01)
#undef z1
#endif
#if defined(HAETAE_STACK_VERIFY_UNION02)
#undef highz
#undef highbits
#endif
  return 0;
}

/*************************************************
 * Name:        crypto_sign_verify
 *
 * Description: Verifies signature.
 *
 * Arguments:   - uint8_t *m: pointer to input signature
 *              - size_t siglen: length of signature
 *              - const uint8_t *m: pointer to message
 *              - size_t mlen: length of message
 *              - const uint8_t *ctx: pointer to context string
 *              - size_t ctxlen: length of context string
 *              - const uint8_t *vk: pointer to bit-packed public key
 *
 * Returns 0 if signature could be verified correctly and -1 otherwise
 *
 * Specification: Implements @[KS X 123456, Algorithm 47, Verify]
 **************************************************/
int crypto_sign_verify(const uint8_t *sig, size_t siglen, const uint8_t *m,
                       size_t mlen, const uint8_t *ctx, size_t ctxlen,
                       const uint8_t *vk) {
  size_t i;
  uint8_t pre[257];

  if (ctxlen > 255)
    return -1;

  pre[0] = ctxlen;
  memcpy(pre + 1, ctx, ctxlen);

  return crypto_sign_verify_internal(sig, siglen, m, mlen, pre, 1 + ctxlen, vk);
}

/*************************************************
 * Name:        crypto_sign_open
 *
 * Description: Verify signed message.
 *
 * Arguments:   - uint8_t *m: pointer to output message (allocated
 *                            array with smlen bytes), can be equal to sm
 *              - size_t *mlen: pointer to output length of message
 *              - const uint8_t *sm: pointer to signed message
 *              - size_t smlen: length of signed message
 *              - const uint8_t *vk: pointer to bit-packed public key
 *
 * Returns 0 if signed message could be verified correctly and -1 otherwise
 **************************************************/
int crypto_sign_open(uint8_t *m, size_t *mlen, const uint8_t *sm, size_t smlen,
                     const uint8_t *ctx, size_t ctxlen, const uint8_t *vk) {
  size_t i;

  if (smlen < HAETAE_CRYPTO_BYTES)
    goto badsig;

  *mlen = smlen - HAETAE_CRYPTO_BYTES;
  if (crypto_sign_verify(sm, HAETAE_CRYPTO_BYTES, sm + HAETAE_CRYPTO_BYTES,
                         *mlen, ctx, ctxlen, vk))
    goto badsig;
  else {
    /* All good, copy msg, return 0 */
    for (i = 0; i < *mlen; ++i)
      m[i] = sm[HAETAE_CRYPTO_BYTES + i];
    return 0;
  }

badsig:
  /* Signature verification failed */
  *mlen = -1;
  for (i = 0; i < smlen; ++i)
    m[i] = 0;

  return -1;
}

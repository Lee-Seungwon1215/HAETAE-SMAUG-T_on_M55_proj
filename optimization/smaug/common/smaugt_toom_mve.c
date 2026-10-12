// SPDX-License-Identifier: MIT

#include "smaugt_toom_mve.h"

#include <arm_mve.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define TOOM_PART 64
#define TOOM_PRODUCT (2 * TOOM_PART - 1)

typedef void (*mul64_fn)(const int16_t *, const int16_t *, uint16_t *);

#if defined(SMAUGT_TOOM_ASM)
extern void smaugt_toom_evaluate_m55(const int16_t *a, int16_t *w,
                                     uint16_t mask);
extern void smaugt_toom_mul64_outer32_m55(const int16_t *a,
                                          const int16_t *b, uint16_t *out);
extern void smaugt_toom_mul64_outer32_acc_m55(const int16_t *a,
                                              const int16_t *b,
                                              uint16_t *accumulated);
extern void smaugt_toom_interpolate_add_m55(uint16_t *c, uint16_t *w);
extern void smaugt_toom_reduce_add_m55(int16_t *res, const uint16_t *c,
                                       unsigned shift);
#endif

/*
 * Constant-time argument shared by all kernels in this file:
 *   - loop bounds depend only on the public sizes 16, 32, 64, 127 and 256;
 *   - every load/store address is an affine function of public loop indices;
 *   - MVE predicates are tail predicates derived only from public lengths.
 * Coefficient values influence arithmetic only and never control a branch,
 * loop trip count, predicate, or address.
 */

/*
 * Convolution modulo 2^16. Reversing b makes every diagonal a dot product of
 * two sequential slices, so VMLADAVA consumes eight 16-bit products at once.
 * Narrowing the accumulator after each group is exact modulo 2^16 and also
 * prevents a C signed-overflow assumption from spanning multiple groups.
 */
static void schoolbook_dot(const int16_t *a, const int16_t *b, uint16_t *out,
                           unsigned n) {
  int16_t reversed[TOOM_PART];

  for (unsigned i = 0; i < n; i++) {
    reversed[i] = b[n - 1u - i];
  }

  for (unsigned k = 0; k < 2u * n - 1u; k++) {
    const unsigned first = k < n ? 0u : k - n + 1u;
    const unsigned last = k < n ? k : n - 1u;
    const unsigned count = last - first + 1u;
    int32_t acc = 0;

    for (unsigned done = 0; done < count; done += 8) {
      const unsigned left = count - done;
      const mve_pred16_t p = vctp16q(left);
      const int16x8_t va = vldrhq_z_s16(a + first + done, p);
      const int16x8_t vb =
          vldrhq_z_s16(reversed + (n - 1u - k + first) + done, p);
      acc = (int16_t)vmladavaq_p_s16(acc, va, vb, p);
    }
    out[k] = (uint16_t)acc;
  }
}

static void compose_karatsuba(uint16_t *out, const uint16_t *z0,
                              const uint16_t *z1, const uint16_t *z2,
                              unsigned half) {
  const unsigned sub_product = 2u * half - 1u;
  const unsigned product = 4u * half - 1u;

  memset(out, 0, product * sizeof(uint16_t));
  for (unsigned i = 0; i < sub_product; i++) {
    out[i] = (uint16_t)(out[i] + z0[i]);
    out[i + 2u * half] = (uint16_t)(out[i + 2u * half] + z2[i]);
    out[i + half] =
        (uint16_t)(out[i + half] + z1[i] - z0[i] - z2[i]);
  }
}

static void karatsuba32(const int16_t *a, const int16_t *b, uint16_t *out) {
  int16_t asum[16], bsum[16];
  uint16_t z0[31], z1[31], z2[31];

  for (unsigned i = 0; i < 16; i += 8) {
    const int16x8_t a0 = vld1q_s16(a + i);
    const int16x8_t a1 = vld1q_s16(a + 16 + i);
    const int16x8_t b0 = vld1q_s16(b + i);
    const int16x8_t b1 = vld1q_s16(b + 16 + i);
    vst1q_s16(asum + i, vaddq_s16(a0, a1));
    vst1q_s16(bsum + i, vaddq_s16(b0, b1));
  }
  schoolbook_dot(a, b, z0, 16);
  schoolbook_dot(a + 16, b + 16, z2, 16);
  schoolbook_dot(asum, bsum, z1, 16);
  compose_karatsuba(out, z0, z1, z2, 16);
}

/* B1: the reference's two Karatsuba levels (64 -> 32 -> 16), MVE leaves. */
static void mul64_b1(const int16_t *a, const int16_t *b, uint16_t *out) {
  int16_t asum[32], bsum[32];
  uint16_t z0[63], z1[63], z2[63];

  for (unsigned i = 0; i < 32; i += 8) {
    const int16x8_t a0 = vld1q_s16(a + i);
    const int16x8_t a1 = vld1q_s16(a + 32 + i);
    const int16x8_t b0 = vld1q_s16(b + i);
    const int16x8_t b1 = vld1q_s16(b + 32 + i);
    vst1q_s16(asum + i, vaddq_s16(a0, a1));
    vst1q_s16(bsum + i, vaddq_s16(b0, b1));
  }
  karatsuba32(a, b, z0);
  karatsuba32(a + 32, b + 32, z2);
  karatsuba32(asum, bsum, z1);
  compose_karatsuba(out, z0, z1, z2, 32);
}

/* B2: no Karatsuba; one 64x64 VMLADAVA schoolbook convolution. */
static void mul64_b2(const int16_t *a, const int16_t *b, uint16_t *out) {
  schoolbook_dot(a, b, out, 64);
}

/* B3: one Karatsuba level (64 -> 32), then VMLADAVA schoolbook leaves. */
static void mul64_b3(const int16_t *a, const int16_t *b, uint16_t *out) {
  int16_t asum[32], bsum[32];
  uint16_t z0[63], z1[63], z2[63];

  for (unsigned i = 0; i < 32; i += 8) {
    const int16x8_t a0 = vld1q_s16(a + i);
    const int16x8_t a1 = vld1q_s16(a + 32 + i);
    const int16x8_t b0 = vld1q_s16(b + i);
    const int16x8_t b1 = vld1q_s16(b + 32 + i);
    vst1q_s16(asum + i, vaddq_s16(a0, a1));
    vst1q_s16(bsum + i, vaddq_s16(b0, b1));
  }
  schoolbook_dot(a, b, z0, 32);
  schoolbook_dot(a + 32, b + 32, z2, 32);
  schoolbook_dot(asum, bsum, z1, 32);
  compose_karatsuba(out, z0, z1, z2, 32);
}

/*
 * B6 outer leaf, 64x64 -> 127 coefficients in Z_2^16.
 * CT: both loop bounds are the public constant 64, and every address is an
 * affine function of public i/j. Coefficients affect only wrapping MVE
 * arithmetic; there are no secret-dependent branches, addresses, or loops.
 */
static void mul64_b6(const int16_t *a, const int16_t *b, uint16_t *out) {
  memset(out, 0, TOOM_PRODUCT * sizeof(uint16_t));
  for (unsigned i = 0; i < 64; i++) {
    const int16_t ai = a[i];
    for (unsigned j = 0; j < 64; j += 8) {
      int16x8_t acc = vld1q_s16((const int16_t *)out + i + j);
      const int16x8_t bv = vld1q_s16(b + j);
      acc = vmlaq_n_s16(acc, bv, ai);
      vst1q_s16((int16_t *)out + i + j, acc);
    }
  }
}

/*
 * 32-coefficient outer leaf used by the one-level Karatsuba candidate.
 * CT: the 32-by-4 iteration space and all addresses are public and fixed;
 * coefficients select no branch, address, predicate, or loop trip count.
 */
#if !defined(SMAUGT_TOOM_ASM_MULTIPLY)
static void schoolbook_outer32(const int16_t *a, const int16_t *b,
                               uint16_t out[63]) {
  memset(out, 0, 63 * sizeof(uint16_t));
  for (unsigned i = 0; i < 32; i++) {
    const int16_t ai = a[i];
    for (unsigned j = 0; j < 32; j += 8) {
      int16x8_t acc = vld1q_s16((const int16_t *)out + i + j);
      const int16x8_t bv = vld1q_s16(b + j);
      acc = vmlaq_n_s16(acc, bv, ai);
      vst1q_s16((int16_t *)out + i + j, acc);
    }
  }
}
#endif

/*
 * Round-1 call-boundary derivative of the same 32x32 outer leaf.
 * CT is identical to schoolbook_outer32: fixed public loops and affine
 * addresses, with no coefficient-dependent branch, address, or iteration.
 */
__attribute__((noinline)) static void
schoolbook_outer32_leafcall(const int16_t *a, const int16_t *b,
                            uint16_t out[63]) {
  memset(out, 0, 63 * sizeof(uint16_t));
  for (unsigned i = 0; i < 32; i++) {
    const int16_t ai = a[i];
    for (unsigned j = 0; j < 32; j += 8) {
      int16x8_t acc = vld1q_s16((const int16_t *)out + i + j);
      const int16x8_t bv = vld1q_s16(b + j);
      acc = vmlaq_n_s16(acc, bv, ai);
      vst1q_s16((int16_t *)out + i + j, acc);
    }
  }
}

/*
 * Round-0 outer32 candidate: one public 64->32 Karatsuba split and three
 * outer leaves. CT follows from the fixed split/leaf loops and affine access.
 */
static void mul64_outer32(const int16_t *a, const int16_t *b, uint16_t *out) {
#if defined(SMAUGT_TOOM_ASM_MULTIPLY)
  smaugt_toom_mul64_outer32_m55(a, b, out);
#else
  int16_t asum[32], bsum[32];
  uint16_t z0[63], z1[63], z2[63];

  for (unsigned i = 0; i < 32; i += 8) {
    const int16x8_t a0 = vld1q_s16(a + i);
    const int16x8_t a1 = vld1q_s16(a + 32 + i);
    const int16x8_t b0 = vld1q_s16(b + i);
    const int16x8_t b1 = vld1q_s16(b + 32 + i);
    vst1q_s16(asum + i, vaddq_s16(a0, a1));
    vst1q_s16(bsum + i, vaddq_s16(b0, b1));
  }
  schoolbook_outer32(a, b, z0);
  schoolbook_outer32(a + 32, b + 32, z2);
  schoolbook_outer32(asum, bsum, z1);
  compose_karatsuba(out, z0, z1, z2, 32);
#endif
}

/*
 * Round-1 derivative: only the 32x32 leaf inlining boundary differs from
 * mul64_outer32. All control remains fixed/public and therefore constant-time.
 */
static void mul64_outer32_leafcall(const int16_t *a, const int16_t *b,
                                   uint16_t *out) {
  int16_t asum[32], bsum[32];
  uint16_t z0[63], z1[63], z2[63];

  for (unsigned i = 0; i < 32; i += 8) {
    const int16x8_t a0 = vld1q_s16(a + i);
    const int16x8_t a1 = vld1q_s16(a + 32 + i);
    const int16x8_t b0 = vld1q_s16(b + i);
    const int16x8_t b1 = vld1q_s16(b + 32 + i);
    vst1q_s16(asum + i, vaddq_s16(a0, a1));
    vst1q_s16(bsum + i, vaddq_s16(b0, b1));
  }
  schoolbook_outer32_leafcall(a, b, z0);
  schoolbook_outer32_leafcall(a + 32, b + 32, z2);
  schoolbook_outer32_leafcall(asum, bsum, z1);
  compose_karatsuba(out, z0, z1, z2, 32);
}

static inline uint32x4_t wrap16_u32(uint32x4_t x) {
  return vandq_u32(x, vdupq_n_u32(0xffffu));
}

static void evaluate(const int16_t *a, int16_t w[7][TOOM_PART],
                     uint16_t mask) {
#if defined(SMAUGT_TOOM_ASM_EVALUATE)
  smaugt_toom_evaluate_m55(a, &w[0][0], mask);
#else
  const int16x8_t vmask = vdupq_n_s16((int16_t)mask);
  const unsigned shift = (unsigned)__builtin_ctz((unsigned)mask);
  const int16x8_t vshift = vdupq_n_s16(-(int16_t)shift);
  /*
   * A literal masked representative is unsuitable for this Toom lift: its
   * interpolation performs exact divisions by powers of two. Factor the
   * public 2^shift out before evaluation and restore it after negacyclic
   * reduction. Thus the result is still (a & mask)*b modulo 2^16, while the
   * intermediate integer lift matches the reference's shift-copy path.
   */
  for (unsigned j = 0; j < TOOM_PART; j += 8) {
    const int16x8_t a0 =
        vshlq_s16(vandq_s16(vld1q_s16(a + j), vmask), vshift);
    const int16x8_t a1 =
        vshlq_s16(vandq_s16(vld1q_s16(a + TOOM_PART + j), vmask), vshift);
    const int16x8_t a2 =
        vshlq_s16(vandq_s16(vld1q_s16(a + 2 * TOOM_PART + j), vmask),
                  vshift);
    const int16x8_t a3 =
        vshlq_s16(vandq_s16(vld1q_s16(a + 3 * TOOM_PART + j), vmask),
                  vshift);
    const int16x8_t even = vaddq_s16(a0, a2);
    const int16x8_t odd = vaddq_s16(a1, a3);
    const int16x8_t scaled_even =
        vshlq_n_s16(vaddq_s16(vshlq_n_s16(a0, 2), a2), 1);
    const int16x8_t scaled_odd = vaddq_s16(vshlq_n_s16(a1, 2), a3);

    vst1q_s16(w[0] + j, a3);
    vst1q_s16(w[1] + j,
              vaddq_s16(vaddq_s16(vshlq_n_s16(a3, 3),
                                   vshlq_n_s16(a2, 2)),
                         vaddq_s16(vshlq_n_s16(a1, 1), a0)));
    vst1q_s16(w[2] + j, vaddq_s16(even, odd));
    vst1q_s16(w[3] + j, vsubq_s16(even, odd));
    vst1q_s16(w[4] + j, vaddq_s16(scaled_even, scaled_odd));
    vst1q_s16(w[5] + j, vsubq_s16(scaled_even, scaled_odd));
    vst1q_s16(w[6] + j, a0);
  }
#endif
}

static void interpolate_add(uint16_t c[512], uint16_t w[7][TOOM_PRODUCT]) {
#if defined(SMAUGT_TOOM_ASM_INTERPOLATE)
  smaugt_toom_interpolate_add_m55(c, &w[0][0]);
#else
  const uint16_t inv3 = 43691, inv9 = 36409, inv15 = 61167;

  /*
   * Use four widened lanes here. The reference assigns back to uint16_t after
   * each statement, represented by wrap16_u32(), but three divisions multiply
   * a signed 32-bit intermediate by an unsigned inverse before shifting. A
   * premature 16-bit truncate at those points is not equivalent.
   */
  for (unsigned i = 0; i < TOOM_PRODUCT; i += 4) {
    const unsigned left = TOOM_PRODUCT - i;
    const mve_pred16_t p = vctp32q(left);
    uint32x4_t r0 = vldrhq_z_u32(w[0] + i, p);
    uint32x4_t r1 = vldrhq_z_u32(w[1] + i, p);
    uint32x4_t r2 = vldrhq_z_u32(w[2] + i, p);
    uint32x4_t r3 = vldrhq_z_u32(w[3] + i, p);
    uint32x4_t r4 = vldrhq_z_u32(w[4] + i, p);
    uint32x4_t r5 = vldrhq_z_u32(w[5] + i, p);
    const uint32x4_t r6 = vldrhq_z_u32(w[6] + i, p);

    r1 = wrap16_u32(vaddq_u32(r1, r4));
    r5 = wrap16_u32(vsubq_u32(r5, r4));
    r3 = wrap16_u32(vreinterpretq_u32_s32(
        vshrq_n_s32(vreinterpretq_s32_u32(vsubq_u32(r3, r2)), 1)));
    r4 = wrap16_u32(vsubq_u32(r4, r0));
    r4 = wrap16_u32(vsubq_u32(r4, vshlq_n_u32(r6, 6)));
    r4 = wrap16_u32(vaddq_u32(vshlq_n_u32(r4, 1), r5));
    r2 = wrap16_u32(vaddq_u32(r2, r3));
    r1 = wrap16_u32(vsubq_u32(
        vsubq_u32(r1, vshlq_n_u32(r2, 6)), r2));
    r2 = wrap16_u32(vsubq_u32(r2, r6));
    r2 = wrap16_u32(vsubq_u32(r2, r0));
    r1 = wrap16_u32(vaddq_u32(r1, vmulq_n_u32(r2, 45)));
    r4 = wrap16_u32(vshrq_n_u32(
        vmulq_n_u32(vsubq_u32(r4, vshlq_n_u32(r2, 3)), inv3), 3));
    r5 = wrap16_u32(vaddq_u32(r5, r1));
    r1 = wrap16_u32(vshrq_n_u32(
        vmulq_n_u32(vaddq_u32(r1, vshlq_n_u32(r3, 4)), inv9), 1));
    r3 = wrap16_u32(vsubq_u32(vdupq_n_u32(0), vaddq_u32(r3, r1)));
    r5 = wrap16_u32(vshrq_n_u32(
        vmulq_n_u32(vsubq_u32(vmulq_n_u32(r1, 30), r5), inv15), 2));
    r2 = wrap16_u32(vsubq_u32(r2, r4));
    r1 = wrap16_u32(vsubq_u32(r1, r5));

    uint32x4_t dst = vldrhq_z_u32(c + i, p);
    vstrhq_p_u32(c + i, vaddq_u32(dst, r6), p);
    dst = vldrhq_z_u32(c + i + 64, p);
    vstrhq_p_u32(c + i + 64, vaddq_u32(dst, r5), p);
    dst = vldrhq_z_u32(c + i + 128, p);
    vstrhq_p_u32(c + i + 128, vaddq_u32(dst, r4), p);
    dst = vldrhq_z_u32(c + i + 192, p);
    vstrhq_p_u32(c + i + 192, vaddq_u32(dst, r3), p);
    dst = vldrhq_z_u32(c + i + 256, p);
    vstrhq_p_u32(c + i + 256, vaddq_u32(dst, r2), p);
    dst = vldrhq_z_u32(c + i + 320, p);
    vstrhq_p_u32(c + i + 320, vaddq_u32(dst, r1), p);
    dst = vldrhq_z_u32(c + i + 384, p);
    vstrhq_p_u32(c + i + 384, vaddq_u32(dst, r0), p);
  }
#endif
}

static void poly_mul_acc(const int16_t a[256], const int16_t b[256],
                         int16_t res[256], uint16_t mask,
                         mul64_fn multiply) {
  int16_t aw[7][TOOM_PART], bw[7][TOOM_PART];
  uint16_t w[7][TOOM_PRODUCT];
  uint16_t c[512];
  const unsigned shift = (unsigned)__builtin_ctz((unsigned)mask);

  memset(c, 0, sizeof(c));
  evaluate(a, aw, mask);
  evaluate(b, bw, 0xffffu);
  for (unsigned point = 0; point < 7; point++) {
    multiply(aw[point], bw[point], w[point]);
  }
  interpolate_add(c, w);

#if defined(SMAUGT_TOOM_ASM_REDUCE)
  smaugt_toom_reduce_add_m55(res, c, shift);
#else
  for (unsigned i = 0; i < 256; i += 8) {
    const int16x8_t old = vld1q_s16(res + i);
    const int16x8_t low = vld1q_s16((const int16_t *)c + i);
    const int16x8_t high = vld1q_s16((const int16_t *)c + 256 + i);
    const int16x8_t scaled =
        vshlq_s16(vsubq_s16(low, high), vdupq_n_s16((int16_t)shift));
    vst1q_s16(res + i, vaddq_s16(old, scaled));
  }
#endif
}

/*
 * Gated Round-0 direct 256x256 outer product in Z_2^16[x]/(x^256+1).
 * CT: both loops have fixed public bounds, mask is public, and every source,
 * product, and reduction address is affine in public indices. Coefficients
 * affect wrapping arithmetic only; there is no secret-dependent branch,
 * address, predicate, or iteration count.
 */
static void direct256_outer(const int16_t a[256], const int16_t b[256],
                            int16_t res[256], uint16_t mask) {
  int16_t product[512];

  memset(product, 0, sizeof(product));
  for (unsigned i = 0; i < 256; i++) {
    const int16_t ai = (int16_t)((uint16_t)a[i] & mask);
    for (unsigned j = 0; j < 256; j += 8) {
      int16x8_t acc = vld1q_s16(product + i + j);
      const int16x8_t bv = vld1q_s16(b + j);
      acc = vmlaq_n_s16(acc, bv, ai);
      vst1q_s16(product + i + j, acc);
    }
  }
  for (unsigned i = 0; i < 256; i += 8) {
    const int16x8_t old = vld1q_s16(res + i);
    const int16x8_t low = vld1q_s16(product + i);
    const int16x8_t high = vld1q_s16(product + 256 + i);
    vst1q_s16(res + i, vaddq_s16(old, vsubq_s16(low, high)));
  }
}

void smaugt_mve_cache_vector(smaugt_mve_eval_cache *cache,
                             const int16_t *b, unsigned terms) {
  for (unsigned term = 0; term < terms; term++) {
    evaluate(b + term * 256u, cache->points[term], 0xffffu);
  }
}

static void vec_mul_acc_cached(const int16_t *a,
                               const smaugt_mve_eval_cache *cache,
                               unsigned terms, unsigned stride, uint16_t mask,
                               int16_t res[256],
                               mul64_fn multiply) {
  int16_t aw[7][TOOM_PART];
#if !defined(SMAUGT_TOOM_ASM_FUSED)
  uint16_t product[TOOM_PRODUCT];
#else
  (void)multiply;
#endif
  uint16_t accumulated[7][TOOM_PRODUCT];
  uint16_t c[512];
  const unsigned shift = (unsigned)__builtin_ctz((unsigned)mask);

  memset(accumulated, 0, sizeof(accumulated));
  memset(c, 0, sizeof(c));
  for (unsigned term = 0; term < terms; term++) {
    evaluate(a + term * stride, aw, mask);
    for (unsigned point = 0; point < 7; point++) {
#if defined(SMAUGT_TOOM_ASM_FUSED)
      smaugt_toom_mul64_outer32_acc_m55(
          aw[point], cache->points[term][point], accumulated[point]);
#else
      multiply(aw[point], cache->points[term][point], product);
      for (unsigned i = 0; i < TOOM_PRODUCT; i += 8) {
        const unsigned left = TOOM_PRODUCT - i;
        const mve_pred16_t p = vctp16q(left);
        const uint16x8_t old = vldrhq_z_u16(accumulated[point] + i, p);
        const uint16x8_t add = vldrhq_z_u16(product + i, p);
        vstrhq_p_u16(accumulated[point] + i, vaddq_u16(old, add), p);
      }
#endif
    }
  }
  interpolate_add(c, accumulated);
#if defined(SMAUGT_TOOM_ASM_REDUCE)
  smaugt_toom_reduce_add_m55(res, c, shift);
#else
  for (unsigned i = 0; i < 256; i += 8) {
    const int16x8_t old = vld1q_s16(res + i);
    const int16x8_t low = vld1q_s16((const int16_t *)c + i);
    const int16x8_t high = vld1q_s16((const int16_t *)c + 256 + i);
    const int16x8_t scaled =
        vshlq_s16(vsubq_s16(low, high), vdupq_n_s16((int16_t)shift));
    vst1q_s16(res + i, vaddq_s16(old, scaled));
  }
#endif
}

void smaugt_poly_mul_acc_b1(const int16_t a[256], const int16_t b[256],
                            int16_t res[256]) {
  poly_mul_acc(a, b, res, 0xffffu, mul64_b1);
}

void smaugt_poly_mul_acc_b2(const int16_t a[256], const int16_t b[256],
                            int16_t res[256]) {
  poly_mul_acc(a, b, res, 0xffffu, mul64_b2);
}

void smaugt_poly_mul_acc_b3(const int16_t a[256], const int16_t b[256],
                            int16_t res[256]) {
  poly_mul_acc(a, b, res, 0xffffu, mul64_b3);
}

void smaugt_poly_mul_acc_b6(const int16_t a[256], const int16_t b[256],
                            int16_t res[256]) {
  poly_mul_acc(a, b, res, 0xffffu, mul64_b6);
}

void smaugt_poly_mul_acc_outer32(const int16_t a[256], const int16_t b[256],
                                 int16_t res[256]) {
  poly_mul_acc(a, b, res, 0xffffu, mul64_outer32);
}

void smaugt_poly_mul_acc_outer32_leafcall(const int16_t a[256],
                                          const int16_t b[256],
                                          int16_t res[256]) {
  poly_mul_acc(a, b, res, 0xffffu, mul64_outer32_leafcall);
}

void smaugt_poly_mul_acc_direct256_outer(const int16_t a[256],
                                         const int16_t b[256],
                                         int16_t res[256]) {
  direct256_outer(a, b, res, 0xffffu);
}

void smaugt_poly_mul_acc_masked_b1(const int16_t a[256],
                                   const int16_t b[256], int16_t res[256],
                                   uint16_t mask) {
  poly_mul_acc(a, b, res, mask, mul64_b1);
}

void smaugt_poly_mul_acc_masked_b2(const int16_t a[256],
                                   const int16_t b[256], int16_t res[256],
                                   uint16_t mask) {
  poly_mul_acc(a, b, res, mask, mul64_b2);
}

void smaugt_poly_mul_acc_masked_b3(const int16_t a[256],
                                   const int16_t b[256], int16_t res[256],
                                   uint16_t mask) {
  poly_mul_acc(a, b, res, mask, mul64_b3);
}

void smaugt_poly_mul_acc_masked_b6(const int16_t a[256],
                                   const int16_t b[256], int16_t res[256],
                                   uint16_t mask) {
  poly_mul_acc(a, b, res, mask, mul64_b6);
}

void smaugt_poly_mul_acc_masked_outer32(const int16_t a[256],
                                        const int16_t b[256],
                                        int16_t res[256], uint16_t mask) {
  poly_mul_acc(a, b, res, mask, mul64_outer32);
}

void smaugt_poly_mul_acc_masked_outer32_leafcall(const int16_t a[256],
                                                 const int16_t b[256],
                                                 int16_t res[256],
                                                 uint16_t mask) {
  poly_mul_acc(a, b, res, mask, mul64_outer32_leafcall);
}

void smaugt_poly_mul_acc_masked_direct256_outer(const int16_t a[256],
                                                const int16_t b[256],
                                                int16_t res[256],
                                                uint16_t mask) {
  direct256_outer(a, b, res, mask);
}

void smaugt_vec_mul_acc_cached_b1(const int16_t *a,
                                  const smaugt_mve_eval_cache *cache,
                                  unsigned terms, unsigned stride,
                                  uint16_t mask, int16_t res[256]) {
  vec_mul_acc_cached(a, cache, terms, stride, mask, res, mul64_b1);
}

void smaugt_vec_mul_acc_cached_b2(const int16_t *a,
                                  const smaugt_mve_eval_cache *cache,
                                  unsigned terms, unsigned stride,
                                  uint16_t mask, int16_t res[256]) {
  vec_mul_acc_cached(a, cache, terms, stride, mask, res, mul64_b2);
}

void smaugt_vec_mul_acc_cached_b3(const int16_t *a,
                                  const smaugt_mve_eval_cache *cache,
                                  unsigned terms, unsigned stride,
                                  uint16_t mask, int16_t res[256]) {
  vec_mul_acc_cached(a, cache, terms, stride, mask, res, mul64_b3);
}

void smaugt_vec_mul_acc_cached_b6(const int16_t *a,
                                  const smaugt_mve_eval_cache *cache,
                                  unsigned terms, unsigned stride,
                                  uint16_t mask, int16_t res[256]) {
  vec_mul_acc_cached(a, cache, terms, stride, mask, res, mul64_b6);
}

void smaugt_vec_mul_acc_cached_outer32(const int16_t *a,
                                       const smaugt_mve_eval_cache *cache,
                                       unsigned terms, unsigned stride,
                                       uint16_t mask, int16_t res[256]) {
  vec_mul_acc_cached(a, cache, terms, stride, mask, res, mul64_outer32);
}

void smaugt_vec_mul_acc_cached_outer32_leafcall(
    const int16_t *a, const smaugt_mve_eval_cache *cache, unsigned terms,
    unsigned stride, uint16_t mask, int16_t res[256]) {
  vec_mul_acc_cached(a, cache, terms, stride, mask, res,
                     mul64_outer32_leafcall);
}

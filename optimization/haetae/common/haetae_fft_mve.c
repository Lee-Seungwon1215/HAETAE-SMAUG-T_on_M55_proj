// SPDX-License-Identifier: MIT

#include "haetae_fft_mve.h"

#include <arm_mve.h>
#include <stdint.h>

static inline int32_t mulrnd16_scalar(int32_t x, int32_t y) {
  return (int32_t)((((int64_t)x * y) + (1 << 15)) >> 16);
}

/* Exact four-lane implementation of (x*y + 2^15) >> 16. */
static inline int32x4_t mulrnd16_wide(int32x4_t x, int32x4_t y) {
  int64x2_t even = vmullbq_int_s32(x, y);
  int64x2_t odd = vmulltq_int_s32(x, y);
  int64_t e0 = (even[0] + (1 << 15)) >> 16;
  int64_t e1 = (even[1] + (1 << 15)) >> 16;
  int64_t o0 = (odd[0] + (1 << 15)) >> 16;
  int64_t o1 = (odd[1] + (1 << 15)) >> 16;

  return (int32x4_t){(int32_t)e0, (int32_t)o0, (int32_t)e1, (int32_t)o1};
}

/*
 * Exact for Q16 twiddles -65536 <= y <= 65535:
 * vqrdmulh(x,y<<15) = (x*y + 2^15) >> 16.  The only +65536 twiddle is
 * handled by the scalar k=0 butterfly and never reaches this helper.
 */
static inline int32x4_t mulrnd16_qrd(int32x4_t x, int32x4_t y) {
  return vqrdmulhq_s32(x, vshlq_n_s32(y, 15));
}

#if defined(HAETAE_FFT_QRD)
#define MULRND16_VECTOR(x, y) mulrnd16_qrd((x), (y))
#else
#define MULRND16_VECTOR(x, y) mulrnd16_wide((x), (y))
#endif

void haetae_mulrnd16_wide4(int32_t out[4], const int32_t x[4],
                           const int32_t y[4]) {
  vst1q_s32(out, mulrnd16_wide(vld1q_s32(x), vld1q_s32(y)));
}

void haetae_mulrnd16_qrd4(int32_t out[4], const int32_t x[4],
                          const int32_t y[4]) {
  vst1q_s32(out, mulrnd16_qrd(vld1q_s32(x), vld1q_s32(y)));
}

static inline void roots_gpr(int32x4_t *wr, int32x4_t *wi,
                             const int32_t roots[512], unsigned int k,
                             unsigned int shift) {
  unsigned int t0 = k << shift;
  unsigned int t1 = (k + 1u) << shift;
  unsigned int t2 = (k + 2u) << shift;
  unsigned int t3 = (k + 3u) << shift;
  *wr = (int32x4_t){roots[2u * t0], roots[2u * t1], roots[2u * t2],
                    roots[2u * t3]};
  *wi = (int32x4_t){roots[2u * t0 + 1u], roots[2u * t1 + 1u],
                    roots[2u * t2 + 1u], roots[2u * t3 + 1u]};
}

static inline void roots_vector_table(int32x4_t *wr, int32x4_t *wi,
                                      const int32_t roots[512], unsigned int k,
                                      unsigned int shift) {
  uint32x4_t offsets =
      (uint32x4_t){8u * (k << shift), 8u * ((k + 1u) << shift),
                   8u * ((k + 2u) << shift), 8u * ((k + 3u) << shift)};
  *wr = vldrwq_gather_offset_s32(roots, offsets);
  *wi = vldrwq_gather_offset_s32(roots + 1, offsets);
}

static inline void scalar_butterfly(int32_t data[512],
                                    const int32_t roots[512],
                                    unsigned int even, unsigned int odd,
                                    unsigned int twid) {
  int32_t ur = data[2u * even];
  int32_t ui = data[2u * even + 1u];
  int32_t xr = data[2u * odd];
  int32_t xi = data[2u * odd + 1u];
  int32_t wr = roots[2u * twid];
  int32_t wi = roots[2u * twid + 1u];
  int32_t tr = mulrnd16_scalar(xr, wr) - mulrnd16_scalar(xi, wi);
  int32_t ti = mulrnd16_scalar(xr, wi) + mulrnd16_scalar(xi, wr);

  data[2u * even] = ur + tr;
  data[2u * even + 1u] = ui + ti;
  data[2u * odd] = ur - tr;
  data[2u * odd + 1u] = ui - ti;
}

static inline void fft_layers_interleaved(int32_t data[512],
                                          const int32_t roots[512],
                                          unsigned int first,
                                          unsigned int last) {
  for (unsigned int r = first; r <= last; r++) {
    unsigned int m = 1u << r;
    unsigned int half = m >> 1;
    unsigned int shift = 9u - r;

    for (unsigned int n = 0; n < HAETAE_FFT_MVE_N; n += m) {
      unsigned int k = 0;

      /* k=0 includes the +65536 root and is exact without multiplication. */
      scalar_butterfly(data, roots, n, n + half, 0);
      k = 1;
      for (; k + 3u < half; k += 4u) {
        unsigned int even = n + k;
        unsigned int odd = even + half;
        int32x4x2_t u = vld2q_s32(data + 2u * even);
        int32x4x2_t x = vld2q_s32(data + 2u * odd);
        int32x4_t wr, wi;
        int32x4_t tr, ti;
        int32x4x2_t left, right;

#if defined(HAETAE_FFT_VTBL)
        roots_vector_table(&wr, &wi, roots, k, shift);
#else
        roots_gpr(&wr, &wi, roots, k, shift);
#endif
        tr = vsubq_s32(MULRND16_VECTOR(x.val[0], wr),
                       MULRND16_VECTOR(x.val[1], wi));
        ti = vaddq_s32(MULRND16_VECTOR(x.val[0], wi),
                       MULRND16_VECTOR(x.val[1], wr));
        left.val[0] = vaddq_s32(u.val[0], tr);
        left.val[1] = vaddq_s32(u.val[1], ti);
        right.val[0] = vsubq_s32(u.val[0], tr);
        right.val[1] = vsubq_s32(u.val[1], ti);
        vst2q_s32(data + 2u * even, left);
        vst2q_s32(data + 2u * odd, right);
      }
      for (; k < half; k++) {
        scalar_butterfly(data, roots, n + k, n + k + half, k << shift);
      }
    }
  }
}

static inline void fft_interleaved(int32_t data[512],
                                   const int32_t roots[512]) {
#if defined(HAETAE_FFT_L332)
  fft_layers_interleaved(data, roots, 1, 3);
  fft_layers_interleaved(data, roots, 4, 6);
  fft_layers_interleaved(data, roots, 7, 8);
#elif defined(HAETAE_FFT_L44)
  fft_layers_interleaved(data, roots, 1, 4);
  fft_layers_interleaved(data, roots, 5, 8);
#else
  fft_layers_interleaved(data, roots, 1, 2);
  fft_layers_interleaved(data, roots, 3, 4);
  fft_layers_interleaved(data, roots, 5, 6);
  fft_layers_interleaved(data, roots, 7, 8);
#endif
}

static inline void fft_layers_soa(int32_t real[256], int32_t imag[256],
                                  const int32_t roots[512], unsigned int first,
                                  unsigned int last) {
  for (unsigned int r = first; r <= last; r++) {
    unsigned int m = 1u << r;
    unsigned int half = m >> 1;
    unsigned int shift = 9u - r;

    for (unsigned int n = 0; n < HAETAE_FFT_MVE_N; n += m) {
      unsigned int k = 0;
      for (; k + 3u < half; k += 4u) {
        unsigned int even = n + k;
        unsigned int odd = even + half;
        int32x4_t ur = vld1q_s32(real + even);
        int32x4_t ui = vld1q_s32(imag + even);
        int32x4_t xr = vld1q_s32(real + odd);
        int32x4_t xi = vld1q_s32(imag + odd);
        int32x4_t wr, wi, tr, ti;

        if (k == 0u) {
#if defined(HAETAE_FFT_VTBL)
          roots_vector_table(&wr, &wi, roots, 0, shift);
#else
          roots_gpr(&wr, &wi, roots, 0, shift);
#endif
          /* Lane zero is repaired below because +65536 cannot be shifted. */
          wr[0] = 0;
        } else {
#if defined(HAETAE_FFT_VTBL)
          roots_vector_table(&wr, &wi, roots, k, shift);
#else
          roots_gpr(&wr, &wi, roots, k, shift);
#endif
        }
        tr = vsubq_s32(MULRND16_VECTOR(xr, wr), MULRND16_VECTOR(xi, wi));
        ti = vaddq_s32(MULRND16_VECTOR(xr, wi), MULRND16_VECTOR(xi, wr));
        if (k == 0u) {
          tr[0] = xr[0];
          ti[0] = xi[0];
        }
        vst1q_s32(real + even, vaddq_s32(ur, tr));
        vst1q_s32(imag + even, vaddq_s32(ui, ti));
        vst1q_s32(real + odd, vsubq_s32(ur, tr));
        vst1q_s32(imag + odd, vsubq_s32(ui, ti));
      }
      for (; k < half; k++) {
        unsigned int even = n + k;
        unsigned int odd = even + half;
        unsigned int twid = k << shift;
        int32_t ur = real[even], ui = imag[even];
        int32_t xr = real[odd], xi = imag[odd];
        int32_t wr = roots[2u * twid], wi = roots[2u * twid + 1u];
        int32_t tr = mulrnd16_scalar(xr, wr) - mulrnd16_scalar(xi, wi);
        int32_t ti = mulrnd16_scalar(xr, wi) + mulrnd16_scalar(xi, wr);
        real[even] = ur + tr;
        imag[even] = ui + ti;
        real[odd] = ur - tr;
        imag[odd] = ui - ti;
      }
    }
  }
}

/*
 * The first two layers use only 1 and -i.  This exact four-point kernel
 * removes all rounded products in those layers: multiply by 1 is identity and
 * multiply by -i maps (real,imag) to (imag,-real).  Its 64 public blocks and
 * addresses are fixed and contain no secret-dependent control flow.
 */
static inline void fft_first2_unit_soa(int32_t real[256], int32_t imag[256]) {
  for (unsigned int n = 0; n < 256; n += 4) {
    int32_t a0r = real[n] + real[n + 1u];
    int32_t a0i = imag[n] + imag[n + 1u];
    int32_t a1r = real[n] - real[n + 1u];
    int32_t a1i = imag[n] - imag[n + 1u];
    int32_t a2r = real[n + 2u] + real[n + 3u];
    int32_t a2i = imag[n + 2u] + imag[n + 3u];
    int32_t a3r = real[n + 2u] - real[n + 3u];
    int32_t a3i = imag[n + 2u] - imag[n + 3u];

    real[n] = a0r + a2r;
    imag[n] = a0i + a2i;
    real[n + 1u] = a1r + a3i;
    imag[n + 1u] = a1i - a3r;
    real[n + 2u] = a0r - a2r;
    imag[n + 2u] = a0i - a2i;
    real[n + 3u] = a1r - a3i;
    imag[n + 3u] = a1i + a3r;
  }
}

static void fft_soa(int32_t data[512], const int32_t roots[512]) {
  int32_t real[256];
  int32_t imag[256];

  for (unsigned int i = 0; i < 256; i += 4) {
    int32x4x2_t value = vld2q_s32(data + 2u * i);
    vst1q_s32(real + i, value.val[0]);
    vst1q_s32(imag + i, value.val[1]);
  }
#if defined(HAETAE_FFT_L332)
  fft_layers_soa(real, imag, roots, 1, 3);
  fft_layers_soa(real, imag, roots, 4, 6);
  fft_layers_soa(real, imag, roots, 7, 8);
#elif defined(HAETAE_FFT_L44)
  fft_layers_soa(real, imag, roots, 1, 4);
  fft_layers_soa(real, imag, roots, 5, 8);
#else
#if defined(HAETAE_FFT_UNIT4)
  fft_first2_unit_soa(real, imag);
#else
  fft_layers_soa(real, imag, roots, 1, 2);
#endif
  fft_layers_soa(real, imag, roots, 3, 4);
  fft_layers_soa(real, imag, roots, 5, 6);
  fft_layers_soa(real, imag, roots, 7, 8);
#endif
  for (unsigned int i = 0; i < 256; i += 4) {
    int32x4x2_t value = {{vld1q_s32(real + i), vld1q_s32(imag + i)}};
    vst2q_s32(data + 2u * i, value);
  }
}

/*
 * Fused final butterfly and squared-absolute accumulation.  The caller does
 * not consume the FFT array afterwards, so final real/imaginary outputs stay
 * in Q registers and are accumulated directly.  Exactly eight FFT layers,
 * 256 public coefficients, and fixed root addresses make
 * this constant-time under the same argument as haetae_fft_mve.
 */
void haetae_fft_sqabs_acc_mve(int32_t sum[256], int32_t data[512],
                              const int32_t roots[512]) {
  int32_t real[256];
  int32_t imag[256];
  const unsigned int shift = 1u; /* layer 8: twid = k << 1 */

  for (unsigned int i = 0; i < 256; i += 4) {
    int32x4x2_t value = vld2q_s32(data + 2u * i);
    vst1q_s32(real + i, value.val[0]);
    vst1q_s32(imag + i, value.val[1]);
  }
#if defined(HAETAE_FFT_UNIT4)
  fft_first2_unit_soa(real, imag);
#else
  fft_layers_soa(real, imag, roots, 1, 2);
#endif
  fft_layers_soa(real, imag, roots, 3, 4);
  fft_layers_soa(real, imag, roots, 5, 6);
  fft_layers_soa(real, imag, roots, 7, 7);

  for (unsigned int k = 0; k < 128; k += 4) {
    int32x4_t ur = vld1q_s32(real + k);
    int32x4_t ui = vld1q_s32(imag + k);
    int32x4_t xr = vld1q_s32(real + k + 128u);
    int32x4_t xi = vld1q_s32(imag + k + 128u);
    int32x4_t wr, wi, tr, ti;
    int32x4_t left_r, left_i, right_r, right_i;
    int32x4_t left_abs, right_abs;

#if defined(HAETAE_FFT_VTBL)
    roots_vector_table(&wr, &wi, roots, k, shift);
#else
    roots_gpr(&wr, &wi, roots, k, shift);
#endif
    if (k == 0u) {
      wr[0] = 0;
    }
    tr = vsubq_s32(MULRND16_VECTOR(xr, wr), MULRND16_VECTOR(xi, wi));
    ti = vaddq_s32(MULRND16_VECTOR(xr, wi), MULRND16_VECTOR(xi, wr));
    if (k == 0u) {
      tr[0] = xr[0];
      ti[0] = xi[0];
    }
    left_r = vaddq_s32(ur, tr);
    left_i = vaddq_s32(ui, ti);
    right_r = vsubq_s32(ur, tr);
    right_i = vsubq_s32(ui, ti);
    left_abs = vaddq_s32(mulrnd16_wide(left_r, left_r),
                         mulrnd16_wide(left_i, left_i));
    right_abs = vaddq_s32(mulrnd16_wide(right_r, right_r),
                          mulrnd16_wide(right_i, right_i));
    vst1q_s32(sum + k, vaddq_s32(vld1q_s32(sum + k), left_abs));
    vst1q_s32(sum + k + 128u,
              vaddq_s32(vld1q_s32(sum + k + 128u), right_abs));
  }
}

void haetae_fft_mve(int32_t data[512], const int32_t roots[512]) {
#if defined(HAETAE_FFT_SOA)
  fft_soa(data, roots);
#else
  fft_interleaved(data, roots);
#endif
}

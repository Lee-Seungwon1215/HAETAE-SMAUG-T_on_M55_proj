// SPDX-License-Identifier: MIT

#ifndef HAETAE_FFT_MVE_H
#define HAETAE_FFT_MVE_H

#include <stdint.h>

#define HAETAE_FFT_MVE_N 256

/*
 * The arrays contain interleaved Q16 real/imaginary int32 values.  All loops,
 * root addresses, and repetition counts are public and fixed; there are no
 * secret-dependent branches, addresses, predicates, or early exits.
 */
void haetae_fft_mve(int32_t data[2 * HAETAE_FFT_MVE_N],
                    const int32_t roots[2 * HAETAE_FFT_MVE_N]);
void haetae_fft_sqabs_acc_mve(int32_t sum[HAETAE_FFT_MVE_N],
                              int32_t data[2 * HAETAE_FFT_MVE_N],
                              const int32_t roots[2 * HAETAE_FFT_MVE_N]);

/* Unit-test entry points for the two exact Q16 multiplication candidates. */
void haetae_mulrnd16_wide4(int32_t out[4], const int32_t x[4],
                           const int32_t y[4]);
void haetae_mulrnd16_qrd4(int32_t out[4], const int32_t x[4],
                          const int32_t y[4]);

#endif

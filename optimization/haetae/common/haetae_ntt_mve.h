// SPDX-License-Identifier: MIT

#ifndef HAETAE_NTT_MVE_H
#define HAETAE_NTT_MVE_H

#include <stdint.h>

#define HAETAE_MVE_N 256

void haetae_ntt_mve_l44(int32_t a[HAETAE_MVE_N]);
void haetae_invntt_mve_l44(int32_t a[HAETAE_MVE_N]);
void haetae_ntt_mve_l332(int32_t a[HAETAE_MVE_N]);
void haetae_invntt_mve_l332(int32_t a[HAETAE_MVE_N]);
void haetae_ntt_mve_l332_vld2(int32_t a[HAETAE_MVE_N]);
void haetae_invntt_mve_l332_vld2(int32_t a[HAETAE_MVE_N]);
void haetae_ntt_mve_l332_vld4(int32_t a[HAETAE_MVE_N]);
void haetae_invntt_mve_l332_vld4(int32_t a[HAETAE_MVE_N]);
void haetae_ntt_mve_l332_vld4_m3(int32_t a[HAETAE_MVE_N]);
void haetae_invntt_mve_l332_vld4_m3(int32_t a[HAETAE_MVE_N]);
void haetae_ntt_mve_l332_vld4_u2(int32_t a[HAETAE_MVE_N]);
void haetae_invntt_mve_l332_vld4_u2(int32_t a[HAETAE_MVE_N]);

void haetae_pointwise_mve(int32_t c[HAETAE_MVE_N],
                          const int32_t a[HAETAE_MVE_N],
                          const int32_t b[HAETAE_MVE_N]);
void haetae_pointwise_acc_mve(int32_t c[HAETAE_MVE_N], const int32_t *a,
                              const int32_t *b, unsigned terms);
void haetae_pointwise_acc_precentered_mve(int32_t c[HAETAE_MVE_N],
                                          const int32_t *a, const int32_t *b,
                                          unsigned terms);
void haetae_pointwise_acc_u16_precentered_mve(
    int32_t c[HAETAE_MVE_N], const uint16_t *a, const int32_t *b,
    unsigned terms);
void haetae_barrett_tight_mve(int32_t dst[HAETAE_MVE_N],
                              const int32_t src[HAETAE_MVE_N]);
void haetae_reduce2q_mve(int32_t a[HAETAE_MVE_N]);
void haetae_freeze2q_mve(int32_t a[HAETAE_MVE_N]);
void haetae_freeze_mve(int32_t a[HAETAE_MVE_N]);

#endif

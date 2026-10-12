#ifndef M55_KECCAK_BENCH_H
#define M55_KECCAK_BENCH_H

#include <stdint.h>

void bench_keccak_prepare_permutation(void);
int bench_keccak_permutation(void);
void bench_keccak_prepare_stream(void);
int bench_keccak_stream(void);
int bench_keccak_measure_x4(void);
int bench_keccak_preflight_x4(void);
void bench_keccak_ct_prepare_x4(uint32_t class_id, uint32_t pair_index);
int bench_keccak_ct_x4(void);
void bench_shake128_ct_prepare_x4(uint32_t class_id, uint32_t pair_index);
int bench_shake128_ct_x4(void);

#endif

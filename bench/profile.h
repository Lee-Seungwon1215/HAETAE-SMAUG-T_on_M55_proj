#ifndef M55_BENCH_PROFILE_H
#define M55_BENCH_PROFILE_H

#include <stdint.h>

typedef enum {
  BENCH_CATEGORY_MULTIPLICATION = 0,
  BENCH_CATEGORY_KECCAK = 1,
  BENCH_CATEGORY_SAMPLER = 2,
  BENCH_CATEGORY_ENCODING = 3,
  BENCH_CATEGORY_PACKING = 4,
  BENCH_CATEGORY_FFT_FIXPOINT = 5,
  BENCH_CATEGORY_OTHER = 6,
  BENCH_CATEGORY_COUNT = 7
} bench_profile_category;

void bench_profile_begin(uint64_t start);
void bench_profile_enter(bench_profile_category category);
void bench_profile_exit(void);
void bench_profile_end(uint64_t end);
uint64_t bench_profile_ticks(bench_profile_category category);

#endif

#include "bench_common.h"
#include "platform.h"

#if defined(BENCH_PROFILE)
#include "profile.h"
#endif

#if defined(BENCH_REJECTION_COUNT)
#include "rejection_counter.h"
#endif

#include <stdint.h>

#ifndef BENCH_ITERATIONS
#define BENCH_ITERATIONS 3
#endif

#ifndef BENCH_PLATFORM_NAME
#define BENCH_PLATFORM_NAME "unknown"
#endif

#ifndef BENCH_WARMUP_ITERATIONS
#define BENCH_WARMUP_ITERATIONS 0
#endif

#if BENCH_ITERATIONS < 1
#error "BENCH_ITERATIONS must be at least 1"
#endif

static uint64_t measurement_samples[BENCH_ITERATIONS];

static uint64_t median(uint64_t *samples) {
  for (size_t i = 1; i < BENCH_ITERATIONS; i++) {
    uint64_t value = samples[i];
    size_t position = i;
    while (position != 0u && samples[position - 1u] > value) {
      samples[position] = samples[position - 1u];
      position--;
    }
    samples[position] = value;
  }

#if BENCH_ITERATIONS % 2 == 1
  return samples[BENCH_ITERATIONS / 2];
#else
  const uint64_t left = samples[BENCH_ITERATIONS / 2 - 1];
  const uint64_t right = samples[BENCH_ITERATIONS / 2];
  return left + (right - left) / 2u;
#endif
}

static void print_field(const char *name, uint64_t value) {
  platform_write(",");
  platform_write(name);
  platform_write("=");
  platform_write_u64(value);
}

void bench_print_header(const char *scheme, const char *implementation) {
  platform_write("KPQC_M55_BENCH,version=1,scheme=");
  platform_write(scheme);
  platform_write(",implementation=");
  platform_write(implementation);
  platform_write(",platform=");
  platform_write(BENCH_PLATFORM_NAME);
  platform_write(",timer=");
  platform_write(platform_timer_name());
  platform_write(",iterations=");
  platform_write_u32(BENCH_ITERATIONS);
  platform_write(",warmup_iterations=");
  platform_write_u32(BENCH_WARMUP_ITERATIONS);
  platform_write("\n");
  platform_print_metadata();
}

void bench_print_sizes3(const char *key1, size_t value1, const char *key2,
                        size_t value2, const char *key3, size_t value3) {
  platform_write("SIZES");
  print_field(key1, value1);
  print_field(key2, value2);
  print_field(key3, value3);
  platform_write("\n");
}

int bench_measure(const char *scheme, const char *implementation,
                  const char *operation,
                  bench_operation function) {
  return bench_measure_prepared(scheme, implementation, operation, NULL,
                                function);
}

int bench_measure_prepared(const char *scheme, const char *implementation,
                           const char *operation, bench_prepare prepare,
                           bench_operation function) {
  uint64_t minimum = UINT64_MAX;
  uint64_t maximum = 0u;
  uint64_t total = 0u;
  size_t operation_stack_peak;
#if defined(BENCH_PROFILE)
  uint64_t profile_total[BENCH_CATEGORY_COUNT] = {0u};
#endif

  platform_stack_operation_reset();
  platform_instruction_cache_reset();
#if BENCH_WARMUP_ITERATIONS > 0
  for (uint32_t warmup = 0; warmup < BENCH_WARMUP_ITERATIONS; warmup++) {
    int warmup_result;
    platform_heap_reset();
#if defined(BENCH_REJECTION_COUNT)
    bench_rejection_counter_reset();
#endif
    if (prepare != NULL) {
      prepare();
    }
    warmup_result = function();
    if (warmup_result != 0) {
      return warmup_result;
    }
  }
#endif
  for (uint32_t i = 0; i < BENCH_ITERATIONS; i++) {
    platform_heap_reset();
#if defined(BENCH_REJECTION_COUNT)
    bench_rejection_counter_reset();
#endif
    if (prepare != NULL) {
      prepare();
    }
    uint64_t start = platform_timer_now();
#if defined(BENCH_PROFILE)
    bench_profile_begin(start);
#endif
    int result = function();
    uint64_t end = platform_timer_now();
#if defined(BENCH_PROFILE)
    bench_profile_end(end);
#endif
    uint64_t elapsed = end - start;
    measurement_samples[i] = elapsed;

    if (result != 0) {
      platform_write("ERROR,scheme=");
      platform_write(scheme);
      platform_write(",implementation=");
      platform_write(implementation);
      platform_write(",operation=");
      platform_write(operation);
      platform_write(",code=");
      platform_write_u32((uint32_t)result);
      platform_write("\n");
      return result;
    }

    if (elapsed < minimum) {
      minimum = elapsed;
    }
    if (elapsed > maximum) {
      maximum = elapsed;
    }
    total += elapsed;

#if defined(BENCH_REJECTION_COUNT)
    {
      uint32_t candidates = bench_rejection_counter_read();
      if (candidates != 0u) {
        platform_write("CANDIDATES,scheme=");
        platform_write(scheme);
        platform_write(",implementation=");
        platform_write(implementation);
        platform_write(",operation=");
        platform_write(operation);
        print_field("sample", (uint64_t)i + 1u);
        print_field("attempts", candidates);
        print_field("rejections", candidates - 1u);
        platform_write("\n");
      }
    }
#endif
#if defined(BENCH_PROFILE)
    platform_write("PROFILE,scheme=");
    platform_write(scheme);
    platform_write(",implementation=");
    platform_write(implementation);
    platform_write(",operation=");
    platform_write(operation);
    print_field("sample", (uint64_t)i + 1u);
    print_field("multiplication",
                bench_profile_ticks(BENCH_CATEGORY_MULTIPLICATION));
    print_field("keccak", bench_profile_ticks(BENCH_CATEGORY_KECCAK));
    print_field("sampler", bench_profile_ticks(BENCH_CATEGORY_SAMPLER));
    print_field("encoding", bench_profile_ticks(BENCH_CATEGORY_ENCODING));
    print_field("packing", bench_profile_ticks(BENCH_CATEGORY_PACKING));
    print_field("fft_fixpoint",
                bench_profile_ticks(BENCH_CATEGORY_FFT_FIXPOINT));
    print_field("other", bench_profile_ticks(BENCH_CATEGORY_OTHER));
    platform_write("\n");
    for (size_t category = 0; category < BENCH_CATEGORY_COUNT; category++) {
      profile_total[category] +=
          bench_profile_ticks((bench_profile_category)category);
    }
#endif
  }
  operation_stack_peak = platform_stack_operation_peak_bytes();

#if !defined(BENCH_REJECTION_COUNT)
  /*
   * Keep UART and formatting code outside the timed sample sequence.  Printing
   * after every sample perturbs the instruction-cache state seen by the next
   * invocation, which is especially visible for short assembly kernels.
   */
  for (uint32_t i = 0; i < BENCH_ITERATIONS; i++) {
    platform_write("RESULT,scheme=");
    platform_write(scheme);
    platform_write(",implementation=");
    platform_write(implementation);
    platform_write(",operation=");
    platform_write(operation);
    print_field("sample", (uint64_t)i + 1u);
    print_field("ticks", measurement_samples[i]);
    platform_write("\n");
  }
#endif

  platform_write("SUMMARY,scheme=");
  platform_write(scheme);
  platform_write(",implementation=");
  platform_write(implementation);
  platform_write(",operation=");
  platform_write(operation);
  print_field("min", minimum);
  print_field("max", maximum);
  print_field("mean", total / BENCH_ITERATIONS);
  print_field("median", median(measurement_samples));
  platform_write("\n");
#if defined(BENCH_PROFILE)
  platform_write("PROFILE_SUMMARY,scheme=");
  platform_write(scheme);
  platform_write(",implementation=");
  platform_write(implementation);
  platform_write(",operation=");
  platform_write(operation);
  print_field("multiplication",
              profile_total[BENCH_CATEGORY_MULTIPLICATION] /
                  BENCH_ITERATIONS);
  print_field("keccak",
              profile_total[BENCH_CATEGORY_KECCAK] / BENCH_ITERATIONS);
  print_field("sampler",
              profile_total[BENCH_CATEGORY_SAMPLER] / BENCH_ITERATIONS);
  print_field("encoding",
              profile_total[BENCH_CATEGORY_ENCODING] / BENCH_ITERATIONS);
  print_field("packing",
              profile_total[BENCH_CATEGORY_PACKING] / BENCH_ITERATIONS);
  print_field("fft_fixpoint",
              profile_total[BENCH_CATEGORY_FFT_FIXPOINT] /
                  BENCH_ITERATIONS);
  print_field("other",
              profile_total[BENCH_CATEGORY_OTHER] / BENCH_ITERATIONS);
  platform_write("\n");
#endif
  platform_write("STACK,scheme=");
  platform_write(scheme);
  platform_write(",implementation=");
  platform_write(implementation);
  platform_write(",operation=");
  platform_write(operation);
  print_field("peak_bytes", operation_stack_peak);
  platform_write("\n");
  return 0;
}

void bench_print_footer(void) {
  platform_write("MEMORY,stack_peak_bytes=");
  platform_write_u64(platform_stack_peak_bytes());
  platform_write("\nSTATUS,ok\n");
}

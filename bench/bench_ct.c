#include "bench_ct.h"

#include "platform.h"

#include <stdint.h>

#ifndef BENCH_CT_PAIRS
#define BENCH_CT_PAIRS 1000u
#endif

#if BENCH_CT_PAIRS < 1
#error "BENCH_CT_PAIRS must be at least 1"
#endif

static uint32_t order_state = 0x6d2b79f5u;

static uint32_t next_order_bit(void) {
  uint32_t value = order_state;
  value ^= value << 13;
  value ^= value >> 17;
  value ^= value << 5;
  order_state = value;
  return value & 1u;
}

static void print_sample(uint32_t class_id, uint32_t pair_index,
                         uint32_t position, uint64_t ticks) {
  /* Compact records keep 10,000 samples below the STM32 output buffer limit. */
  platform_write("CT,c=");
  platform_write_u32(class_id);
  platform_write(",p=");
  platform_write_u32(pair_index);
  platform_write(",o=");
  platform_write_u32(position);
  platform_write(",t=");
  platform_write_u64(ticks);
  platform_write("\n");
}

static int measure_one(uint32_t class_id, uint32_t pair_index,
                       uint32_t position, bench_ct_prepare prepare,
                       bench_ct_operation function) {
  uint64_t start;
  uint64_t end;
  int result;

  platform_heap_reset();
  prepare(class_id, pair_index);
  start = platform_timer_now();
  result = function();
  end = platform_timer_now();
  if (result != 0) {
    return result;
  }
  print_sample(class_id, pair_index, position, end - start);
  return 0;
}

int bench_ct_measure(const char *scheme, const char *backend,
                     const char *operation, bench_ct_prepare prepare,
                     bench_ct_operation function) {
  int result;

  platform_write("CT_TEST,version=1,scheme=");
  platform_write(scheme);
  platform_write(",backend=");
  platform_write(backend);
  platform_write(",operation=");
  platform_write(operation);
  platform_write(",pairs=");
  platform_write_u32(BENCH_CT_PAIRS);
  platform_write(",class0=fixed,class1=random,order=randomized-pairs\n");

  /* Warm both paths before collecting either population. */
  for (uint32_t class_id = 0; class_id < 2u; class_id++) {
    platform_heap_reset();
    prepare(class_id, UINT32_MAX);
    result = function();
    if (result != 0) {
      return result;
    }
  }

  platform_stack_operation_reset();
  for (uint32_t pair = 0; pair < BENCH_CT_PAIRS; pair++) {
    const uint32_t first = next_order_bit();
    result = measure_one(first, pair, 0u, prepare, function);
    if (result != 0) {
      return result;
    }
    result = measure_one(first ^ 1u, pair, 1u, prepare, function);
    if (result != 0) {
      return result;
    }
  }

  platform_write("CT_STACK,peak_bytes=");
  platform_write_u64(platform_stack_operation_peak_bytes());
  platform_write("\nCT_STATUS,ok\n");
  return 0;
}

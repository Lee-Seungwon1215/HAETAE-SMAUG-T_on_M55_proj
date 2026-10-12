#include "profile.h"
#include "platform.h"

#include <stddef.h>

#define PROFILE_STACK_DEPTH 8u

static uint64_t category_ticks[BENCH_CATEGORY_COUNT];
static bench_profile_category category_stack[PROFILE_STACK_DEPTH];
static size_t category_depth;
static uint64_t last_timestamp;
static int profile_active;

void bench_profile_begin(uint64_t start) {
  for (size_t i = 0; i < BENCH_CATEGORY_COUNT; i++) {
    category_ticks[i] = 0u;
  }
  category_stack[0] = BENCH_CATEGORY_OTHER;
  category_depth = 1u;
  last_timestamp = start;
  profile_active = 1;
}

void bench_profile_enter(bench_profile_category category) {
  uint64_t now;

  if (profile_active == 0) {
    return;
  }
  now = platform_timer_now();
  category_ticks[category_stack[category_depth - 1u]] += now - last_timestamp;
  if (category_depth < PROFILE_STACK_DEPTH) {
    category_stack[category_depth++] = category;
  }
  last_timestamp = now;
}

void bench_profile_exit(void) {
  uint64_t now;

  if (profile_active == 0) {
    return;
  }
  now = platform_timer_now();
  category_ticks[category_stack[category_depth - 1u]] += now - last_timestamp;
  if (category_depth > 1u) {
    category_depth--;
  }
  last_timestamp = now;
}

void bench_profile_end(uint64_t end) {
  if (profile_active != 0) {
    category_ticks[category_stack[category_depth - 1u]] +=
        end - last_timestamp;
  }
  profile_active = 0;
}

uint64_t bench_profile_ticks(bench_profile_category category) {
  return category_ticks[category];
}

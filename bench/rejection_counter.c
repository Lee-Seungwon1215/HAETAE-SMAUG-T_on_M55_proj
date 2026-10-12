#include "rejection_counter.h"

#include "polyfix.h"

#define WRAP_NAME_INNER(name) __wrap_##name
#define WRAP_NAME(name) WRAP_NAME_INNER(name)
#define REAL_NAME_INNER(name) __real_##name
#define REAL_NAME(name) REAL_NAME_INNER(name)

static uint32_t candidate_count;

void bench_rejection_counter_reset(void) { candidate_count = 0u; }

uint32_t bench_rejection_counter_read(void) { return candidate_count; }

extern uint16_t REAL_NAME(polyfixveclk_sample_hyperball)(
    polyfixvecl *, polyfixveck *, uint8_t *, const uint8_t[HAETAE_CRHBYTES],
    uint16_t);

uint16_t WRAP_NAME(polyfixveclk_sample_hyperball)(
    polyfixvecl *y1, polyfixveck *y2, uint8_t *b,
    const uint8_t seed[HAETAE_CRHBYTES], uint16_t nonce) {
  candidate_count++;
  return REAL_NAME(polyfixveclk_sample_hyperball)(y1, y2, b, seed, nonce);
}

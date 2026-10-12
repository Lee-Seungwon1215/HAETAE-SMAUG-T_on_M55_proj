#include "keccak_bench.h"

#include "bench_common.h"
#include "fips202.h"
#include "keccak_m55/fips202x4_mve.h"
#include "keccak_m55/keccakx4_mve.h"
#include "string.h"

#include <stddef.h>
#include <stdint.h>

#define STREAM_INPUT_BYTES (2u * SHAKE256_RATE + 17u)
#define STREAM_OUTPUT_BYTES (2u * SHAKE256_RATE + 29u)

extern void m55_keccak_bench_permute(uint64_t state[25]);
#if defined(M55_KECCAK_PQM4)
extern void KeccakF1600_StatePermute_adomnicai_m4_standard(uint64_t state[25]);
#endif

static uint64_t permutation_state[25];
static uint8_t stream_input[STREAM_INPUT_BYTES];
static uint8_t stream_output[STREAM_OUTPUT_BYTES];
static uint64_t sequential_x4_states[4][25];
static m55_keccakx4_state vector_x4_state;

#if defined(BENCH_CT_TEST)
static uint32_t ct_keccak_state = 0xa4093822u;

static uint32_t ct_keccak_next(void) {
  uint32_t value = ct_keccak_state;
  value ^= value << 13;
  value ^= value >> 17;
  value ^= value << 5;
  ct_keccak_state = value;
  return value;
}
#endif

#define SHAKE_X4_TEST_MAX_INPUT (2u * SHAKE128_RATE + 9u)
#define SHAKE_X4_TEST_MAX_OUTPUT (2u * SHAKE128_RATE + 9u)
#define SHAKE_X4_TEST_GUARD 8u
#define SHAKE_X4_TEST_STRIDE                                                   \
  ((SHAKE_X4_TEST_MAX_OUTPUT + SHAKE_X4_TEST_GUARD + 15u) & ~(size_t)15u)

static void reference_permute_standard(uint64_t state[25]) {
#if defined(M55_KECCAK_PQM4)
  KeccakF1600_StatePermute_adomnicai_m4_standard(state);
#else
  m55_keccak_bench_permute(state);
#endif
}

void bench_keccak_prepare_permutation(void) {
  memset(permutation_state, 0, sizeof(permutation_state));
}

int bench_keccak_permutation(void) {
  m55_keccak_bench_permute(permutation_state);
  return 0;
}

void bench_keccak_prepare_stream(void) {
  for (size_t i = 0; i < sizeof(stream_input); i++) {
    stream_input[i] = (uint8_t)(i * 29u + 7u);
  }
  memset(stream_output, 0, sizeof(stream_output));
}

int bench_keccak_stream(void) {
  shake256incctx state;

  /* The deliberately fragmented public lengths cover partial-lane absorb and
   * squeeze boundaries as well as five fixed permutation calls. */
  shake256_inc_init(&state);
  shake256_inc_absorb(&state, stream_input, 19u);
  shake256_inc_absorb(&state, stream_input + 19u,
                      sizeof(stream_input) - 19u);
  shake256_inc_finalize(&state);
  shake256_inc_squeeze(stream_output, 23u, &state);
  shake256_inc_squeeze(stream_output + 23u,
                       sizeof(stream_output) - 23u, &state);
  shake256_inc_ctx_release(&state);
  return 0;
}

static int bench_keccak_x4_correctness(void) {
  uint64_t input[4][25];
  uint64_t expected[4][25];
  uint64_t actual[4][25];

  /* Exercise more than the all-zero timing state. This is deliberately kept
   * outside the timed region and compares every vector lane with the existing
   * standard-state permutation adapter. */
  for (size_t test = 0; test < 32u; test++) {
    for (size_t instance = 0; instance < 4u; instance++) {
      for (size_t lane = 0; lane < 25u; lane++) {
        input[instance][lane] =
            0x9e3779b97f4a7c15ULL *
                (uint64_t)(1u + lane + 25u * instance + 100u * test) ^
            (0xd1b54a32d192ed03ULL * (uint64_t)(test + instance));
        expected[instance][lane] = input[instance][lane];
      }
      reference_permute_standard(expected[instance]);
    }

    m55_keccakx4_import(&vector_x4_state, input);
    m55_keccakx4_permute(&vector_x4_state);
    m55_keccakx4_export(actual, &vector_x4_state);
    if (memcmp(actual, expected, sizeof(actual)) != 0) {
      return 91;
    }
  }
  return 0;
}

static int bench_shake128_x4_correctness(void) {
  static const struct {
    size_t inlen;
    size_t outlen;
  } cases[] = {
      {0u, 0u},
      {0u, 1u},
      {1u, 7u},
      {7u, 8u},
      {8u, 9u},
      {9u, SHAKE128_RATE - 1u},
      {SHAKE128_RATE - 1u, SHAKE128_RATE},
      {SHAKE128_RATE, SHAKE128_RATE + 1u},
      {SHAKE128_RATE + 1u, 2u * SHAKE128_RATE + 9u},
      {2u * SHAKE128_RATE + 9u, 2u * SHAKE128_RATE + 9u},
  };
  static const size_t input_offset[][4] = {
      {0u, 0u, 0u, 0u},
      {0u, 1u, 2u, 3u},
  };
  static const size_t output_offset[][4] = {
      {0u, 0u, 0u, 0u},
      {3u, 2u, 1u, 0u},
  };
  _Alignas(16) uint8_t shake_x4_input[4][SHAKE_X4_TEST_STRIDE];
  _Alignas(16) uint8_t shake_x4_expected[4][SHAKE_X4_TEST_STRIDE];
  _Alignas(16) uint8_t shake_x4_actual[4][SHAKE_X4_TEST_STRIDE];

  for (size_t instance = 0; instance < 4u; instance++) {
    for (size_t i = 0; i < sizeof(shake_x4_input[instance]); i++) {
      shake_x4_input[instance][i] =
          (uint8_t)(17u * i + 61u * instance + 3u);
    }
  }

  for (size_t alignment = 0;
       alignment < sizeof(input_offset) / sizeof(input_offset[0]);
       alignment++) {
    for (size_t test = 0; test < sizeof(cases) / sizeof(cases[0]); test++) {
      memset(shake_x4_expected, 0xa5, sizeof(shake_x4_expected));
      memset(shake_x4_actual, 0xa5, sizeof(shake_x4_actual));

      for (size_t instance = 0; instance < 4u; instance++) {
        shake128(
            shake_x4_expected[instance] + output_offset[alignment][instance],
            cases[test].outlen,
            shake_x4_input[instance] + input_offset[alignment][instance],
            cases[test].inlen);
      }
      m55_shake128x4(
          shake_x4_actual[0] + output_offset[alignment][0],
          shake_x4_actual[1] + output_offset[alignment][1],
          shake_x4_actual[2] + output_offset[alignment][2],
          shake_x4_actual[3] + output_offset[alignment][3], cases[test].outlen,
          shake_x4_input[0] + input_offset[alignment][0],
          shake_x4_input[1] + input_offset[alignment][1],
          shake_x4_input[2] + input_offset[alignment][2],
          shake_x4_input[3] + input_offset[alignment][3], cases[test].inlen);

      if (memcmp(shake_x4_actual, shake_x4_expected,
                 sizeof(shake_x4_actual)) != 0) {
        return (int)(100u + 16u * alignment + test);
      }
    }
  }
  return 0;
}

static void bench_keccak_prepare_x4_sequential(void) {
  memset(sequential_x4_states, 0, sizeof(sequential_x4_states));
}

static int bench_keccak_x4_sequential(void) {
  for (size_t instance = 0; instance < 4u; instance++) {
    m55_keccak_bench_permute(sequential_x4_states[instance]);
  }
  return 0;
}

static void bench_keccak_prepare_x4_mve(void) {
  m55_keccakx4_zero(&vector_x4_state);
}

static int bench_keccak_x4_mve(void) {
  m55_keccakx4_permute(&vector_x4_state);
  return 0;
}

void bench_keccak_ct_prepare_x4(uint32_t class_id, uint32_t pair_index) {
  (void)pair_index;
  for (size_t instance = 0; instance < 4u; instance++) {
    for (size_t lane = 0; lane < 25u; lane++) {
#if defined(BENCH_CT_TEST)
      const uint64_t low = class_id == 0u ? 0u : ct_keccak_next();
      const uint64_t high = class_id == 0u ? 0u : ct_keccak_next();
      sequential_x4_states[instance][lane] = low | (high << 32);
#else
      sequential_x4_states[instance][lane] = 0u;
#endif
    }
  }
  m55_keccakx4_import(&vector_x4_state, sequential_x4_states);
}

int bench_keccak_ct_x4(void) { return bench_keccak_x4_mve(); }

#define SHAKE_X4_BENCH_INPUT 34u
#define SHAKE_X4_BENCH_OUTPUT 168u

static uint8_t *bench_shake_x4_input(size_t instance) {
  return (uint8_t *)(void *)sequential_x4_states[instance];
}

static uint8_t *bench_shake_x4_output(size_t instance) {
  return (uint8_t *)(void *)&vector_x4_state + 200u * instance;
}

static void bench_shake128_x4_prepare(void) {
  for (size_t instance = 0; instance < 4u; instance++) {
    for (size_t i = 0; i < SHAKE_X4_BENCH_INPUT; i++) {
      bench_shake_x4_input(instance)[i] =
          (uint8_t)(31u * i + 47u * instance + 5u);
    }
    memset(bench_shake_x4_output(instance), 0, SHAKE_X4_BENCH_OUTPUT);
  }
}

static int bench_shake128_x4_sequential(void) {
  for (size_t instance = 0; instance < 4u; instance++) {
    shake128(bench_shake_x4_output(instance), SHAKE_X4_BENCH_OUTPUT,
             bench_shake_x4_input(instance), SHAKE_X4_BENCH_INPUT);
  }
  return 0;
}

static int bench_shake128_x4_mve(void) {
  m55_shake128x4(
      bench_shake_x4_output(0), bench_shake_x4_output(1),
      bench_shake_x4_output(2), bench_shake_x4_output(3),
      SHAKE_X4_BENCH_OUTPUT, bench_shake_x4_input(0),
      bench_shake_x4_input(1), bench_shake_x4_input(2),
      bench_shake_x4_input(3), SHAKE_X4_BENCH_INPUT);
  return 0;
}

void bench_shake128_ct_prepare_x4(uint32_t class_id, uint32_t pair_index) {
  (void)pair_index;
  for (size_t instance = 0; instance < 4u; instance++) {
    for (size_t i = 0; i < SHAKE_X4_BENCH_INPUT; i++) {
#if defined(BENCH_CT_TEST)
      bench_shake_x4_input(instance)[i] =
          class_id == 0u ? (uint8_t)(31u * i + 47u * instance + 5u)
                         : (uint8_t)ct_keccak_next();
#else
      bench_shake_x4_input(instance)[i] = 0u;
#endif
    }
    memset(bench_shake_x4_output(instance), 0, SHAKE_X4_BENCH_OUTPUT);
  }
}

int bench_shake128_ct_x4(void) { return bench_shake128_x4_mve(); }

int bench_keccak_preflight_x4(void) {
  int result = bench_keccak_x4_correctness();
  if (result != 0) {
    return result;
  }
  result = bench_shake128_x4_correctness();
  if (result != 0) {
    return result;
  }
  return 0;
}

int bench_keccak_measure_x4(void) {
  int result = bench_keccak_preflight_x4();
  if (result != 0) {
    return result;
  }
  if (bench_measure_prepared(BENCH_SCHEME_NAME, BENCH_IMPLEMENTATION,
                             "keccak_permutation_x4_sequential",
                             bench_keccak_prepare_x4_sequential,
                             bench_keccak_x4_sequential) != 0) {
    return 92;
  }
  if (bench_measure_prepared(BENCH_SCHEME_NAME, BENCH_IMPLEMENTATION,
                             "keccak_permutation_x4_mve",
                             bench_keccak_prepare_x4_mve,
                             bench_keccak_x4_mve) != 0 ||
      bench_measure_prepared(BENCH_SCHEME_NAME, BENCH_IMPLEMENTATION,
                             "shake128_x4_sequential",
                             bench_shake128_x4_prepare,
                             bench_shake128_x4_sequential) != 0) {
    return 94;
  }
  return bench_measure_prepared(BENCH_SCHEME_NAME, BENCH_IMPLEMENTATION,
                                "shake128_x4_mve",
                                bench_shake128_x4_prepare,
                                bench_shake128_x4_mve);
}

#include "diff_test.h"

#include "fips202.h"
#include "string.h"

#include <stddef.h>
#include <stdint.h>

#define HASH_INPUT_MAX (2u * SHAKE128_RATE + 19u)
#define HASH_OUTPUT_MAX (2u * SHAKE128_RATE + 31u)

extern void clean_shake128(uint8_t *, size_t, const uint8_t *, size_t);
extern void clean_shake256(uint8_t *, size_t, const uint8_t *, size_t);
extern void clean_sha3_256(uint8_t *, const uint8_t *, size_t);
extern void clean_sha3_512(uint8_t *, const uint8_t *, size_t);
extern void clean_shake128_inc_init(shake128incctx *);
extern void clean_shake128_inc_absorb(shake128incctx *, const uint8_t *,
                                      size_t);
extern void clean_shake128_inc_finalize(shake128incctx *);
extern void clean_shake128_inc_squeeze(uint8_t *, size_t, shake128incctx *);
extern void clean_shake128_inc_ctx_release(shake128incctx *);
extern void clean_shake256_inc_init(shake256incctx *);
extern void clean_shake256_inc_absorb(shake256incctx *, const uint8_t *,
                                      size_t);
extern void clean_shake256_inc_finalize(shake256incctx *);
extern void clean_shake256_inc_squeeze(uint8_t *, size_t, shake256incctx *);
extern void clean_shake256_inc_ctx_release(shake256incctx *);

static uint8_t hash_input[HASH_INPUT_MAX];
static uint8_t clean_hash_output[HASH_OUTPUT_MAX];
static uint8_t m55_hash_output[HASH_OUTPUT_MAX];

static void fill_hash_input(unsigned int test, size_t inlen) {
  uint32_t value = 0x6A09E667u ^ test;
  for (size_t i = 0; i < inlen; i++) {
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    hash_input[i] = (uint8_t)value;
  }
}

static int diff_shake128_incremental(size_t inlen, size_t outlen,
                                     size_t absorb_split,
                                     size_t squeeze_split) {
  shake128incctx clean_state;
  shake128incctx m55_state;

  clean_shake128_inc_init(&clean_state);
  shake128_inc_init(&m55_state);
  clean_shake128_inc_absorb(&clean_state, hash_input, absorb_split);
  shake128_inc_absorb(&m55_state, hash_input, absorb_split);
  clean_shake128_inc_absorb(&clean_state, hash_input + absorb_split,
                            inlen - absorb_split);
  shake128_inc_absorb(&m55_state, hash_input + absorb_split,
                      inlen - absorb_split);
  clean_shake128_inc_finalize(&clean_state);
  shake128_inc_finalize(&m55_state);
  clean_shake128_inc_squeeze(clean_hash_output, squeeze_split, &clean_state);
  shake128_inc_squeeze(m55_hash_output, squeeze_split, &m55_state);
  clean_shake128_inc_squeeze(clean_hash_output + squeeze_split,
                             outlen - squeeze_split, &clean_state);
  shake128_inc_squeeze(m55_hash_output + squeeze_split,
                       outlen - squeeze_split, &m55_state);
  clean_shake128_inc_ctx_release(&clean_state);
  shake128_inc_ctx_release(&m55_state);
  return memcmp(clean_hash_output, m55_hash_output, outlen) == 0 ? 0 : 305;
}

static int diff_shake256_incremental(size_t inlen, size_t outlen,
                                     size_t absorb_split,
                                     size_t squeeze_split) {
  shake256incctx clean_state;
  shake256incctx m55_state;

  clean_shake256_inc_init(&clean_state);
  shake256_inc_init(&m55_state);
  clean_shake256_inc_absorb(&clean_state, hash_input, absorb_split);
  shake256_inc_absorb(&m55_state, hash_input, absorb_split);
  clean_shake256_inc_absorb(&clean_state, hash_input + absorb_split,
                            inlen - absorb_split);
  shake256_inc_absorb(&m55_state, hash_input + absorb_split,
                      inlen - absorb_split);
  clean_shake256_inc_finalize(&clean_state);
  shake256_inc_finalize(&m55_state);
  clean_shake256_inc_squeeze(clean_hash_output, squeeze_split, &clean_state);
  shake256_inc_squeeze(m55_hash_output, squeeze_split, &m55_state);
  clean_shake256_inc_squeeze(clean_hash_output + squeeze_split,
                             outlen - squeeze_split, &clean_state);
  shake256_inc_squeeze(m55_hash_output + squeeze_split,
                       outlen - squeeze_split, &m55_state);
  clean_shake256_inc_ctx_release(&clean_state);
  shake256_inc_ctx_release(&m55_state);
  return memcmp(clean_hash_output, m55_hash_output, outlen) == 0 ? 0 : 306;
}

int bench_fips202_diff_case(unsigned int test) {
  const size_t inlen = (size_t)((test * 37u) % (HASH_INPUT_MAX + 1u));
  const size_t outlen = 1u + (size_t)((test * 53u) % HASH_OUTPUT_MAX);
  const size_t absorb_split = inlen == 0u ? 0u : (test * 11u) % (inlen + 1u);
  const size_t squeeze_split = (test * 17u) % (outlen + 1u);

  fill_hash_input(test, inlen);
  memset(clean_hash_output, 0, sizeof(clean_hash_output));
  memset(m55_hash_output, 0, sizeof(m55_hash_output));

  switch (test % 6u) {
  case 0u:
    clean_shake128(clean_hash_output, outlen, hash_input, inlen);
    shake128(m55_hash_output, outlen, hash_input, inlen);
    return memcmp(clean_hash_output, m55_hash_output, outlen) == 0 ? 0 : 301;
  case 1u:
    clean_shake256(clean_hash_output, outlen, hash_input, inlen);
    shake256(m55_hash_output, outlen, hash_input, inlen);
    return memcmp(clean_hash_output, m55_hash_output, outlen) == 0 ? 0 : 302;
  case 2u:
    clean_sha3_256(clean_hash_output, hash_input, inlen);
    sha3_256(m55_hash_output, hash_input, inlen);
    return memcmp(clean_hash_output, m55_hash_output, 32u) == 0 ? 0 : 303;
  case 3u:
    clean_sha3_512(clean_hash_output, hash_input, inlen);
    sha3_512(m55_hash_output, hash_input, inlen);
    return memcmp(clean_hash_output, m55_hash_output, 64u) == 0 ? 0 : 304;
  case 4u:
    return diff_shake128_incremental(inlen, outlen, absorb_split,
                                     squeeze_split);
  default:
    return diff_shake256_incremental(inlen, outlen, absorb_split,
                                     squeeze_split);
  }
}

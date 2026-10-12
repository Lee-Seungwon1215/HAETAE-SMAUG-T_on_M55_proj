#include "fips202x4_mve.h"

#include "fips202.h"
#include "keccakx4_mve.h"

static uint64_t load64(const uint8_t *input) {
  uint64_t value = 0;
  for (size_t i = 0; i < 8u; i++) {
    value |= (uint64_t)input[i] << (8u * i);
  }
  return value;
}

static uint64_t load_partial64(const uint8_t *input, size_t length) {
  uint64_t value = 0;
  for (size_t i = 0; i < length; i++) {
    value |= (uint64_t)input[i] << (8u * i);
  }
  return value;
}

static void store64(uint8_t *output, uint64_t value) {
  for (size_t i = 0; i < 8u; i++) {
    output[i] = (uint8_t)(value >> (8u * i));
  }
}

static void store_partial64(uint8_t *output, uint64_t value, size_t length) {
  for (size_t i = 0; i < length; i++) {
    output[i] = (uint8_t)(value >> (8u * i));
  }
}

static uint32x4_t gather4_u32(uint32_t value0, uint32_t value1,
                              uint32_t value2, uint32_t value3) {
  uint32x4_t vector = vdupq_n_u32(value0);
  vector = vsetq_lane_u32(value1, vector, 1);
  vector = vsetq_lane_u32(value2, vector, 2);
  vector = vsetq_lane_u32(value3, vector, 3);
  return vector;
}

static void xor_word(m55_keccakx4_state *state, size_t lane,
                     uint64_t value0, uint64_t value1, uint64_t value2,
                     uint64_t value3) {
  const uint32x4_t low =
      gather4_u32((uint32_t)value0, (uint32_t)value1, (uint32_t)value2,
                  (uint32_t)value3);
  const uint32x4_t high = gather4_u32(
      (uint32_t)(value0 >> 32), (uint32_t)(value1 >> 32),
      (uint32_t)(value2 >> 32), (uint32_t)(value3 >> 32));
  state->lo[lane] = veorq_u32(state->lo[lane], low);
  state->hi[lane] = veorq_u32(state->hi[lane], high);
}

static void xor_broadcast_byte(m55_keccakx4_state *state, size_t position,
                               uint8_t value) {
  const size_t lane = position / 8u;
  const unsigned int byte_in_lane = (unsigned int)(position % 8u);
  if (byte_in_lane < 4u) {
    state->lo[lane] = veorq_u32(
        state->lo[lane], vdupq_n_u32((uint32_t)value << (8u * byte_in_lane)));
  } else {
    state->hi[lane] = veorq_u32(
        state->hi[lane],
        vdupq_n_u32((uint32_t)value << (8u * (byte_in_lane - 4u))));
  }
}

static void absorb_block(m55_keccakx4_state *state, const uint8_t *input0,
                         const uint8_t *input1, const uint8_t *input2,
                         const uint8_t *input3, size_t length) {
  size_t offset = 0;
  size_t lane = 0;

  while (length - offset >= 8u) {
    xor_word(state, lane, load64(input0 + offset), load64(input1 + offset),
             load64(input2 + offset), load64(input3 + offset));
    offset += 8u;
    lane++;
  }
  if (offset < length) {
    const size_t tail = length - offset;
    xor_word(state, lane, load_partial64(input0 + offset, tail),
             load_partial64(input1 + offset, tail),
             load_partial64(input2 + offset, tail),
             load_partial64(input3 + offset, tail));
  }
}

static void squeeze_block(uint8_t *output0, uint8_t *output1,
                          uint8_t *output2, uint8_t *output3, size_t length,
                          const m55_keccakx4_state *state) {
  uint32_t low[4];
  uint32_t high[4];
  size_t offset = 0;
  size_t lane = 0;

  while (length - offset >= 8u) {
    vst1q_u32(low, state->lo[lane]);
    vst1q_u32(high, state->hi[lane]);
    store64(output0 + offset, (uint64_t)low[0] | ((uint64_t)high[0] << 32));
    store64(output1 + offset, (uint64_t)low[1] | ((uint64_t)high[1] << 32));
    store64(output2 + offset, (uint64_t)low[2] | ((uint64_t)high[2] << 32));
    store64(output3 + offset, (uint64_t)low[3] | ((uint64_t)high[3] << 32));
    offset += 8u;
    lane++;
  }
  if (offset < length) {
    const size_t tail = length - offset;
    vst1q_u32(low, state->lo[lane]);
    vst1q_u32(high, state->hi[lane]);
    store_partial64(output0 + offset,
                    (uint64_t)low[0] | ((uint64_t)high[0] << 32), tail);
    store_partial64(output1 + offset,
                    (uint64_t)low[1] | ((uint64_t)high[1] << 32), tail);
    store_partial64(output2 + offset,
                    (uint64_t)low[2] | ((uint64_t)high[2] << 32), tail);
    store_partial64(output3 + offset,
                    (uint64_t)low[3] | ((uint64_t)high[3] << 32), tail);
  }
}

void m55_shake128x4(uint8_t *output0, uint8_t *output1, uint8_t *output2,
                    uint8_t *output3, size_t outlen, const uint8_t *input0,
                    const uint8_t *input1, const uint8_t *input2,
                    const uint8_t *input3, size_t inlen) {
  m55_keccakx4_state state;

  m55_keccakx4_zero(&state);
  while (inlen >= SHAKE128_RATE) {
    absorb_block(&state, input0, input1, input2, input3, SHAKE128_RATE);
    m55_keccakx4_permute(&state);
    input0 += SHAKE128_RATE;
    input1 += SHAKE128_RATE;
    input2 += SHAKE128_RATE;
    input3 += SHAKE128_RATE;
    inlen -= SHAKE128_RATE;
  }
  absorb_block(&state, input0, input1, input2, input3, inlen);
  xor_broadcast_byte(&state, inlen, 0x1fu);
  xor_broadcast_byte(&state, SHAKE128_RATE - 1u, 0x80u);

  while (outlen != 0u) {
    const size_t block = outlen < SHAKE128_RATE ? outlen : SHAKE128_RATE;
    m55_keccakx4_permute(&state);
    squeeze_block(output0, output1, output2, output3, block, &state);
    output0 += block;
    output1 += block;
    output2 += block;
    output3 += block;
    outlen -= block;
  }
}

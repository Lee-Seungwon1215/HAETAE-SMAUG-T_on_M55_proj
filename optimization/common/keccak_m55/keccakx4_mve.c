#include "keccakx4_mve.h"

#include <stddef.h>

/* Standard Keccak-f[1600] round constants. Each 64-bit word is broadcast to
 * the four independent states only at the public, fixed Iota step. */
static const uint64_t keccak_round_constants[24] = {
    0x0000000000000001ULL, 0x0000000000008082ULL,
    0x800000000000808aULL, 0x8000000080008000ULL,
    0x000000000000808bULL, 0x0000000080000001ULL,
    0x8000000080008081ULL, 0x8000000000008009ULL,
    0x000000000000008aULL, 0x0000000000000088ULL,
    0x0000000080008009ULL, 0x000000008000000aULL,
    0x000000008000808bULL, 0x800000000000008bULL,
    0x8000000000008089ULL, 0x8000000000008003ULL,
    0x8000000000008002ULL, 0x8000000000000080ULL,
    0x000000000000800aULL, 0x800000008000000aULL,
    0x8000000080008081ULL, 0x8000000000008080ULL,
    0x0000000080000001ULL, 0x8000000080008008ULL,
};

/* MVE has no shifted operand on VEOR/VBIC like scalar Thumb-2. These macros
 * make every Rho offset a compile-time immediate, avoiding variable-shift
 * vectors and the scalar setup that they would require. */
#define ROL64_0(dst_lo, dst_hi, src_lo, src_hi)                               \
  do {                                                                        \
    (dst_lo) = (src_lo);                                                       \
    (dst_hi) = (src_hi);                                                       \
  } while (0)

#define ROL64_LT32(dst_lo, dst_hi, src_lo, src_hi, amount)                    \
  do {                                                                        \
    (dst_lo) = vorrq_u32(vshlq_n_u32((src_lo), (amount)),                     \
                         vshrq_n_u32((src_hi), 32 - (amount)));                \
    (dst_hi) = vorrq_u32(vshlq_n_u32((src_hi), (amount)),                     \
                         vshrq_n_u32((src_lo), 32 - (amount)));                \
  } while (0)

#define ROL64_GT32(dst_lo, dst_hi, src_lo, src_hi, amount)                    \
  do {                                                                        \
    (dst_lo) =                                                                \
        vorrq_u32(vshlq_n_u32((src_hi), (amount) - 32),                       \
                   vshrq_n_u32((src_lo), 64 - (amount)));                     \
    (dst_hi) =                                                                \
        vorrq_u32(vshlq_n_u32((src_lo), (amount) - 32),                       \
                   vshrq_n_u32((src_hi), 64 - (amount)));                     \
  } while (0)

#define RHO_PI_STEP_LT32(dst, amount)                                         \
  do {                                                                        \
    saved_lo = state->lo[(dst)];                                              \
    saved_hi = state->hi[(dst)];                                              \
    ROL64_LT32(rotated_lo, rotated_hi, t_lo, t_hi, (amount));                 \
    state->lo[(dst)] = rotated_lo;                                            \
    state->hi[(dst)] = rotated_hi;                                            \
    t_lo = saved_lo;                                                          \
    t_hi = saved_hi;                                                          \
  } while (0)

#define RHO_PI_STEP_GT32(dst, amount)                                         \
  do {                                                                        \
    saved_lo = state->lo[(dst)];                                              \
    saved_hi = state->hi[(dst)];                                              \
    ROL64_GT32(rotated_lo, rotated_hi, t_lo, t_hi, (amount));                 \
    state->lo[(dst)] = rotated_lo;                                            \
    state->hi[(dst)] = rotated_hi;                                            \
    t_lo = saved_lo;                                                          \
    t_hi = saved_hi;                                                          \
  } while (0)

void m55_keccakx4_zero(m55_keccakx4_state *state) {
  const uint32x4_t zero = vdupq_n_u32(0u);
  for (size_t lane = 0; lane < 25u; lane++) {
    state->lo[lane] = zero;
    state->hi[lane] = zero;
  }
}

void m55_keccakx4_import(m55_keccakx4_state *state,
                         const uint64_t input[4][25]) {
  uint32_t words[4];

  for (size_t lane = 0; lane < 25u; lane++) {
    for (size_t instance = 0; instance < 4u; instance++) {
      words[instance] = (uint32_t)input[instance][lane];
    }
    state->lo[lane] = vld1q_u32(words);
    for (size_t instance = 0; instance < 4u; instance++) {
      words[instance] = (uint32_t)(input[instance][lane] >> 32);
    }
    state->hi[lane] = vld1q_u32(words);
  }
}

void m55_keccakx4_export(uint64_t output[4][25],
                         const m55_keccakx4_state *state) {
  uint32_t low_words[4];
  uint32_t high_words[4];

  for (size_t lane = 0; lane < 25u; lane++) {
    vst1q_u32(low_words, state->lo[lane]);
    vst1q_u32(high_words, state->hi[lane]);
    for (size_t instance = 0; instance < 4u; instance++) {
      output[instance][lane] =
          (uint64_t)low_words[instance] | ((uint64_t)high_words[instance] << 32);
    }
  }
}

void m55_keccakx4_permute(m55_keccakx4_state *state) {
  uint32x4_t c_lo[5], c_hi[5];

  /* All loop bounds, indices, Rho offsets and addresses are public constants.
   * No state or message bit selects control flow or a memory address. */
  for (size_t round = 0; round < 24u; round++) {
    /* Theta: XOR the five lanes in each column. */
    for (size_t x = 0; x < 5u; x++) {
      c_lo[x] = veorq_u32(
          veorq_u32(state->lo[x], state->lo[x + 5u]),
          veorq_u32(veorq_u32(state->lo[x + 10u], state->lo[x + 15u]),
                    state->lo[x + 20u]));
      c_hi[x] = veorq_u32(
          veorq_u32(state->hi[x], state->hi[x + 5u]),
          veorq_u32(veorq_u32(state->hi[x + 10u], state->hi[x + 15u]),
                    state->hi[x + 20u]));
    }
    for (size_t x = 0; x < 5u; x++) {
      uint32x4_t rotated_lo, rotated_hi;
      uint32x4_t d_lo, d_hi;
      ROL64_LT32(rotated_lo, rotated_hi, c_lo[(x + 1u) % 5u],
                   c_hi[(x + 1u) % 5u], 1);
      d_lo = veorq_u32(c_lo[(x + 4u) % 5u], rotated_lo);
      d_hi = veorq_u32(c_hi[(x + 4u) % 5u], rotated_hi);
      for (size_t y = 0; y < 5u; y++) {
        const size_t lane = x + 5u * y;
        state->lo[lane] = veorq_u32(state->lo[lane], d_lo);
        state->hi[lane] = veorq_u32(state->hi[lane], d_hi);
      }
    }

    /* Rho/Pi form one 24-lane cycle around lane 0. Moving that cycle in place
     * avoids a 25-lane vector scratch state and its load/store traffic. */
    {
      uint32x4_t t_lo = state->lo[1];
      uint32x4_t t_hi = state->hi[1];
      uint32x4_t saved_lo, saved_hi, rotated_lo, rotated_hi;

      RHO_PI_STEP_LT32(10, 1);
      RHO_PI_STEP_LT32(7, 3);
      RHO_PI_STEP_LT32(11, 6);
      RHO_PI_STEP_LT32(17, 10);
      RHO_PI_STEP_LT32(18, 15);
      RHO_PI_STEP_LT32(3, 21);
      RHO_PI_STEP_LT32(5, 28);
      RHO_PI_STEP_GT32(16, 36);
      RHO_PI_STEP_GT32(8, 45);
      RHO_PI_STEP_GT32(21, 55);
      RHO_PI_STEP_LT32(24, 2);
      RHO_PI_STEP_LT32(4, 14);
      RHO_PI_STEP_LT32(15, 27);
      RHO_PI_STEP_GT32(23, 41);
      RHO_PI_STEP_GT32(19, 56);
      RHO_PI_STEP_LT32(13, 8);
      RHO_PI_STEP_LT32(12, 25);
      RHO_PI_STEP_GT32(2, 43);
      RHO_PI_STEP_GT32(20, 62);
      RHO_PI_STEP_LT32(14, 18);
      RHO_PI_STEP_GT32(22, 39);
      RHO_PI_STEP_GT32(9, 61);
      RHO_PI_STEP_LT32(6, 20);
      RHO_PI_STEP_GT32(1, 44);
    }

    /* Chi: a ^ ((~b) & c). Preserve only one five-lane row at a time;
     * VBIC(c,b) combines NOT and AND. */
    for (size_t y = 0; y < 5u; y++) {
      const size_t row = 5u * y;
      uint32x4_t a0 = state->lo[row];
      uint32x4_t a1 = state->lo[row + 1u];
      uint32x4_t a2 = state->lo[row + 2u];
      uint32x4_t a3 = state->lo[row + 3u];
      uint32x4_t a4 = state->lo[row + 4u];
      state->lo[row] = veorq_u32(a0, vbicq_u32(a2, a1));
      state->lo[row + 1u] = veorq_u32(a1, vbicq_u32(a3, a2));
      state->lo[row + 2u] = veorq_u32(a2, vbicq_u32(a4, a3));
      state->lo[row + 3u] = veorq_u32(a3, vbicq_u32(a0, a4));
      state->lo[row + 4u] = veorq_u32(a4, vbicq_u32(a1, a0));

      a0 = state->hi[row];
      a1 = state->hi[row + 1u];
      a2 = state->hi[row + 2u];
      a3 = state->hi[row + 3u];
      a4 = state->hi[row + 4u];
      state->hi[row] = veorq_u32(a0, vbicq_u32(a2, a1));
      state->hi[row + 1u] = veorq_u32(a1, vbicq_u32(a3, a2));
      state->hi[row + 2u] = veorq_u32(a2, vbicq_u32(a4, a3));
      state->hi[row + 3u] = veorq_u32(a3, vbicq_u32(a0, a4));
      state->hi[row + 4u] = veorq_u32(a4, vbicq_u32(a1, a0));
    }

    /* Iota. */
    state->lo[0] = veorq_u32(
        state->lo[0], vdupq_n_u32((uint32_t)keccak_round_constants[round]));
    state->hi[0] = veorq_u32(
        state->hi[0],
        vdupq_n_u32((uint32_t)(keccak_round_constants[round] >> 32)));
  }
}

#undef RHO_PI_STEP_GT32
#undef RHO_PI_STEP_LT32
#undef ROL64_GT32
#undef ROL64_LT32
#undef ROL64_0

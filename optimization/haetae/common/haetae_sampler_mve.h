// SPDX-License-Identifier: MIT

#ifndef HAETAE_SAMPLER_MVE_H
#define HAETAE_SAMPLER_MVE_H

#include <stdint.h>

#if !defined(HAETAE_SAMPLER_ASM)
#include <arm_mve.h>
#endif

typedef struct {
  uint16_t gauss16;
  uint64_t rejection48;
  uint64_t y_low48;
  uint32_t y_high24;
} haetae_gauss17_fields;

uint64_t haetae_sample_gauss16_cdt_mve(uint16_t random16);

/*
 * Load bytes 0..15 in one fixed vector access and byte 16 in one fixed scalar
 * access.  Word/lane extraction is fixed; no loaded value controls an address,
 * branch, predicate, or repetition count.
 */
#if defined(HAETAE_SAMPLER_ASM)
haetae_gauss17_fields haetae_load_gauss17_mve(const uint8_t random[17]);
#else
static inline haetae_gauss17_fields haetae_load_gauss17_mve(
    const uint8_t random[17]) {
  uint32x4_t words = vreinterpretq_u32_u8(vld1q_u8(random));
  uint32_t w0 = words[0];
  uint32_t w1 = words[1];
  uint32_t w2 = words[2];
  uint32_t w3 = words[3];
  haetae_gauss17_fields result;

  result.gauss16 = (uint16_t)w0;
  result.rejection48 = (uint64_t)(w0 >> 16) | ((uint64_t)w1 << 16);
  result.y_low48 = (uint64_t)w2 | ((uint64_t)(w3 & 0xFFFFu) << 32);
  result.y_high24 = (w3 >> 16) | ((uint32_t)random[16] << 16);
  return result;
}
#endif

#endif

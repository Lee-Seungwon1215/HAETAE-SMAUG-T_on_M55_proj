// SPDX-License-Identifier: MIT

#ifndef HAETAE_ASM_M55_H
#define HAETAE_ASM_M55_H

#include <stdint.h>

/* Backend-neutral public ABI. No MVE intrinsic type crosses this boundary. */
#define HAETAE_ASM_M55_N 256

typedef struct {
  uint16_t gauss16;
  uint64_t rejection48;
  uint64_t y_low48;
  uint32_t y_high24;
} haetae_asm_gauss17_fields;

uint64_t haetae_sample_gauss16_cdt_mve(uint16_t random16);
haetae_asm_gauss17_fields haetae_load_gauss17_mve(
    const uint8_t random[17]);

#endif

#include "fips202.h"
#include "randombytes.h"

#include <stddef.h>
#include <stdint.h>

/*
 * Deterministic benchmark-only input source.  Reproducible inputs are needed
 * for clean/M55 differential tests and paired timing runs.  This is not an
 * entropy source and MUST NOT be used by a deployed cryptographic product.
 */
int randombytes(uint8_t *output, size_t length) {
  static const uint8_t seed[32] = {
      0x4b, 0x50, 0x51, 0x43, 0x2d, 0x4d, 0x35, 0x35,
      0x2d, 0x53, 0x54, 0x4d, 0x33, 0x32, 0x4e, 0x36,
      0x35, 0x37, 0x2d, 0x42, 0x45, 0x4e, 0x43, 0x48,
      0x2d, 0x4f, 0x4e, 0x4c, 0x59, 0x2d, 0x52, 0x4e,
  };
  static uint64_t counter;
  uint8_t input[sizeof(seed) + sizeof(counter)];

  for (size_t i = 0; i < sizeof(seed); i++) {
    input[i] = seed[i];
  }
  for (size_t i = 0; i < sizeof(counter); i++) {
    input[sizeof(seed) + i] = (uint8_t)(counter >> (8u * i));
  }
  counter++;
  shake256(output, length, input, sizeof(input));
  return 0;
}

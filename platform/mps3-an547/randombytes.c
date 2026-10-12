#include "fips202.h"
#include "randombytes.h"

#include <stddef.h>
#include <stdint.h>

/*
 * Deterministic QEMU-only entropy source. This is intentionally reproducible
 * for functional tests and benchmarks and MUST NOT be used on real hardware.
 * A board port must replace this translation unit with its TRNG/HAL adapter.
 */
int randombytes(uint8_t *output, size_t length) {
  static const uint8_t seed[32] = {
      0x4b, 0x70, 0x71, 0x43, 0x6c, 0x65, 0x61, 0x6e,
      0x2d, 0x4d, 0x35, 0x35, 0x2d, 0x51, 0x45, 0x4d,
      0x55, 0x2d, 0x4f, 0x4e, 0x4c, 0x59, 0x2d, 0x44,
      0x4f, 0x2d, 0x4e, 0x4f, 0x54, 0x2d, 0x55, 0x53,
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

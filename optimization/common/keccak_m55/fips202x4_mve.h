#ifndef M55_FIPS202X4_MVE_H
#define M55_FIPS202X4_MVE_H

#include <stddef.h>
#include <stdint.h>

/* Compute four independent SHAKE128 instances with common public input and
 * output lengths. Vector lane n always carries instance n. */
void m55_shake128x4(uint8_t *output0, uint8_t *output1, uint8_t *output2,
                    uint8_t *output3, size_t outlen, const uint8_t *input0,
                    const uint8_t *input1, const uint8_t *input2,
                    const uint8_t *input3, size_t inlen);

#endif

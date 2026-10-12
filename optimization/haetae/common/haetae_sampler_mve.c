// SPDX-License-Identifier: MIT

#include "haetae_sampler_mve.h"

#include <arm_mve.h>
#include <stdint.h>

static const uint16_t cdt16[64] = {
    3266,  6520,  9748,  12938, 16079, 19159, 22168, 25096,
    27934, 30674, 33309, 35833, 38241, 40531, 42698, 44742,
    46663, 48460, 50135, 51690, 53128, 54454, 55670, 56781,
    57794, 58712, 59541, 60287, 60956, 61554, 62085, 62556,
    62972, 63337, 63657, 63936, 64178, 64388, 64569, 64724,
    64857, 64970, 65066, 65148, 65216, 65273, 65321, 65361,
    65394, 65422, 65444, 65463, 65478, 65490, 65500, 65508,
    65514, 65519, 65523, 65527, 65529, 65531, 65533, 65534};

uint64_t haetae_sample_gauss16_cdt_mve(uint16_t random16) {
  uint16x8_t count = vdupq_n_u16(0);
  uint16x8_t one = vdupq_n_u16(1);
  uint16x8_t zero = vdupq_n_u16(0);
  uint16x8_t random = vdupq_n_u16(random16);

  /* Fixed full-table scan: exactly eight groups and all 64 entries. */
  for (unsigned int i = 0; i < 64; i += 8) {
    uint16x8_t threshold = vld1q_u16(cdt16 + i);
    mve_pred16_t below = vcmphiq_u16(random, threshold);
    count = vaddq_u16(count, vpselq_u16(one, zero, below));
  }
  return (uint64_t)count[0] + count[1] + count[2] + count[3] + count[4] +
         count[5] + count[6] + count[7];
}

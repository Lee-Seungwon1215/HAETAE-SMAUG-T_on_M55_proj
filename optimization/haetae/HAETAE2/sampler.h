// SPDX-License-Identifier: MIT

#ifndef HAETAE_SAMPLER_H
#define HAETAE_SAMPLER_H

#include "fixpoint.h"
#include "params.h"
#if defined(HAETAE_STACK_GAUSS_OUTBUF03)
#include "symmetric.h"
#endif

#include <stdint.h>

#define rej_uniform HAETAE_NAMESPACE(rej_uniform)
unsigned int rej_uniform(int32_t *a, unsigned int len, const uint8_t *buf,
                         unsigned int buflen);
#define rej_eta HAETAE_NAMESPACE(rej_eta)
unsigned int rej_eta(int32_t *a, unsigned int len, const uint8_t *buf,
                     unsigned int buflen);

#define sample_gauss_N HAETAE_NAMESPACE(sample_gauss_N)
void sample_gauss_N(uint64_t *r, uint8_t *signs, fp96_76 *sqsum,
                    const uint8_t seed[HAETAE_CRHBYTES], uint16_t nonce,
                    size_t len);

#if defined(HAETAE_STACK_GAUSS_OUTBUF03)
#define HAETAE_GAUSS_N_BUFFER_BYTES                                           \
  (((17 * HAETAE_N + SHAKE256_RATE - 1) / SHAKE256_RATE) * SHAKE256_RATE)
#define sample_gauss HAETAE_NAMESPACE(sample_gauss)
int sample_gauss(uint64_t *r, fp96_76 *sqsum, const uint8_t *buf,
                 size_t buflen, size_t len, int dont_write_last);

static inline __attribute__((always_inline)) void sample_gauss_N_outbuf_inline(
    uint64_t *r, uint8_t *signs, fp96_76 *sqsum,
    const uint8_t seed[HAETAE_CRHBYTES], uint16_t nonce, size_t len,
    uint8_t buf[HAETAE_GAUSS_N_BUFFER_BYTES]) {
  const size_t nblocks = HAETAE_GAUSS_N_BUFFER_BYTES / SHAKE256_RATE;
  size_t bytecnt, coefcnt, firstflag = 1;
  stream256_state state;
  stream256_init(&state, seed, nonce);

  stream256_squeezeblocks(buf, nblocks, &state);
  for (size_t i = 0; i < len / 8; i++) {
    signs[i] = buf[i];
  }
  bytecnt = HAETAE_GAUSS_N_BUFFER_BYTES - len / 8;
  coefcnt = sample_gauss(r, sqsum, buf + len / 8, bytecnt, len,
                         len % HAETAE_N);
  while (coefcnt < len) {
    const size_t off = bytecnt % 17;
    for (size_t i = 0; i < off; i++) {
      buf[i] = buf[bytecnt + len / 8 * firstflag - off + i];
    }
    stream256_squeezeblocks(buf + off, 1, &state);
    bytecnt = SHAKE256_RATE + off;

    coefcnt += sample_gauss(r + coefcnt, sqsum, buf, bytecnt, len - coefcnt,
                            len % HAETAE_N);
    firstflag = 0;
  }
}
#endif

#endif /* !HAETAE_SAMPLER_H */

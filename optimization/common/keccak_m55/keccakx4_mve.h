#ifndef M55_KECCAKX4_MVE_H
#define M55_KECCAKX4_MVE_H

#include <stdint.h>

#if !defined(M55_KECCAK_X4_ASM)
#include <arm_mve.h>
#endif

/* Four independent standard-lane Keccak states in structure-of-arrays form.
 * Vector lane n always belongs to independent state n. Keeping this layout
 * across permutations is the x4 equivalent of the scalar persistent-state
 * path: the timed core does not transpose at every round or permutation. */
#if defined(M55_KECCAK_X4_ASM)
typedef struct {
  _Alignas(16) uint32_t lo[25][4];
  _Alignas(16) uint32_t hi[25][4];
} m55_keccakx4_state;
#else
typedef struct {
  uint32x4_t lo[25];
  uint32x4_t hi[25];
} m55_keccakx4_state;
#endif

void m55_keccakx4_zero(m55_keccakx4_state *state);
void m55_keccakx4_import(m55_keccakx4_state *state,
                         const uint64_t input[4][25]);
void m55_keccakx4_export(uint64_t output[4][25],
                         const m55_keccakx4_state *state);
void m55_keccakx4_permute(m55_keccakx4_state *state);

#endif

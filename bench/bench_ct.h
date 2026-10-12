#ifndef M55_BENCH_CT_H
#define M55_BENCH_CT_H

#include <stdint.h>

typedef void (*bench_ct_prepare)(uint32_t class_id, uint32_t pair_index);
typedef int (*bench_ct_operation)(void);

int bench_ct_measure(const char *scheme, const char *backend,
                     const char *operation, bench_ct_prepare prepare,
                     bench_ct_operation function);

#endif

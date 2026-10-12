#ifndef M55_BENCH_COMMON_H
#define M55_BENCH_COMMON_H

#include <stddef.h>

typedef int (*bench_operation)(void);
typedef void (*bench_prepare)(void);

int bench_measure(const char *scheme, const char *implementation,
                  const char *operation,
                  bench_operation function);
int bench_measure_prepared(const char *scheme, const char *implementation,
                           const char *operation, bench_prepare prepare,
                           bench_operation function);
void bench_print_header(const char *scheme, const char *implementation);
void bench_print_sizes3(const char *key1, size_t value1, const char *key2,
                        size_t value2, const char *key3, size_t value3);
void bench_print_footer(void);

#endif

#ifndef M55_MINIMAL_STDLIB_H
#define M55_MINIMAL_STDLIB_H

#include <stddef.h>

#ifndef NULL
#define NULL ((void *)0)
#endif

void *malloc(size_t size);
void free(void *pointer);
void exit(int status) __attribute__((noreturn));
void abort(void) __attribute__((noreturn));
int abs(int value);

#endif

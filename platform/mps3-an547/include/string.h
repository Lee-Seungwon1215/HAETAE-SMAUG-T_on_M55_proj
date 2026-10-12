#ifndef M55_MINIMAL_STRING_H
#define M55_MINIMAL_STRING_H

#include <stddef.h>

void *memcpy(void *dest, const void *src, size_t count);
void *memmove(void *dest, const void *src, size_t count);
void *memset(void *dest, int value, size_t count);
int memcmp(const void *left, const void *right, size_t count);
size_t strlen(const char *text);

#endif

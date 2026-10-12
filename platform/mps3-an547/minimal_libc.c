#include "platform.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct heap_block {
  size_t size;
  struct heap_block *next;
  uint32_t is_free;
} heap_block;

extern uint8_t __heap_start__;
extern uint8_t __heap_end__;

static heap_block *heap_head;

void platform_heap_reset(void) { heap_head = NULL; }

static size_t align_up(size_t value) {
  const size_t alignment = sizeof(uintptr_t);
  return (value + alignment - 1u) & ~(alignment - 1u);
}

static void heap_initialize(void) {
  uintptr_t start = align_up((uintptr_t)&__heap_start__);
  uintptr_t end = (uintptr_t)&__heap_end__;

  heap_head = (heap_block *)start;
  heap_head->size = end - start - sizeof(*heap_head);
  heap_head->next = NULL;
  heap_head->is_free = 1u;
}

void *malloc(size_t size) {
  heap_block *block;
  size_t requested;

  if (size == 0u) {
    return NULL;
  }
  if (heap_head == NULL) {
    heap_initialize();
  }

  requested = align_up(size);
  for (block = heap_head; block != NULL; block = block->next) {
    if (block->is_free == 0u || block->size < requested) {
      continue;
    }

    if (block->size >= requested + sizeof(heap_block) + sizeof(uintptr_t)) {
      heap_block *split = (heap_block *)((uint8_t *)(block + 1) + requested);
      split->size = block->size - requested - sizeof(heap_block);
      split->next = block->next;
      split->is_free = 1u;
      block->next = split;
      block->size = requested;
    }
    block->is_free = 0u;
    return block + 1;
  }
  return NULL;
}

void free(void *pointer) {
  heap_block *block;
  heap_block *cursor;

  if (pointer == NULL) {
    return;
  }
  block = ((heap_block *)pointer) - 1;
  block->is_free = 1u;

  for (cursor = heap_head; cursor != NULL && cursor->next != NULL;) {
    if (cursor->is_free != 0u && cursor->next->is_free != 0u) {
      cursor->size += sizeof(heap_block) + cursor->next->size;
      cursor->next = cursor->next->next;
    } else {
      cursor = cursor->next;
    }
  }
}

void *memcpy(void *destination, const void *source, size_t count) {
  uint8_t *out = destination;
  const uint8_t *in = source;
  for (size_t i = 0; i < count; i++) {
    out[i] = in[i];
  }
  return destination;
}

void *memmove(void *destination, const void *source, size_t count) {
  uint8_t *out = destination;
  const uint8_t *in = source;

  if (out < in) {
    for (size_t i = 0; i < count; i++) {
      out[i] = in[i];
    }
  } else if (out > in) {
    while (count != 0u) {
      count--;
      out[count] = in[count];
    }
  }
  return destination;
}

void *memset(void *destination, int value, size_t count) {
  uint8_t *out = destination;
  for (size_t i = 0; i < count; i++) {
    out[i] = (uint8_t)value;
  }
  return destination;
}

int memcmp(const void *left, const void *right, size_t count) {
  const uint8_t *a = left;
  const uint8_t *b = right;
  for (size_t i = 0; i < count; i++) {
    if (a[i] != b[i]) {
      return (int)a[i] - (int)b[i];
    }
  }
  return 0;
}

size_t strlen(const char *text) {
  size_t length = 0u;
  while (text[length] != '\0') {
    length++;
  }
  return length;
}

int abs(int value) { return value < 0 ? -value : value; }

void exit(int status) { platform_exit(status); }

void abort(void) { platform_exit(134); }

void __aeabi_memcpy(void *destination, const void *source, size_t count) {
  (void)memcpy(destination, source, count);
}

void __aeabi_memcpy4(void *destination, const void *source, size_t count) {
  (void)memcpy(destination, source, count);
}

void __aeabi_memcpy8(void *destination, const void *source, size_t count) {
  (void)memcpy(destination, source, count);
}

void __aeabi_memmove(void *destination, const void *source, size_t count) {
  (void)memmove(destination, source, count);
}

void __aeabi_memset(void *destination, size_t count, int value) {
  (void)memset(destination, value, count);
}

void __aeabi_memclr(void *destination, size_t count) {
  (void)memset(destination, 0, count);
}

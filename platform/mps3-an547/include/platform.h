#ifndef M55_PLATFORM_H
#define M55_PLATFORM_H

#include <stddef.h>
#include <stdint.h>

void platform_init(void);
void platform_exit(int status) __attribute__((noreturn));

void platform_write(const char *text);
void platform_write_u64(uint64_t value);
void platform_write_u32(uint32_t value);

uint64_t platform_timer_now(void);
const char *platform_timer_name(void);
void platform_print_metadata(void);
void platform_instruction_cache_reset(void);

void platform_stack_watermark_init(void);
void platform_stack_operation_reset(void);
size_t platform_stack_operation_peak_bytes(void);
size_t platform_stack_peak_bytes(void);
void platform_heap_reset(void);

#endif

#include "platform.h"

#include <stdint.h>

#define MMIO32(address) (*(volatile uint32_t *)(uintptr_t)(address))

#define COREDEBUG_DEMCR MMIO32(0xE000EDFCu)
#define COREDEBUG_DEMCR_TRCENA (1u << 24)
#define DWT_CTRL MMIO32(0xE0001000u)
#define DWT_CYCCNT MMIO32(0xE0001004u)
#define DWT_LAR MMIO32(0xE0001FB0u)
#define DWT_CTRL_CYCCNTENA (1u << 0)

#define SYST_CSR MMIO32(0xE000E010u)
#define SYST_RVR MMIO32(0xE000E014u)
#define SYST_CVR MMIO32(0xE000E018u)
#define SYST_CSR_ENABLE (1u << 0)
#define SYST_CSR_TICKINT (1u << 1)
#define SYST_CSR_CLKSOURCE (1u << 2)
#define SYST_CSR_COUNTFLAG (1u << 16)
#define SYSTICK_RELOAD 0x00FFFFFFu

#define SEMIHOST_SYS_WRITE0 0x04u
#define SEMIHOST_SYS_EXIT_EXTENDED 0x20u
#define SEMIHOST_ADP_STOPPED_APPLICATION_EXIT 0x00020026u

#define STACK_PATTERN 0xA5A5A5A5u

extern uint32_t __stack_limit__;
extern uint32_t _estack;

static volatile uint32_t systick_wraps;
static uint64_t last_systick_timestamp;
static size_t whole_run_stack_peak;
static uint64_t timer_read_overhead;
static uint32_t use_dwt;

static uint32_t semihost_call(uint32_t operation, const void *argument) {
  register uint32_t r0 __asm("r0") = operation;
  register const void *r1 __asm("r1") = argument;

  __asm volatile("bkpt 0xAB" : "+r"(r0) : "r"(r1) : "memory");
  return r0;
}

void platform_write(const char *text) {
  (void)semihost_call(SEMIHOST_SYS_WRITE0, text);
}

void platform_write_u64(uint64_t value) {
  char buffer[21];
  size_t position = sizeof(buffer);

  buffer[--position] = '\0';
  do {
    buffer[--position] = (char)('0' + (value % 10u));
    value /= 10u;
  } while (value != 0u);
  platform_write(&buffer[position]);
}

void platform_write_u32(uint32_t value) { platform_write_u64(value); }

void SysTick_Handler(void) {
  (void)SYST_CSR; /* Clear COUNTFLAG before publishing the new high word. */
  systick_wraps++;
}

static uint64_t systick_now(void) {
  const uint64_t period = (uint64_t)SYSTICK_RELOAD + 1u;
  uint32_t high;
  uint32_t current;
  uint32_t control;
  uint32_t primask;
  uint64_t timestamp;

  /*
   * Sample the software high word and the down-counter atomically.  The
   * handler clears COUNTFLAG before incrementing the high word.  Therefore a
   * set COUNTFLAG here unambiguously means that a reload occurred while its
   * handler was pending.  Reading CVR before CSR also closes the case where a
   * reload occurs during this sampling sequence: the set flag causes CVR to
   * be sampled again on the new period.
   */
  __asm volatile("mrs %0, primask\n\tcpsid i" : "=r"(primask) : : "memory");
  high = systick_wraps;
  current = SYST_CVR & SYSTICK_RELOAD;
  control = SYST_CSR;
  if ((control & SYST_CSR_COUNTFLAG) != 0u) {
    high++;
    current = SYST_CVR & SYSTICK_RELOAD;
  }
  if ((primask & 1u) == 0u) {
    __asm volatile("cpsie i" : : : "memory");
  }

  timestamp = ((uint64_t)high * period) +
              (uint64_t)(SYSTICK_RELOAD - current);
  if (timestamp < last_systick_timestamp) {
    /* Repair a pending-handler visibility edge observed at the QEMU reload. */
    timestamp += period;
  }
  last_systick_timestamp = timestamp;
  return timestamp;
}

void platform_init(void) {
  uint32_t before;
  uint32_t after;

  COREDEBUG_DEMCR |= COREDEBUG_DEMCR_TRCENA;
  DWT_LAR = 0xC5ACCE55u;
  DWT_CYCCNT = 0u;
  DWT_CTRL |= DWT_CTRL_CYCCNTENA;
  __asm volatile("dsb sy\n\tisb" ::: "memory");

  before = DWT_CYCCNT;
  for (volatile uint32_t i = 0; i < 64u; i++) {
    __asm volatile("nop");
  }
  after = DWT_CYCCNT;
  use_dwt = (after != before) ? 1u : 0u;

  if (use_dwt == 0u) {
    systick_wraps = 0u;
    last_systick_timestamp = 0u;
    SYST_CSR = 0u;
    SYST_RVR = SYSTICK_RELOAD;
    SYST_CVR = 0u;
    SYST_CSR = SYST_CSR_ENABLE | SYST_CSR_TICKINT | SYST_CSR_CLKSOURCE;
  }

  timer_read_overhead = UINT64_MAX;
  for (uint32_t i = 0; i < 32u; i++) {
    uint64_t start = platform_timer_now();
    uint64_t end = platform_timer_now();
    if (end - start < timer_read_overhead) {
      timer_read_overhead = end - start;
    }
  }
}

uint64_t platform_timer_now(void) {
  if (use_dwt != 0u) {
    return DWT_CYCCNT;
  }
  return systick_now();
}

void platform_instruction_cache_reset(void) {
  /* The MPS3/QEMU measurement platform does not model a controllable I-cache. */
}

const char *platform_timer_name(void) {
  return use_dwt != 0u ? "dwt_cyccnt" : "qemu_systick_ticks";
}

void platform_print_metadata(void) {
  platform_write("PLATFORM_CONFIG,name=mps3-an547,clock=model,icache=model,dcache=model,code=itcm,data=dtcm,timer_read_overhead=");
  platform_write_u64(timer_read_overhead);
  platform_write("\n");
}

static void stack_watermark_fill(void) {
  uint32_t *cursor = &__stack_limit__;
  uintptr_t stack_pointer;
  uintptr_t safe_end;

  __asm volatile("mov %0, sp" : "=r"(stack_pointer));
  safe_end = stack_pointer > 256u ? stack_pointer - 256u : stack_pointer;
  while ((uintptr_t)cursor + sizeof(*cursor) <= safe_end) {
    *cursor++ = STACK_PATTERN;
  }
}

static size_t stack_watermark_peak_bytes(void) {
  const uint32_t *cursor = &__stack_limit__;
  const uint32_t *top = &_estack;

  while (cursor < top && *cursor == STACK_PATTERN) {
    cursor++;
  }
  return (size_t)((uintptr_t)top - (uintptr_t)cursor);
}

static size_t stack_record_peak(void) {
  size_t current = stack_watermark_peak_bytes();
  if (current > whole_run_stack_peak) {
    whole_run_stack_peak = current;
  }
  return current;
}

void platform_stack_watermark_init(void) {
  whole_run_stack_peak = 0u;
  stack_watermark_fill();
}

void platform_stack_operation_reset(void) {
  (void)stack_record_peak();
  stack_watermark_fill();
}

size_t platform_stack_operation_peak_bytes(void) {
  return stack_record_peak();
}

size_t platform_stack_peak_bytes(void) {
  (void)stack_record_peak();
  return whole_run_stack_peak;
}

void platform_exit(int status) {
  uint32_t arguments[2];
  arguments[0] = SEMIHOST_ADP_STOPPED_APPLICATION_EXIT;
  arguments[1] = (uint32_t)status;
  (void)semihost_call(SEMIHOST_SYS_EXIT_EXTENDED, arguments);
  for (;;) {
    __asm volatile("wfi");
  }
}

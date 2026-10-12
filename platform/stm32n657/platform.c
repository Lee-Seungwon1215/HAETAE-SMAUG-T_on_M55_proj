#include "platform.h"

#include <stdint.h>

#define MMIO32(address) (*(volatile uint32_t *)(uintptr_t)(address))

#define SCB_CCR MMIO32(0xE000ED14u)
#define SCB_CCSIDR MMIO32(0xE000ED80u)
#define SCB_CSSELR MMIO32(0xE000ED84u)
#define SCB_ICIALLU MMIO32(0xE000EF50u)
#define SCB_DCISW MMIO32(0xE000EF60u)
#define SCB_CCR_DC (1u << 16)
#define SCB_CCR_IC (1u << 17)
#define SCB_CCSIDR_ASSOCIATIVITY_SHIFT 3u
#define SCB_CCSIDR_ASSOCIATIVITY_MASK 0x3FFu
#define SCB_CCSIDR_NUMSETS_SHIFT 13u
#define SCB_CCSIDR_NUMSETS_MASK 0x7FFFu

#define COREDEBUG_DEMCR MMIO32(0xE000EDFCu)
#define COREDEBUG_DEMCR_TRCENA (1u << 24)
#define DWT_CTRL MMIO32(0xE0001000u)
#define DWT_CYCCNT MMIO32(0xE0001004u)
#define DWT_LAR MMIO32(0xE0001FB0u)
#define DWT_CTRL_CYCCNTENA (1u << 0)

#define STACK_PATTERN 0xA5A5A5A5u

#define BENCH_OUTPUT_MAGIC 0x4D35354Fu
#define BENCH_OUTPUT_COMPLETE 0x434F4D50u
#define BENCH_OUTPUT_CAPACITY (512u * 1024u)

typedef struct {
  uint32_t magic;
  uint32_t capacity;
  uint32_t length;
  uint32_t overflow;
  uint32_t complete;
  uint32_t exit_status;
  uint8_t bytes[BENCH_OUTPUT_CAPACITY];
} bench_output_state_t;

__attribute__((section(".bench_output"), aligned(32), used))
volatile bench_output_state_t bench_output_state;

#define STM32_RCC_BASE 0x56028000u
#define STM32_PWR_BASE 0x56024800u
#define RCC_SR MMIO32(STM32_RCC_BASE + 0x0004u)
#define RCC_CFGR1 MMIO32(STM32_RCC_BASE + 0x0020u)
#define RCC_CFGR2 MMIO32(STM32_RCC_BASE + 0x0024u)
#define RCC_HSICFGR MMIO32(STM32_RCC_BASE + 0x0048u)
#define RCC_PLL1CFGR1 MMIO32(STM32_RCC_BASE + 0x0080u)
#define RCC_PLL1CFGR2 MMIO32(STM32_RCC_BASE + 0x0084u)
#define RCC_PLL1CFGR3 MMIO32(STM32_RCC_BASE + 0x0088u)
#define RCC_IC1CFGR MMIO32(STM32_RCC_BASE + 0x00C4u)
#define RCC_IC2CFGR MMIO32(STM32_RCC_BASE + 0x00C8u)
#define RCC_IC6CFGR MMIO32(STM32_RCC_BASE + 0x00D8u)
#define RCC_IC11CFGR MMIO32(STM32_RCC_BASE + 0x00ECu)
#define RCC_CSR MMIO32(STM32_RCC_BASE + 0x0800u)
#define RCC_DIVENSR MMIO32(STM32_RCC_BASE + 0x0A40u)
#define RCC_CCR MMIO32(STM32_RCC_BASE + 0x1000u)
#define PWR_CR1 MMIO32(STM32_PWR_BASE + 0x0000u)
#define PWR_VOSCR MMIO32(STM32_PWR_BASE + 0x0020u)

#define RCC_HSI_ON (1u << 3)
#define RCC_PLL1_ON (1u << 8)
#define RCC_CFGR1_CPUSW_MASK (3u << 16)
#define RCC_CFGR1_CPUSWS_MASK (3u << 20)
#define RCC_CFGR1_SYSSW_MASK (3u << 24)
#define RCC_CFGR1_SYSSWS_MASK (3u << 28)
#define RCC_CFGR1_CPU_IC1 (3u << 16)
#define RCC_CFGR1_CPU_IC1_STATUS (3u << 20)
#define RCC_CFGR1_SYS_IC2_IC6_IC11 (3u << 24)
#define RCC_CFGR1_SYS_IC2_IC6_IC11_STATUS (3u << 28)
#define RCC_CFGR2_BUS_PRESCALER_MASK 0x00777077u
#define RCC_CFGR2_HCLK_DIV2 (1u << 20)
#define RCC_DIVEN_IC1 (1u << 0)
#define RCC_DIVEN_IC2 (1u << 1)
#define RCC_DIVEN_IC6 (1u << 5)
#define RCC_DIVEN_IC11 (1u << 10)
#define RCC_HSIDIV_MASK (3u << 7)
#define PWR_SMPS_ENABLE (1u << 2)
#define PWR_VOS_SCALE_MASK (1u << 0)
#define PWR_VOS_READY (1u << 1)
#define PWR_ACTIVE_VOS_READY (1u << 17)

#define STM32N657_CORE_CLOCK_HZ 600000000u
#define STM32N657_HCLK_HZ 200000000u

extern uint32_t __stack_limit__;
extern uint32_t _estack;

static uint32_t dwt_previous;
static uint64_t dwt_high;
static uint64_t timer_read_overhead;
static size_t whole_run_stack_peak;

static void barrier(void) {
  __asm volatile("dsb sy\n\tisb" ::: "memory");
}

static uint32_t count_leading_zeros(uint32_t value) {
  uint32_t result;
  __asm volatile("clz %0, %1" : "=r"(result) : "r"(value));
  return result;
}

#if defined(STM32N657_CLOCK_600MHZ)
static int wait_register(volatile uint32_t *address, uint32_t mask,
                         uint32_t expected) {
  uint32_t timeout = 10000000u;
  while ((*address & mask) != expected) {
    if (timeout-- == 0u) {
      return 0;
    }
  }
  return 1;
}

static int configure_clock_600mhz(void) {
  /*
   * Board-common subset of ST's NUCLEO-N657X0-Q FSBL clock tree:
   * HSI 64 MHz -> PLL1 (M=4,N=75,P1=P2=1) -> 1200 MHz,
   * IC1 /2 = CPU 600 MHz, IC2 /3 = AXI 400 MHz,
   * IC6 /4 = NPU 300 MHz, IC11 /3 = AXISRAM 400 MHz, HCLK /2.
   */
  RCC_CSR = RCC_HSI_ON;
  if (!wait_register(&RCC_SR, RCC_HSI_ON, RCC_HSI_ON)) {
    return 0;
  }
  RCC_HSICFGR &= ~RCC_HSIDIV_MASK;

  /* Reconfigure PLL only while CPU and system buses use the safe HSI path. */
  RCC_CFGR1 &= ~(RCC_CFGR1_CPUSW_MASK | RCC_CFGR1_SYSSW_MASK);
  if (!wait_register(&RCC_CFGR1,
                     RCC_CFGR1_CPUSWS_MASK | RCC_CFGR1_SYSSWS_MASK, 0u)) {
    return 0;
  }

  /* Both supported ST boards use an external VCORE source. Scale 1 supports 600 MHz. */
  PWR_CR1 &= ~PWR_SMPS_ENABLE;
  if (!wait_register(&PWR_VOSCR, PWR_ACTIVE_VOS_READY,
                     PWR_ACTIVE_VOS_READY)) {
    return 0;
  }
  PWR_VOSCR &= ~PWR_VOS_SCALE_MASK;
  if (!wait_register(&PWR_VOSCR, PWR_VOS_READY, PWR_VOS_READY)) {
    return 0;
  }

  RCC_CCR = RCC_PLL1_ON;
  if (!wait_register(&RCC_SR, RCC_PLL1_ON, 0u)) {
    return 0;
  }
  RCC_PLL1CFGR1 = (4u << 20) | (75u << 8); /* PLL source 0 is HSI. */
  RCC_PLL1CFGR2 = 0u;
  RCC_PLL1CFGR3 = (1u << 30) | (1u << 27) | (1u << 24) |
                  (1u << 2) | (1u << 0);
  RCC_CSR = RCC_PLL1_ON;
  if (!wait_register(&RCC_SR, RCC_PLL1_ON, RCC_PLL1_ON)) {
    return 0;
  }

  RCC_IC1CFGR = 1u << 16;  /* PLL1 / 2. */
  RCC_IC2CFGR = 2u << 16;  /* PLL1 / 3. */
  RCC_IC6CFGR = 3u << 16;  /* PLL1 / 4. */
  RCC_IC11CFGR = 2u << 16; /* PLL1 / 3. */
  RCC_DIVENSR = RCC_DIVEN_IC1 | RCC_DIVEN_IC2 | RCC_DIVEN_IC6 |
                 RCC_DIVEN_IC11;

  RCC_CFGR2 = (RCC_CFGR2 & ~RCC_CFGR2_BUS_PRESCALER_MASK) |
              RCC_CFGR2_HCLK_DIV2;
  RCC_CFGR1 = (RCC_CFGR1 &
               ~(RCC_CFGR1_CPUSW_MASK | RCC_CFGR1_SYSSW_MASK)) |
              RCC_CFGR1_CPU_IC1 | RCC_CFGR1_SYS_IC2_IC6_IC11;
  return wait_register(&RCC_CFGR1,
                       RCC_CFGR1_CPUSWS_MASK | RCC_CFGR1_SYSSWS_MASK,
                       RCC_CFGR1_CPU_IC1_STATUS |
                           RCC_CFGR1_SYS_IC2_IC6_IC11_STATUS);
}
#endif

void platform_write(const char *text) {
  uint32_t length = bench_output_state.length;

  while (*text != '\0') {
    if (length >= BENCH_OUTPUT_CAPACITY) {
      bench_output_state.overflow = 1u;
      break;
    }
    bench_output_state.bytes[length++] = (uint8_t)*text++;
  }
  bench_output_state.length = length;
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

static void enable_caches(void) {
  /*
   * A debugger-assisted reload can leave the I-cache enabled across resets.
   * Invalidate it unconditionally so that a newly loaded benchmark image
   * cannot execute cache lines belonging to the preceding image.
   */
  SCB_ICIALLU = 0u;
  barrier();
  if ((SCB_CCR & SCB_CCR_IC) == 0u) {
    SCB_CCR |= SCB_CCR_IC;
    barrier();
  }

  if ((SCB_CCR & SCB_CCR_DC) == 0u) {
    uint32_t ccsidr;
    uint32_t sets;
    uint32_t ways;
    uint32_t way_shift;

    SCB_CSSELR = 0u; /* Level 1 data/unified cache. */
    barrier();
    ccsidr = SCB_CCSIDR;
    sets = (ccsidr >> SCB_CCSIDR_NUMSETS_SHIFT) &
           SCB_CCSIDR_NUMSETS_MASK;
    ways = (ccsidr >> SCB_CCSIDR_ASSOCIATIVITY_SHIFT) &
           SCB_CCSIDR_ASSOCIATIVITY_MASK;
    way_shift = ways == 0u ? 0u : count_leading_zeros(ways);

    do {
      uint32_t way = ways;
      do {
        SCB_DCISW = (sets << 5) | (way << way_shift);
      } while (way-- != 0u);
    } while (sets-- != 0u);
    barrier();
    SCB_CCR |= SCB_CCR_DC;
    barrier();
  }
}

void platform_instruction_cache_reset(void) {
  SCB_ICIALLU = 0u;
  barrier();
}

void platform_init(void) {
  uint32_t before;
  uint32_t after;

  bench_output_state.magic = BENCH_OUTPUT_MAGIC;
  bench_output_state.capacity = BENCH_OUTPUT_CAPACITY;
  bench_output_state.length = 0u;
  bench_output_state.overflow = 0u;
  bench_output_state.complete = 0u;
  bench_output_state.exit_status = UINT32_MAX;

#if defined(STM32N657_CLOCK_600MHZ)
  if (!configure_clock_600mhz()) {
    platform_write("FATAL,clock_configuration_failed\n");
    platform_exit(3);
  }
#endif
  enable_caches();

  COREDEBUG_DEMCR |= COREDEBUG_DEMCR_TRCENA;
  DWT_LAR = 0xC5ACCE55u;
  DWT_CYCCNT = 0u;
  DWT_CTRL |= DWT_CTRL_CYCCNTENA;
  barrier();

  before = DWT_CYCCNT;
  for (volatile uint32_t i = 0; i < 64u; i++) {
    __asm volatile("nop");
  }
  after = DWT_CYCCNT;
  if (after == before) {
    platform_write("FATAL,dwt_cyccnt_unavailable\n");
    platform_exit(2);
  }
  dwt_previous = after;
  dwt_high = 0u;

  timer_read_overhead = UINT64_MAX;
  for (uint32_t i = 0; i < 32u; i++) {
    uint64_t start = platform_timer_now();
    uint64_t end = platform_timer_now();
    if (end - start < timer_read_overhead) {
      timer_read_overhead = end - start;
    }
  }

  /* No interrupt source participates in a reportable measurement. */
  __asm volatile("cpsid i" : : : "memory");
}

uint64_t platform_timer_now(void) {
  uint32_t current = DWT_CYCCNT;
  if (current < dwt_previous) {
    dwt_high += UINT64_C(1) << 32;
  }
  dwt_previous = current;
  return dwt_high | current;
}

const char *platform_timer_name(void) { return "dwt_cyccnt"; }

void platform_print_metadata(void) {
#if defined(STM32N657_CLOCK_600MHZ)
  platform_write("PLATFORM_CONFIG,name=stm32n657,core_clock_hz=600000000,hclk_hz=200000000,axisram_clock_hz=400000000,icache=on,dcache=on,interrupts=disabled,code_data=secure_axisram,timer_read_overhead=");
#else
  platform_write("PLATFORM_CONFIG,name=stm32n657,core_clock_hz=boot_unverified,icache=on,dcache=on,interrupts=disabled,code_data=secure_axisram,timer_read_overhead=");
#endif
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

__attribute__((noinline, noreturn, used)) void platform_exit_halt(void) {
  for (;;) {
    __asm volatile("wfi");
  }
}

void platform_exit(int status) {
  bench_output_state.exit_status = (uint32_t)status;
  bench_output_state.complete = BENCH_OUTPUT_COMPLETE;
  barrier();
  platform_exit_halt();
}

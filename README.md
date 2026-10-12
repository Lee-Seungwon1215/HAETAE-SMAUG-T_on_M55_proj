# HAETAE and SMAUG-T on Arm Cortex-M55

Artifact for **“Optimizing HAETAE and SMAUG-T on Arm Cortex-M55: From MVE
Intrinsics to Slothy-Based Assembly Scheduling.”** It contains the six KpqC
parameter-set implementations used in the paper, the MPS3 AN547 and STM32N657
measurement harnesses, the selected generated assembly, and the raw cycle logs
needed to recompute the reported tables.

The main comparison has three implementation stages:

| `CONFIG` | Meaning |
|---|---|
| `intrinsic` | MVE-intrinsic baseline |
| `assembly` | manually optimized MVE assembly |
| `final` | selected Slothy-scheduled assembly; the manually unrolled HAETAE sampler is retained because its generated schedule was slower on hardware |

`CONFIG=reference` selects the preserved KpqClean C baseline used for
correctness and additional context.

## Quick start with QEMU

Required commands are `arm-none-eabi-gcc`, `arm-none-eabi-size`,
`arm-none-eabi-objcopy`, `qemu-system-arm`, Python 3, and GNU Make.

```sh
make doctor

# Functional smoke tests of the portable reference and final implementation.
make CONFIG=reference ALGORITHM=haetae2 ITERATIONS=1 run
make CONFIG=final     ALGORITHM=haetae2 ITERATIONS=1 run

# KAT plus 1,000-case differential test against the reference implementation.
make CONFIG=final ALGORITHM=haetae2 diff

# Repeat the differential test for all six parameter sets.
make CONFIG=final diff-all
```

A successful run ends with `SELFTEST,ok` and `STATUS,ok`. Differential runs
also print `KAT,status=ok` and `DIFF,iterations=1000,status=ok`.

Supported algorithm names are `haetae2`, `haetae3`, `haetae5`, `smaugt1`,
`smaugt3`, and `smaugt5`. QEMU is used for functional regression and
instruction compatibility only; its timer values are not reported as physical
Cortex-M55 cycle counts.

## Recompute the archived results

The repository publishes compact versions of the two physical-board campaigns
used by the paper. Raw benchmark logs are retained; duplicate ELF files, linker
maps, local probe identifiers, and dirty-worktree snapshots are not.

```sh
# Four-backend end-to-end campaign: four order-balanced sets x 100 samples.
python3 scripts/analyze_final_paper_campaign.py \
  measurements/stm32n657/20260822-final-paper-4backend-02

# Matched kernel campaign: two reversed-order sets x 100 samples.
python3 scripts/stm32n657_slothy_matched_campaign.py analyze \
  --output measurements/stm32n657/20260823-slothy-matched-per-operation-icache-board-01
```

The first command regenerates the CSV files under `statistics/`; the second
regenerates both `statistics/slothy-matched-pairs.csv` and `REPORT.md`.

## Physical STM32N657 measurement

Board runs additionally require `arm-none-eabi-gdb`, `probe-rs`, an ST-Link
connection, and an STM32N657 board in development boot mode. Check the host
tools and run a one-iteration smoke test before starting a full campaign:

```sh
make doctor-board
make PLATFORM=stm32n657 CONFIG=final ALGORITHM=haetae2 \
  ITERATIONS=1 PROBE_SERIAL=SERIAL run

python3 scripts/stm32n657_final_paper_campaign.py smoke \
  --output measurements/stm32n657/my-smoke \
  --probe-serial SERIAL
```

The complete four-backend experiment is:

```sh
python3 scripts/stm32n657_final_paper_campaign.py all \
  --output measurements/stm32n657/my-final-campaign \
  --iterations 100 \
  --probe-serial SERIAL

python3 scripts/analyze_final_paper_campaign.py \
  measurements/stm32n657/my-final-campaign
```

The runner loads a RAM image into secure AXISRAM and does not erase or program
external flash. The reportable environment is a NUCLEO-N657X0-Q/STM32N657 at a
600 MHz core clock, 200 MHz HCLK, and 400 MHz AXISRAM, with L1 instruction and
data caches enabled, interrupts disabled during timed regions, and DWT
`CYCCNT` timing. Exact commands, expected outputs, and the matched-kernel
campaign are documented in [REPRODUCING.md](REPRODUCING.md).

## Repository layout

```text
reference/                    preserved KpqClean comparison baseline
optimization/                 Cortex-M55 intrinsics and MVE assembly
  common/keccak_m55/          shared scalar and four-way Keccak/SHAKE
  haetae/                     HAETAE2/3/5 plus NTT, FFT, and sampler kernels
  smaug/                      SMAUG-T1/3/5 plus Toom--Cook kernels
bench/                        self-test, KAT, differential, and timing harnesses
platform/mps3-an547/          QEMU Cortex-M55 platform
platform/stm32n657/           physical STM32N657 platform
scripts/                      campaign runners and analysis scripts
tools/slothy_haetae/          project-owned Slothy drivers, patch, and provenance
measurements/                 compact publication evidence
docs/                         design decisions, provenance, and optimization log
```

The imported baseline is identified in `reference/UPSTREAM.md`; its license is
preserved in `reference/LICENSE`. Generated schedules are committed, so Slothy
is not required to build, test, or measure the selected implementations. See
`tools/slothy_haetae/README.md` only when regenerating schedules.

## Measurement and security notes

- The deterministic benchmark RNG exists only for repeatable tests. It must not
  be used in a deployed cryptographic system.
- Cycle counts depend on the processor, memory placement, cache state,
  compiler, and measurement order. Only results collected with the documented
  STM32N657 setup are directly comparable to the paper.
- This artifact does not claim that every complete API is constant-time.
- A repository-wide license for newly authored project material must be chosen
  before the GitHub repository is made public; the imported reference license
  does not automatically license the new implementation files.

# Reproducing the paper artifact

This guide separates three claims that require different environments:

1. **Build and functional correctness** can be reproduced with QEMU.
2. **Published statistics** can be recomputed offline from the archived raw
   STM32N657 logs.
3. **Physical cycle measurements** require the documented STM32N657 setup.

Run every command from the repository root. For a paper comparison, use the
release tag or commit cited by the paper rather than a moving branch.

## 1. Dependencies

### QEMU workflow

- GNU Make
- Python 3
- Arm GNU Toolchain: `arm-none-eabi-gcc`, `arm-none-eabi-size`, and
  `arm-none-eabi-objcopy`
- QEMU with the `mps3-an547` machine and Cortex-M55 CPU model

Check command availability:

```sh
make doctor
arm-none-eabi-gcc --version
qemu-system-arm --version
```

The archived physical measurements used GCC 16.1.0 and
`-mcpu=cortex-m55 -mthumb -mfloat-abi=hard -O3`. Other compiler versions may
produce different code and cycle counts even when correctness is unchanged.

### Physical-board workflow

In addition to the QEMU dependencies:

- `arm-none-eabi-gdb`
- `probe-rs` 0.32.0 for the archived campaign
- NUCLEO-N657X0-Q or equivalent STM32N657 target with ST-Link
- development boot mode; for NUCLEO-N657X0-Q, BOOT1 uses pins 2--3

```sh
make doctor-board
```

If several probes are connected, pass the ST-Link serial through
`PROBE_SERIAL=SERIAL` to Make or `--probe-serial SERIAL` to a campaign script.
The serial identifies the probe only and is intentionally absent from the
public measurement archive.

## 2. Build and run each paper stage

The four named configurations centralize all build selectors used by the
paper. Generated Slothy outputs are committed, so no solver installation is
needed here.

```sh
make CONFIG=reference ALGORITHM=haetae2 ITERATIONS=1 run
make CONFIG=intrinsic ALGORITHM=haetae2 ITERATIONS=1 run
make CONFIG=assembly  ALGORITHM=haetae2 ITERATIONS=1 run
make CONFIG=final     ALGORITHM=haetae2 ITERATIONS=1 run
```

Replace `haetae2` with any of:

```text
haetae2  haetae3  haetae5  smaugt1  smaugt3  smaugt5
```

Successful ordinary runs contain:

```text
SELFTEST,ok
STATUS,ok
```

QEMU executes the same Cortex-M55 instructions but does not reproduce the
STM32N657 memory hierarchy. Treat its tick output only as a regression signal.

## 3. Correctness

`diff` builds the selected optimized configuration together with independently
renamed reference objects. It performs a known-answer test and 1,000
differential cases.

```sh
make CONFIG=intrinsic ALGORITHM=haetae2 diff
make CONFIG=assembly  ALGORITHM=haetae2 diff
make CONFIG=final     ALGORITHM=haetae2 diff
```

The stronger all-parameter-set gate is:

```sh
make CONFIG=final diff-all
```

Successful differential runs additionally contain:

```text
KAT,status=ok
DIFF,iterations=1000,status=ok
```

## 4. Recompute the published statistics offline

### End-to-end, stack, and code size

The archived four-backend campaign contains four cyclically ordered sets of
100 samples for every configuration, parameter set, and top-level operation.

```sh
python3 scripts/analyze_final_paper_campaign.py \
  measurements/stm32n657/20260822-final-paper-4backend-02
```

This regenerates:

- `statistics/raw-cycles.csv`
- `statistics/top-level-performance.csv`
- `statistics/geomean-speedups.csv`
- `statistics/stack-comparison.csv`
- `statistics/code-size-comparison.csv`
- `statistics/run-status.csv`
- `statistics/correctness.csv`

The analyzer fails if any expected performance or correctness log is missing,
if a configuration has the wrong number of samples, or if code size is not
constant across sets.

### Matched pre-/post-Slothy kernels

The kernel campaign contains two reversed-order sets of 100 samples for the
exact assembly supplied to Slothy (`pre`) and its generated output (`post`).
Every non-target selector is held fixed.

```sh
python3 scripts/stm32n657_slothy_matched_campaign.py analyze \
  --output measurements/stm32n657/20260823-slothy-matched-per-operation-icache-board-01
```

This regenerates `statistics/slothy-matched-pairs.csv` and `REPORT.md` directly
from the archived `output.log` files. The `analyze` command is offline and does
not query a probe or overwrite measurement provenance.

## 5. Repeat the physical end-to-end campaign

First verify that the board is visible and run the two-image smoke set:

```sh
python3 scripts/stm32n657_campaign.py check \
  --output measurements/stm32n657/local-board-check \
  --probe-serial SERIAL

python3 scripts/stm32n657_final_paper_campaign.py smoke \
  --output measurements/stm32n657/local-final-paper \
  --probe-serial SERIAL
```

Then run all correctness gates and the order-balanced performance experiment:

```sh
python3 scripts/stm32n657_final_paper_campaign.py all \
  --output measurements/stm32n657/local-final-paper \
  --iterations 100 \
  --probe-serial SERIAL

python3 scripts/analyze_final_paper_campaign.py \
  measurements/stm32n657/local-final-paper
```

The four orders form a Latin square, so every backend appears once in every
early/late position. The campaign records 400 performance samples per
configuration and cell. It also runs self-tests for the reference and KAT plus
1,000 differential cases for every optimized stage.

The target image configures:

| Item | Setting |
|---|---|
| Core | Cortex-M55 at 600 MHz |
| HCLK / AXISRAM | 200 / 400 MHz |
| Code and data | secure AXISRAM |
| L1 caches | instruction and data enabled |
| Timed-region interrupts | disabled |
| Timer | DWT `CYCCNT` |
| Compiler flags | `-mcpu=cortex-m55 -mthumb -mfloat-abi=hard -O3` |

The runner writes only the development-mode RAM image. It does not erase or
program external flash.

## 6. Repeat the matched kernel campaign

This experiment is longer because it builds and measures every selected
pre-/post-Slothy pair independently.

```sh
python3 scripts/stm32n657_slothy_matched_campaign.py all \
  --output measurements/stm32n657/local-matched-kernels \
  --iterations 100 \
  --probe-serial SERIAL
```

Interrupted performance runs can be resumed with the same output directory and
`--resume`. A run is skipped only if its descriptor, selectors, success marker,
and sample count all match the request.

## 7. Regenerate Slothy schedules (optional)

Schedule regeneration is not required for normal artifact evaluation. The
project-owned drivers and model patch are under `tools/slothy_haetae/`; the
large upstream checkout and downloaded compiler package are deliberately not
committed.

The recorded Slothy revision is
`241093fa5ddc1e21f75dd9f6588733bc3097872b`. Recreate the Python environment
and checkout as follows:

```sh
python3 -m venv .venv-slothy
.venv-slothy/bin/pip install -r tools/slothy_haetae/requirements.txt

git clone https://github.com/slothy-optimizer/slothy.git \
  tools/slothy_haetae/upstream/slothy
git -C tools/slothy_haetae/upstream/slothy checkout \
  241093fa5ddc1e21f75dd9f6588733bc3097872b
git -C tools/slothy_haetae/upstream/slothy apply \
  ../../patches/arm_v81m_lsl_strb_regidx.patch
```

See `tools/slothy_haetae/README.md` for the individual generators, scheduling
drivers, constraints, and qualification commands. `provenance.json` records
the upstream commit, dependency versions, and hashes of the model files used.

## 8. Interpreting differences

- Small cycle differences are expected if toolchain versions, code placement,
  cache state, clocks, or run ordering differ.
- A QEMU timing difference is not evidence of the same hardware speedup.
- The archived medians are recomputed from raw integer cycle samples; do not
  recompute ratios from the rounded values printed in the paper.
- The deterministic test RNG is not suitable for deployment.

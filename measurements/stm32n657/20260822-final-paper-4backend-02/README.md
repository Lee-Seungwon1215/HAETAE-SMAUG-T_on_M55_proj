# Final four-backend STM32N657 campaign

This is the end-to-end publication campaign for Reference, MVE Intrinsic,
Manually Optimized MVE Assembly, and Final/Slothy configurations.

## Method

- Target: NUCLEO-N657X0-Q, Arm Cortex-M55
- Core / HCLK / AXISRAM: 600 / 200 / 400 MHz
- Code and data: secure AXISRAM
- L1 instruction/data caches: enabled
- Interrupts: disabled during timed regions
- Timer: DWT `CYCCNT`
- Compiler: `arm-none-eabi-gcc` 16.1.0, Cortex-M55 hard-float, `-O3`
- Sampling: four cyclically ordered sets of 100 samples
- Correctness: self-test for Reference; KAT and 1,000-case reference
  differential test for Intrinsic, Assembly, and Final

The exact backend selectors and run orders are recorded in
`final-paper-campaign.json`.

## Reanalysis

From the repository root:

```sh
python3 scripts/analyze_final_paper_campaign.py \
  measurements/stm32n657/20260822-final-paper-4backend-02
```

The analyzer reads the archived `runs/**/output.log` and `size-summary.txt`
files and rewrites the CSV files under `statistics/`. It validates sample
counts, correctness markers, and code-size consistency while doing so.

The public archive is intentionally compact. ELF files, maps, local probe and
host logs, per-run descriptors, and the dirty-tree source snapshot are not
needed for the analysis and are excluded from Git. The release commit itself is
the canonical source snapshot for reproduction.

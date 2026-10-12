# Reference implementations

This directory contains the unoptimized HAETAE and SMAUG-T code used as the
comparison baseline. The sources were imported from KPQClean v2; provenance and
licensing information are recorded in `UPSTREAM.md`, `THIRD_PARTY.md`, and
`LICENSE`.

The benchmark harness selects these files with:

```sh
make IMPLEMENTATION=reference ALGORITHM=haetae2 run
```

Keep algorithm changes out of this tree. Cortex-M55-specific changes belong in
`../optimization/` so that source-level and measurement comparisons retain a
clear baseline.

# Cortex-M55 optimized implementations

This tree contains the Cortex-M55 versions compared against the immutable
KpqClean snapshot under `reference/`.

```text
common/                     shared portable support and Keccak
common/keccak_m55/          selected scalar and MVE x4 Keccak kernels
haetae/common/              shared NTT, FFT, sampler, and helper code
haetae/HAETAE{2,3,5}/       parameter-set implementations
smaug/common/               shared Toom--Cook kernels
smaug/SMAUG-T{1,3,5}/       parameter-set implementations
```

HAETAE and SMAUG share the same optimized Keccak implementation. Algorithm-
specific helpers are kept under their family so a reader can identify the
ownership of each optimization without tracing the build first.

```sh
make IMPLEMENTATION=optimization ALGORITHM=haetae2 run
make IMPLEMENTATION=reference ALGORITHM=haetae2 run
make ALGORITHM=haetae2 compare
make ALGORITHM=haetae2 diff
```

QEMU is used for functional and relative development checks. Reportable cycle
counts come from the physical STM32N657 campaigns described in the root
README.

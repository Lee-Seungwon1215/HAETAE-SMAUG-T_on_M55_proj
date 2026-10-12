# Vendored KpqClean v2 sources

- Repository: <https://github.com/kpqc-cryptocraft/KpqClean_ver2>
- Pinned commit: `c20b422b715c6e12edb04cfe0ceb659d8ac7b7a9`
- Commit date: 2026-06-24
- Imported directories:
  - `common/`
  - `crypto_sign/HAETAE{2,3,5}/clean/`
  - `crypto_kem/SMAUG-T{1,3,5}/clean/`
- License: MIT; see `LICENSE` and the original per-file headers.

The source contents are kept unchanged; only their enclosing paths were
flattened into `reference/haetae/` and `reference/smaug/`. Cortex-M55-specific
replacements and build logic live under `optimization/`.

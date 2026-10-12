# Third-party source provenance

## pqmx

- Repository: <https://github.com/slothy-optimizer/pqmx.git>
- Pinned commit: `89465b9756cd9b321966c99e21301b7a0a2e4de0`
- Nested Slothy submodule commit: `55983e6760e98aece5359055085a7def96c688b7`
- Local development path: `third_party/pqmx/` (not part of the public source tree)
- Integration policy: this directory is an independent Git checkout and is
  ignored by the parent repository. Recreate it by cloning the repository at
  the pinned commit and initializing its submodules recursively.

## KpqClean v2

- Repository: <https://github.com/kpqc-cryptocraft/KpqClean_ver2>
- Pinned commit: `c20b422b715c6e12edb04cfe0ceb659d8ac7b7a9`
- Local path: `reference/`
- Integration policy: this is a vendored source snapshot, not a nested Git
  checkout. It is committed in the parent repository as the immutable clean
  baseline. See `reference/UPSTREAM.md` for imported paths and
  license details.

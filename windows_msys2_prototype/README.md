# Windows / MSYS2 deterministic-repair prototype

This directory contains the earlier C++ prototype developed before the LaZer-backed implementation. It is retained for traceability and for reproducing early deterministic-repair, abort/retry, tamper, refresh, and boundary-sweep experiments.

## Role

- This is not the final LaZer-backed proof backend used for the main system-level proof-size and timing results.
- It is useful for checking the original repair-vs-abort logic, local boundary behavior, tamper tests, refresh tests, and supplemental sweeps.
- The offload-related files are retained as archival/optional code and are not part of the manuscript's main claim.

## Directory guide

```text
src/common/ shared parameters, hashing, bytes, timers, and utilities
src/lattice/ module-lattice arithmetic and polynomial/vector/matrix helpers
src/preauth/ pre-authentication path, repair generator, transcript, and statement code
src/zk/ early proof-engine and repair-aware verification prototype
src/offload/ optional outsourced-computation experiments
src/legacy/ compatibility code retained for provenance
src/main.cpp command-line dispatcher for prototype experiments
```

## Build

Use an MSYS2 UCRT64 shell:

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-gcc make
bash build.sh
```

The executable is written to:

```text
build/repair_windows_prototype.exe
```

## Example runs

```bash
./build/repair_windows_prototype.exe --show_presets 1
./build/repair_windows_prototype.exe --preset 3 --exp_repair_suite 1 --exp_trials 200 --exp_max_attempts 2000
./build/repair_windows_prototype.exe --preset 3 --exp_a1a2_budget 1 --exp_trials 200
./build/repair_windows_prototype.exe --preset 3 --exp_review_supp 1 --review_seeds 5 --review_sessions 100 --review_trials 1000 --exp_multiseed 1 --exp_accept_prob 1 --exp_metadata_leak 1 --exp_context_mismatch 1
```

Outputs are written under `data/` as CSV files.

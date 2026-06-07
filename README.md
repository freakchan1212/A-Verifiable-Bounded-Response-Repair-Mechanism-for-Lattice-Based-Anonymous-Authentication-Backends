# Verifiable Bounded-Response Repair for Lattice-Based Anonymous Authentication Backends

This repository contains the research code and figure-generation material for the paper on verifier-checkable bounded-response repair in lattice-based anonymous authentication backends.

The repository is organized into three parts:

```text
windows_msys2_prototype/ Earlier C++ prototype used for repair-vs-abort, boundary, tamper, and refresh experiments
ubuntu_lazer_backend/ Main Ubuntu/LaZer-backed research implementation used for the system-level results
figures/ Figure data and generated PDF/PNG/SVG outputs used by the manuscript
```

## Quick start: Ubuntu / LaZer-backed implementation

```bash
cd ubuntu_lazer_backend
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt

export LAZER_ROOT=${LAZER_ROOT:-third_party/lazer}
export HEXL_LIB=$LAZER_ROOT/third_party/hexl-development/build/hexl/lib
export LD_LIBRARY_PATH=$LAZER_ROOT:$HEXL_LIB:${LD_LIBRARY_PATH:-}

bash scripts/build_project.sh
PROFILE=fast ALLOW_FALLBACK=1 REQUIRE_LAZER=0 bash scripts/run_full_experiments.sh
```

For a full LaZer run, prepare a built LaZer dependency and use:

```bash
PROFILE=final REQUIRE_LAZER=1 bash scripts/run_full_experiments.sh
```


## Internal Python package dependency

Several Ubuntu experiment scripts import an internal Python package named `fullzk`. This package is not included in this cleaned repository archive. Before running the full Ubuntu/LaZer experiments, place the package directory at `ubuntu_lazer_backend/fullzk/` or install it into the active Python environment so that imports such as `from fullzk.generator import ...` resolve correctly. If the package is maintained in a separate private repository, clone or install that repository first, then run the scripts in `ubuntu_lazer_backend/`.

## Windows/MSYS2 prototype

The Windows/MSYS2 C++ prototype is retained for traceability. It is not the final LaZer-backed implementation used for the main system-level proof-size and timing results. See `windows_msys2_prototype/README.md`.

## Figures

The `figures/` directory contains source CSV files and generated outputs in `pdf/`, `png/`, and `svg/`. The plotting helper is `figures/make_figures.py`.

## Research boundary

This repository is a research prototype. It supports experiments for verifier-checkable bounded-response repair layers. It is not an audited production anonymous credential system, and it does not replace formal proofs of external issuance, revocation, opening, or universal-composability layers.

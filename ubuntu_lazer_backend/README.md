# Ubuntu / LaZer-backed research implementation

This directory contains the main implementation used for the LaZer-backed system-level experiments in the manuscript.

## Directory layout

```text
experiments/ experiment entry points
lazer_relations/ LaZer relation material for the committed-repair path
lazer_relations_minimal/ minimal baseline relation material
scripts/ build and experiment orchestration scripts
tools/ reporting, summarization, figure/table, and validation helpers
requirements.txt Python dependencies
```

Generated binaries and object files are intentionally excluded. Rebuild relation modules on the target machine.

## Environment

```bash
sudo apt update
sudo apt install -y python3 python3-venv python3-pip build-essential make git
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

LaZer is an external dependency. Place a built LaZer tree under `third_party/lazer` or set `LAZER_ROOT`:

```bash
export LAZER_ROOT=${LAZER_ROOT:-third_party/lazer}
export HEXL_LIB=$LAZER_ROOT/third_party/hexl-development/build/hexl/lib
export LD_LIBRARY_PATH=$LAZER_ROOT:$HEXL_LIB:${LD_LIBRARY_PATH:-}
```


## `fullzk` package

The experiment entry points in `experiments/` and several helper scripts in `tools/` import an internal package named `fullzk`. This package provides the protocol model, instance generator, LaZer bridge, digest-relation helpers, and SHA-256 relation utilities used by the experimental drivers. The cleaned repository archive does not include this package. To run the full Ubuntu/LaZer experiments, either:

1. copy the package directory into `ubuntu_lazer_backend/fullzk/`; or
2. install the package into the active virtual environment; or
3. add the parent directory containing `fullzk` to `PYTHONPATH`.

After that, verify the dependency with:

```bash
python3 -c "import fullzk; print(fullzk.__file__)"
```

## Build relation modules

```bash
cd lazer_relations_minimal
make clean || true
make LAZER_ROOT=../third_party/lazer
cd ..

cd lazer_relations
make clean || true
make LAZER_ROOT=../third_party/lazer
cd ..
```

## Typical runs

Fast smoke run:

```bash
PROFILE=fast ALLOW_FALLBACK=1 REQUIRE_LAZER=0 bash scripts/run_full_experiments.sh
```

Full manuscript-scale run, when LaZer is available:

```bash
PROFILE=final REQUIRE_LAZER=1 bash scripts/run_full_experiments.sh
```

Individual experiments can also be invoked directly, for example:

```bash
python3 experiments/run_boundary_probability.py --help
python3 experiments/run_abort_retry_sweep.py --help
python3 experiments/run_minimal_lazer_baseline.py --help
python3 experiments/run_fullrelation_batch.py --help
python3 experiments/run_serialization_fuzz.py --help
```

## Notes

The implementation models a lattice-based anonymous-authentication backend with verifier-checkable bounded-response repair. Credential issuance, revocation, opening, and composable security layers are outside this code package.

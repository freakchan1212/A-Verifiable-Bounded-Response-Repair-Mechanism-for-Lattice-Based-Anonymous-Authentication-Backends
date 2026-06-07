#!/usr/bin/env bash
set -euo pipefail
EXE=${EXE:-build/repair_windows_prototype.exe}
mkdir -p data

./$EXE --show_presets 1

./$EXE --preset 3 --exp_repair_suite 1 --exp_trials 200 --exp_max_attempts 2000 --exp_tamper_trials 50 --exp_refresh_sessions 200

./$EXE --preset 3 --exp_a1a2_budget 1 --exp_trials 200

./$EXE --preset 3 --exp_review_supp 1 --review_seeds 5 --review_sessions 100 --review_trials 1000 --exp_multiseed 1 --exp_accept_prob 1 --exp_metadata_leak 1 --exp_context_mismatch 1

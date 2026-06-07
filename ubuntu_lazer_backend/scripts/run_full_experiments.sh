#!/usr/bin/env bash
set -euo pipefail

PROFILE=${PROFILE:-final}
OUT=${OUT:-results/final_experiments}
FIG_OUT=${FIG_OUT:-figures}
LAZER_ROOT=${LAZER_ROOT:-third_party/lazer}
REQUIRE_LAZER=${REQUIRE_LAZER:-1}
ALLOW_FALLBACK=${ALLOW_FALLBACK:-0}
RUN_FULLRELATION=${RUN_FULLRELATION:-1}
RUN_SPLICE=${RUN_SPLICE:-1}
RUN_REPAIR_ONLY=${RUN_REPAIR_ONLY:-1}

case "$PROFILE" in
  fast)
    PAPER_SEEDS=${PAPER_SEEDS:-2}; PAPER_SESSIONS=${PAPER_SESSIONS:-20}; HIT_TRIALS=${HIT_TRIALS:-1000}
    LAZER_SEEDS=${LAZER_SEEDS:-1}; LAZER_SESSIONS=${LAZER_SESSIONS:-5}
    FULLREL_SEEDS=${FULLREL_SEEDS:-1}; FULLREL_SESSIONS=${FULLREL_SESSIONS:-5}
    ;;
  paper|final)
    PAPER_SEEDS=${PAPER_SEEDS:-10}; PAPER_SESSIONS=${PAPER_SESSIONS:-200}; HIT_TRIALS=${HIT_TRIALS:-10000}
    LAZER_SEEDS=${LAZER_SEEDS:-10}; LAZER_SESSIONS=${LAZER_SESSIONS:-200}
    FULLREL_SEEDS=${FULLREL_SEEDS:-10}; FULLREL_SESSIONS=${FULLREL_SESSIONS:-200}
    ;;
  strict)
    PAPER_SEEDS=${PAPER_SEEDS:-15}; PAPER_SESSIONS=${PAPER_SESSIONS:-200}; HIT_TRIALS=${HIT_TRIALS:-15000}
    LAZER_SEEDS=${LAZER_SEEDS:-10}; LAZER_SESSIONS=${LAZER_SESSIONS:-300}
    FULLREL_SEEDS=${FULLREL_SEEDS:-10}; FULLREL_SESSIONS=${FULLREL_SESSIONS:-200}
    ;;
  *) echo "Unknown PROFILE=$PROFILE" >&2; exit 2;;
esac

mkdir -p "$OUT/raw" "$OUT/derived" "$OUT/logs" "$OUT/build"
export PYTHONPATH=.

log_step() { echo; echo "========== $* =========="; }
run_log() {
  local name="$1"; shift
  echo "+ $*" | tee "$OUT/logs/${name}.log"
  "$@" 2>&1 | tee -a "$OUT/logs/${name}.log"
}
run_log_mem() {
  local name="$1"; shift
  echo "+ $*" | tee "$OUT/logs/${name}.log"
  if command -v /usr/bin/time >/dev/null 2>&1; then
    /usr/bin/time -v -o "$OUT/logs/${name}.timev" "$@" 2>&1 | tee -a "$OUT/logs/${name}.log"
  else
    "$@" 2>&1 | tee -a "$OUT/logs/${name}.log"
    echo "timev_unavailable=1" > "$OUT/logs/${name}.timev"
  fi
}

log_step "0. Environment and preflight"
python3 tools/collect_environment.py --out "$OUT/environment.json" 2>&1 | tee "$OUT/logs/environment.log" || true
lazer_ready=1
if [[ ! -d "$LAZER_ROOT/python" ]]; then
  echo "LaZer Python directory not found: $LAZER_ROOT/python" | tee "$OUT/logs/lazer_preflight.log"
  lazer_ready=0
fi
if ! command -v sage >/dev/null 2>&1; then
  echo "Sage is not available on PATH; LaZer parameter code generation cannot be rebuilt." | tee -a "$OUT/logs/lazer_preflight.log"
  lazer_ready=0
fi
if [[ "$lazer_ready" != "1" ]]; then
  if [[ "$REQUIRE_LAZER" == "1" && "$ALLOW_FALLBACK" != "1" ]]; then
    echo "REQUIRE_LAZER=1: stopping. Install/build LaZer+Sage or run PROFILE=fast ALLOW_FALLBACK=1 for smoke only." >&2
    exit 3
  fi
else
  export LD_LIBRARY_PATH="$LAZER_ROOT:$LAZER_ROOT/third_party/hexl-development/build/hexl/lib:${LD_LIBRARY_PATH:-}"
fi

log_step "1. Build local C++ wrapper and LaZer relations"
bash scripts/build_project.sh 2>&1 | tee "$OUT/logs/build_project.log" || true
if [[ "$lazer_ready" == "1" ]]; then
  (cd lazer_relations_minimal && make LAZER_ROOT=../$LAZER_ROOT) 2>&1 | tee "$OUT/logs/build_minimal_relation.log"
  (cd lazer_relations && make LAZER_ROOT=../$LAZER_ROOT) 2>&1 | tee "$OUT/logs/build_repair_relation.log"
else
  echo "LaZer relation build skipped; fallback/smoke mode only." | tee "$OUT/logs/build_lazer_skipped.log"
fi

log_step "2. E3 multi-seed boundary hit probability"
run_log boundary_hit python3 experiments/run_boundary_probability.py \
  --seeds "$PAPER_SEEDS" --trials "$HIT_TRIALS" \
  --out-csv "$OUT/raw/boundary_hit_probability.csv"
run_log boundary_hit_seed_level python3 tools/summarize_seed_level.py \
  --csv "$OUT/raw/boundary_hit_probability.csv" \
  --group-cols "param_id,param_label,boundary_id,boundary_label,B" \
  --metrics "hit_rate" \
  --out "$OUT/derived/boundary_hit_seed_level.csv"

log_step "3. E4 multi-seed repair vs abort/retry"
run_log abort_retry python3 experiments/run_abort_retry_sweep.py \
  --seeds "$PAPER_SEEDS" --sessions "$PAPER_SESSIONS" \
  --out-session "$OUT/raw/abort_retry_session.csv" \
  --out-summary "$OUT/derived/abort_retry_summary.csv"
run_log abort_retry_seed_level python3 tools/summarize_seed_level.py \
  --csv "$OUT/raw/abort_retry_session.csv" \
  --group-cols "param_id,param_label,boundary_id,boundary_label,B,method,budget" \
  --metrics "success,attempts,total_ms,prove_ms,verify_ms" \
  --out "$OUT/derived/abort_retry_seed_level.csv"

log_step "4. E5 multi-seed local clipping semantic gap"
run_log local_clipping python3 experiments/run_local_clipping_baseline.py \
  --seeds "$PAPER_SEEDS" --sessions "$PAPER_SESSIONS" \
  --out-session "$OUT/raw/local_clipping_baseline_session.csv" \
  --out-summary "$OUT/derived/local_clipping_baseline_summary.csv"
run_log local_clipping_seed_level python3 tools/summarize_seed_level.py \
  --csv "$OUT/raw/local_clipping_baseline_session.csv" \
  --group-cols "param_id,param_label,boundary_id,boundary_label,B" \
  --metrics "repair_triggered,alg_z_ok,alg_recovered_ok,full_repair_ok,semantic_gap,delta_l0" \
  --out "$OUT/derived/local_clipping_seed_level.csv"

log_step "5. E6 multi-seed context/refresh binding ablation"
run_log binding_ablation python3 experiments/run_binding_ablation.py \
  --seeds "$PAPER_SEEDS" --sessions "$PAPER_SESSIONS" \
  --out-session "$OUT/raw/binding_ablation_session.csv" \
  --out-summary "$OUT/derived/binding_ablation_summary.csv"
run_log binding_ablation_seed_level python3 tools/summarize_seed_level.py \
  --csv "$OUT/raw/binding_ablation_session.csv" \
  --group-cols "attack,attack_label" \
  --metrics "full_accept,no_context_accept,no_refresh_accept,no_binding_accept" \
  --out "$OUT/derived/binding_ablation_seed_level.csv"

log_step "6. E7 leakage statistics: delta_l0/delta_linf/public-z saturation"
run_log leakage python3 experiments/run_repair_metadata_leakage.py \
  --seeds "$PAPER_SEEDS" --sessions "$PAPER_SESSIONS" \
  --out-session "$OUT/raw/repair_metadata_leakage_session.csv" \
  --out-summary "$OUT/derived/repair_metadata_leakage_summary.csv"
run_log leakage_seed_level python3 tools/summarize_seed_level.py \
  --csv "$OUT/raw/repair_metadata_leakage_session.csv" \
  --group-cols "param_id,param_label,boundary_id,boundary_label,B,privacy" \
  --metrics "repair_flag,repair_count,delta_l0,delta_linf,repair_ratio,saturated_ratio,meta_bytes,delta_commitment_len" \
  --out "$OUT/derived/repair_metadata_leakage_seed_level.csv"

backend="lazer"
if [[ "$lazer_ready" != "1" ]]; then
  backend="fallback"
fi
if [[ "$backend" == "fallback" ]]; then
  echo "WARNING: Running fallback backend. These outputs must not be reported as real LaZer." | tee "$OUT/logs/fallback_warning.log"
fi

log_step "6b. Natural, conditional and controlled workload profiles"
run_log workload_profiles python3 experiments/run_workload_profiles.py \
  --backend "$backend" --lazer-root "$LAZER_ROOT" \
  --seeds "$PAPER_SEEDS" --sessions "$PAPER_SESSIONS" \
  --out "$OUT/raw/workload_profiles.csv"

log_step "7. E1 true minimal LaZer baseline"
if [[ "$backend" == "lazer" ]]; then
  run_log_mem minimal_lazer python3 experiments/run_minimal_lazer_baseline.py \
    --seeds "$LAZER_SEEDS" --sessions "$LAZER_SESSIONS" \
    --out-csv "$OUT/minimal_lazer_baseline.csv" \
    --out-dir "$OUT/build/minimal_lazer_baseline" \
    --lazer-root "$LAZER_ROOT"
  run_log minimal_lazer_seed_level python3 tools/summarize_seed_level.py \
    --csv "$OUT/minimal_lazer_baseline.csv" \
    --group-cols "backend" \
    --metrics "proof_bytes,statement_bytes,payload_bytes,prove_ms,verify_ms,total_ms" \
    --out "$OUT/minimal_lazer_baseline_seed_level.csv"
else
  echo "minimal_lazer_baseline skipped because real LaZer is unavailable" | tee "$OUT/logs/minimal_lazer_skipped.log"
fi

log_step "8. E2/E8 zero-offset, repair-enabled and controlled-mixed committed paths"
run_log_mem committed_no_repair python3 experiments/run_batch.py \
  --backend "$backend" --privacy committed --path-mode no_repair \
  --seeds "$LAZER_SEEDS" --sessions "$LAZER_SESSIONS" \
  --out-csv "$OUT/system_lazer_committed_no_repair/batch.csv" \
  --out-dir "$OUT/system_lazer_committed_no_repair/build" \
  --lazer-root "$LAZER_ROOT"
run_log committed_no_repair_seed_level python3 tools/summarize_seed_level.py \
  --csv "$OUT/system_lazer_committed_no_repair/batch.csv" \
  --group-cols "backend,privacy,path_mode" \
  --metrics "proof_bytes,statement_bytes,payload_bytes,repair_metadata_bytes,delta_commitment_len,credential_binding_bytes,prove_ms,verify_ms,total_ms" \
  --out "$OUT/system_lazer_committed_no_repair/batch_seed_level.csv"

if [[ "$RUN_REPAIR_ONLY" == "1" ]]; then
  run_log_mem committed_repair_only python3 experiments/run_batch.py \
    --backend "$backend" --privacy committed --path-mode repair_only \
    --seeds "$LAZER_SEEDS" --sessions "$LAZER_SESSIONS" \
    --out-csv "$OUT/system_lazer_committed_repair_only/batch.csv" \
    --out-dir "$OUT/system_lazer_committed_repair_only/build" \
    --lazer-root "$LAZER_ROOT"
  run_log committed_repair_only_seed_level python3 tools/summarize_seed_level.py \
    --csv "$OUT/system_lazer_committed_repair_only/batch.csv" \
    --group-cols "backend,privacy,path_mode" \
    --metrics "proof_bytes,statement_bytes,payload_bytes,repair_metadata_bytes,delta_l0,delta_linf,saturated_ratio,delta_commitment_len,credential_binding_bytes,prove_ms,verify_ms,total_ms" \
    --out "$OUT/system_lazer_committed_repair_only/batch_seed_level.csv"
fi

run_log_mem committed_mixed python3 experiments/run_batch.py \
  --backend "$backend" --privacy committed --path-mode mixed \
  --seeds "$LAZER_SEEDS" --sessions "$LAZER_SESSIONS" \
  --out-csv "$OUT/system_lazer_committed_mixed/batch.csv" \
  --out-dir "$OUT/system_lazer_committed_mixed/build" \
  --lazer-root "$LAZER_ROOT"
run_log committed_mixed_seed_level python3 tools/summarize_seed_level.py \
  --csv "$OUT/system_lazer_committed_mixed/batch.csv" \
  --group-cols "backend,privacy,path_mode" \
  --metrics "proof_bytes,statement_bytes,payload_bytes,repair_metadata_bytes,delta_l0,delta_linf,saturated_ratio,delta_commitment_len,credential_binding_bytes,prove_ms,verify_ms,total_ms" \
  --out "$OUT/system_lazer_committed_mixed/batch_seed_level.csv"

log_step "9. SHA-256 fullrelation extension"
if [[ "$RUN_FULLRELATION" == "1" ]]; then
  run_log_mem fullrelation python3 experiments/run_fullrelation_batch.py \
    --backend "$backend" --privacy committed --path-mode mixed \
    --seeds "$FULLREL_SEEDS" --sessions "$FULLREL_SESSIONS" \
    --out-csv "$OUT/fullrelation_committed_mixed.csv" \
    --out-dir "$OUT/build/fullrelation_committed_mixed" \
    --lazer-root "$LAZER_ROOT"
  run_log fullrelation_seed_level python3 tools/summarize_seed_level.py \
    --csv "$OUT/fullrelation_committed_mixed.csv" \
    --group-cols "backend,privacy,path_mode" \
    --metrics "proof_bytes,prove_ms,verify_ms,total_ms,sha256_rounds,sha256_est_bool_constraints,sha256_est_word_constraints" \
    --out "$OUT/fullrelation_committed_mixed_seed_level.csv"
fi

log_step "10. E9 constructed negative tests and output-limb splice upper-bound"
run_log serialization_fuzz python3 experiments/run_serialization_fuzz.py --backend "$backend" --lazer-root "$LAZER_ROOT" --out "$OUT/serialization_fuzz_summary.csv"
run_log state_rollback python3 experiments/run_state_rollback.py --backend "$backend" --lazer-root "$LAZER_ROOT" --out "$OUT/state_rollback_summary.csv"
run_log combo_tamper python3 experiments/run_combo_tamper.py --backend "$backend" --lazer-root "$LAZER_ROOT" --out-csv "$OUT/combo_tamper_summary.csv"
run_log profile_nonce_digest_negative python3 experiments/run_profile_nonce_digest_negative.py --backend "$backend" --lazer-root "$LAZER_ROOT" --out "$OUT/profile_nonce_digest_negative.csv"
if [[ "$RUN_SPLICE" == "1" ]]; then
  run_log_mem multi_native_splice python3 experiments/run_multi_digest_splice.py --backend "$backend" --lazer-root "$LAZER_ROOT" --out "$OUT/multi_native_splice.csv"
fi

log_step "11. E10 artifact mapping, tables, figures, checksums"
run_log collect_memory_footprint python3 tools/collect_memory_footprint.py --logs "$OUT/logs" --out "$OUT/derived/memory_footprint.csv"
run_log final_tables python3 tools/make_final_tables.py --root "$OUT" --out-dir "$OUT/derived"
run_log final_figures python3 tools/make_final_figures.py --root "$OUT" --out-dir "$FIG_OUT"
run_log statistical_report python3 tools/statistical_report.py --artifact-dir "$OUT" --out-md "$OUT/statistical_report.md" --out-json "$OUT/statistical_report.json" || true
run_log checksums python3 tools/make_checksums.py --root "$OUT" --out "$OUT/checksums.sha256" || true
run_log figure_checksums python3 tools/make_checksums.py --root "$FIG_OUT" --out "$FIG_OUT/checksums.sha256" || true
if [[ "$REQUIRE_LAZER" == "1" ]]; then
  req_flag="--require-lazer"
else
  req_flag=""
fi
if [[ "$ALLOW_FALLBACK" == "1" ]]; then
  fb_flag="--allow-fallback"
else
  fb_flag=""
fi
run_log validate_final python3 tools/validate_results.py --root "$OUT" --figures "$FIG_OUT" $req_flag $fb_flag

tar -czf "${OUT}.tar.gz" "$OUT" "$FIG_OUT" 2>/dev/null || true
if [[ -f "${OUT}.tar.gz" ]]; then
  sha256sum "${OUT}.tar.gz" > "${OUT}.tar.gz.sha256"
fi

log_step "DONE"
echo "Artifact root: $OUT"
echo "Figure root: $FIG_OUT"
echo "Key outputs:"
echo "  $OUT/minimal_lazer_baseline.csv"
echo "  $OUT/system_lazer_committed_no_repair/batch.csv"
echo "  $OUT/system_lazer_committed_repair_only/batch.csv"
echo "  $OUT/system_lazer_committed_mixed/batch.csv"
echo "  $OUT/derived/cost_breakdown_summary.csv"
echo "  $OUT/derived/repair_metadata_leakage_summary.csv"
echo "  $OUT/derived/memory_footprint.csv"
echo "  $OUT/raw/workload_profiles.csv"
echo "  $OUT/profile_nonce_digest_negative.csv"
echo "  $OUT/experiment_mapping.md"
echo "  $OUT/VALIDATION_REPORT.md"
echo "  $FIG_OUT/figure_manifest.md"
echo "  ${OUT}.tar.gz"

                      
from __future__ import annotations

import argparse
import csv
import json
import math
import statistics
from pathlib import Path
from typing import Any, Dict, Iterable, List, Tuple

try:
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
except ModuleNotFoundError as e:
    raise SystemExit("Missing matplotlib. Install it with: python3 -m pip install matplotlib") from e

def read_csv(path: Path) -> List[Dict[str, str]]:
    if not path.exists():
        return []
    with path.open(newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))

def write_csv(path: Path, rows: List[Dict[str, Any]], fields: List[str] | None = None) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    if fields is None:
        fields = []
        for r in rows:
            for k in r:
                if k not in fields:
                    fields.append(k)
    with path.open("w", newline="", encoding="utf-8") as f:
        wr = csv.DictWriter(f, fieldnames=fields)
        wr.writeheader()
        for r in rows:
            wr.writerow(r)

def fnum(x: Any, default: float = 0.0) -> float:
    try:
        if x is None:
            return default
        s = str(x).strip()
        if s == "" or s.lower() in {"nan", "none", "inf"}:
            return default
        return float(s)
    except Exception:
        return default

def bval(x: Any) -> bool:
    return str(x).strip().lower() in {"1", "true", "yes", "y"}

def mean(vals: Iterable[float]) -> float:
    vals = list(vals)
    return statistics.mean(vals) if vals else 0.0

def ci95(vals: List[float]) -> Tuple[float, float, float, float]:
    if not vals:
        return 0.0, 0.0, 0.0, 0.0
    m = statistics.mean(vals)
    if len(vals) == 1:
        return m, 0.0, m, m
    sd = statistics.stdev(vals)
    half = 1.96 * sd / math.sqrt(len(vals))
    return m, sd, m - half, m + half

def percentile(vals: List[float], q: float) -> float:
    vals = sorted(vals)
    if not vals:
        return 0.0
    return vals[int(q * (len(vals) - 1))]

class finalFigureWriter:
    def __init__(self, root: Path, out: Path):
        self.root = root
        self.out = out
        self.data = out / "data"
        self.png = out / "png"
        self.pdf = out / "pdf"
        self.svg = out / "svg"
        for p in [self.data, self.png, self.pdf, self.svg]:
            p.mkdir(parents=True, exist_ok=True)
        self.manifest: List[Dict[str, Any]] = []

    def save(self, name: str, title: str, note: str = "") -> None:
        plt.tight_layout()
        plt.savefig(self.png / f"{name}.png", dpi=300, bbox_inches="tight")
        plt.savefig(self.pdf / f"{name}.pdf", bbox_inches="tight")
        plt.savefig(self.svg / f"{name}.svg", bbox_inches="tight")
        plt.close()
        self.manifest.append({
            "name": name,
            "title": title,
            "png": f"png/{name}.png",
            "pdf": f"pdf/{name}.pdf",
            "svg": f"svg/{name}.svg",
            "data": f"data/{name}.csv",
            "note": note,
        })

    def write_manifest(self) -> None:
        write_csv(self.out / "figure_manifest_final.csv", self.manifest)
        lines = ["# JISA final Figure Manifest", "", "All figure source CSVs are in `data/`; exported figures are in `pdf/`, `svg/`, and `png/`.", "", "| Figure | Title | Data | PDF | SVG | PNG | Note |", "|---|---|---|---|---|---|---|"]
        for r in self.manifest:
            lines.append(f"| {r['name']} | {r['title']} | `{r['data']}` | `{r['pdf']}` | `{r['svg']}` | `{r['png']}` | {r['note']} |")
        (self.out / "figure_manifest.md").write_text("\n".join(lines) + "\n", encoding="utf-8")

    def fig7_1_boundary_hit(self) -> None:
        rows = read_csv(self.root / "raw" / "boundary_hit_probability.csv")
        data: List[Dict[str, Any]] = []
        groups: Dict[Tuple[str, str, str, str], Dict[str, List[float]]] = {}
        for r in rows:
            k = (r.get("param_id", ""), r.get("param_label", ""), r.get("boundary_id", ""), r.get("boundary_label", ""))
            groups.setdefault(k, {"hit_rate": []})["hit_rate"].append(fnum(r.get("hit_rate")))
        order = [("main", "loose"), ("main", "medium"), ("main", "tight"), ("aligned", "loose"), ("aligned", "medium"), ("aligned", "tight")]
        for pid, bid in order:
            match = next(((k, v) for k, v in groups.items() if k[0] == pid and k[2] == bid), None)
            if not match:
                continue
            k, v = match
            m, sd, lo, hi = ci95(v["hit_rate"])
            data.append({"param_id": k[0], "param_label": k[1], "boundary_id": k[2], "boundary_label": k[3], "mean_hit_rate": m, "std_seed": sd, "ci95_low": lo, "ci95_high": hi, "seed_count": len(v["hit_rate"])})
        name = "fig7_1_final_boundary_hit_probability"
        write_csv(self.data / f"{name}.csv", data)
        labels = [f"{d['param_id']}\n{d['boundary_id']}" for d in data]
        vals = [float(d["mean_hit_rate"]) for d in data]
        yerr = [[max(0.0, vals[i] - float(data[i]["ci95_low"])) for i in range(len(data))], [max(0.0, float(data[i]["ci95_high"]) - vals[i]) for i in range(len(data))]]
        plt.figure(figsize=(8.8, 4.8))
        plt.bar(range(len(vals)), vals, yerr=yerr, capsize=3)
        plt.xticks(range(len(vals)), labels)
        plt.ylabel("Single-try hit probability")
        plt.ylim(0, 1.08)
        plt.title("Boundary hit probability with seed-level 95% CI")
        self.save(name, "Boundary hit probability with seed-level 95% CI", "Figure 7-1 replacement; uses multi-seed input")

    def fig7_2_abort_retry(self) -> None:
        rows = read_csv(self.root / "raw" / "abort_retry_session.csv")
        groups: Dict[Tuple[str, str], Dict[str, List[float]]] = {}
        for r in rows:
            if r.get("boundary_id") != "tight":
                continue
            if r.get("method") == "deterministic_repair":
                label = "repair"
            elif r.get("method") == "abort_retry" and str(r.get("budget")) == "2000":
                label = "abort_2000"
            else:
                continue
                                    
            k = (r.get("param_id", ""), label)
            groups.setdefault(k, {}).setdefault(str(r.get("seed", "0")), []).append(fnum(r.get("total_ms")))
        data: List[Dict[str, Any]] = []
        for param in ["main", "aligned"]:
            for method in ["repair", "abort_2000"]:
                seed_means = [mean(v) for (p, m), by_seed in groups.items() if p == param and m == method for v in by_seed.values()]
                m, sd, lo, hi = ci95(seed_means)
                data.append({"param_id": param, "method": method, "mean_total_ms": m, "std_seed": sd, "ci95_low": lo, "ci95_high": hi, "seed_count": len(seed_means)})
        name = "fig7_2_final_repair_vs_abort_retry_latency"
        write_csv(self.data / f"{name}.csv", data)
        labels = [f"{d['param_id']}\n{d['method']}" for d in data]
        vals = [float(d["mean_total_ms"]) for d in data]
        yerr = [[max(0.0, vals[i] - float(data[i]["ci95_low"])) for i in range(len(data))], [max(0.0, float(data[i]["ci95_high"]) - vals[i]) for i in range(len(data))]]
        plt.figure(figsize=(8.8, 4.8))
        plt.bar(range(len(vals)), vals, yerr=yerr, capsize=3)
        plt.yscale("log")
        plt.xticks(range(len(vals)), labels)
        plt.ylabel("Mean total latency (ms, log scale)")
        plt.title("Tight-bound deterministic repair vs abort/retry budget 2000")
        self.save(name, "Tight-bound repair vs abort/retry latency", "Figure 7-2 replacement; local boundary-path timing")

    def fig7_3_local_clipping(self) -> None:
        rows = read_csv(self.root / "derived" / "local_clipping_baseline_summary.csv")
        data = []
        for r in rows:
            if r.get("param_id") != "main":
                continue
            data.append({"boundary_id": r.get("boundary_id"), "direct_z": fnum(r.get("alg_z_ok_rate")), "recovered_z_delta": fnum(r.get("alg_recovered_ok_rate")), "full_repair": fnum(r.get("full_repair_ok_rate")), "semantic_gap": fnum(r.get("semantic_gap_rate"))})
        order = ["loose", "medium", "tight"]
        data.sort(key=lambda r: order.index(r["boundary_id"]) if r["boundary_id"] in order else 99)
        name = "fig7_3_final_local_clipping_semantic_gap"
        write_csv(self.data / f"{name}.csv", data)
        labels = [r["boundary_id"] for r in data]
        x = list(range(len(labels)))
        width = 0.24
        plt.figure(figsize=(8.2, 4.8))
        plt.bar([i - width for i in x], [r["direct_z"] for r in data], width, label="direct z")
        plt.bar(x, [r["recovered_z_delta"] for r in data], width, label="recover z+Delta")
        plt.bar([i + width for i in x], [r["full_repair"] for r in data], width, label="full repair relation")
        plt.xticks(x, labels)
        plt.ylabel("Pass rate")
        plt.ylim(0, 1.08)
        plt.legend(frameon=False)
        plt.title("Local clipping is not semantic repair")
        self.save(name, "Local clipping versus semantic repair", "Figure 7-3 replacement")

    def fig7_4_binding_ablation(self) -> None:
        rows = read_csv(self.root / "derived" / "binding_ablation_summary.csv")
        attacks = ["honest", "wrong_context", "stale_refresh", "wrong_context_and_refresh"]
        cols = ["full_accept_rate", "no_context_accept_rate", "no_refresh_accept_rate", "no_binding_accept_rate"]
        data = []
        matrix = []
        for atk in attacks:
            r = next((x for x in rows if x.get("attack") == atk), {})
            vals = [fnum(r.get(c)) for c in cols]
            matrix.append(vals)
            for c, v in zip(cols, vals):
                data.append({"attack": atk, "verifier_variant": c.replace("_accept_rate", ""), "accept_rate": v})
        name = "fig7_4_final_binding_ablation_heatmap"
        write_csv(self.data / f"{name}.csv", data)
        plt.figure(figsize=(8.8, 4.8))
        im = plt.imshow(matrix, vmin=0, vmax=1, aspect="auto")
        plt.colorbar(im, label="Acceptance rate")
        plt.xticks(range(len(cols)), ["full", "no ctx", "no refresh", "no binding"])
        plt.yticks(range(len(attacks)), ["honest", "wrong ctx", "stale refresh", "both wrong"])
        for i, row in enumerate(matrix):
            for j, v in enumerate(row):
                plt.text(j, i, f"{v:.1f}", ha="center", va="center")
        plt.title("Binding ablation: context and refresh checks are necessary")
        self.save(name, "Context/refresh binding ablation heatmap", "Figure 7-4 replacement")

    def _summarize_path(self, name: str, path: Path, sample_kind: str, display: str) -> Dict[str, Any] | None:
        rows = read_csv(path)
        if not rows:
            return None
        out: Dict[str, Any] = {"path": name, "display": display, "sample_kind": sample_kind, "samples": len(rows)}
        for metric in ["proof_bytes", "statement_bytes", "repair_metadata_bytes", "meta_bytes", "delta_commitment_len", "credential_binding_bytes", "payload_bytes", "prove_ms", "verify_ms", "total_ms"]:
            vals = [fnum(r.get(metric)) for r in rows if r.get(metric, "") != ""]
            if vals:
                out[f"{metric}_mean"] = mean(vals)
                out[f"{metric}_median"] = statistics.median(vals)
                out[f"{metric}_p95"] = percentile(vals, 0.95)
            else:
                out[f"{metric}_mean"] = 0.0
                out[f"{metric}_median"] = 0.0
                out[f"{metric}_p95"] = 0.0
        return out

    def collect_cost_rows(self) -> List[Dict[str, Any]]:
        specs = [
            ("minimal_lazer", self.root / "minimal_lazer_baseline.csv", "seeded", "minimal"),
            ("zero_offset_committed", self.root / "system_lazer_committed_no_repair" / "batch.csv", "seeded", "zero-offset"),
            ("repair_enabled", self.root / "system_lazer_committed_repair_only" / "batch.csv", "seeded", "repair-only"),
            ("mixed_committed", self.root / "system_lazer_committed_mixed" / "batch.csv", "seeded", "mixed"),
            ("sha256_fullrelation", self.root / "fullrelation_committed_mixed.csv", "seeded", "full-relation"),
            ("output_limb_splice", self.root / "multi_native_splice.csv", "n=1 upper-bound", "splice n=1"),
        ]
        rows: List[Dict[str, Any]] = []
        for spec in specs:
            r = self._summarize_path(*spec)
            if r:
                rows.append(r)
        return rows

    def fig7_5_cost_proof(self) -> None:
        data = self.collect_cost_rows()
        name = "fig7_5_final_coverage_cost_proof_bytes"
        write_csv(self.data / f"{name}.csv", data)
        if not data:
            return
        labels = [r["display"] for r in data]
        vals = [float(r.get("proof_bytes_mean", 0.0)) / 1024.0 for r in data]
        plt.figure(figsize=(9.2, 4.8))
        plt.bar(range(len(vals)), vals)
        plt.xticks(range(len(vals)), labels, rotation=20, ha="right")
        plt.ylabel("Mean proof size (KB)")
        plt.title("Coverage-cost comparison: proof size")
        self.save(name, "Coverage-cost comparison: proof size", "Figure 7-5 replacement; n=1 splice marked in data CSV")

    def fig7_6_cost_time(self) -> None:
        data = self.collect_cost_rows()
        name = "fig7_6_final_coverage_cost_timing"
        write_csv(self.data / f"{name}.csv", data)
        if not data:
            return
        labels = [r["display"] for r in data]
        x = list(range(len(labels)))
        width = 0.24
        plt.figure(figsize=(9.4, 4.8))
        plt.bar([i - width for i in x], [float(r.get("prove_ms_mean", 0.0)) for r in data], width, label="prove")
        plt.bar(x, [float(r.get("verify_ms_mean", 0.0)) for r in data], width, label="verify")
        plt.bar([i + width for i in x], [float(r.get("total_ms_mean", 0.0)) for r in data], width, label="total")
        plt.xticks(x, labels, rotation=20, ha="right")
        plt.ylabel("Mean time (ms)")
        plt.title("Coverage-cost comparison: timing")
        plt.legend(frameon=False)
        self.save(name, "Coverage-cost comparison: timing", "Figure 7-6 replacement; time口径见表格 CSV")

    def fig7_7_leakage(self) -> None:
        rows = read_csv(self.root / "raw" / "repair_metadata_leakage_session.csv")
        groups: Dict[Tuple[str, str, str], Dict[str, List[float]]] = {}
        for r in rows:
            if r.get("param_id") != "main":
                continue
            k = (r.get("boundary_id", ""), r.get("privacy", ""), r.get("B", ""))
            g = groups.setdefault(k, {"delta_l0": [], "delta_linf": [], "saturated_ratio": []})
            g["delta_l0"].append(fnum(r.get("delta_l0")))
            g["delta_linf"].append(fnum(r.get("delta_linf")))
            g["saturated_ratio"].append(fnum(r.get("saturated_ratio")))
        order = [("loose", "committed"), ("medium", "committed"), ("tight", "committed"), ("loose", "transparent"), ("medium", "transparent"), ("tight", "transparent")]
        data: List[Dict[str, Any]] = []
        for bid, priv in order:
            match = next(((k, v) for k, v in groups.items() if k[0] == bid and k[1] == priv), None)
            if not match:
                continue
            k, v = match
            m, sd, lo, hi = ci95(v["delta_l0"])
            data.append({"boundary_id": bid, "privacy": priv, "B": k[2], "delta_l0_mean": m, "delta_l0_std": sd, "delta_l0_ci95_low": lo, "delta_l0_ci95_high": hi, "delta_linf_mean": mean(v["delta_linf"]), "saturated_ratio_mean": mean(v["saturated_ratio"]), "samples": len(v["delta_l0"])})
        name = "fig7_7_final_leakage_delta_l0_distribution"
        write_csv(self.data / f"{name}.csv", data)
        labels = [f"{r['privacy']}\n{r['boundary_id']}" for r in data]
        vals = [float(r["delta_l0_mean"]) for r in data]
        yerr = [[max(0.0, vals[i] - float(data[i]["delta_l0_ci95_low"])) for i in range(len(data))], [max(0.0, float(data[i]["delta_l0_ci95_high"]) - vals[i]) for i in range(len(data))]]
        plt.figure(figsize=(9.0, 4.8))
        plt.bar(range(len(vals)), vals, yerr=yerr, capsize=3)
        plt.xticks(range(len(vals)), labels)
        plt.ylabel("Mean delta_l0")
        plt.title("Repair metadata leakage: delta_l0 distribution")
        self.save(name, "Repair metadata leakage: delta_l0", "Figure 7-7 replacement; data CSV also includes delta_linf and saturated_ratio")

    def fig7_8_negative_tests(self) -> None:
        sources = [
            ("serialization", self.root / "serialization_fuzz_summary.csv", "case", "accepted", "expected_accept"),
            ("state_rollback", self.root / "state_rollback_summary.csv", "case", "accepted", "expected_accept"),
            ("combo_tamper", self.root / "combo_tamper_summary.csv", "case", "verify_ok", None),
            ("output_limb_splice", self.root / "multi_native_splice.csv", "case", "verify_ok", None),
        ]
        data: List[Dict[str, Any]] = []
        for category, path, case_col, accepted_col, expected_col in sources:
            rows = read_csv(path)
            if not rows:
                continue
            total_attack = 0
            rejected = 0
            honest_ok = 0
            honest_total = 0
            for r in rows:
                case = r.get(case_col, "")
                is_honest = case.startswith("honest") or case == "honest_new_epoch"
                accepted = bval(r.get(accepted_col, False))
                if is_honest:
                    honest_total += 1
                    honest_ok += 1 if accepted else 0
                else:
                    total_attack += 1
                    rejected += 1 if not accepted else 0
            if total_attack:
                data.append({"category": category, "attack_samples": total_attack, "rejected_attack_samples": rejected, "rejection_rate": rejected / total_attack, "honest_samples": honest_total, "honest_accept_rate": honest_ok / honest_total if honest_total else ""})
        name = "fig7_8_final_constructed_negative_tests"
        write_csv(self.data / f"{name}.csv", data)
        labels = [r["category"] for r in data]
        vals = [float(r["rejection_rate"]) for r in data]
        plt.figure(figsize=(8.8, 4.8))
        plt.bar(range(len(vals)), vals)
        plt.xticks(range(len(vals)), labels, rotation=20, ha="right")
        plt.ylabel("Rejected attack sample ratio")
        plt.ylim(0, 1.08)
        plt.title("Constructed negative tests: implementation-level rejection")
        for i, r in enumerate(data):
            plt.text(i, min(1.02, vals[i] + 0.02), f"{r['rejected_attack_samples']}/{r['attack_samples']}", ha="center", fontsize=9)
        self.save(name, "Constructed negative tests", "Figure 7-8 replacement; not a coverage probability")

    def figS1_payload_breakdown(self) -> None:
        data = self.collect_cost_rows()
        name = "figS1_final_payload_breakdown"
        write_csv(self.data / f"{name}.csv", data)
        if not data:
            return
        labels = [r["display"] for r in data]
        x = list(range(len(labels)))
        metrics = ["statement_bytes_mean", "repair_metadata_bytes_mean", "delta_commitment_len_mean", "credential_binding_bytes_mean", "proof_bytes_mean"]
        bottoms = [0.0] * len(data)
        plt.figure(figsize=(9.4, 4.8))
        for metric in metrics:
            vals = [float(r.get(metric, 0.0)) / 1024.0 for r in data]
            plt.bar(x, vals, bottom=bottoms, label=metric.replace("_mean", ""))
            bottoms = [bottoms[i] + vals[i] for i in range(len(vals))]
        plt.xticks(x, labels, rotation=20, ha="right")
        plt.ylabel("Payload component size (KB)")
        plt.title("Payload/cost breakdown")
        plt.legend(frameon=False, fontsize=8)
        self.save(name, "Payload/cost breakdown", "Supplementary final figure for payload breakdown")

    def figS2_memory_footprint(self) -> None:
        rows = read_csv(self.root / "derived" / "memory_footprint.csv")
        name = "figS2_final_memory_footprint"
        write_csv(self.data / f"{name}.csv", rows)
        if not rows:
            return
        labels = [r.get("path_label", r.get("sample_kind", "")) for r in rows]
        vals = [fnum(r.get("max_rss_mb")) for r in rows]
        plt.figure(figsize=(9.4, 4.8))
        plt.bar(range(len(vals)), vals)
        plt.xticks(range(len(vals)), labels, rotation=22, ha="right")
        plt.ylabel("Peak RSS (MB)")
        plt.title("Memory footprint from /usr/bin/time -v")
        self.save(name, "Memory footprint", "Supplementary final figure; generated from time -v logs")

    def run_all(self) -> None:
        self.fig7_1_boundary_hit()
        self.fig7_2_abort_retry()
        self.fig7_3_local_clipping()
        self.fig7_4_binding_ablation()
        self.fig7_5_cost_proof()
        self.fig7_6_cost_time()
        self.fig7_7_leakage()
        self.fig7_8_negative_tests()
        self.figS1_payload_breakdown()
        self.figS2_memory_footprint()
        self.write_manifest()

def main() -> int:
    ap = argparse.ArgumentParser(description="Generate JISA final figures with data/pdf/svg/png directories")
    ap.add_argument("--root", default="results/final_experiments", help="experiment artifact root")
    ap.add_argument("--out-dir", default="figures", help="output figure directory")
    args = ap.parse_args()
    writer = finalFigureWriter(Path(args.root), Path(args.out_dir))
    writer.run_all()
    print(f"wrote {args.out_dir}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())

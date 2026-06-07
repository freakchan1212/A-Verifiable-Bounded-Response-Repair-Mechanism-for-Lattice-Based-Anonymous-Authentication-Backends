from pathlib import Path

import numpy as np
import pandas as pd
import matplotlib.pyplot as plt

BASE = Path(__file__).resolve().parent
DATA = BASE / "data"
OUT_DIRS = {name: BASE / name for name in ["png", "pdf", "svg"]}


def ensure_dirs():
    for directory in OUT_DIRS.values():
        directory.mkdir(parents=True, exist_ok=True)


def save_all(fig, stem):
    fig.savefig(OUT_DIRS["png"] / f"{stem}.png", dpi=300, bbox_inches="tight")
    fig.savefig(OUT_DIRS["pdf"] / f"{stem}.pdf", bbox_inches="tight")
    fig.savefig(OUT_DIRS["svg"] / f"{stem}.svg", bbox_inches="tight")
    plt.close(fig)


def fig_7_1():
    stem = "Fig_7_1_boundary_hit_probability"
    df = pd.read_csv(DATA / f"{stem}.csv")
    order = [("main", "loose"), ("main", "medium"), ("main", "tight"), ("aligned", "loose"), ("aligned", "medium"), ("aligned", "tight")]
    labels = []
    values = []
    errors = []
    for param, bound in order:
        row = df[(df["param_id"] == param) & (df["boundary_id"] == bound)].iloc[0]
        labels.append(f"{param.capitalize()}\n{bound.capitalize()}")
        values.append(float(row["mean_hit_rate"]))
        errors.append(max(float(row["mean_hit_rate"]) - float(row["ci95_low"]), float(row["ci95_high"]) - float(row["mean_hit_rate"])))
    fig, ax = plt.subplots(figsize=(7.2, 4.2))
    x = np.arange(len(labels))
    ax.bar(x, values, yerr=errors, capsize=3)
    ax.axvline(2.5, linestyle="--", linewidth=0.8)
    ax.set_xticks(x)
    ax.set_xticklabels(labels)
    ax.set_ylim(0, 1.08)
    ax.set_ylabel("Single-try hit probability")
    ax.grid(axis="y", linestyle="--", linewidth=0.5, alpha=0.5)
    for xi, val in zip(x, values):
        ax.text(xi, min(val + 0.03, 1.04), f"{val:.3f}", ha="center", va="bottom", fontsize=9)
    fig.tight_layout()
    save_all(fig, stem)


def fig_7_2():
    stem = "Fig_7_2_retry_budget"
    df = pd.read_csv(DATA / f"{stem}.csv")
    methods = ["Repair", "Abort/retry T=10", "Abort/retry T=2000"]
    params = ["Main", "Aligned"]
    fig, ax = plt.subplots(figsize=(7.0, 4.2))
    x = np.arange(len(params))
    width = 0.24
    for idx, method in enumerate(methods):
        vals = [df[(df["param_label"] == param) & (df["method_label"] == method)]["total_ms_mean"].iloc[0] for param in params]
        ax.bar(x + (idx - 1) * width, vals, width, label=method)
    ax.set_yscale("log")
    ax.set_xticks(x)
    ax.set_xticklabels(params)
    ax.set_ylabel("Mean local handling time (ms, log scale)")
    ax.set_xlabel("Parameter set under tight boundary B=3")
    ax.legend(frameon=False, ncol=3, loc="lower center", bbox_to_anchor=(0.5, 1.02))
    ax.grid(axis="y", linestyle="--", linewidth=0.5, alpha=0.5)
    fig.tight_layout()
    save_all(fig, stem)


def fig_7_3():
    stem = "Fig_7_3_local_clipping_semantic_gap"
    df = pd.read_csv(DATA / f"{stem}.csv")
    bounds = ["loose", "medium", "tight"]
    labels = [b.capitalize() for b in bounds]
    series = [("direct_z", "Direct z"), ("recovered_z_delta", "Recovered z + Δ"), ("full_repair", "Full repair relation")]
    fig, ax = plt.subplots(figsize=(7.0, 4.2))
    x = np.arange(len(labels))
    width = 0.24
    for idx, (col, name) in enumerate(series):
        vals = [df[df["boundary_id"] == b][col].iloc[0] for b in bounds]
        ax.bar(x + (idx - 1) * width, vals, width, label=name)
    ax.set_xticks(x)
    ax.set_xticklabels(labels)
    ax.set_ylabel("Pass rate")
    ax.set_ylim(0, 1.1)
    ax.legend(frameon=False, ncol=3, loc="lower center", bbox_to_anchor=(0.5, 1.02))
    ax.grid(axis="y", linestyle="--", linewidth=0.5, alpha=0.5)
    fig.tight_layout()
    save_all(fig, stem)


def fig_7_4():
    stem = "Fig_7_4_binding_ablation_heatmap"
    df = pd.read_csv(DATA / f"{stem}.csv")
    rows = ["Honest", "Wrong ctx", "Stale refresh", "Both wrong"]
    cols = ["Full", "No ctx", "No refresh", "No binding"]
    mat = np.array([[df[(df["attack"] == r) & (df["verifier_variant"] == c)]["accept_rate"].iloc[0] for c in cols] for r in rows])
    fig, ax = plt.subplots(figsize=(6.4, 4.5))
    im = ax.imshow(mat, vmin=0, vmax=1, aspect="auto")
    ax.set_xticks(np.arange(len(cols)))
    ax.set_xticklabels(cols)
    ax.set_yticks(np.arange(len(rows)))
    ax.set_yticklabels(rows)
    ax.set_xlabel("Verifier configuration")
    ax.set_ylabel("Test case")
    for i in range(mat.shape[0]):
        for j in range(mat.shape[1]):
            ax.text(j, i, f"{mat[i, j]:.1f}", ha="center", va="center", fontsize=9)
    fig.colorbar(im, ax=ax, label="Acceptance rate")
    fig.tight_layout()
    save_all(fig, stem)


def fig_7_5():
    stem = "Fig_7_5_coverage_cost_proof_bytes"
    df = pd.read_csv(DATA / f"{stem}.csv")
    fig, ax = plt.subplots(figsize=(7.4, 4.4))
    labels = df["display"].astype(str).tolist()
    vals = df["proof_bytes_mean"].astype(float) / 1024.0
    x = np.arange(len(labels))
    ax.bar(x, vals)
    ax.set_xticks(x)
    ax.set_xticklabels(labels)
    ax.set_ylabel("Mean proof size (KB)")
    ax.grid(axis="y", linestyle="--", linewidth=0.5, alpha=0.5)
    for xi, val in zip(x, vals):
        ax.text(xi, val + max(vals) * 0.015, f"{val:.1f}", ha="center", va="bottom", fontsize=9)
    fig.tight_layout()
    save_all(fig, stem)


def fig_7_6():
    stem = "Fig_7_6_coverage_cost_timing"
    df = pd.read_csv(DATA / f"{stem}.csv")
    labels = df["display"].astype(str).tolist()
    x = np.arange(len(labels))
    width = 0.25
    fig, ax = plt.subplots(figsize=(7.5, 4.5))
    for idx, (col, name) in enumerate([("prove_ms_mean", "Prove"), ("verify_ms_mean", "Verify"), ("total_ms_mean", "Total")]):
        ax.bar(x + (idx - 1) * width, df[col].astype(float), width, label=name)
    ax.set_xticks(x)
    ax.set_xticklabels(labels)
    ax.set_ylabel("Mean time (ms)")
    ax.legend(frameon=False, ncol=3, loc="lower center", bbox_to_anchor=(0.5, 1.02))
    ax.grid(axis="y", linestyle="--", linewidth=0.5, alpha=0.5)
    fig.tight_layout()
    save_all(fig, stem)


def fig_7_7():
    stem = "Fig_7_7_leakage_delta_l0_distribution"
    df = pd.read_csv(DATA / f"{stem}.csv")
    labels = []
    vals = []
    errs = []
    for privacy in ["committed", "transparent"]:
        for bound in ["loose", "medium", "tight"]:
            row = df[(df["privacy"] == privacy) & (df["boundary_id"] == bound)].iloc[0]
            labels.append(f"{privacy.capitalize()}\n{bound.capitalize()}")
            vals.append(float(row["delta_l0_mean"]))
            errs.append(max(float(row["delta_l0_mean"]) - float(row["delta_l0_ci95_low"]), float(row["delta_l0_ci95_high"]) - float(row["delta_l0_mean"])))
    fig, ax = plt.subplots(figsize=(7.2, 4.4))
    x = np.arange(len(labels))
    ax.bar(x, vals, yerr=errs, capsize=3)
    ax.axvline(2.5, linestyle="--", linewidth=0.8)
    ax.set_xticks(x)
    ax.set_xticklabels(labels)
    ax.set_ylabel(r"Mean $\|\Delta\|_0$")
    ax.grid(axis="y", linestyle="--", linewidth=0.5, alpha=0.5)
    for xi, val in zip(x, vals):
        ax.text(xi, val + max(vals) * 0.015, f"{val:.2f}", ha="center", va="bottom", fontsize=9)
    fig.tight_layout()
    save_all(fig, stem)


def fig_7_8():
    stem = "Fig_7_8_payload_breakdown"
    df = pd.read_csv(DATA / f"{stem}.csv")
    labels = df["display"].astype(str).tolist()
    proof = df["proof_bytes_mean"].astype(float) / 1024.0
    statement = df["statement_bytes_mean"].astype(float) / 1024.0
    metadata = (df["repair_metadata_bytes_mean"].astype(float) + df["credential_binding_bytes_mean"].astype(float) + df["delta_commitment_len_mean"].astype(float)) / 1024.0
    x = np.arange(len(labels))
    fig, ax = plt.subplots(figsize=(7.6, 4.4))
    ax.bar(x, proof, label="Proof")
    ax.bar(x, statement, bottom=proof, label="Statement")
    ax.bar(x, metadata, bottom=proof + statement, label="Metadata/bindings")
    ax.set_xticks(x)
    ax.set_xticklabels(labels)
    ax.set_ylabel("Payload component (KB)")
    ax.legend(frameon=False, ncol=3, loc="lower center", bbox_to_anchor=(0.5, 1.02))
    ax.grid(axis="y", linestyle="--", linewidth=0.5, alpha=0.5)
    fig.tight_layout()
    save_all(fig, stem)


def fig_7_9():
    stem = "Fig_7_9_peak_rss"
    df = pd.read_csv(DATA / f"{stem}.csv")
    df["short_label"] = df["short_label"].replace({"SHA-256 fullrelation": "SHA-256 full-relation"})
    order = ["Minimal LaZer", "Zero-offset", "Repair-enabled", "Controlled mixed", "SHA-256 full-relation", "Output-limb splice"]
    df["short_label"] = pd.Categorical(df["short_label"], categories=order, ordered=True)
    df = df.sort_values("short_label")
    y = np.arange(len(df))
    fig, ax = plt.subplots(figsize=(7.4, 4.8))
    ax.barh(y, df["max_rss_mb"].astype(float))
    ax.set_yticks(y)
    ax.set_yticklabels(df["short_label"].astype(str))
    ax.invert_yaxis()
    ax.set_xlabel("Peak RSS (MB)")
    ax.grid(axis="x", linestyle="--", linewidth=0.5, alpha=0.5)
    max_val = df["max_rss_mb"].astype(float).max()
    ax.set_xlim(0, max_val * 1.16)
    for yi, val in zip(y, df["max_rss_mb"].astype(float)):
        ax.text(val + max_val * 0.015, yi, f"{val:.1f}", va="center", fontsize=9)
    fig.tight_layout()
    save_all(fig, stem)


def fig_7_10():
    stem = "Fig_7_10_constructed_negative_tests"
    df = pd.read_csv(DATA / f"{stem}.csv")
    labels = df["category"].astype(str).tolist()
    vals = df["rejection_rate"].astype(float)
    totals = df["rejected_attack_samples"].astype(int).astype(str) + "/" + df["attack_samples"].astype(int).astype(str)
    fig, ax = plt.subplots(figsize=(7.0, 4.2))
    x = np.arange(len(labels))
    ax.bar(x, vals)
    ax.set_xticks(x)
    ax.set_xticklabels(labels)
    ax.set_ylim(0, 1.1)
    ax.set_ylabel("Rejected sample ratio")
    ax.grid(axis="y", linestyle="--", linewidth=0.5, alpha=0.5)
    for xi, val, total in zip(x, vals, totals):
        ax.text(xi, val + 0.03, total, ha="center", va="bottom", fontsize=9)
    fig.tight_layout()
    save_all(fig, stem)


def main():
    ensure_dirs()
    for func in [fig_7_1, fig_7_2, fig_7_3, fig_7_4, fig_7_5, fig_7_6, fig_7_7, fig_7_8, fig_7_9, fig_7_10]:
        func()
    print("Figures regenerated.")


if __name__ == "__main__":
    main()

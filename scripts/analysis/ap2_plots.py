#!/usr/bin/env python3
"""Make AP2 graphs (metric x threads) in docs/report/ap2/figures/ (needs matplotlib).

Use: python3 scripts/analysis/ap2_plots.py
"""
import os
import sys

from common import FIGURES_AP2, SIZES, load_ap2

try:
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
except ImportError:
    sys.exit("matplotlib is not installed. Run: pip install matplotlib")

METRICS = [
    ("execution_time_s_median", "Time per call (s)", "time", True),
    ("speedup_vs_t1", "Speedup (T1 / Tp)", "speedup", False),
    ("parallel_efficiency", "Parallel efficiency (S / p)", "efficiency", False),
    ("mflops_median", "MFLOPS", "mflops", False),
    ("power_watts_median", "Power (W)", "power", False),
    ("energy_joules_median", "Energy per call (J)", "energy", True),
    ("mflops_per_watt_median", "MFLOPS/W", "mflops_per_watt", False),
    ("l1_misses_median", "L1 misses per call", "l1", True),
    ("l2_misses_median", "L2 misses per call", "l2", True),
    ("l3_misses_median", "L3 misses per call", "l3", True),
]
THREADS = list(range(1, 17))
# Core regions of the p-ht-e placement
REGIONS = [(0.5, 4.5, "P-cores"), (4.5, 8.5, "+HT"), (8.5, 16.5, "+E-cores")]


def series(d, n, places, schedule):
    r = [x for x in d if x["binary_name"] == "parallel_omp" and x["matrix_size"] == n
         and x["places"] == places and x["schedule"] == schedule]
    return sorted(r, key=lambda x: x["threads"])


def shade(ax):
    for i, (a, b, name) in enumerate(REGIONS):
        ax.axvspan(a, b, color="0.9" if i % 2 == 0 else "0.97", zorder=0)
        ax.text((a + b) / 2, 0.98, name, transform=ax.get_xaxis_transform(),
                ha="center", va="top", fontsize=8, color="0.4")


def xaxis(ax):
    ax.set_xlim(0.5, 16.5)
    ax.set_xticks(THREADS)


def save(fig, name):
    os.makedirs(FIGURES_AP2, exist_ok=True)
    fig.tight_layout()
    fig.savefig(os.path.join(FIGURES_AP2, name), dpi=130)
    plt.close(fig)


def plot_main(d):
    """One figure per metric: one line per N (p-ht-e, static)."""
    for key, ylabel, tag, log in METRICS:
        fig, ax = plt.subplots(figsize=(8, 4.5))
        shade(ax)
        for n in SIZES:
            s = [r for r in series(d, n, "p-ht-e", "static") if r[key] is not None]
            ax.plot([r["threads"] for r in s], [r[key] for r in s], marker="o", label=f"N={n}")
        if tag == "speedup":
            ax.plot(THREADS, THREADS, "k--", lw=0.8, label="Ideal")
        ax.set_xlabel("Threads")
        ax.set_ylabel(ylabel)
        xaxis(ax)
        if log:
            ax.set_yscale("log")
        ax.grid(alpha=0.3)
        ax.legend()
        save(fig, f"threads_{tag}.png")


def plot_variants(d, field, fixed, tag):
    """Time and MFLOPS/W x threads, one subplot per N, one line per variant."""
    values = sorted({x[field] for x in d if x["binary_name"] == "parallel_omp" and x[field]})
    for key, ylabel, mtag in [("execution_time_s_median", "Time per call (s)", "time"),
                              ("mflops_per_watt_median", "MFLOPS/W", "mflops_per_watt")]:
        fig, axes = plt.subplots(1, len(SIZES), figsize=(15, 4.5))
        for ax, n in zip(axes, SIZES):
            shade(ax)
            for v in values:
                s = series(d, n, v, fixed) if field == "places" else series(d, n, fixed, v)
                s = [r for r in s if r[key] is not None]
                if s:
                    ax.plot([r["threads"] for r in s], [r[key] for r in s], marker=".", label=v)
            ax.set_title(f"N={n}")
            ax.set_xlabel("Threads")
            xaxis(ax)
            ax.grid(alpha=0.3)
        axes[0].set_ylabel(ylabel)
        axes[0].legend()
        save(fig, f"{tag}_{mtag}.png")


def main():
    d = load_ap2()
    if not d:
        sys.exit("No AP2 data in summary.csv. Run make parallel-run and compile_data.py.")
    plot_main(d)
    plot_variants(d, "places", "static", "places")
    plot_variants(d, "schedule", "p-ht-e", "schedule")
    print(f"Figures in {FIGURES_AP2}")


if __name__ == "__main__":
    main()

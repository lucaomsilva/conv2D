#!/usr/bin/env python3
"""Make comparison graphs in docs/report/figures/ (needs matplotlib).

Use: python3 scripts/analysis/plot_results.py
Install: pip install matplotlib
"""
import os
import sys

from common import FIGURES, SIZES, best_block, get, label, load

try:
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
except ImportError:
    sys.exit("matplotlib is not installed. Run: pip install matplotlib")

METRICS = [
    ("execution_time_s_median", "Time (s)", "time", True),
    ("mflops_median", "MFLOPS", "mflops", False),
    ("power_watts_median", "Power (W)", "power", False),
    ("energy_joules_median", "Energy (J)", "energy", True),
    ("mflops_per_watt_median", "MFLOPS/W", "efficiency", False),
]


def versions(d, n):
    """Return the list (name, row) for the main versions at size n."""
    bs, bm = best_block(d, "blocking_seq", n), best_block(d, "blocking_simd", n)
    return [
        ("Base -O0 row", get(d, "baseline_no_o3", n)),
        ("Base -O0 col", get(d, "baseline_no_o3", n, "col")),
        ("Base -O3 row", get(d, "baseline_o3", n)),
        ("Base -O3 col", get(d, "baseline_o3", n, "col")),
        (label("simd_unaligned", "row"), get(d, "simd_unaligned", n)),
        (label("simd_aligned", "row"), get(d, "simd_aligned", n)),
        # The best B changes with N, so the label cannot show one B
        ("Tiling (best B)", bs),
        ("Tiling+SIMD (best B)", bm),
        (label("library_o3", "row"), get(d, "library_o3", n)),
    ]


def plot_versions(d):
    """One figure for each metric: grouped bars (versions) for each N."""
    for key, ylabel, tag, log in METRICS:
        fig, ax = plt.subplots(figsize=(11, 5))
        names = [v[0] for v in versions(d, SIZES[0])]
        width = 0.8 / len(SIZES)
        for i, n in enumerate(SIZES):
            rows = [r for _, r in versions(d, n)]
            xs = [x + i * width for x in range(len(names))]
            ax.bar(xs, [r[key] for r in rows], width, label=f"N={n}")
            for x, r in zip(xs, rows):
                if r["block_size"]:
                    ax.text(x, r[key], f"B={int(r['block_size'])}", ha="center",
                            va="bottom", fontsize=7)
        ax.set_xticks([x + width for x in range(len(names))])
        ax.set_xticklabels(names, rotation=30, ha="right")
        ax.set_ylabel(ylabel)
        if log:
            ax.set_yscale("log")
        ax.set_title(f"{ylabel} for each version")
        ax.legend()
        fig.tight_layout()
        fig.savefig(os.path.join(FIGURES, f"versions_{tag}.png"), dpi=150)
        plt.close(fig)


def plot_blocks(d):
    """One figure for each metric: bars for each B and N, Tiling and Tiling+SIMD."""
    blocks = (8, 16, 32, 64)
    width = 0.8 / len(SIZES)
    for key, ylabel, tag, log in METRICS + [("l1_misses_median", "L1 misses", "l1", True)]:
        fig, axes = plt.subplots(1, 2, figsize=(12, 4.5))
        for ax, binary in zip(axes, ("blocking_seq", "blocking_simd")):
            for i, n in enumerate(SIZES):
                vals = [get(d, binary, n, f"block_{b}", b)[key] for b in blocks]
                ax.bar([x + i * width for x in range(len(blocks))], vals, width, label=f"N={n}")
            ax.set_xticks([x + width for x in range(len(blocks))])
            ax.set_xticklabels([f"B={b}" for b in blocks])
            ax.set_ylabel(ylabel)
            if log:
                ax.set_yscale("log")
            ax.set_title("Tiling" if binary == "blocking_seq" else "Tiling+SIMD")
            ax.legend()
        fig.suptitle(f"{ylabel} for each block size")
        fig.tight_layout()
        fig.savefig(os.path.join(FIGURES, f"blocks_{tag}.png"), dpi=150)
        plt.close(fig)


def plot_cache(d):
    """L1/L2/L3 misses for each version at the largest N."""
    n = SIZES[-1]
    vs = [(a, r) for a, r in versions(d, n) if r["l1_misses_median"] is not None]
    fig, ax = plt.subplots(figsize=(10, 4.5))
    width = 0.27
    for i, (key, name) in enumerate([("l1_misses_median", "L1"),
                                     ("l2_misses_median", "L2"),
                                     ("l3_misses_median", "L3")]):
        ax.bar([x + i * width for x in range(len(vs))],
               [r[key] or 0 for _, r in vs], width, label=name)
    ax.set_xticks([x + width for x in range(len(vs))])
    ax.set_xticklabels([a for a, _ in vs], rotation=30, ha="right")
    ax.set_yscale("log")
    ax.set_ylabel("Misses")
    ax.set_title(f"Cache misses, N={n}")
    ax.legend()
    fig.tight_layout()
    fig.savefig(os.path.join(FIGURES, "cache_misses.png"), dpi=150)
    plt.close(fig)


def main():
    os.makedirs(FIGURES, exist_ok=True)
    d = load()
    plot_versions(d)
    plot_blocks(d)
    plot_cache(d)
    print(f"Figures written to {FIGURES}")


if __name__ == "__main__":
    main()

"""Shared helpers for the analysis scripts.

Read docs/data/compiled/summary.csv (made by scripts/compile_data.py).
Use the median of each metric.
"""
import csv
import os

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SUMMARY = os.path.join(ROOT, "docs", "data", "compiled", "summary.csv")
REPORT = os.path.join(ROOT, "docs", "report", "ap1")
FIGURES = os.path.join(REPORT, "figures")
REPORT_AP2 = os.path.join(ROOT, "docs", "report", "ap2")
FIGURES_AP2 = os.path.join(REPORT_AP2, "figures")
AP2_CATEGORY = "parallel"
TEXT_FIELDS = ("category", "binary_name", "mode_name", "schedule", "places")
SIZES = [1024, 2048, 4096]

# Label for each implementation: (binary_name, mode_name) -> label.
LABELS = {
    ("baseline_no_o3", "col"): "Base -O0 col",
    ("baseline_no_o3", "row"): "Base -O0 row",
    ("baseline_o3", "col"): "Base -O3 col",
    ("baseline_o3", "row"): "Base -O3 row",
    ("simd_autovec", "row"): "SIMD autovec",
    ("simd_unaligned", "row"): "SIMD pragma",
    ("simd_aligned", "row"): "SIMD pragma+aligned",
    ("library_o3", "row"): "OpenCV filter2D",
}


def num(value):
    return float(value) if value not in (None, "") else None


def read_rows():
    with open(SUMMARY, newline="") as f:
        return [{k: (v if k in TEXT_FIELDS else num(v)) for k, v in r.items()}
                for r in csv.DictReader(f)]


def load_ap2():
    """Return the AP2 rows (docs/data/parallel/) with float metrics."""
    return [r for r in read_rows() if r["category"] == AP2_CATEGORY]


def load():
    """Return a dict: (binary, size, mode, block) -> row with float metrics (AP1 only)."""
    data = {}
    with open(SUMMARY, newline="") as f:
        for r in csv.DictReader(f):
            if r["category"] == AP2_CATEGORY:
                continue
            key = (r["binary_name"], int(r["matrix_size"]), r["mode_name"],
                   int(r["block_size"] or 0))
            data[key] = {k: (num(v) if k not in TEXT_FIELDS else v)
                         for k, v in r.items()}
    return data


def get(data, binary, size, mode="row", block=0):
    return data.get((binary, size, mode, block))


def best_block(data, binary, size):
    """Return the row with the lowest median time among the block sizes."""
    rows = [v for k, v in data.items() if k[0] == binary and k[1] == size]
    return min(rows, key=lambda r: r["execution_time_s_median"])


def label(binary, mode, block=0):
    if binary.startswith("blocking"):
        kind = "Tiling" if binary.endswith("seq") else "Tiling+SIMD"
        return f"{kind} B={block}"
    return LABELS.get((binary, mode), f"{binary} {mode}")

#!/usr/bin/env python3
"""Compile all benchmark CSVs in docs/data/* into:
  - docs/data/compiled/all_runs.csv : every run, unified columns
  - docs/data/compiled/summary.csv  : one row per (binary, size, mode, block,
                                      threads, schedule, places)
Perf counters equal to 0 are treated as failed reads (missing), not real zeros.
"""
import csv
import glob
import os
import statistics as st
from collections import defaultdict

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATA = os.path.join(ROOT, "docs", "data")
OUT = os.path.join(DATA, "compiled")

METRICS = ["execution_time_s", "l1_misses", "l2_misses", "l3_misses",
           "instructions", "cycles", "cpi", "mflops", "energy_joules",
           "power_watts", "mflops_per_watt", "start_temp_c"]
COUNTERS = {"l1_misses", "l2_misses", "l3_misses", "instructions", "cycles", "cpi"}
BASELINE = ("baseline_no_o3", "row")  # speedup reference, same matrix size
KEYS = ["category", "binary_name", "matrix_size", "mode_name", "block_size",
        "threads", "schedule", "places"]


def sched(r):
    """'static', 'dynamic,16', ... ('' for single-thread binaries)."""
    kind = r.get("schedule", "") or ""
    chunk = r.get("chunk", "") or ""
    return f"{kind},{chunk}" if kind and chunk not in ("", "0") else kind


def load_runs():
    runs = []
    for path in sorted(glob.glob(os.path.join(DATA, "*", "*.csv"))):
        category = os.path.basename(os.path.dirname(path))
        if category == "compiled":
            continue
        with open(path, newline="") as f:
            for i, r in enumerate(csv.DictReader(f)):
                row = {
                    "category": category,
                    "source_file": os.path.basename(path),
                    "run": i + 1,
                    "binary_name": r["binary_name"],
                    "matrix_size": int(r["matrix_size"]),
                    "mode_name": r.get("mode_name", ""),
                    "block_size": r.get("block_size", "") or "",
                    "threads": int(r["threads"]) if r.get("threads") else "",
                    "schedule": sched(r),
                    "places": r.get("places", "") or "",
                }
                for m in METRICS:
                    v = float(r[m]) if r.get(m) not in (None, "") else None
                    if m in COUNTERS and v == 0:
                        v = None  # failed perf read
                    row[m] = v
                runs.append(row)
    return runs


def summarize(runs):
    groups = defaultdict(list)
    for r in runs:
        key = tuple(r[k] for k in KEYS)
        groups[key].append(r)

    summary = []
    for key, rows in groups.items():
        s = dict(zip(KEYS, key))
        s["n_runs"] = len(rows)
        for m in METRICS:
            vals = [r[m] for r in rows if r[m] is not None]
            s[f"{m}_mean"] = st.mean(vals) if vals else None
            s[f"{m}_std"] = st.stdev(vals) if len(vals) > 1 else None
            s[f"{m}_median"] = st.median(vals) if vals else None
            s[f"{m}_valid"] = len(vals)
        temps = [r["start_temp_c"] for r in rows if r["start_temp_c"] is not None]
        s["start_temp_c_max"] = max(temps) if temps else None
        summary.append(s)

    base = {(s["matrix_size"]): s for s in summary
            if (s["binary_name"], s["mode_name"]) == BASELINE}
    for s in summary:
        b = base.get(s["matrix_size"])
        t = s["execution_time_s_median"]
        s["speedup_vs_baseline"] = (b["execution_time_s_median"] / t) if b and t else None
        e = s["energy_joules_median"]
        s["energy_ratio_vs_baseline"] = (e / b["energy_joules_median"]) if b and e else None

    # Parallel speedup: same binary, size, schedule and places with 1 thread
    t1 = {(s["binary_name"], s["matrix_size"], s["schedule"], s["places"]): s
          for s in summary if s["threads"] == 1}
    for s in summary:
        b = t1.get((s["binary_name"], s["matrix_size"], s["schedule"], s["places"]))
        t = s["execution_time_s_median"]
        ok = s["threads"] != "" and b and t
        s["speedup_vs_t1"] = (b["execution_time_s_median"] / t) if ok else None
        s["parallel_efficiency"] = (s["speedup_vs_t1"] / s["threads"]) if ok else None

    summary.sort(key=lambda s: (s["matrix_size"], s["category"], s["binary_name"],
                                s["mode_name"], int(s["block_size"] or 0),
                                s["places"], s["schedule"], s["threads"] or 0))
    return summary


def write(path, rows):
    with open(path, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        w.writeheader()
        for r in rows:
            w.writerow({k: (f"{v:.6g}" if isinstance(v, float) else ("" if v is None else v))
                        for k, v in r.items()})


def main():
    os.makedirs(OUT, exist_ok=True)
    runs = load_runs()
    summary = summarize(runs)
    write(os.path.join(OUT, "all_runs.csv"), runs)
    write(os.path.join(OUT, "summary.csv"), summary)
    print(f"{len(runs)} runs -> {len(summary)} configurations in {OUT}")
    print(f"\n{'binary':<16}{'size':>6}{'mode':>9}{'blk':>5}{'thr':>4}{'places':>8}{'sched':>11}{'time_ms':>10}"
          f"{'MFLOPS':>11}{'J':>9}{'MFLOPS/W':>10}{'speedup':>9}")
    for s in summary:
        f = lambda v, fmt: format(v, fmt) if v is not None else "-"
        print(f"{s['binary_name']:<16}{s['matrix_size']:>6}{s['mode_name']:>9}"
              f"{s['block_size'] or '-':>5}{s['threads'] or '-':>4}{s['places'] or '-':>8}"
              f"{s['schedule'] or '-':>11}{f(s['execution_time_s_median'] and s['execution_time_s_median']*1e3, '.3f'):>10}"
              f"{f(s['mflops_median'], '.0f'):>11}{f(s['energy_joules_median'], '.3f'):>9}"
              f"{f(s['mflops_per_watt_median'], '.2f'):>10}{f(s['speedup_vs_baseline'], '.2f'):>9}")


if __name__ == "__main__":
    main()

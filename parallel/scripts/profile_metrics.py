#!/usr/bin/env python3
import sys
import os
import subprocess
import json
import re
import argparse
import tempfile
import time
import glob

TEMP_LIMIT_C = 60.0
TEMP_MAX_WAIT_S = 60

# ==============================================================================
# STAGE 1: Argument Parsing (Extensible CLI Flags)
# ==============================================================================
def parse_args():
    parser = argparse.ArgumentParser(description="2D Convolution Performance & Energy Profiler")
    parser.add_argument("--binary", required=True, help="Path to binary executable")
    parser.add_argument("--size", type=int, required=True, help="Matrix size (N x N)")
    parser.add_argument("--mode", type=int, default=0, help="Access mode (0 = Row-Major, 1 = Column-Major)")
    parser.add_argument("--output-csv", required=True, help="Target CSV filepath")
    parser.add_argument("--runs", type=int, default=10, help="Number of benchmark runs")
    parser.add_argument("--threads", type=int, required=True, help="OpenMP thread count")
    parser.add_argument("--places", required=True, help="OMP_PLACES value, or 'none' for no binding")
    parser.add_argument("--places-name", required=True, help="Short name of the placement (CSV)")
    parser.add_argument("--bind", default="close", help="OMP_PROC_BIND value")
    parser.add_argument("--schedule", default="static", help="OMP_SCHEDULE value")
    return parser.parse_args()


# ==============================================================================
# STAGE 2: CPU Detection (Intel vs AMD Detection)
# ==============================================================================
def detect_cpu_vendor() -> str:
    """Detects whether CPU is Intel ('Intel'), AMD ('AMD'), or 'Unknown' from /proc/cpuinfo."""
    cpuinfo_path = "/proc/cpuinfo"
    if not os.path.exists(cpuinfo_path):
        return "Unknown"

    try:
        with open(cpuinfo_path, "r") as f:
            for line in f:
                if "vendor_id" in line:
                    if "GenuineIntel" in line:
                        return "Intel"
                    elif "AuthenticAMD" in line:
                        return "AMD"
    except Exception:
        pass
    return "Unknown"


# ==============================================================================
# STAGE 3: Binary Validation
# ==============================================================================
def validate_binary(binary_path:str):
    """Checks if the binary executable exists and is executable."""
    if not os.path.exists(binary_path):
        print(f"Error: Target binary '{binary_path}' does not exist.")
        sys.exit(1)
    if not os.access(binary_path, os.X_OK):
        print(f"Error: Target binary '{binary_path}' is not executable.")
        sys.exit(1)


# ==============================================================================
# STAGE 4: Output Folder Creation
# ==============================================================================
def ensure_output_directory(csv_filepath):
    """Creates the target directory for the output CSV if it doesn't exist."""
    dir_path = os.path.dirname(os.path.abspath(csv_filepath))
    if dir_path:
        os.makedirs(dir_path, exist_ok=True)


# ==============================================================================
# STAGE 5: CSV File & Header Initialization
# ==============================================================================
def initialize_csv_header(csv_filepath:str):
    """Creates the CSV file and writes the header if it does not exist or is empty."""
    write_header = not os.path.exists(csv_filepath) or os.path.getsize(csv_filepath) == 0
    if write_header:
        with open(csv_filepath, "w") as f:
            f.write("binary_name,matrix_size,mode,mode_name,execution_time_s,l1_misses,l2_misses,l3_misses,instructions,cycles,cpi,mflops,energy_joules,power_watts,mflops_per_watt,start_temp_c,threads,schedule,chunk,places\n")


# ==============================================================================
# Helper Function: perf Control FIFOs
# ==============================================================================
def create_perf_fifos(tmp_dir:str):
    ctl_path = os.path.join(tmp_dir, "perf_ctl.fifo")
    ack_path = os.path.join(tmp_dir, "perf_ack.fifo")
    os.mkfifo(ctl_path)
    os.mkfifo(ack_path)
    return ctl_path, ack_path


def find_pkg_temp_path():
    for zone in glob.glob("/sys/class/thermal/thermal_zone*"):
        with open(os.path.join(zone, "type")) as f:
            if f.read().strip() == "x86_pkg_temp":
                return os.path.join(zone, "temp")
    return None


def read_temp_c(path):
    with open(path) as f:
        return int(f.read().strip()) / 1000.0


def wait_for_cooldown(path):
    """Waits until the CPU package is at or below TEMP_LIMIT_C (max TEMP_MAX_WAIT_S)."""
    if path is None:
        return 0.0
    start = time.monotonic()
    temp = read_temp_c(path)
    while temp > TEMP_LIMIT_C and time.monotonic() - start < TEMP_MAX_WAIT_S:
        time.sleep(1)
        temp = read_temp_c(path)
    if temp > TEMP_LIMIT_C:
        print(f"    Warning: {temp:.1f} C after {TEMP_MAX_WAIT_S} s, running anyway")
    return temp


# ==============================================================================
# STAGE 6: Execution Engine
# ==============================================================================
def execute_profiling_benchmark(binary_path:str, size:int, mode:int, benchmark_runs:int, csv_path:str, cpu_vendor:str, args):
    """Executes the measured benchmark runs under perf stat."""
    binary_name = os.path.basename(binary_path)
    mode_name = "row" if mode == 0 else "col"
    print(f"=== Profiling {binary_name} | CPU: {cpu_vendor} | Size: {size}x{size} | Threads: {args.threads} | Places: {args.places_name} | Schedule: {args.schedule} ===")

    # OpenMP binds the threads (no taskset): one place per logical CPU
    omp_env = {"OMP_PLACES": args.places, "OMP_PROC_BIND": args.bind,
               "OMP_SCHEDULE": args.schedule}
    if args.places == "none":  # no binding: the Linux scheduler places the threads
        omp_env = {"OMP_PROC_BIND": "false", "OMP_SCHEDULE": args.schedule}
    bin_cmd = [binary_path, str(size), str(args.threads)]

    # Define hardware counter perf events according to CPU vendor
    if cpu_vendor == "AMD":
        events = ["L1-dcache-load-misses", "l2_misses", "cache-misses", "cycles", "instructions"]
    else:
        # Same event semantics on P-cores (cpu_core) and E-cores (cpu_atom)
        events = ["mem_load_retired.l1_miss", "mem_load_uops_retired.l1_miss",
                  "l2_request.miss", "longest_lat_cache.miss", "cycles", "instructions"]

    temp_path = find_pkg_temp_path()

    print(f" -> Executing {benchmark_runs} profiled benchmark runs...")
    for i in range(benchmark_runs):
        start_temp_c = wait_for_cooldown(temp_path)

        # Counters start disabled (-D -1); the binary enables them around its loop
        with tempfile.TemporaryDirectory() as tmp_dir:
            ctl_path, ack_path = create_perf_fifos(tmp_dir)
            env = dict(os.environ, PERF_CTL_FIFO=ctl_path, PERF_ACK_FIFO=ack_path, **omp_env)
            cmd = ["perf", "stat", "-D", "-1", "--control", f"fifo:{ctl_path},{ack_path}",
                   "-e", ",".join(events)] + bin_cmd
            try:
                proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, env=env)
                stdout_text = proc.stdout
                stderr_text = proc.stderr
            except Exception as e:
                # Fallback if perf fails or permission denied
                print(f" -> perf command failed: {e}")
                proc = subprocess.run(bin_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, env=dict(os.environ, **omp_env))
                stdout_text = proc.stdout
                stderr_text = ""

        # Stage 7: Parse C stdout JSON & perf stderr
        c_json, perf_data = parse_outputs(stdout_text, stderr_text)

        # Stage 8: Compute metrics
        metric_row = compute_metrics(
            c_json=c_json,
            perf_counters=perf_data,
            matrix_size=size,
            mode=mode,
            binary_name=binary_name
        )
        metric_row["start_temp_c"] = start_temp_c
        metric_row["threads"] = c_json.get("threads", args.threads)
        metric_row["schedule"] = c_json.get("schedule", "")
        metric_row["chunk"] = c_json.get("chunk", "")
        metric_row["places"] = args.places_name

        # Stage 9: Export metric row to CSV
        append_metric_row(csv_path, metric_row)

        print(f"    Run {i+1}/{benchmark_runs}: Time={metric_row['execution_time_s']:.6f}s | MFLOPS={metric_row['mflops']:.2f} | CPI={metric_row['cpi']:.2f} | Energy={metric_row['energy_joules']:.4f}J | Power={metric_row['power_watts']:.2f}W | Start={start_temp_c:.1f}C")

    print(f"Successfully saved profiling data to: {csv_path}")


# ==============================================================================
# STAGE 7: Output Parsing (C Stdout JSON & Perf Stderr Counters)
# ==============================================================================
def parse_c_stdout_json(stdout_text):
    """Extracts kernel_time_s from C's JSON_METRICS stdout line."""
    c_data = {"kernel_time_s": 0.0}
    for line in stdout_text.splitlines():
        if line.startswith("JSON_METRICS:"):
            json_str = line.replace("JSON_METRICS:", "").strip()
            try:
                c_data = json.loads(json_str)
            except Exception:
                pass
            break
    return c_data


def parse_perf_counter(stderr_text, event_names):
    """Robustly searches for and sums matching counter numbers (supporting Intel hybrid Core/Atom architectures)."""
    if not isinstance(event_names, list):
        event_names = [event_names]
 
    total_val = 0
    found = False
 
    for line in stderr_text.splitlines():
        if "<not" in line or "<supported" in line:
            continue
        for event_name in event_names:
            if event_name in line:
                # Matches leading numbers like " 145,472,101 cpu_core/cycles/" or " 123456 cycles"
                match = re.search(r'^\s*([\d,]+)\s+', line)
                if match:
                    try:
                        total_val += int(match.group(1).replace(',', ''))
                        found = True
                        break
                    except ValueError:
                        pass
    return total_val if found else 0


def parse_outputs(stdout_text, stderr_text):
    """Parses both C stdout JSON metrics and perf stat stderr hardware counters."""
    c_json = parse_c_stdout_json(stdout_text)

    perf_data = {
        "l1_misses": parse_perf_counter(stderr_text, ["l1_miss", "L1-dcache-load-misses"]),
        "l2_misses": parse_perf_counter(stderr_text, ["l2_request.miss", "l2_misses"]),
        "l3_misses": parse_perf_counter(stderr_text, ["longest_lat_cache.miss", "cache-misses"]),
        "cycles": parse_perf_counter(stderr_text, ["cycles", "cpu-cycles"]),
        "instructions": parse_perf_counter(stderr_text, ["instructions"])
    }
    return c_json, perf_data


# ==============================================================================
# STAGE 8: Derived Metrics Computation
# ==============================================================================
def compute_metrics(c_json, perf_counters, matrix_size, mode, binary_name):
    """Computes per-call metrics. Counters and energy are divided by `reps`."""
    mode_name = "row" if mode == 0 else "col"

    kernel_time_s = c_json.get("kernel_time_s", 0.0)
    reps = max(int(c_json.get("reps", 1)), 1)
    total_time_s = c_json.get("total_time_s", 0.0)
    loop_energy_j = c_json.get("energy_j", -1.0)

    per_call = {k: round(perf_counters.get(k, 0) / reps)
                for k in ("l1_misses", "l2_misses", "l3_misses", "cycles", "instructions")}

    cycles = per_call["cycles"]
    instructions = per_call["instructions"]
    cpi = (cycles / instructions) if (cycles > 0 and instructions > 0) else 0.0

    # 2D Convolution 3x3 filter FLOP calculation: (N-2)^2 * 17 FLOPs
    if kernel_time_s > 0.0 and matrix_size > 2:
        total_flops = float(matrix_size - 2) * float(matrix_size - 2) * 17.0
        mflops = (total_flops / 1e6) / kernel_time_s
    else:
        mflops = 0.0

    energy_joules = (loop_energy_j / reps) if loop_energy_j > 0 else 0.0
    power_watts = (loop_energy_j / total_time_s) if (loop_energy_j > 0 and total_time_s > 0) else 0.0
    mflops_per_watt = (mflops / power_watts) if power_watts > 0 else 0.0

    return {
        "binary_name": binary_name,
        "matrix_size": matrix_size,
        "mode": mode,
        "mode_name": mode_name,
        "execution_time_s": kernel_time_s,
        "l1_misses": per_call["l1_misses"],
        "l2_misses": per_call["l2_misses"],
        "l3_misses": per_call["l3_misses"],
        "instructions": instructions,
        "cycles": cycles,
        "cpi": cpi,
        "mflops": mflops,
        "energy_joules": energy_joules,
        "power_watts": power_watts,
        "mflops_per_watt": mflops_per_watt
    }


# ==============================================================================
# STAGE 9: Metric CSV Export
# ==============================================================================
def append_metric_row(csv_filepath, m):
    """Appends a single calculated metric dictionary row to the target CSV file."""
    with open(csv_filepath, "a") as f:
        row = (
            f"{m['binary_name']},{m['matrix_size']},{m['mode']},{m['mode_name']},"
            f"{m['execution_time_s']:.9f},{m['l1_misses']},{m['l2_misses']},{m['l3_misses']},"
            f"{m['instructions']},{m['cycles']},{m['cpi']:.4f},{m['mflops']:.4f},"
            f"{m['energy_joules']:.9f},{m['power_watts']:.4f},{m['mflops_per_watt']:.4f},{m['start_temp_c']:.1f},"
            f"{m['threads']},{m['schedule']},{m['chunk']},{m['places']}\n"
        )
        f.write(row)


def main():
    args = parse_args()
    cpu_vendor = detect_cpu_vendor()
    validate_binary(args.binary)
    ensure_output_directory(args.output_csv)
    initialize_csv_header(args.output_csv)

    execute_profiling_benchmark(
        binary_path=args.binary,
        size=args.size,
        mode=args.mode,
        benchmark_runs=args.runs,
        csv_path=args.output_csv,
        cpu_vendor=cpu_vendor,
        args=args
    )

if __name__ == "__main__":
    main()

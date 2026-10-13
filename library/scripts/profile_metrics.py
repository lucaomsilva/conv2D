#!/usr/bin/env python3
import sys
import os
import subprocess
import json
import re
import argparse

# ==============================================================================
# STAGE 1: Argument Parsing (Extensible CLI Flags)
# ==============================================================================
def parse_args():
    parser = argparse.ArgumentParser(description="2D Convolution Performance & Energy Profiler")
    parser.add_argument("--binary", required=True, help="Path to binary executable")
    parser.add_argument("--size", type=int, required=True, help="Matrix size (N x N)")
    parser.add_argument("--mode", type=int, required=True, help="Access mode (0 = Row-Major, 1 = Column-Major)")
    parser.add_argument("--output-csv", required=True, help="Target CSV filepath")
    parser.add_argument("--runs", type=int, default=10, help="Number of benchmark runs")
    parser.add_argument("--warmup", type=int, default=5, help="Number of warmup runs")
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
            f.write("binary_name,matrix_size,mode,mode_name,execution_time_s,l1_misses,l2_misses,l3_misses,instructions,cycles,cpi,mflops,energy_joules,power_watts,mflops_per_watt\n")


# ==============================================================================
# Helper Function: RAPL Energy Reader
# ==============================================================================
def read_rapl_energy_uj():
    """Reads RAPL energy in microjoules if accessible, checking intel-rapl powercap zones."""
    total_uj = 0
    found = False
    rapl_base = "/sys/class/powercap/intel-rapl"
    if not os.path.exists(rapl_base):
        return None

    try:
        for zone in os.listdir(rapl_base):
            if zone.startswith("intel-rapl:"):
                energy_file = os.path.join(rapl_base, zone, "energy_uj")
                name_file = os.path.join(rapl_base, zone, "name")
                if os.path.exists(energy_file):
                    name = ""
                    if os.path.exists(name_file):
                        with open(name_file, 'r') as f:
                            name = f.read().strip()
                    if "package" in name.lower() or name == "" or zone == "intel-rapl:0":
                        with open(energy_file, 'r') as f:
                            val = int(f.read().strip())
                            total_uj += val
                            found = True
    except Exception:
        pass

    return total_uj if found else None


# ==============================================================================
# STAGE 6: Combined Warmup & Execution Engine
# ==============================================================================
def execute_profiling_benchmark(binary_path:str, size:int, mode:int, warmup_runs:int, benchmark_runs:int, csv_path:str, cpu_vendor:str):
    """
    Executes warmup runs first, followed by measured benchmark runs with perf stat & RAPL energy reads.
    """
    binary_name = os.path.basename(binary_path)
    mode_name = "row" if mode == 0 else "col"
    print(f"=== Profiling {binary_name} | CPU: {cpu_vendor} | Size: {size}x{size} | Mode: {mode_name} ===")

    # --------------------------------------------------------------------------
    # Step 6.1: Warmup Phase (Runs FIRST)
    # --------------------------------------------------------------------------
    print(f" -> Executing {warmup_runs} warmup runs...")
    for _ in range(warmup_runs):
        subprocess.run([binary_path, str(size), str(mode)], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    # Define hardware counter perf events according to CPU vendor
    if cpu_vendor == "AMD":
        events = ["L1-dcache-load-misses", "l2_misses", "cache-misses", "cycles", "instructions"]
    else:
        events = ["L1-dcache-load-misses", "l2_rqsts.miss", "LLC-load-misses", "cycles", "instructions"]

    # --------------------------------------------------------------------------
    # Step 6.2: Benchmark & Profiling Phase (Runs AFTER Warmup)
    # --------------------------------------------------------------------------
    print(f" -> Executing {benchmark_runs} profiled benchmark runs...")
    for i in range(benchmark_runs):
        # 1. Read RAPL start
        rapl_start_uj = read_rapl_energy_uj()

        # 2. Execute binary under perf stat (with fallback handling)
        cmd = ["perf", "stat", "-e", ",".join(events), binary_path, str(size), str(mode)]
        try:
            proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            stdout_text = proc.stdout
            stderr_text = proc.stderr
        except Exception as e:
            # Fallback if perf fails or permission denied
            print(f" -> perf command failed: {e}")
            proc = subprocess.run([binary_path, str(size), str(mode)], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            stdout_text = proc.stdout
            stderr_text = ""

        # 3. Read RAPL end
        rapl_end_uj = read_rapl_energy_uj()
        energy_joules = 0.0
        if rapl_start_uj is not None and rapl_end_uj is not None and rapl_end_uj >= rapl_start_uj:
            energy_joules = (rapl_end_uj - rapl_start_uj) / 1e6

        # 4. Stage 7: Parse C stdout JSON & perf stderr
        c_json, perf_data = parse_outputs(stdout_text, stderr_text)

        # 5. Stage 8: Compute metrics
        metric_row = compute_metrics(
            kernel_time_s=c_json.get("kernel_time_s", 0.0),
            perf_counters=perf_data,
            energy_joules=energy_joules,
            matrix_size=size,
            mode=mode,
            binary_name=binary_name
        )

        # 6. Stage 9: Export metric row to CSV
        append_metric_row(csv_path, metric_row)

        print(f"    Run {i+1}/{benchmark_runs}: Time={metric_row['execution_time_s']:.6f}s | MFLOPS={metric_row['mflops']:.2f} | CPI={metric_row['cpi']:.2f} | Energy={metric_row['energy_joules']:.4f}J | Power={metric_row['power_watts']:.2f}W")

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
        "l1_misses": parse_perf_counter(stderr_text, ["L1-dcache-load-misses", "L1-dcache-misses", "l1d"]),
        "l2_misses": parse_perf_counter(stderr_text, ["l2_rqsts.miss", "l2_misses", "L2_lines_in.all"]),
        "l3_misses": parse_perf_counter(stderr_text, ["LLC-load-misses", "cache-misses", "l3_misses"]),
        "cycles": parse_perf_counter(stderr_text, ["cycles", "cpu-cycles"]),
        "instructions": parse_perf_counter(stderr_text, ["instructions"])
    }
    return c_json, perf_data


# ==============================================================================
# STAGE 8: Derived Metrics Computation
# ==============================================================================
def compute_metrics(kernel_time_s, perf_counters, energy_joules, matrix_size, mode, binary_name):
    """Computes CPI, MFLOPS, Power, and MFLOPS/Watt using kernel_time_s from C."""
    mode_name = "row" if mode == 0 else "col"

    cycles = perf_counters.get("cycles", 0)
    instructions = perf_counters.get("instructions", 0)
    cpi = (cycles / instructions) if (cycles > 0 and instructions > 0) else 0.0

    # 2D Convolution 3x3 filter FLOP calculation: (N-2)^2 * 17 FLOPs
    if kernel_time_s > 0.0 and matrix_size > 2:
        total_flops = float(matrix_size - 2) * float(matrix_size - 2) * 17.0
        mflops = (total_flops / 1e6) / kernel_time_s
    else:
        mflops = 0.0

    power_watts = (energy_joules / kernel_time_s) if (energy_joules > 0 and kernel_time_s > 0) else 0.0
    mflops_per_watt = (mflops / power_watts) if power_watts > 0 else (mflops / energy_joules if energy_joules > 0 else 0.0)

    return {
        "binary_name": binary_name,
        "matrix_size": matrix_size,
        "mode": mode,
        "mode_name": mode_name,
        "execution_time_s": kernel_time_s,
        "l1_misses": perf_counters.get("l1_misses", 0),
        "l2_misses": perf_counters.get("l2_misses", 0),
        "l3_misses": perf_counters.get("l3_misses", 0),
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
            f"{m['execution_time_s']:.6f},{m['l1_misses']},{m['l2_misses']},{m['l3_misses']},"
            f"{m['instructions']},{m['cycles']},{m['cpi']:.4f},{m['mflops']:.4f},"
            f"{m['energy_joules']:.6f},{m['power_watts']:.4f},{m['mflops_per_watt']:.4f}\n"
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
        warmup_runs=args.warmup,
        benchmark_runs=args.runs,
        csv_path=args.output_csv,
        cpu_vendor=cpu_vendor
    )

if __name__ == "__main__":
    main()

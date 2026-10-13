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
    parser = argparse.ArgumentParser(description="2D Convolution Blocking Performance & Energy Profiler")
    parser.add_argument("--binary", required=True, help="Path to binary executable")
    parser.add_argument("--size", type=int, required=True, help="Matrix size (N x N)")
    parser.add_argument("--block-size", type=int, default=32, help="Tile Block Size (B x B, e.g. 8, 16, 32, 64)")
    parser.add_argument("--mode", type=int, default=None, help="Alias for block size (compatibility)")
    parser.add_argument("--output-csv", required=True, help="Target CSV filepath")
    parser.add_argument("--runs", type=int, default=10, help="Number of benchmark runs")
    parser.add_argument("--warmup", type=int, default=5, help="Number of warmup runs")
    args = parser.parse_args()
    if args.mode is not None:
        args.block_size = args.mode
    return args


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
            header = (
                "binary_name,matrix_size,block_size,mode_name,execution_time_s,"
                "l1_misses,l2_misses,l3_misses,instructions,cycles,cpi,mflops,"
                "energy_joules,power_watts,mflops_per_watt\n"
            )
            f.write(header)


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
def execute_profiling_benchmark(binary_path:str, size:int, block_size:int, warmup_runs:int, benchmark_runs:int, csv_path:str, cpu_vendor:str):
    """
    Executes warmup runs first, followed by measured benchmark runs with perf stat & RAPL energy reads.
    """
    binary_name = os.path.basename(binary_path)
    mode_name = f"block_{block_size}"
    print(f"=== Profiling {binary_name} | CPU: {cpu_vendor} | Size: {size}x{size} | Block: {block_size}x{block_size} ===")

    # --------------------------------------------------------------------------
    # Step 6.1: Warmup Phase (Runs FIRST)
    # --------------------------------------------------------------------------
    print(f" -> Executing {warmup_runs} warmup runs...")
    for _ in range(warmup_runs):
        subprocess.run([binary_path, str(size), str(block_size)], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

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
        cmd = ["perf", "stat", "-e", ",".join(events), binary_path, str(size), str(block_size)]
        try:
            proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            stdout_text = proc.stdout
            stderr_text = proc.stderr
        except Exception as e:
            # Fallback if perf fails or permission denied
            print(f" -> perf command failed: {e}")
            proc = subprocess.run([binary_path, str(size), str(block_size)], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            stdout_text = proc.stdout
            stderr_text = ""

        # 3. Read RAPL end
        rapl_end_uj = read_rapl_energy_uj()
        energy_joules = 0.0
        if rapl_start_uj is not None and rapl_end_uj is not None and rapl_end_uj >= rapl_start_uj:
            energy_joules = (rapl_end_uj - rapl_start_uj) / 1e6

        # 4. Parse stdout metrics (JSON kernel time)
        kernel_time = parse_kernel_time(stdout_text)

        # 5. Parse stderr perf counters
        perf_data = parse_perf_stat(stderr_text, cpu_vendor)

        # 6. Calculate derived metrics
        metrics = compute_metrics(
            binary_name=binary_name,
            matrix_size=size,
            block_size=block_size,
            execution_time_s=kernel_time,
            energy_joules=energy_joules,
            perf_dict=perf_data
        )

        # 7. Append row to CSV
        append_metric_row(csv_path, metrics)
        print(f"    [Run {i+1}/{benchmark_runs}] Time: {kernel_time:.6f}s | Energy: {energy_joules:.4f}J | Power: {metrics['power_watts']:.2f}W | MFLOPS: {metrics['mflops']:.2f}")


# ==============================================================================
# STAGE 7: Output & Counter Parsing
# ==============================================================================
def parse_kernel_time(stdout_str:str) -> float:
    """Parses JSON_METRICS:{"kernel_time_s": <val>} from stdout."""
    match = re.search(r'JSON_METRICS:(\{.*\})', stdout_str)
    if match:
        try:
            data = json.loads(match.group(1))
            return float(data.get("kernel_time_s", 0.0))
        except Exception:
            pass
    # Fallback to execution time text regex
    fallback = re.search(r'Execution Time:\s*([\d\.]+)', stdout_str)
    if fallback:
        return float(fallback.group(1))
    return 0.0


def parse_perf_stat(stderr_str:str, cpu_vendor:str) -> dict:
    """Extracts raw PMU counter values from perf stat stderr output."""
    counters = {
        "l1_misses": 0,
        "l2_misses": 0,
        "l3_misses": 0,
        "cycles": 0,
        "instructions": 0
    }

    l2_event = "l2_misses" if cpu_vendor == "AMD" else "l2_rqsts.miss"
    l3_event = "cache-misses" if cpu_vendor == "AMD" else "LLC-load-misses"

    for line in stderr_str.splitlines():
        line = line.strip().replace('\x00', '')
        if not line or line.startswith('#'):
            continue

        # Look for L1 misses
        if "L1-dcache-load-misses" in line:
            counters["l1_misses"] = extract_counter_val(line)
        # Look for L2 misses
        elif l2_event in line:
            counters["l2_misses"] = extract_counter_val(line)
        # Look for L3/LLC misses
        elif l3_event in line:
            counters["l3_misses"] = extract_counter_val(line)
        # Look for CPU cycles
        elif "cycles" in line and "cpu-cycles" not in line:
            counters["cycles"] = extract_counter_val(line)
        elif "cpu-cycles" in line:
            counters["cycles"] = extract_counter_val(line)
        # Look for instructions
        elif "instructions" in line:
            counters["instructions"] = extract_counter_val(line)

    return counters


def extract_counter_val(line:str) -> int:
    """Extracts numeric integer count from a perf stat formatted line."""
    parts = line.split()
    if parts:
        val_str = parts[0].replace('.', '').replace(',', '')
        if val_str.isdigit():
            return int(val_str)
    return 0


# ==============================================================================
# STAGE 8: Metric Computation Engine
# ==============================================================================
def compute_metrics(binary_name:str, matrix_size:int, block_size:int, execution_time_s:float, energy_joules:float, perf_dict:dict) -> dict:
    """Calculates all 15 performance and energy efficiency metrics."""
    l1_misses = perf_dict.get("l1_misses", 0)
    l2_misses = perf_dict.get("l2_misses", 0)
    l3_misses = perf_dict.get("l3_misses", 0)
    instructions = perf_dict.get("instructions", 0)
    cycles = perf_dict.get("cycles", 0)

    # CPI = Cycles / Instructions
    cpi = (cycles / instructions) if instructions > 0 else 0.0

    # Total floating point operations for 3x3 convolution:
    # (N-2)^2 * 17 FLOPs (9 multiplications + 8 additions = 17 FLOPs per inner stencil pixel)
    flops = (matrix_size - 2) * (matrix_size - 2) * 17
    mflops = (flops / (execution_time_s * 1e6)) if execution_time_s > 0 else 0.0

    # Average Power in Watts = Joules / Time
    power_watts = (energy_joules / execution_time_s) if execution_time_s > 0 else 0.0

    # Energy Efficiency = MFLOPS / Power(W)
    mflops_per_watt = (mflops / power_watts) if power_watts > 0 else 0.0

    return {
        "binary_name": binary_name,
        "matrix_size": matrix_size,
        "block_size": block_size,
        "mode_name": f"block_{block_size}",
        "execution_time_s": execution_time_s,
        "l1_misses": l1_misses,
        "l2_misses": l2_misses,
        "l3_misses": l3_misses,
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
            f"{m['binary_name']},{m['matrix_size']},{m['block_size']},{m['mode_name']},"
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
        block_size=args.block_size,
        warmup_runs=args.warmup,
        benchmark_runs=args.runs,
        csv_path=args.output_csv,
        cpu_vendor=cpu_vendor
    )

if __name__ == "__main__":
    main()

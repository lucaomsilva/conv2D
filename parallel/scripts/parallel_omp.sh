#!/bin/bash

# Resolve parallel and root directories
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PARALLEL_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
ROOT_DIR="$(cd "$PARALLEL_DIR/.." && pwd)"

BINARY="$PARALLEL_DIR/bin/parallel_omp"
REF_BINARY="$ROOT_DIR/vectorize/bin/simd_aligned"
CV_BINARY="$ROOT_DIR/library/bin/library_o3"
PROFILER="$PARALLEL_DIR/scripts/profile_metrics.py"
REF_PROFILER="$ROOT_DIR/vectorize/scripts/profile_metrics.py"
CV_PROFILER="$ROOT_DIR/library/scripts/profile_metrics.py"
OUT_DIR="$ROOT_DIR/docs/data/parallel"

for b in "$BINARY" "$REF_BINARY" "$CV_BINARY"; do
  if [ ! -f "$b" ]; then
    echo "Error: Binary $b not found. Run 'make parallel-build vectorize-build-aligned library-build' first."
    exit 1
  fi
done

mkdir -p "$OUT_DIR"

# Override any list from the environment, e.g. EXPERIMENTS="main" SIZES="1024" RUNS=2
# THREADS: main experiment; CMP_THREADS: places and schedules experiments
EXPERIMENTS=(${EXPERIMENTS:-reference main places schedules})
SIZES=(${SIZES:-1024 2048 4096})
THREADS=(${THREADS:-$(seq 1 16)})
CMP_THREADS=(${CMP_THREADS:-1 2 4 6 8 10 12 14 16})
RUNS=${RUNS:-10}
ALT_PLACES=(${ALT_PLACES:-ht-p-e p-e-ht none})
ALT_SCHEDULES=(${ALT_SCHEDULES:-dynamic,1 dynamic,16 guided})

# One place per logical CPU; thread i goes to place i (OMP_PROC_BIND=close).
# CPU 2 first, as in AP1 (CPU 0 handles more interrupts).
P="{2},{4},{6},{0}"
HT="{3},{5},{7},{1}"
E="{8},{9},{10},{11},{12},{13},{14},{15}"
declare -A PLACES=(
  [p-ht-e]="$P,$HT,$E"
  [ht-p-e]="{2},{3},{4},{5},{6},{7},{0},{1},$E"
  [p-e-ht]="$P,$E,$HT"
  [none]="none"
)

# ------------------------------------------------------------------------------
# Power profile: tuned throughput-performance (= GNOME "Performance": governor,
# EPP and platform_profile = performance, as in AP1), restored at the end
# ------------------------------------------------------------------------------
PERF_PROFILE=throughput-performance
SAVED_PROFILE=""
RESTORED=0

show_policy() {
  local p=/sys/devices/system/cpu/cpufreq/policy0
  echo " $1: tuned=$(tuned-adm active | sed 's/.*: //') governor=$(cat $p/scaling_governor)" \
    "epp=$(cat $p/energy_performance_preference 2>/dev/null)" \
    "platform_profile=$(cat /sys/firmware/acpi/platform_profile 2>/dev/null)"
}

power_performance_on() {
  SAVED_PROFILE=$(tuned-adm active | sed 's/.*: //')
  sudo tuned-adm profile "$PERF_PROFILE" || exit 1
  show_policy "Power profile "
}

power_restore() {
  [ "$RESTORED" = 1 ] || [ -z "$SAVED_PROFILE" ] && return
  RESTORED=1
  sudo tuned-adm profile "$SAVED_PROFILE"
  show_policy "Power profile restored"
}

trap power_restore EXIT
trap 'exit 130' INT TERM

# ------------------------------------------------------------------------------
# Runners
# ------------------------------------------------------------------------------
run_parallel() { # <N> <T> <places_name> <schedule>
  local csv="$OUT_DIR/parallel_omp_${3}_${4//,/-}_${1}_t${2}.csv"
  rm -f "$csv"
  echo "--- N=$1 | T=$2 | places=$3 | schedule=$4"
  sudo python3 "$PROFILER" --binary "$BINARY" --size "$1" --mode 0 \
    --threads "$2" --places "${PLACES[$3]}" --places-name "$3" \
    --bind close --schedule "$4" --output-csv "$csv" --runs "$RUNS"
}

run_reference() { # <N>: AP1 best version and OpenCV (1 thread, CPU 2), same session
  local csv="$OUT_DIR/simd_aligned_${1}.csv"
  rm -f "$csv"
  echo "--- N=$1 | simd_aligned (AP1 best, CPU 2)"
  sudo python3 "$REF_PROFILER" --binary "$REF_BINARY" --size "$1" --mode 0 \
    --output-csv "$csv" --runs "$RUNS"
  csv="$OUT_DIR/library_o3_${1}.csv"
  rm -f "$csv"
  echo "--- N=$1 | library_o3 (OpenCV filter2D, 1 thread, CPU 2)"
  sudo python3 "$CV_PROFILER" --binary "$CV_BINARY" --size "$1" --mode 0 \
    --output-csv "$csv" --runs "$RUNS"
}

count_configs() {
  local n=0 e
  for e in "${EXPERIMENTS[@]}"; do
    case $e in
      reference) n=$((n + 2 * ${#SIZES[@]})) ;;
      main) n=$((n + ${#SIZES[@]} * ${#THREADS[@]})) ;;
      places) n=$((n + ${#SIZES[@]} * ${#CMP_THREADS[@]} * ${#ALT_PLACES[@]})) ;;
      schedules) n=$((n + ${#SIZES[@]} * ${#CMP_THREADS[@]} * ${#ALT_SCHEDULES[@]})) ;;
    esac
  done
  echo $n
}

# ------------------------------------------------------------------------------
# Main
# ------------------------------------------------------------------------------
sudo -v || exit 1
CONFIGS=$(count_configs)

echo "=========================================================="
echo " parallel_omp benchmarks (OpenMP + SIMD)"
echo " Experiments   : ${EXPERIMENTS[*]}"
echo " Threads       : main=${THREADS[*]} | compare=${CMP_THREADS[*]}"
echo " Configurations: $CONFIGS x $RUNS runs (~4 s each => ~$((CONFIGS * RUNS * 4 / 60)) min)"
power_performance_on
echo "=========================================================="

for e in "${EXPERIMENTS[@]}"; do
  echo "=================== Experiment: $e ==================="
  for N in "${SIZES[@]}"; do
    case $e in
      reference) run_reference "$N" ;;
      main) for T in "${THREADS[@]}"; do run_parallel "$N" "$T" p-ht-e static; done ;;
      places)
        for pl in "${ALT_PLACES[@]}"; do
          for T in "${CMP_THREADS[@]}"; do run_parallel "$N" "$T" "$pl" static; done
        done ;;
      schedules)
        for s in "${ALT_SCHEDULES[@]}"; do
          for T in "${CMP_THREADS[@]}"; do run_parallel "$N" "$T" p-ht-e "$s"; done
        done ;;
      *) echo "Unknown experiment: $e"; exit 1 ;;
    esac
  done
done

echo "=========================================================="
echo " All parallel_omp benchmarks completed successfully!       "
echo "=========================================================="

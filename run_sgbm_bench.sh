#!/usr/bin/env bash
#
# run_sgbm_bench.sh
# Runs the SGBM host executable N times on a given left/right image pair and
# reports average latency, parsed directly from the latency figure the host
# binary itself prints each iteration (no shell-side wall-clock timing, so no
# extra process-spawn overhead gets counted against the hardware/kernel
# latency).
#
# Usage:
#   ./run_sgbm_bench.sh [-n RUNS] [-p REGEX] LEFT_IMAGE RIGHT_IMAGE
#
# Example:
#   ./run_sgbm_bench.sh -n 100 left.png right.png
#
# Notes:
#   - Runs ./sgbm_host itself. Edit HOST_BINARY below if the path
#     or executable name differs.
#   - LEFT_IMAGE and RIGHT_IMAGE are passed to the host binary as
#     positional args: "$HOST_BINARY" "$LEFT_IMAGE" "$RIGHT_IMAGE"
#     Edit the invocation below if sgbm_host expects flags instead
#     (e.g. --left/--right) or a different argument order.
#   - REGEX must contain exactly one capture group: the numeric latency value.
#   - Default REGEX matches the host binary's own printed line:
#       "INFO: Latency for hardware function is 18.0853ms"
#     i.e. it averages the hardware/kernel latency, not the post-processing
#     filter latency or the kernel+filter total that also get printed.
#   - Override with -p if your print format changes, e.g.:
#       -p 'Total \(kernel \+ filter\) is\s*([0-9.]+)ms'

set -euo pipefail

RUNS=100
PATTERN='Latency for hardware function is\s*([0-9]+\.?[0-9]*)\s*ms'
HOST_BINARY="./sgbm_host"

usage() {
    echo "Usage: $0 [-n RUNS] [-p REGEX] LEFT_IMAGE RIGHT_IMAGE" >&2
}

# --- parse -n / -p flags, collect positional args ---
POSITIONAL=()
while [[ $# -gt 0 ]]; do
    case "$1" in
        -n)
            RUNS="$2"
            shift 2
            ;;
        -p)
            PATTERN="$2"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        --)
            shift
            POSITIONAL+=("$@")
            break
            ;;
        -*)
            echo "Unknown argument: $1" >&2
            usage
            exit 1
            ;;
        *)
            POSITIONAL+=("$1")
            shift
            ;;
    esac
done

if [[ ${#POSITIONAL[@]} -ne 2 ]]; then
    echo "Error: expected exactly two positional args (LEFT_IMAGE RIGHT_IMAGE), got ${#POSITIONAL[@]}" >&2
    usage
    exit 1
fi

LEFT_IMAGE="${POSITIONAL[0]}"
RIGHT_IMAGE="${POSITIONAL[1]}"

for img in "$LEFT_IMAGE" "$RIGHT_IMAGE"; do
    if [[ ! -f "$img" ]]; then
        echo "Error: image not found: $img" >&2
        exit 1
    fi
done

echo "Running: $HOST_BINARY"
echo "Left image      : $LEFT_IMAGE"
echo "Right image     : $RIGHT_IMAGE"
echo "Iterations: $RUNS"
echo

total=0
count=0
declare -a latencies

for ((i = 1; i <= RUNS; i++)); do
    output=$("$HOST_BINARY" "$LEFT_IMAGE" "$RIGHT_IMAGE" 2>&1) || {
        echo "  [run $i] FAILED (nonzero exit), skipping" >&2
        continue
    }
    elapsed_ms=$(echo "$output" | grep -oP "$PATTERN" | grep -oP '[0-9]+\.?[0-9]*' | tail -1)
    if [[ -z "${elapsed_ms:-}" ]]; then
        echo "  [run $i] FAILED (no latency match in output), skipping" >&2
        echo "    --- output was ---" >&2
        echo "$output" | sed 's/^/    /' >&2
        continue
    fi
    latencies+=("$elapsed_ms")
    total=$(echo "$total + $elapsed_ms" | bc)
    count=$((count + 1))
    printf "  [run %3d/%d] %.3f ms\n" "$i" "$RUNS" "$elapsed_ms"
done

if [[ $count -eq 0 ]]; then
    echo "All runs failed. Nothing to average." >&2
    exit 1
fi

avg=$(echo "$total / $count" | bc -l)

# min/max
sorted=($(printf '%s\n' "${latencies[@]}" | sort -n))
min=${sorted[0]}
max=${sorted[-1]}

echo
echo "===== Summary ====="
printf "Successful runs : %d / %d\n" "$count" "$RUNS"
printf "Average latency : %.3f ms\n" "$avg"
printf "Min latency     : %.3f ms\n" "$min"
printf "Max latency     : %.3f ms\n" "$max"

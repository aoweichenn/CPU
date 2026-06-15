#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
LAB_DIR="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
ROOT_DIR="$(cd -- "${LAB_DIR}/../.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/lab00-release"
RESULT_DIR="${ROOT_DIR}/results/lab00"
BIN_DIR="${BUILD_DIR}/bin/cpu-volume-1/lab00"

mkdir -p "${BUILD_DIR}" "${RESULT_DIR}"

cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release -DCPU_VOLUME_1_BUILD_LABS=ON
cmake --build "${BUILD_DIR}" --target lab00_bench lab00_bad_benchmarks lab00_tests
ctest --test-dir "${BUILD_DIR}" -R lab00_tests --output-on-failure

"${ROOT_DIR}/tools/env_report.sh" > "${RESULT_DIR}/env.txt"
"${BIN_DIR}/lab00_bench" \
    --sizes 1024,16384,1048576 \
    --warmup 3 \
    --iterations 10 \
    --csv "${RESULT_DIR}/foundation.csv"

"${ROOT_DIR}/tools/summarize_bench_csv.py" \
    "${RESULT_DIR}/foundation.csv" \
    > "${RESULT_DIR}/summary.md"

"${BIN_DIR}/lab00_bad_benchmarks" \
    > "${RESULT_DIR}/bad_benchmarks.txt"

echo "Lab 00 finished."
echo "Environment report: ${RESULT_DIR}/env.txt"
echo "Benchmark CSV:       ${RESULT_DIR}/foundation.csv"
echo "Benchmark summary:   ${RESULT_DIR}/summary.md"
echo "Bad examples:        ${RESULT_DIR}/bad_benchmarks.txt"

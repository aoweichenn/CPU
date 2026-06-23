#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VOLUME_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
REPO_DIR="$(cd "${VOLUME_DIR}/../.." && pwd)"
BUILD_DIR="${VOLUME_DIR}/build/lcqi-release"
RESULT_DIR="${VOLUME_DIR}/results"
REPEAT="${1:-200}"
STAMP="$(date -u +%Y%m%dT%H%M%SZ)"
CSV_PATH="${RESULT_DIR}/lcqi-bench-${STAMP}.csv"
META_PATH="${RESULT_DIR}/lcqi-bench-${STAMP}.meta.txt"
PERF_PATH="${RESULT_DIR}/lcqi-bench-${STAMP}.perf.txt"

mkdir -p "${RESULT_DIR}"

cmake -S "${VOLUME_DIR}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release
cmake --build "${BUILD_DIR}"
ctest --test-dir "${BUILD_DIR}" --output-on-failure

{
  echo "timestamp_utc=${STAMP}"
  echo "repeat=${REPEAT}"
  echo "commit=$(git -C "${REPO_DIR}" rev-parse --short HEAD)"
  echo "uname=$(uname -a)"
  echo "compiler=$(${CXX:-c++} --version | sed -n '1p')"
  echo "cmake=$(cmake --version | sed -n '1p')"
  echo "bench=${BUILD_DIR}/labs/linux_cpu_inference/lcqi_bench"
} > "${META_PATH}"

"${BUILD_DIR}/labs/linux_cpu_inference/lcqi_bench" "${REPEAT}" > "${CSV_PATH}"

if command -v perf >/dev/null 2>&1; then
  perf stat \
    -e cycles,instructions,cache-misses,branches,branch-misses \
    "${BUILD_DIR}/labs/linux_cpu_inference/lcqi_bench" "${REPEAT}" \
    > /dev/null 2> "${PERF_PATH}" || true
else
  echo "perf_unavailable=1" > "${PERF_PATH}"
fi

echo "csv=${CSV_PATH}"
echo "meta=${META_PATH}"
echo "perf=${PERF_PATH}"

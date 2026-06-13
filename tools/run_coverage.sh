#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/lab00-coverage"
RESULT_DIR="${ROOT_DIR}/results/coverage"
GCOV_DIR="${RESULT_DIR}/gcov"

mkdir -p "${BUILD_DIR}" "${RESULT_DIR}" "${GCOV_DIR}"
rm -f "${RESULT_DIR}"/*.txt
rm -f "${GCOV_DIR}"/*

cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCPU_BUILD_LABS=ON \
    -DCPU_ENABLE_COVERAGE=ON

cmake --build "${BUILD_DIR}" --target lab00_bench lab00_bad_benchmarks lab00_tests
ctest --test-dir "${BUILD_DIR}" -R lab00_tests --output-on-failure

"${BUILD_DIR}/labs/lab00_benchmark_foundation/lab00_bench" --help > /dev/null
"${BUILD_DIR}/labs/lab00_benchmark_foundation/lab00_bench" --list > /dev/null
"${BUILD_DIR}/labs/lab00_benchmark_foundation/lab00_bench" \
    --sizes 16 \
    --warmup 1 \
    --iterations 1 \
    > /dev/null
"${BUILD_DIR}/labs/lab00_benchmark_foundation/lab00_bench" \
    --only dot_f32 \
    --sizes 16,32 \
    --warmup 1 \
    --iterations 1 \
    --csv "${RESULT_DIR}/coverage_bench.csv"

if "${BUILD_DIR}/labs/lab00_benchmark_foundation/lab00_bench" --unknown-option > /dev/null 2>&1; then
    echo "expected --unknown-option to fail" >&2
    exit 1
fi

if "${BUILD_DIR}/labs/lab00_benchmark_foundation/lab00_bench" --csv > /dev/null 2>&1; then
    echo "expected --csv without value to fail" >&2
    exit 1
fi

if "${BUILD_DIR}/labs/lab00_benchmark_foundation/lab00_bench" --sizes 8,0 > /dev/null 2>&1; then
    echo "expected invalid size list to fail" >&2
    exit 1
fi

if "${BUILD_DIR}/labs/lab00_benchmark_foundation/lab00_bench" --csv / > /dev/null 2>&1; then
    echo "expected unwritable CSV path to fail" >&2
    exit 1
fi

"${BUILD_DIR}/labs/lab00_benchmark_foundation/lab00_bad_benchmarks" > /dev/null

ROW_FILE="${RESULT_DIR}/rows.tsv"
rm -f "${ROW_FILE}"

summarize_source() {
    local source_path="$1"
    local gcno_path="$2"
    local relative_path="$3"
    local source_name
    source_name="$(basename -- "${source_path}")"

    (
        cd "${GCOV_DIR}"
        gcov -b -c -o "${gcno_path}" "${source_path}" > "${source_name}.gcov.raw.txt"
    )

    local gcov_path="${GCOV_DIR}/${source_name}.gcov"
    if [ ! -f "${gcov_path}" ]; then
        echo "missing gcov output for ${relative_path}" >&2
        return 1
    fi

    awk -F: -v file="${relative_path}" '
        $2 ~ /^ *[0-9]+ *$/ && $1 !~ /^ *-$/ {
            total += 1
            if ($1 !~ /#####/ && $1 !~ /=====/) {
                covered += 1
            }
        }
        END {
            printf "%s\t%d\t%d\n", file, covered, total
        }
    ' "${gcov_path}" >> "${ROW_FILE}"
}

summarize_source \
    "${ROOT_DIR}/labs/lab00_benchmark_foundation/src/benchmark.cpp" \
    "${BUILD_DIR}/labs/lab00_benchmark_foundation/CMakeFiles/lab00_benchmark.dir/src/benchmark.cpp.gcno" \
    "src/benchmark.cpp"

summarize_source \
    "${ROOT_DIR}/labs/lab00_benchmark_foundation/src/kernels.cpp" \
    "${BUILD_DIR}/labs/lab00_benchmark_foundation/CMakeFiles/lab00_benchmark.dir/src/kernels.cpp.gcno" \
    "src/kernels.cpp"

summarize_source \
    "${ROOT_DIR}/labs/lab00_benchmark_foundation/src/main.cpp" \
    "${BUILD_DIR}/labs/lab00_benchmark_foundation/CMakeFiles/lab00_bench.dir/src/main.cpp.gcno" \
    "src/main.cpp"

summarize_source \
    "${ROOT_DIR}/labs/lab00_benchmark_foundation/src/bad_benchmarks.cpp" \
    "${BUILD_DIR}/labs/lab00_benchmark_foundation/CMakeFiles/lab00_bad_benchmarks.dir/src/bad_benchmarks.cpp.gcno" \
    "src/bad_benchmarks.cpp"

summarize_source \
    "${ROOT_DIR}/labs/lab00_benchmark_foundation/tests/lab00_tests.cpp" \
    "${BUILD_DIR}/labs/lab00_benchmark_foundation/CMakeFiles/lab00_tests.dir/tests/lab00_tests.cpp.gcno" \
    "tests/lab00_tests.cpp"

{
    echo "# Lab 00 Coverage Summary"
    echo
    echo "| file | covered lines | executable lines | line coverage |"
    echo "|---|---:|---:|---:|"
    awk -F '\t' '
        {
            covered += $2
            total += $3
            rate = $3 == 0 ? 100.0 : ($2 * 100.0 / $3)
            printf "| %s | %d | %d | %.1f%% |\n", $1, $2, $3, rate
        }
        END {
            total_rate = total == 0 ? 100.0 : (covered * 100.0 / total)
            printf "| **total** | **%d** | **%d** | **%.1f%%** |\n", covered, total, total_rate
        }
    ' "${ROW_FILE}"
} > "${RESULT_DIR}/summary.txt"

echo "Coverage summary: ${RESULT_DIR}/summary.txt"

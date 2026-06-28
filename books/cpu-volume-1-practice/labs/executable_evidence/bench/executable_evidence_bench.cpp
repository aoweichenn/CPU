#include <cpu1/elf_fixture.hpp>
#include <cpu1/executable_evidence.hpp>

#include <benchmark/benchmark.h>

#include <cstdint>
#include <vector>

static void BMInspectSyntheticElf(benchmark::State& state)
{
    const std::vector<std::uint8_t> bytes = cpu1::make_synthetic_elf();
    for (auto _ : state) {
        const cpu1::ElfReport report = cpu1::inspect_elf(bytes, "bench.elf");
        benchmark::DoNotOptimize(report.sections.size());
        benchmark::DoNotOptimize(report.symbols.size());
        benchmark::ClobberMemory();
    }
}

BENCHMARK(BMInspectSyntheticElf);
BENCHMARK_MAIN();

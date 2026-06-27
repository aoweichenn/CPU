#include <benchmark/benchmark.h>

#include <cse/reference_pipeline.hpp>

#include <string>

namespace {

[[nodiscard]] std::string make_input(int records)
{
    std::string text;
    text.reserve(static_cast<std::size_t>(records) * 12);
    for (int index = 0; index < records; ++index) {
        text.append(index % 3 == 0 ? "view,1\n" : "click,1\n");
    }
    return text;
}

void bm_run_reference(benchmark::State& state)
{
    const cse::InputCase input{
        "bench",
        1,
        make_input(static_cast<int>(state.range(0))),
    };

    for (auto _ : state) {
        benchmark::DoNotOptimize(cse::run_reference(input));
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}

BENCHMARK(bm_run_reference)->Arg(128)->Arg(4096)->Arg(65536);

}  // namespace

BENCHMARK_MAIN();

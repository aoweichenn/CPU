#include <benchmark/benchmark.h>

#include <cppbook/practice_workbook.hpp>

#include <string>

namespace {

[[nodiscard]] std::string make_text(int repeat)
{
    constexpr std::string_view LINE =
        "C++ systems code needs tests, design, ownership, and readable contracts.\n";
    std::string text;
    text.reserve(LINE.size() * static_cast<std::size_t>(repeat));
    for (int index = 0; index < repeat; ++index) {
        text.append(LINE);
    }
    return text;
}

void bm_top_words(benchmark::State& state)
{
    cppbook::TextConfig config;
    config.min_word_length = 3;
    config.keep_numbers = false;
    config.stop_words = {"and", "the"};
    const cppbook::TextAnalyzer analyzer(config);
    const std::string text = make_text(static_cast<int>(state.range(0)));

    for (auto _ : state) {
        benchmark::DoNotOptimize(analyzer.top_words(text, 8));
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}

BENCHMARK(bm_top_words)->Arg(64)->Arg(1024)->Arg(8192);

}  // namespace

BENCHMARK_MAIN();

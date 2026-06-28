#include <cppbook/practice_workbook.hpp>

#include <charconv>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <span>
#include <string>
#include <string_view>

namespace {

constexpr int CPPBOOK_ARGC_SAMPLE_MODE = 1;
constexpr int CPPBOOK_ARGC_FILE_MODE_MIN = 3;
constexpr int CPPBOOK_ARGC_FILE_MODE_MAX = 4;
constexpr int CPPBOOK_CONFIG_PATH_ARG = 1;
constexpr int CPPBOOK_TEXT_PATH_ARG = 2;
constexpr int CPPBOOK_LIMIT_ARG = 3;
constexpr std::size_t CPPBOOK_DEFAULT_LIMIT = 5;

[[nodiscard]] std::size_t parse_limit(std::string_view text)
{
    std::size_t value = 0;
    const char* first = text.data();
    const char* last = text.data() + text.size();
    const auto [ptr, error] = std::from_chars(first, last, value);
    if (error != std::errc{} || ptr != last || value == 0) {
        throw std::runtime_error("limit must be a positive integer");
    }
    return value;
}

void print_words(const cppbook::TextAnalyzer& analyzer,
                 std::string_view text,
                 std::size_t limit)
{
    for (const cppbook::WordCount& word : analyzer.top_words(text, limit)) {
        std::cout << word.word << ',' << word.count << '\n';
    }
}

int run_sample()
{
    constexpr std::string_view CONFIG = R"(
min_word_length=3
keep_numbers=false
stop_words=the,and,with
)";
    constexpr std::string_view TEXT =
        "The C++ systems book connects code with systems, tests, and design.";

    const cppbook::ParseConfigResult parsed = cppbook::parse_text_config(CONFIG);
    if (!parsed.ok()) {
        for (const cppbook::Diagnostic& diagnostic : parsed.diagnostics) {
            std::cerr << "config_error line=" << diagnostic.line_number
                      << " message=" << diagnostic.message << '\n';
        }
        return 1;
    }

    const cppbook::TextAnalyzer analyzer(parsed.config);
    print_words(analyzer, TEXT, CPPBOOK_DEFAULT_LIMIT);
    return 0;
}

int run_file_mode(std::span<char* const> arguments)
{
    if (arguments.size() < CPPBOOK_ARGC_FILE_MODE_MIN ||
        arguments.size() > CPPBOOK_ARGC_FILE_MODE_MAX) {
        std::cerr << "usage: cppbook_practice_demo <config-file> <text-file> [limit]\n";
        return 1;
    }

    const std::filesystem::path config_path = arguments[CPPBOOK_CONFIG_PATH_ARG];
    const std::filesystem::path text_path = arguments[CPPBOOK_TEXT_PATH_ARG];
    const std::size_t limit = arguments.size() == CPPBOOK_ARGC_FILE_MODE_MAX
        ? parse_limit(arguments[CPPBOOK_LIMIT_ARG])
        : CPPBOOK_DEFAULT_LIMIT;

    const std::string config_text = cppbook::read_text_file(config_path);
    const std::string text = cppbook::read_text_file(text_path);
    const cppbook::ParseConfigResult parsed = cppbook::parse_text_config(config_text);
    if (!parsed.ok()) {
        for (const cppbook::Diagnostic& diagnostic : parsed.diagnostics) {
            std::cerr << "config_error line=" << diagnostic.line_number
                      << " message=" << diagnostic.message << '\n';
        }
        return 2;
    }

    const cppbook::TextAnalyzer analyzer(parsed.config);
    print_words(analyzer, text, limit);
    return 0;
}

}  // namespace

int main(int argc, char** argv)
{
    try {
        if (argc == CPPBOOK_ARGC_SAMPLE_MODE) {
            return run_sample();
        }
        return run_file_mode(std::span<char* const>(argv, static_cast<std::size_t>(argc)));
    } catch (const std::exception& error) {
        std::cerr << "cppbook practice demo failure: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}

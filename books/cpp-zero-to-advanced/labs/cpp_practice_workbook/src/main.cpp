#include <cppbook/practice_workbook.hpp>

#include <iostream>
#include <string_view>

int main()
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
    for (const cppbook::WordCount& word : analyzer.top_words(TEXT, 5)) {
        std::cout << word.word << ',' << word.count << '\n';
    }
    return 0;
}

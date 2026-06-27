#pragma once

#include <cstddef>
#include <cstdint>
#include <list>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace cppbook {

struct Diagnostic {
    std::int32_t line_number = 0;
    std::string message;
};

struct TextConfig {
    std::size_t min_word_length = 1;
    bool keep_numbers = false;
    std::set<std::string> stop_words;
};

struct ParseConfigResult {
    TextConfig config;
    std::vector<Diagnostic> diagnostics;

    [[nodiscard]] bool ok() const;
};

struct WordCount {
    std::string word;
    std::uint64_t count = 0;
};

[[nodiscard]] ParseConfigResult parse_text_config(std::string_view text);

class TextAnalyzer {
public:
    explicit TextAnalyzer(TextConfig config);

    [[nodiscard]] std::vector<std::string> tokenize(std::string_view text) const;
    [[nodiscard]] std::map<std::string, std::uint64_t> count_words(
        std::string_view text) const;
    [[nodiscard]] std::vector<WordCount> top_words(std::string_view text,
                                                   std::size_t limit) const;

private:
    [[nodiscard]] bool keep_word(std::string_view word) const;

    TextConfig config_;
};

class AnalysisCache {
public:
    explicit AnalysisCache(std::size_t capacity);

    [[nodiscard]] bool contains(std::string_view key) const;
    [[nodiscard]] std::optional<std::vector<WordCount>> get(std::string_view key);
    void put(std::string key, std::vector<WordCount> value);
    [[nodiscard]] std::size_t size() const;
    [[nodiscard]] std::vector<std::string> keys_most_recent_first() const;

private:
    struct Entry {
        std::string key;
        std::vector<WordCount> value;
    };

    using EntryList = std::list<Entry>;
    using EntryIterator = EntryList::iterator;

    void touch(EntryIterator iterator);
    void evict_until_valid();

    std::size_t capacity_;
    EntryList entries_;
    std::unordered_map<std::string, EntryIterator> index_;
};

}  // namespace cppbook

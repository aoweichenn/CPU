#include <cppbook/practice_workbook.hpp>

#include <algorithm>
#include <charconv>
#include <cctype>
#include <stdexcept>
#include <utility>

namespace cppbook {
namespace {

constexpr std::size_t CPPBOOK_MIN_WORD_LENGTH = 1;
constexpr std::size_t CPPBOOK_MIN_CACHE_CAPACITY = 1;
constexpr std::int32_t CPPBOOK_FIRST_CONFIG_LINE = 1;
constexpr std::string_view CPPBOOK_KEY_MIN_WORD_LENGTH = "min_word_length";
constexpr std::string_view CPPBOOK_KEY_KEEP_NUMBERS = "keep_numbers";
constexpr std::string_view CPPBOOK_KEY_STOP_WORDS = "stop_words";

[[nodiscard]] char to_lower_ascii(char ch)
{
    return static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
}

[[nodiscard]] bool is_ascii_alnum(char ch)
{
    return std::isalnum(static_cast<unsigned char>(ch)) != 0;
}

[[nodiscard]] bool is_ascii_alpha(char ch)
{
    return std::isalpha(static_cast<unsigned char>(ch)) != 0;
}

[[nodiscard]] std::string trim_copy(std::string_view text)
{
    std::size_t begin = 0;
    while (begin < text.size() &&
           std::isspace(static_cast<unsigned char>(text[begin])) != 0) {
        ++begin;
    }

    std::size_t end = text.size();
    while (end > begin &&
           std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
        --end;
    }

    return std::string(text.substr(begin, end - begin));
}

[[nodiscard]] std::string lower_copy(std::string_view text)
{
    std::string result;
    result.reserve(text.size());
    for (const char ch : text) {
        result.push_back(to_lower_ascii(ch));
    }
    return result;
}

[[nodiscard]] std::optional<std::size_t> parse_size(std::string_view text)
{
    std::size_t value = 0;
    const char* first = text.data();
    const char* last = text.data() + text.size();
    const auto [ptr, error] = std::from_chars(first, last, value);
    if (error != std::errc{} || ptr != last) {
        return std::nullopt;
    }
    return value;
}

[[nodiscard]] std::optional<bool> parse_bool(std::string_view text)
{
    const std::string normalized = lower_copy(trim_copy(text));
    if (normalized == "true" || normalized == "1") {
        return true;
    }
    if (normalized == "false" || normalized == "0") {
        return false;
    }
    return std::nullopt;
}

void add_diagnostic(std::vector<Diagnostic>& diagnostics,
                    std::int32_t line_number,
                    std::string message)
{
    diagnostics.push_back(Diagnostic{line_number, std::move(message)});
}

void parse_stop_words(std::string_view value, TextConfig& config)
{
    std::size_t begin = 0;
    while (begin <= value.size()) {
        const std::size_t comma = value.find(',', begin);
        const std::size_t end = comma == std::string_view::npos ? value.size() : comma;
        const std::string word = lower_copy(trim_copy(value.substr(begin, end - begin)));
        if (!word.empty()) {
            config.stop_words.insert(word);
        }
        if (comma == std::string_view::npos) {
            break;
        }
        begin = comma + 1;
    }
}

void parse_config_line(std::string_view raw_line,
                       std::int32_t line_number,
                       TextConfig& config,
                       std::vector<Diagnostic>& diagnostics)
{
    const std::size_t comment = raw_line.find('#');
    const std::string line = trim_copy(raw_line.substr(0, comment));
    if (line.empty()) {
        return;
    }

    const std::size_t equals = line.find('=');
    if (equals == std::string::npos) {
        add_diagnostic(diagnostics, line_number, "missing '='");
        return;
    }

    const std::string key = lower_copy(trim_copy(std::string_view(line).substr(0, equals)));
    const std::string value = trim_copy(std::string_view(line).substr(equals + 1));

    if (key == CPPBOOK_KEY_MIN_WORD_LENGTH) {
        const std::optional<std::size_t> parsed = parse_size(value);
        if (!parsed.has_value() || *parsed < CPPBOOK_MIN_WORD_LENGTH) {
            add_diagnostic(diagnostics, line_number, "min_word_length must be positive");
            return;
        }
        config.min_word_length = *parsed;
        return;
    }

    if (key == CPPBOOK_KEY_KEEP_NUMBERS) {
        const std::optional<bool> parsed = parse_bool(value);
        if (!parsed.has_value()) {
            add_diagnostic(diagnostics, line_number, "keep_numbers must be true or false");
            return;
        }
        config.keep_numbers = *parsed;
        return;
    }

    if (key == CPPBOOK_KEY_STOP_WORDS) {
        parse_stop_words(value, config);
        return;
    }

    add_diagnostic(diagnostics, line_number, "unknown key: " + key);
}

[[nodiscard]] bool contains_alpha(std::string_view word)
{
    return std::ranges::any_of(word, [](char ch) {
        return is_ascii_alpha(ch);
    });
}

}  // namespace

bool ParseConfigResult::ok() const
{
    return this->diagnostics.empty();
}

ParseConfigResult parse_text_config(std::string_view text)
{
    ParseConfigResult result;
    std::size_t begin = 0;
    std::int32_t line_number = CPPBOOK_FIRST_CONFIG_LINE;

    while (begin < text.size()) {
        const std::size_t newline = text.find('\n', begin);
        const std::size_t end = newline == std::string_view::npos ? text.size() : newline;
        std::string_view line = text.substr(begin, end - begin);
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }
        parse_config_line(line, line_number, result.config, result.diagnostics);
        if (newline == std::string_view::npos) {
            break;
        }
        begin = newline + 1;
        ++line_number;
    }

    return result;
}

TextAnalyzer::TextAnalyzer(TextConfig config)
    : config_(std::move(config))
{
    if (this->config_.min_word_length < CPPBOOK_MIN_WORD_LENGTH) {
        throw std::runtime_error("min_word_length must be positive");
    }
}

std::vector<std::string> TextAnalyzer::tokenize(std::string_view text) const
{
    std::vector<std::string> tokens;
    std::string current;

    for (const char ch : text) {
        if (is_ascii_alnum(ch)) {
            current.push_back(to_lower_ascii(ch));
            continue;
        }

        if (!current.empty()) {
            if (this->keep_word(current)) {
                tokens.push_back(current);
            }
            current.clear();
        }
    }

    if (!current.empty() && this->keep_word(current)) {
        tokens.push_back(current);
    }

    return tokens;
}

std::map<std::string, std::uint64_t> TextAnalyzer::count_words(std::string_view text) const
{
    std::map<std::string, std::uint64_t> counts;
    for (const std::string& token : this->tokenize(text)) {
        ++counts[token];
    }
    return counts;
}

std::vector<WordCount> TextAnalyzer::top_words(std::string_view text,
                                               std::size_t limit) const
{
    std::vector<WordCount> words;
    for (const auto& [word, count] : this->count_words(text)) {
        words.push_back(WordCount{word, count});
    }

    std::ranges::sort(words, [](const WordCount& lhs, const WordCount& rhs) {
        if (lhs.count != rhs.count) {
            return lhs.count > rhs.count;
        }
        return lhs.word < rhs.word;
    });

    if (words.size() > limit) {
        words.resize(limit);
    }
    return words;
}

bool TextAnalyzer::keep_word(std::string_view word) const
{
    if (word.size() < this->config_.min_word_length) {
        return false;
    }
    if (!this->config_.keep_numbers && !contains_alpha(word)) {
        return false;
    }
    return !this->config_.stop_words.contains(std::string(word));
}

AnalysisCache::AnalysisCache(std::size_t capacity)
    : capacity_(capacity)
{
    if (this->capacity_ < CPPBOOK_MIN_CACHE_CAPACITY) {
        throw std::runtime_error("cache capacity must be positive");
    }
}

bool AnalysisCache::contains(std::string_view key) const
{
    return this->index_.find(std::string(key)) != this->index_.end();
}

std::optional<std::vector<WordCount>> AnalysisCache::get(std::string_view key)
{
    const auto found = this->index_.find(std::string(key));
    if (found == this->index_.end()) {
        return std::nullopt;
    }

    this->touch(found->second);
    return this->entries_.front().value;
}

void AnalysisCache::put(std::string key, std::vector<WordCount> value)
{
    const auto found = this->index_.find(key);
    if (found != this->index_.end()) {
        found->second->value = std::move(value);
        this->touch(found->second);
        return;
    }

    this->entries_.push_front(Entry{std::move(key), std::move(value)});
    this->index_.emplace(this->entries_.front().key, this->entries_.begin());
    this->evict_until_valid();
}

std::size_t AnalysisCache::size() const
{
    return this->entries_.size();
}

std::vector<std::string> AnalysisCache::keys_most_recent_first() const
{
    std::vector<std::string> keys;
    keys.reserve(this->entries_.size());
    for (const Entry& entry : this->entries_) {
        keys.push_back(entry.key);
    }
    return keys;
}

void AnalysisCache::touch(EntryIterator iterator)
{
    if (iterator == this->entries_.begin()) {
        return;
    }
    this->entries_.splice(this->entries_.begin(), this->entries_, iterator);
    this->index_[this->entries_.front().key] = this->entries_.begin();
}

void AnalysisCache::evict_until_valid()
{
    while (this->entries_.size() > this->capacity_) {
        const std::string key = this->entries_.back().key;
        this->entries_.pop_back();
        this->index_.erase(key);
    }
}

}  // namespace cppbook

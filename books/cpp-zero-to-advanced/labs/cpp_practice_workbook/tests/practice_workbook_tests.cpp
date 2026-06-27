#include <cppbook/practice_workbook.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <stdexcept>
#include <string_view>
#include <vector>

namespace {

using ::testing::ElementsAre;
using ::testing::HasSubstr;

TEST(CppPracticeWorkbookTest, ConfigParserAcceptsCommentsAndStopWords)
{
    constexpr std::string_view CONFIG = R"(
# text analyzer configuration
min_word_length = 3
keep_numbers = false
stop_words = the, and, Cpp
)";

    const cppbook::ParseConfigResult parsed = cppbook::parse_text_config(CONFIG);
    EXPECT_TRUE(parsed.ok());
    EXPECT_EQ(parsed.config.min_word_length, 3U);
    EXPECT_FALSE(parsed.config.keep_numbers);
    EXPECT_TRUE(parsed.config.stop_words.contains("the"));
    EXPECT_TRUE(parsed.config.stop_words.contains("and"));
    EXPECT_TRUE(parsed.config.stop_words.contains("cpp"));
}

TEST(CppPracticeWorkbookTest, ConfigParserReportsBadLines)
{
    constexpr std::string_view CONFIG = R"(
min_word_length=0
keep_numbers=maybe
unknown=value
missing_equals
)";

    const cppbook::ParseConfigResult parsed = cppbook::parse_text_config(CONFIG);
    ASSERT_FALSE(parsed.ok());
    ASSERT_EQ(parsed.diagnostics.size(), 4U);
    EXPECT_EQ(parsed.diagnostics[0].line_number, 2);
    EXPECT_EQ(parsed.diagnostics[1].message, "keep_numbers must be true or false");
    EXPECT_EQ(parsed.diagnostics[2].message, "unknown key: unknown");
    EXPECT_EQ(parsed.diagnostics[3].message, "missing '='");
}

TEST(CppPracticeWorkbookTest, TextAnalyzerTokenizesCountsAndSorts)
{
    cppbook::TextConfig config;
    config.min_word_length = 3;
    config.keep_numbers = false;
    config.stop_words = {"the", "and"};

    const cppbook::TextAnalyzer analyzer(config);
    const std::vector<cppbook::WordCount> top =
        analyzer.top_words("The CPU systems and cpu systems 42; cache!", 3);

    ASSERT_EQ(top.size(), 3U);
    EXPECT_EQ(top[0].word, "cpu");
    EXPECT_EQ(top[0].count, 2U);
    EXPECT_EQ(top[1].word, "systems");
    EXPECT_EQ(top[1].count, 2U);
    EXPECT_EQ(top[2].word, "cache");
    EXPECT_EQ(top[2].count, 1U);
}

TEST(CppPracticeWorkbookTest, TextAnalyzerCanKeepNumericTokens)
{
    cppbook::TextConfig config;
    config.min_word_length = 2;
    config.keep_numbers = true;

    const cppbook::TextAnalyzer analyzer(config);
    const std::vector<std::string> tokens = analyzer.tokenize("v2 42 c++ 42");

    EXPECT_THAT(tokens, ElementsAre("v2", "42", "42"));
}

TEST(CppPracticeWorkbookTest, AnalysisCacheEvictionAndRecency)
{
    cppbook::AnalysisCache cache(2);
    cache.put("a", {cppbook::WordCount{"alpha", 1}});
    cache.put("b", {cppbook::WordCount{"beta", 1}});

    const auto first = cache.get("a");
    ASSERT_TRUE(first.has_value());
    cache.put("c", {cppbook::WordCount{"cache", 1}});

    EXPECT_EQ(cache.size(), 2U);
    EXPECT_TRUE(cache.contains("a"));
    EXPECT_FALSE(cache.contains("b"));
    EXPECT_TRUE(cache.contains("c"));
    EXPECT_THAT(cache.keys_most_recent_first(), ElementsAre("c", "a"));
}

TEST(CppPracticeWorkbookTest, AnalysisCacheRejectsZeroCapacity)
{
    EXPECT_THROW(cppbook::AnalysisCache cache(0), std::runtime_error);
}

TEST(CppPracticeWorkbookTest, DiagnosticMessagesRemainActionable)
{
    const cppbook::ParseConfigResult parsed =
        cppbook::parse_text_config("keep_numbers=maybe\n");

    ASSERT_THAT(parsed.diagnostics, testing::SizeIs(1));
    EXPECT_THAT(parsed.diagnostics[0].message, HasSubstr("true or false"));
}

}  // namespace

#include <cse/reference_pipeline.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>

namespace {

using ::testing::HasSubstr;

TEST(CseReferencePipelineTest, NormalInputCountsAndChecksumAreStable)
{
    const cse::InputCase input{
        "normal_5_lines",
        1,
        "view,1\nclick,1\nview,2\nlogin,1\nlogout,1\n",
    };

    const cse::ReferenceResult first = cse::run_reference(input);
    const cse::ReferenceResult second = cse::run_reference(input);

    EXPECT_EQ(first.records_total, 5U);
    EXPECT_EQ(first.accepted, 5U);
    EXPECT_TRUE(first.diagnostics.empty());
    EXPECT_EQ(first.counts.at("view"), 2U);
    EXPECT_EQ(first.counts.at("click"), 1U);
    EXPECT_NE(first.checksum, 0U);
    EXPECT_EQ(first.checksum, second.checksum);
}

TEST(CseReferencePipelineTest, BadLinesReturnStructuredDiagnostics)
{
    const cse::InputCase input{
        "bad_lines",
        7,
        "view,1\nmissing\nclick,1,extra\n,2\nlogin,abc",
    };

    const cse::ReferenceResult result = cse::run_reference(input);

    EXPECT_EQ(result.records_total, 5U);
    EXPECT_EQ(result.accepted, 1U);
    ASSERT_EQ(result.diagnostics.size(), 4U);
    EXPECT_EQ(result.diagnostics[0].error_kind, "missing_field");
    EXPECT_EQ(result.diagnostics[0].record.line_number, 2U);
    EXPECT_EQ(result.diagnostics[1].error_kind, "extra_field");
    EXPECT_EQ(result.diagnostics[2].error_kind, "empty_event");
    EXPECT_EQ(result.diagnostics[3].error_kind, "bad_value");
    EXPECT_EQ(result.diagnostics[3].record.byte_offset, 32U);
}

TEST(CseReferencePipelineTest, ReportAndManifestAreDeterministic)
{
    const cse::InputCase input{
        "mixed",
        3,
        "view,1\nview,2\nbad\n",
    };

    const cse::ReferenceResult result = cse::run_reference(input);
    const std::string report = cse::format_report(result);
    const cse::ManifestRow row = cse::make_manifest_row(result);
    const std::string manifest = cse::format_manifest_row(row);

    EXPECT_EQ(row.input_name, "mixed");
    EXPECT_EQ(row.records_total, 3U);
    EXPECT_EQ(row.accepted, 2U);
    EXPECT_EQ(row.rejected, 1U);
    EXPECT_THAT(report, HasSubstr("count.view=2"));
    EXPECT_THAT(report, HasSubstr("error.missing_field=1"));
    EXPECT_THAT(manifest, testing::StartsWith("mixed,3,3,2,1,"));
}

TEST(CseReferencePipelineTest, EmptyInputHasZeroRecordsButStableChecksum)
{
    const cse::InputCase input{"empty", 1, ""};
    const cse::ReferenceResult result = cse::run_reference(input);

    EXPECT_EQ(result.records_total, 0U);
    EXPECT_EQ(result.accepted, 0U);
    EXPECT_TRUE(result.diagnostics.empty());
    EXPECT_TRUE(result.counts.empty());
    EXPECT_EQ(result.checksum, cse::run_reference(input).checksum);
}

TEST(CseReferencePipelineTest, DiagnosticDetailNamesTheContract)
{
    const cse::ReferenceResult result =
        cse::run_reference(cse::InputCase{"bad", 1, "too,few,nope\n"});

    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics[0].error_kind, "extra_field");
    EXPECT_THAT(result.diagnostics[0].detail, HasSubstr("exactly two fields"));
}

}  // namespace

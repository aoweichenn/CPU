#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace cse {

struct RecordId {
    std::string input_name;
    std::uint64_t input_generation = 0;
    std::uint64_t byte_offset = 0;
    std::uint64_t line_number = 0;
};

struct ParsedRecord {
    RecordId record;
    std::string event_name;
    std::int64_t value = 0;
};

struct ParseDiagnostic {
    RecordId record;
    std::string error_kind;
    std::string detail;
};

struct InputCase {
    std::string input_name;
    std::uint64_t input_generation = 0;
    std::string text;
};

struct ReferenceResult {
    std::string input_name;
    std::uint64_t input_generation = 0;
    std::uint64_t records_total = 0;
    std::uint64_t accepted = 0;
    std::vector<ParseDiagnostic> diagnostics;
    std::map<std::string, std::uint64_t> counts;
    std::uint64_t checksum = 0;
};

struct ManifestRow {
    std::string input_name;
    std::uint64_t input_generation = 0;
    std::uint64_t records_total = 0;
    std::uint64_t accepted = 0;
    std::uint64_t rejected = 0;
    std::uint64_t checksum = 0;
};

[[nodiscard]] ReferenceResult run_reference(const InputCase& input);
[[nodiscard]] ManifestRow make_manifest_row(const ReferenceResult& result);
[[nodiscard]] std::string format_report(const ReferenceResult& result);
[[nodiscard]] std::string format_manifest_row(const ManifestRow& row);

}  // namespace cse

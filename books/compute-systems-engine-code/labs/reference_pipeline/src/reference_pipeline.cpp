#include <cse/reference_pipeline.hpp>

#include <charconv>
#include <optional>
#include <sstream>
#include <utility>

namespace cse {
namespace {

constexpr std::uint64_t CSE_FIRST_LINE_NUMBER = 1;
constexpr std::uint64_t CSE_FNV_OFFSET_BASIS = 14695981039346656037ULL;
constexpr std::uint64_t CSE_FNV_PRIME = 1099511628211ULL;
constexpr std::string_view CSE_ERROR_MISSING_FIELD = "missing_field";
constexpr std::string_view CSE_ERROR_EXTRA_FIELD = "extra_field";
constexpr std::string_view CSE_ERROR_EMPTY_EVENT = "empty_event";
constexpr std::string_view CSE_ERROR_BAD_VALUE = "bad_value";

[[nodiscard]] std::string trim_copy(std::string_view text)
{
    std::size_t begin = 0;
    while (begin < text.size() &&
           (text[begin] == ' ' || text[begin] == '\t' || text[begin] == '\r')) {
        ++begin;
    }

    std::size_t end = text.size();
    while (end > begin &&
           (text[end - 1] == ' ' || text[end - 1] == '\t' || text[end - 1] == '\r')) {
        --end;
    }

    return std::string(text.substr(begin, end - begin));
}

[[nodiscard]] std::vector<std::string_view> split_csv_line(std::string_view line)
{
    std::vector<std::string_view> fields;
    std::size_t begin = 0;
    while (begin <= line.size()) {
        const std::size_t comma = line.find(',', begin);
        const std::size_t end = comma == std::string_view::npos ? line.size() : comma;
        fields.push_back(line.substr(begin, end - begin));
        if (comma == std::string_view::npos) {
            break;
        }
        begin = comma + 1;
    }
    return fields;
}

[[nodiscard]] std::optional<std::int64_t> parse_int64(std::string_view text)
{
    std::int64_t value = 0;
    const char* first = text.data();
    const char* last = text.data() + text.size();
    const auto [ptr, error] = std::from_chars(first, last, value);
    if (error != std::errc{} || ptr != last) {
        return std::nullopt;
    }
    return value;
}

void append_diagnostic(ReferenceResult& result,
                       const RecordId& record,
                       std::string_view error_kind,
                       std::string detail)
{
    result.diagnostics.push_back(ParseDiagnostic{
        record,
        std::string(error_kind),
        std::move(detail),
    });
}

void update_checksum_byte(std::uint64_t& checksum, unsigned char byte)
{
    checksum ^= byte;
    checksum *= CSE_FNV_PRIME;
}

void update_checksum_text(std::uint64_t& checksum, std::string_view text)
{
    for (const char ch : text) {
        update_checksum_byte(checksum, static_cast<unsigned char>(ch));
    }
    update_checksum_byte(checksum, 0);
}

void update_checksum_u64(std::uint64_t& checksum, std::uint64_t value)
{
    constexpr std::int32_t CSE_U64_BYTE_COUNT = 8;
    constexpr std::int32_t CSE_BITS_PER_BYTE = 8;
    for (std::int32_t index = 0; index < CSE_U64_BYTE_COUNT; ++index) {
        const auto byte = static_cast<unsigned char>(
            (value >> (index * CSE_BITS_PER_BYTE)) & 0xffU);
        update_checksum_byte(checksum, byte);
    }
}

[[nodiscard]] std::uint64_t compute_checksum(const ReferenceResult& result)
{
    std::uint64_t checksum = CSE_FNV_OFFSET_BASIS;
    update_checksum_text(checksum, result.input_name);
    update_checksum_u64(checksum, result.input_generation);
    update_checksum_u64(checksum, result.records_total);
    update_checksum_u64(checksum, result.accepted);

    for (const auto& [event_name, count] : result.counts) {
        update_checksum_text(checksum, event_name);
        update_checksum_u64(checksum, count);
    }

    for (const ParseDiagnostic& diagnostic : result.diagnostics) {
        update_checksum_u64(checksum, diagnostic.record.line_number);
        update_checksum_u64(checksum, diagnostic.record.byte_offset);
        update_checksum_text(checksum, diagnostic.error_kind);
    }

    return checksum;
}

void process_line(std::string_view line,
                  const RecordId& record,
                  ReferenceResult& result)
{
    ++result.records_total;
    const std::vector<std::string_view> fields = split_csv_line(line);
    if (fields.size() < 2) {
        append_diagnostic(result, record, CSE_ERROR_MISSING_FIELD, "expected event,value");
        return;
    }
    if (fields.size() > 2) {
        append_diagnostic(result, record, CSE_ERROR_EXTRA_FIELD, "expected exactly two fields");
        return;
    }

    const std::string event_name = trim_copy(fields[0]);
    if (event_name.empty()) {
        append_diagnostic(result, record, CSE_ERROR_EMPTY_EVENT, "event name is empty");
        return;
    }

    const std::string value_text = trim_copy(fields[1]);
    const std::optional<std::int64_t> value = parse_int64(value_text);
    if (!value.has_value()) {
        append_diagnostic(result, record, CSE_ERROR_BAD_VALUE, "value is not a full integer");
        return;
    }

    ++result.accepted;
    ++result.counts[event_name];
}

}  // namespace

ReferenceResult run_reference(const InputCase& input)
{
    ReferenceResult result;
    result.input_name = input.input_name;
    result.input_generation = input.input_generation;

    std::size_t begin = 0;
    std::uint64_t line_number = CSE_FIRST_LINE_NUMBER;
    while (begin < input.text.size()) {
        const std::size_t newline = input.text.find('\n', begin);
        const std::size_t end = newline == std::string::npos ? input.text.size() : newline;
        const RecordId record{
            input.input_name,
            input.input_generation,
            static_cast<std::uint64_t>(begin),
            line_number,
        };
        process_line(std::string_view(input.text).substr(begin, end - begin), record, result);
        if (newline == std::string::npos) {
            break;
        }
        begin = newline + 1;
        ++line_number;
    }

    result.checksum = compute_checksum(result);
    return result;
}

ManifestRow make_manifest_row(const ReferenceResult& result)
{
    return ManifestRow{
        result.input_name,
        result.input_generation,
        result.records_total,
        result.accepted,
        static_cast<std::uint64_t>(result.diagnostics.size()),
        result.checksum,
    };
}

std::string format_report(const ReferenceResult& result)
{
    std::map<std::string, std::uint64_t> error_counts;
    for (const ParseDiagnostic& diagnostic : result.diagnostics) {
        ++error_counts[diagnostic.error_kind];
    }

    std::ostringstream out;
    out << "input_name=" << result.input_name << '\n';
    out << "input_generation=" << result.input_generation << '\n';
    out << "records_total=" << result.records_total << '\n';
    out << "accepted=" << result.accepted << '\n';
    out << "rejected=" << result.diagnostics.size() << '\n';
    out << "checksum=" << result.checksum << '\n';

    for (const auto& [event_name, count] : result.counts) {
        out << "count." << event_name << '=' << count << '\n';
    }
    for (const auto& [error_kind, count] : error_counts) {
        out << "error." << error_kind << '=' << count << '\n';
    }
    return out.str();
}

std::string format_manifest_row(const ManifestRow& row)
{
    std::ostringstream out;
    out << row.input_name << ','
        << row.input_generation << ','
        << row.records_total << ','
        << row.accepted << ','
        << row.rejected << ','
        << row.checksum;
    return out.str();
}

}  // namespace cse

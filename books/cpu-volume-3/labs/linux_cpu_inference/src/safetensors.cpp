#include <lcqi/safetensors.hpp>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace lcqi {
namespace {

constexpr std::size_t LCQI_SAFETENSORS_PREFIX_BYTES = 8;
constexpr std::uint64_t LCQI_SAFETENSORS_MAX_HEADER_BYTES = 16U * 1024U * 1024U;
constexpr std::size_t LCQI_SAFETENSORS_OFFSET_COUNT = 2;
constexpr std::uint64_t LCQI_BYTE_BITS = 8;
constexpr std::uint64_t LCQI_DECIMAL_BASE = 10;
constexpr std::uint64_t LCQI_BYTE_MASK = 0xFFU;
constexpr unsigned char LCQI_JSON_CONTROL_CHAR_LIMIT = 0x20U;
constexpr std::uint64_t LCQI_DTYPE_BYTES_F64 = 8;
constexpr std::uint64_t LCQI_DTYPE_BYTES_F32 = 4;
constexpr std::uint64_t LCQI_DTYPE_BYTES_F16 = 2;
constexpr std::uint64_t LCQI_DTYPE_BYTES_I64 = 8;
constexpr std::uint64_t LCQI_DTYPE_BYTES_I32 = 4;
constexpr std::uint64_t LCQI_DTYPE_BYTES_I16 = 2;
constexpr std::uint64_t LCQI_DTYPE_BYTES_I8 = 1;
constexpr std::uint64_t LCQI_DTYPE_BYTES_U8 = 1;

class HeaderCursor {
public:
    explicit HeaderCursor(std::string_view text) : text_(text) {}

    void expect(char expected) {
        this->skip_ws();
        if (this->eof() || this->text_[this->position_] != expected) {
            throw std::runtime_error("invalid safetensors header syntax");
        }
        ++this->position_;
    }

    [[nodiscard]] bool consume(char expected) {
        this->skip_ws();
        if (!this->eof() && this->text_[this->position_] == expected) {
            ++this->position_;
            return true;
        }
        return false;
    }

    [[nodiscard]] std::string parse_string() {
        this->skip_ws();
        this->expect('"');
        std::string result;
        while (!this->eof()) {
            const char ch = this->text_[this->position_++];
            if (ch == '"') {
                return result;
            }
            if (static_cast<unsigned char>(ch) < LCQI_JSON_CONTROL_CHAR_LIMIT) {
                throw std::runtime_error("control character in safetensors string");
            }
            if (ch == '\\') {
                result.push_back(this->parse_escape());
            } else {
                result.push_back(ch);
            }
        }
        throw std::runtime_error("unterminated safetensors string");
    }

    [[nodiscard]] std::uint64_t parse_u64() {
        this->skip_ws();
        if (this->eof() ||
            !std::isdigit(static_cast<unsigned char>(this->text_[this->position_]))) {
            throw std::runtime_error("expected unsigned integer in safetensors header");
        }
        std::uint64_t value = 0;
        while (!this->eof() &&
               std::isdigit(static_cast<unsigned char>(this->text_[this->position_]))) {
            const std::uint64_t digit =
                static_cast<std::uint64_t>(this->text_[this->position_] - '0');
            if (value >
                (std::numeric_limits<std::uint64_t>::max() - digit) / LCQI_DECIMAL_BASE) {
                throw std::runtime_error("safetensors integer overflow");
            }
            value = value * LCQI_DECIMAL_BASE + digit;
            ++this->position_;
        }
        return value;
    }

    [[nodiscard]] std::vector<std::int64_t> parse_i64_array() {
        std::vector<std::int64_t> values;
        this->expect('[');
        if (this->consume(']')) {
            return values;
        }
        while (true) {
            const std::uint64_t raw = this->parse_u64();
            if (raw > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
                throw std::runtime_error("safetensors shape value is too large");
            }
            values.push_back(static_cast<std::int64_t>(raw));
            if (this->consume(']')) {
                return values;
            }
            this->expect(',');
        }
    }

    [[nodiscard]] std::vector<std::uint64_t> parse_u64_array() {
        std::vector<std::uint64_t> values;
        this->expect('[');
        if (this->consume(']')) {
            return values;
        }
        while (true) {
            values.push_back(this->parse_u64());
            if (this->consume(']')) {
                return values;
            }
            this->expect(',');
        }
    }

    [[nodiscard]] bool eof_after_ws() {
        this->skip_ws();
        return this->eof();
    }

private:
    std::string_view text_;
    std::size_t position_ = 0;

    [[nodiscard]] bool eof() const {
        return this->position_ >= this->text_.size();
    }

    void skip_ws() {
        while (!this->eof() &&
               std::isspace(static_cast<unsigned char>(this->text_[this->position_]))) {
            ++this->position_;
        }
    }

    [[nodiscard]] char parse_escape() {
        if (this->eof()) {
            throw std::runtime_error("unterminated safetensors escape");
        }
        const char escaped = this->text_[this->position_++];
        switch (escaped) {
            case '"':
            case '\\':
            case '/':
                return escaped;
            case 'b':
                return '\b';
            case 'f':
                return '\f';
            case 'n':
                return '\n';
            case 'r':
                return '\r';
            case 't':
                return '\t';
            default:
                throw std::runtime_error("unsupported safetensors string escape");
        }
    }
};

std::uint64_t read_le_u64(const std::string& bytes) {
    if (bytes.size() < LCQI_SAFETENSORS_PREFIX_BYTES) {
        throw std::runtime_error("safetensors file is too small");
    }
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < LCQI_SAFETENSORS_PREFIX_BYTES; ++i) {
        value |= static_cast<std::uint64_t>(
                     static_cast<unsigned char>(bytes[i]))
                 << (LCQI_BYTE_BITS * i);
    }
    return value;
}

std::string read_file_bytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("cannot open safetensors file: " + path.string());
    }
    input.seekg(0, std::ios::end);
    const std::streamoff size = input.tellg();
    if (size < 0) {
        throw std::runtime_error("cannot determine safetensors file size");
    }
    input.seekg(0, std::ios::beg);
    std::string bytes(static_cast<std::size_t>(size), '\0');
    if (!bytes.empty() && !input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()))) {
        throw std::runtime_error("cannot read safetensors file");
    }
    return bytes;
}

void parse_metadata_value(HeaderCursor& cursor,
                          SafeTensorManifest& manifest,
                          const std::string& key) {
    if (key == "__metadata__") {
        cursor.expect('{');
        if (cursor.consume('}')) {
            return;
        }
        while (true) {
            const std::string metadata_key = cursor.parse_string();
            cursor.expect(':');
            const std::string metadata_value = cursor.parse_string();
            manifest.metadata.emplace_back(metadata_key, metadata_value);
            if (cursor.consume('}')) {
                return;
            }
            cursor.expect(',');
        }
    }
    throw std::runtime_error("unexpected safetensors metadata value");
}

SafeTensorEntry parse_tensor_entry(HeaderCursor& cursor, const std::string& name) {
    SafeTensorEntry entry;
    entry.name = name;
    bool saw_dtype = false;
    bool saw_shape = false;
    bool saw_offsets = false;

    cursor.expect('{');
    if (cursor.consume('}')) {
        throw std::runtime_error("empty safetensors tensor entry");
    }
    while (true) {
        const std::string field = cursor.parse_string();
        cursor.expect(':');
        if (field == "dtype") {
            entry.dtype = cursor.parse_string();
            saw_dtype = true;
        } else if (field == "shape") {
            entry.shape = cursor.parse_i64_array();
            saw_shape = true;
        } else if (field == "data_offsets") {
            const std::vector<std::uint64_t> offsets = cursor.parse_u64_array();
            if (offsets.size() != LCQI_SAFETENSORS_OFFSET_COUNT) {
                throw std::runtime_error("safetensors data_offsets must contain two values");
            }
            entry.data_begin = offsets[0];
            entry.data_end = offsets[1];
            saw_offsets = true;
        } else {
            throw std::runtime_error("unsupported safetensors tensor field: " + field);
        }
        if (cursor.consume('}')) {
            break;
        }
        cursor.expect(',');
    }

    if (!saw_dtype || !saw_shape || !saw_offsets) {
        throw std::runtime_error("safetensors tensor entry is missing required fields");
    }
    if (entry.data_end < entry.data_begin) {
        throw std::runtime_error("safetensors tensor offset range is invalid");
    }
    return entry;
}

void parse_header(std::string_view header, SafeTensorManifest& manifest) {
    HeaderCursor cursor(header);
    cursor.expect('{');
    if (cursor.consume('}')) {
        throw std::runtime_error("safetensors header is empty");
    }

    while (true) {
        const std::string key = cursor.parse_string();
        cursor.expect(':');
        if (key == "__metadata__") {
            parse_metadata_value(cursor, manifest, key);
        } else {
            manifest.tensors.push_back(parse_tensor_entry(cursor, key));
        }
        if (cursor.consume('}')) {
            break;
        }
        cursor.expect(',');
    }

    if (!cursor.eof_after_ws()) {
        throw std::runtime_error("trailing data after safetensors header");
    }
}

std::uint64_t expected_tensor_bytes(const SafeTensorEntry& entry);

void validate_manifest(const SafeTensorManifest& manifest,
                       std::uint64_t file_size) {
    const std::uint64_t payload_size = file_size - manifest.data_start_offset;
    for (std::size_t i = 0; i < manifest.tensors.size(); ++i) {
        const SafeTensorEntry& left = manifest.tensors[i];
        if (left.name.empty() || left.dtype.empty()) {
            throw std::runtime_error("safetensors tensor has empty name or dtype");
        }
        if (left.data_end > payload_size) {
            throw std::runtime_error("safetensors tensor data_offsets exceed payload size");
        }
        const std::uint64_t expected_bytes = expected_tensor_bytes(left);
        if (left.byte_size() != expected_bytes) {
            throw std::runtime_error("safetensors tensor byte size does not match dtype and shape");
        }
        for (const std::int64_t dim : left.shape) {
            if (dim < 0) {
                throw std::runtime_error("safetensors shape dimension is negative");
            }
        }
        for (std::size_t j = i + 1; j < manifest.tensors.size(); ++j) {
            const SafeTensorEntry& right = manifest.tensors[j];
            if (left.name == right.name) {
                throw std::runtime_error("duplicate safetensors tensor name");
            }
            const bool overlaps =
                left.data_begin < right.data_end && right.data_begin < left.data_end;
            if (overlaps) {
                throw std::runtime_error("overlapping safetensors tensor data range");
            }
        }
    }
}

std::uint64_t dtype_byte_size(std::string_view dtype) {
    if (dtype == "F64") {
        return LCQI_DTYPE_BYTES_F64;
    }
    if (dtype == "F32") {
        return LCQI_DTYPE_BYTES_F32;
    }
    if (dtype == "F16" || dtype == "BF16") {
        return LCQI_DTYPE_BYTES_F16;
    }
    if (dtype == "I64" || dtype == "U64") {
        return LCQI_DTYPE_BYTES_I64;
    }
    if (dtype == "I32" || dtype == "U32") {
        return LCQI_DTYPE_BYTES_I32;
    }
    if (dtype == "I16" || dtype == "U16") {
        return LCQI_DTYPE_BYTES_I16;
    }
    if (dtype == "I8") {
        return LCQI_DTYPE_BYTES_I8;
    }
    if (dtype == "U8" || dtype == "BOOL") {
        return LCQI_DTYPE_BYTES_U8;
    }
    throw std::runtime_error("unsupported safetensors dtype: " + std::string(dtype));
}

std::uint64_t expected_tensor_bytes(const SafeTensorEntry& entry) {
    std::uint64_t element_count = 1;
    for (const std::int64_t dim : entry.shape) {
        if (dim < 0) {
            throw std::runtime_error("safetensors shape dimension is negative");
        }
        const std::uint64_t extent = static_cast<std::uint64_t>(dim);
        if (extent != 0 &&
            element_count > std::numeric_limits<std::uint64_t>::max() / extent) {
            throw std::runtime_error("safetensors shape element count overflow");
        }
        element_count *= extent;
    }
    const std::uint64_t dtype_bytes = dtype_byte_size(entry.dtype);
    if (dtype_bytes != 0 &&
        element_count > std::numeric_limits<std::uint64_t>::max() / dtype_bytes) {
        throw std::runtime_error("safetensors tensor byte size overflow");
    }
    return element_count * dtype_bytes;
}

}  // namespace

std::uint64_t SafeTensorEntry::byte_size() const {
    return this->data_end - this->data_begin;
}

const SafeTensorEntry* SafeTensorManifest::find_tensor(std::string_view name) const {
    const auto found = std::find_if(
        this->tensors.begin(),
        this->tensors.end(),
        [name](const SafeTensorEntry& entry) {
            return entry.name == name;
        });
    if (found == this->tensors.end()) {
        return nullptr;
    }
    return &(*found);
}

SafeTensorManifest load_safetensors_manifest(const std::filesystem::path& path) {
    const std::string bytes = read_file_bytes(path);
    const std::uint64_t header_size = read_le_u64(bytes);
    if (header_size == 0 || header_size > LCQI_SAFETENSORS_MAX_HEADER_BYTES) {
        throw std::runtime_error("safetensors header size is invalid");
    }
    const std::uint64_t data_start_offset =
        static_cast<std::uint64_t>(LCQI_SAFETENSORS_PREFIX_BYTES) + header_size;
    if (data_start_offset > bytes.size()) {
        throw std::runtime_error("safetensors header extends beyond file size");
    }

    SafeTensorManifest manifest;
    manifest.header_size = header_size;
    manifest.data_start_offset = data_start_offset;
    const std::string_view header(
        bytes.data() + LCQI_SAFETENSORS_PREFIX_BYTES,
        static_cast<std::size_t>(header_size));
    parse_header(header, manifest);
    validate_manifest(manifest, static_cast<std::uint64_t>(bytes.size()));
    return manifest;
}

}  // namespace lcqi

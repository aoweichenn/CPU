#include <lcqi/gguf.hpp>

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace lcqi {
namespace {

constexpr std::uint32_t LCQI_GGUF_SUPPORTED_VERSION = 3;
constexpr std::uint64_t LCQI_GGUF_DEFAULT_ALIGNMENT = 32;
constexpr std::uint64_t LCQI_GGUF_MAX_STRING_BYTES = 16U * 1024U * 1024U;
constexpr std::uint64_t LCQI_GGUF_BYTE_BITS = 8;
constexpr std::uint64_t LCQI_GGUF_TYPE_U8_BYTES = 1;
constexpr std::uint64_t LCQI_GGUF_TYPE_U16_BYTES = 2;
constexpr std::uint64_t LCQI_GGUF_TYPE_U32_BYTES = 4;
constexpr std::uint64_t LCQI_GGUF_TYPE_U64_BYTES = 8;
constexpr std::int32_t LCQI_GGUF_TYPE_UINT8 = 0;
constexpr std::int32_t LCQI_GGUF_TYPE_INT8 = 1;
constexpr std::int32_t LCQI_GGUF_TYPE_UINT16 = 2;
constexpr std::int32_t LCQI_GGUF_TYPE_INT16 = 3;
constexpr std::int32_t LCQI_GGUF_TYPE_UINT32 = 4;
constexpr std::int32_t LCQI_GGUF_TYPE_INT32 = 5;
constexpr std::int32_t LCQI_GGUF_TYPE_FLOAT32 = 6;
constexpr std::int32_t LCQI_GGUF_TYPE_BOOL = 7;
constexpr std::int32_t LCQI_GGUF_TYPE_STRING = 8;
constexpr std::int32_t LCQI_GGUF_TYPE_ARRAY = 9;
constexpr std::int32_t LCQI_GGUF_TYPE_UINT64 = 10;
constexpr std::int32_t LCQI_GGUF_TYPE_INT64 = 11;
constexpr std::int32_t LCQI_GGUF_TYPE_FLOAT64 = 12;
constexpr std::uint32_t LCQI_GGUF_MAX_DIMS = 4;
constexpr std::int32_t LCQI_GGUF_TENSOR_TYPE_COUNT = 42;
constexpr const char* LCQI_GGUF_ALIGNMENT_KEY = "general.alignment";

class BinaryReader {
public:
    explicit BinaryReader(const std::filesystem::path& path) : input_(path, std::ios::binary) {
        if (!this->input_) {
            throw std::runtime_error("cannot open GGUF file: " + path.string());
        }
        this->input_.seekg(0, std::ios::end);
        const std::streamoff size = this->input_.tellg();
        if (size < 0) {
            throw std::runtime_error("cannot determine GGUF file size");
        }
        this->file_size_ = static_cast<std::uint64_t>(size);
        this->input_.seekg(0, std::ios::beg);
    }

    [[nodiscard]] std::uint64_t file_size() const {
        return this->file_size_;
    }

    [[nodiscard]] std::uint64_t offset() {
        const std::streamoff current = this->input_.tellg();
        if (current < 0) {
            throw std::runtime_error("cannot determine GGUF reader offset");
        }
        return static_cast<std::uint64_t>(current);
    }

    void seek(std::uint64_t offset) {
        if (offset > this->file_size_) {
            throw std::runtime_error("GGUF seek offset is beyond file size");
        }
        this->input_.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
        if (!this->input_) {
            throw std::runtime_error("cannot seek GGUF file");
        }
    }

    void skip(std::uint64_t bytes) {
        const std::uint64_t current = this->offset();
        if (bytes > this->file_size_ - current) {
            throw std::runtime_error("GGUF skip extends beyond file size");
        }
        this->seek(current + bytes);
    }

    [[nodiscard]] std::uint8_t read_u8() {
        char byte = 0;
        this->read_exact(&byte, LCQI_GGUF_TYPE_U8_BYTES);
        return static_cast<std::uint8_t>(byte);
    }

    [[nodiscard]] std::uint32_t read_u32() {
        std::array<std::uint8_t, LCQI_GGUF_TYPE_U32_BYTES> bytes{};
        this->read_bytes(bytes.data(), bytes.size());
        std::uint32_t value = 0;
        for (std::size_t index = 0; index < bytes.size(); ++index) {
            value |= static_cast<std::uint32_t>(bytes[index]) <<
                     (LCQI_GGUF_BYTE_BITS * index);
        }
        return value;
    }

    [[nodiscard]] std::int32_t read_i32() {
        const std::uint32_t value = this->read_u32();
        std::int32_t result = 0;
        std::memcpy(&result, &value, sizeof(result));
        return result;
    }

    [[nodiscard]] std::uint64_t read_u64() {
        std::array<std::uint8_t, LCQI_GGUF_TYPE_U64_BYTES> bytes{};
        this->read_bytes(bytes.data(), bytes.size());
        std::uint64_t value = 0;
        for (std::size_t index = 0; index < bytes.size(); ++index) {
            value |= static_cast<std::uint64_t>(bytes[index]) <<
                     (LCQI_GGUF_BYTE_BITS * index);
        }
        return value;
    }

    [[nodiscard]] std::int64_t read_i64() {
        const std::uint64_t value = this->read_u64();
        std::int64_t result = 0;
        std::memcpy(&result, &value, sizeof(result));
        return result;
    }

    [[nodiscard]] std::string read_string() {
        const std::uint64_t size = this->read_u64();
        if (size > LCQI_GGUF_MAX_STRING_BYTES) {
            throw std::runtime_error("GGUF string is too large");
        }
        if (size > this->file_size_ - this->offset()) {
            throw std::runtime_error("GGUF string extends beyond file size");
        }
        std::string text(static_cast<std::size_t>(size), '\0');
        if (!text.empty()) {
            this->read_exact(text.data(), text.size());
        }
        return text;
    }

private:
    std::ifstream input_;
    std::uint64_t file_size_ = 0;

    void read_bytes(std::uint8_t* output, std::size_t bytes) {
        this->read_exact(reinterpret_cast<char*>(output), bytes);
    }

    void read_exact(char* output, std::uint64_t bytes) {
        if (bytes > this->file_size_ - this->offset()) {
            throw std::runtime_error("GGUF read extends beyond file size");
        }
        if (bytes == 0) {
            return;
        }
        this->input_.read(output, static_cast<std::streamsize>(bytes));
        if (!this->input_) {
            throw std::runtime_error("cannot read GGUF file");
        }
    }
};

std::uint64_t checked_product(std::uint64_t lhs, std::uint64_t rhs, const char* name) {
    if (rhs != 0 && lhs > std::numeric_limits<std::uint64_t>::max() / rhs) {
        throw std::runtime_error(std::string(name) + " overflow");
    }
    return lhs * rhs;
}

std::uint64_t checked_add(std::uint64_t lhs, std::uint64_t rhs, const char* name) {
    if (lhs > std::numeric_limits<std::uint64_t>::max() - rhs) {
        throw std::runtime_error(std::string(name) + " overflow");
    }
    return lhs + rhs;
}

std::uint64_t align_up(std::uint64_t value, std::uint64_t alignment) {
    if (alignment == 0 || (alignment & (alignment - 1U)) != 0U) {
        throw std::runtime_error("GGUF alignment must be a non-zero power of two");
    }
    const std::uint64_t mask = alignment - 1U;
    return checked_add(value, mask, "GGUF alignment") & ~mask;
}

std::string gguf_value_type_name(std::int32_t type) {
    switch (type) {
        case LCQI_GGUF_TYPE_UINT8:
            return "UINT8";
        case LCQI_GGUF_TYPE_INT8:
            return "INT8";
        case LCQI_GGUF_TYPE_UINT16:
            return "UINT16";
        case LCQI_GGUF_TYPE_INT16:
            return "INT16";
        case LCQI_GGUF_TYPE_UINT32:
            return "UINT32";
        case LCQI_GGUF_TYPE_INT32:
            return "INT32";
        case LCQI_GGUF_TYPE_FLOAT32:
            return "FLOAT32";
        case LCQI_GGUF_TYPE_BOOL:
            return "BOOL";
        case LCQI_GGUF_TYPE_STRING:
            return "STRING";
        case LCQI_GGUF_TYPE_ARRAY:
            return "ARRAY";
        case LCQI_GGUF_TYPE_UINT64:
            return "UINT64";
        case LCQI_GGUF_TYPE_INT64:
            return "INT64";
        case LCQI_GGUF_TYPE_FLOAT64:
            return "FLOAT64";
        default:
            throw std::runtime_error("unsupported GGUF metadata value type");
    }
}

std::uint64_t gguf_scalar_type_size(std::int32_t type) {
    switch (type) {
        case LCQI_GGUF_TYPE_UINT8:
        case LCQI_GGUF_TYPE_INT8:
        case LCQI_GGUF_TYPE_BOOL:
            return LCQI_GGUF_TYPE_U8_BYTES;
        case LCQI_GGUF_TYPE_UINT16:
        case LCQI_GGUF_TYPE_INT16:
            return LCQI_GGUF_TYPE_U16_BYTES;
        case LCQI_GGUF_TYPE_UINT32:
        case LCQI_GGUF_TYPE_INT32:
        case LCQI_GGUF_TYPE_FLOAT32:
            return LCQI_GGUF_TYPE_U32_BYTES;
        case LCQI_GGUF_TYPE_UINT64:
        case LCQI_GGUF_TYPE_INT64:
        case LCQI_GGUF_TYPE_FLOAT64:
            return LCQI_GGUF_TYPE_U64_BYTES;
        default:
            throw std::runtime_error("GGUF metadata scalar type is variable-sized or invalid");
    }
}

std::string read_scalar_preview(BinaryReader& reader,
                                std::int32_t type,
                                std::string_view key,
                                GgufManifest& manifest) {
    switch (type) {
        case LCQI_GGUF_TYPE_UINT8:
            return std::to_string(reader.read_u8());
        case LCQI_GGUF_TYPE_INT8:
            return std::to_string(static_cast<std::int8_t>(reader.read_u8()));
        case LCQI_GGUF_TYPE_UINT16: {
            const std::uint32_t value = reader.read_u8() |
                                        (static_cast<std::uint32_t>(reader.read_u8())
                                         << LCQI_GGUF_BYTE_BITS);
            return std::to_string(value);
        }
        case LCQI_GGUF_TYPE_INT16:
            reader.skip(LCQI_GGUF_TYPE_U16_BYTES);
            return "<int16>";
        case LCQI_GGUF_TYPE_UINT32: {
            const std::uint32_t value = reader.read_u32();
            if (key == LCQI_GGUF_ALIGNMENT_KEY) {
                manifest.alignment = value;
            }
            return std::to_string(value);
        }
        case LCQI_GGUF_TYPE_INT32:
            return std::to_string(reader.read_i32());
        case LCQI_GGUF_TYPE_UINT64:
            return std::to_string(reader.read_u64());
        case LCQI_GGUF_TYPE_INT64:
            return std::to_string(reader.read_i64());
        case LCQI_GGUF_TYPE_STRING:
            return reader.read_string();
        case LCQI_GGUF_TYPE_FLOAT32:
            reader.skip(LCQI_GGUF_TYPE_U32_BYTES);
            return "<float32>";
        case LCQI_GGUF_TYPE_FLOAT64:
            reader.skip(LCQI_GGUF_TYPE_U64_BYTES);
            return "<float64>";
        case LCQI_GGUF_TYPE_BOOL:
            return reader.read_u8() == 0U ? "false" : "true";
        default:
            throw std::runtime_error("unsupported GGUF metadata scalar type");
    }
}

void skip_array_values(BinaryReader& reader, std::int32_t element_type, std::uint64_t count) {
    if (element_type == LCQI_GGUF_TYPE_STRING) {
        for (std::uint64_t index = 0; index < count; ++index) {
            static_cast<void>(reader.read_string());
        }
        return;
    }
    const std::uint64_t element_size = gguf_scalar_type_size(element_type);
    reader.skip(checked_product(element_size, count, "GGUF metadata array byte size"));
}

void read_metadata(BinaryReader& reader, GgufManifest& manifest) {
    for (std::uint64_t index = 0; index < manifest.metadata_count; ++index) {
        GgufMetadataEntry entry;
        entry.key = reader.read_string();
        const std::int32_t type = reader.read_i32();
        entry.type = gguf_value_type_name(type);
        if (type == LCQI_GGUF_TYPE_ARRAY) {
            const std::int32_t element_type = reader.read_i32();
            const std::uint64_t count = reader.read_u64();
            entry.type = "ARRAY[" + gguf_value_type_name(element_type) + "]";
            entry.value_preview = "count=" + std::to_string(count);
            skip_array_values(reader, element_type, count);
        } else {
            entry.value_preview = read_scalar_preview(reader, type, entry.key, manifest);
        }
        manifest.metadata.push_back(std::move(entry));
    }
}

GgmlType read_tensor_type(BinaryReader& reader) {
    const std::int32_t raw_type = reader.read_i32();
    if (raw_type < 0 || raw_type >= LCQI_GGUF_TENSOR_TYPE_COUNT) {
        throw std::runtime_error("GGUF tensor type is outside the known ggml enum range");
    }
    return static_cast<GgmlType>(raw_type);
}

void validate_tensor(const GgufTensorInfo& tensor, std::uint64_t file_size) {
    if (tensor.name.empty()) {
        throw std::runtime_error("GGUF tensor name is empty");
    }
    if (tensor.shape.empty()) {
        throw std::runtime_error("GGUF tensor rank is zero");
    }
    const std::uint64_t end =
        checked_add(tensor.absolute_offset, tensor.byte_size, "GGUF tensor end offset");
    if (end > file_size) {
        throw std::runtime_error("GGUF tensor bytes extend beyond file size: " + tensor.name);
    }
}

void read_tensor_infos(BinaryReader& reader, GgufManifest& manifest) {
    manifest.tensors.reserve(static_cast<std::size_t>(manifest.tensor_count));
    for (std::uint64_t index = 0; index < manifest.tensor_count; ++index) {
        GgufTensorInfo tensor;
        tensor.name = reader.read_string();
        const std::uint32_t rank = reader.read_u32();
        if (rank == 0 || rank > LCQI_GGUF_MAX_DIMS) {
            throw std::runtime_error("GGUF tensor rank is invalid: " + tensor.name);
        }
        tensor.shape.reserve(rank);
        for (std::uint32_t dim = 0; dim < rank; ++dim) {
            const std::int64_t extent = reader.read_i64();
            if (extent < 0) {
                throw std::runtime_error("GGUF tensor dimension is negative: " + tensor.name);
            }
            tensor.shape.push_back(extent);
        }
        tensor.type = read_tensor_type(reader);
        tensor.relative_offset = reader.read_u64();
        tensor.byte_size = ggml_tensor_byte_size(tensor.type, tensor.shape);
        manifest.tensors.push_back(std::move(tensor));
    }
}

void fill_absolute_offsets(GgufManifest& manifest, std::uint64_t file_size) {
    for (GgufTensorInfo& tensor : manifest.tensors) {
        tensor.absolute_offset =
            checked_add(manifest.data_offset, tensor.relative_offset, "GGUF tensor offset");
        validate_tensor(tensor, file_size);
    }
}

}  // namespace

std::uint64_t GgufTensorInfo::element_count() const {
    std::uint64_t count = 1;
    for (const std::int64_t extent : this->shape) {
        if (extent < 0) {
            throw std::runtime_error("GGUF tensor shape contains a negative dimension");
        }
        count = checked_product(count,
                                static_cast<std::uint64_t>(extent),
                                "GGUF tensor element count");
    }
    return count;
}

const GgufTensorInfo* GgufManifest::find_tensor(std::string_view name) const {
    const auto found = std::find_if(
        this->tensors.begin(),
        this->tensors.end(),
        [name](const GgufTensorInfo& tensor) {
            return tensor.name == name;
        });
    if (found == this->tensors.end()) {
        return nullptr;
    }
    return &(*found);
}

GgmlTypeLayout ggml_type_layout(GgmlType type) {
    switch (type) {
        case GgmlType::f32:
            return {1, 4, "F32", false};
        case GgmlType::f16:
            return {1, 2, "F16", false};
        case GgmlType::q4_0:
            return {32, 18, "Q4_0", true};
        case GgmlType::q4_1:
            return {32, 20, "Q4_1", true};
        case GgmlType::q5_0:
            return {32, 22, "Q5_0", true};
        case GgmlType::q5_1:
            return {32, 24, "Q5_1", true};
        case GgmlType::q8_0:
            return {32, 34, "Q8_0", true};
        case GgmlType::q8_1:
            return {32, 36, "Q8_1", true};
        case GgmlType::q2_k:
            return {256, 84, "Q2_K", true};
        case GgmlType::q3_k:
            return {256, 110, "Q3_K", true};
        case GgmlType::q4_k:
            return {256, 144, "Q4_K", true};
        case GgmlType::q5_k:
            return {256, 176, "Q5_K", true};
        case GgmlType::q6_k:
            return {256, 210, "Q6_K", true};
        case GgmlType::q8_k:
            return {256, 292, "Q8_K", true};
        case GgmlType::i8:
            return {1, 1, "I8", false};
        case GgmlType::i16:
            return {1, 2, "I16", false};
        case GgmlType::i32:
            return {1, 4, "I32", false};
        case GgmlType::i64:
            return {1, 8, "I64", false};
        case GgmlType::f64:
            return {1, 8, "F64", false};
        case GgmlType::bf16:
            return {1, 2, "BF16", false};
        case GgmlType::q1_0:
            return {128, 18, "Q1_0", true};
        default:
            throw std::runtime_error("unsupported GGML tensor type in LCQI GGUF parser");
    }
}

const char* ggml_type_name(GgmlType type) {
    return ggml_type_layout(type).name;
}

std::uint64_t ggml_tensor_byte_size(GgmlType type, std::span<const std::int64_t> shape) {
    if (shape.empty()) {
        throw std::runtime_error("GGUF tensor shape is empty");
    }
    const GgmlTypeLayout layout = ggml_type_layout(type);
    if (shape[0] % layout.block_size != 0) {
        throw std::runtime_error("GGUF tensor row width is not divisible by ggml block size");
    }
    std::uint64_t elements = 1;
    for (const std::int64_t extent : shape) {
        if (extent < 0) {
            throw std::runtime_error("GGUF tensor shape contains a negative dimension");
        }
        elements = checked_product(elements,
                                   static_cast<std::uint64_t>(extent),
                                   "GGUF tensor element count");
    }
    const std::uint64_t blocks =
        elements / static_cast<std::uint64_t>(layout.block_size);
    return checked_product(blocks, layout.type_size, "GGUF tensor byte size");
}

GgufManifest load_gguf_manifest(const std::filesystem::path& path) {
    BinaryReader reader(path);
    std::array<char, 4> magic{};
    for (char& byte : magic) {
        byte = static_cast<char>(reader.read_u8());
    }
    if (magic != std::array<char, 4>{'G', 'G', 'U', 'F'}) {
        throw std::runtime_error("file is not a GGUF file: " + path.string());
    }

    GgufManifest manifest;
    manifest.version = reader.read_u32();
    if (manifest.version == 0 || manifest.version > LCQI_GGUF_SUPPORTED_VERSION) {
        throw std::runtime_error("unsupported GGUF version");
    }
    const std::int64_t tensor_count = reader.read_i64();
    const std::int64_t metadata_count = reader.read_i64();
    if (tensor_count < 0 || metadata_count < 0) {
        throw std::runtime_error("GGUF tensor or metadata count is negative");
    }
    manifest.tensor_count = static_cast<std::uint64_t>(tensor_count);
    manifest.metadata_count = static_cast<std::uint64_t>(metadata_count);
    manifest.alignment = LCQI_GGUF_DEFAULT_ALIGNMENT;

    read_metadata(reader, manifest);
    read_tensor_infos(reader, manifest);
    manifest.data_offset = align_up(reader.offset(), manifest.alignment);
    reader.seek(manifest.data_offset);
    fill_absolute_offsets(manifest, reader.file_size());
    return manifest;
}

std::vector<std::uint8_t> read_gguf_tensor_bytes(const std::filesystem::path& path,
                                                 const GgufTensorInfo& tensor) {
    if (tensor.byte_size >
        static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        throw std::runtime_error("GGUF tensor is too large to read into memory: " +
                                 tensor.name);
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("cannot open GGUF file: " + path.string());
    }
    input.seekg(static_cast<std::streamoff>(tensor.absolute_offset), std::ios::beg);
    if (!input) {
        throw std::runtime_error("cannot seek GGUF tensor: " + tensor.name);
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(tensor.byte_size), 0);
    if (!bytes.empty() &&
        !input.read(reinterpret_cast<char*>(bytes.data()),
                    static_cast<std::streamsize>(bytes.size()))) {
        throw std::runtime_error("cannot read GGUF tensor: " + tensor.name);
    }
    return bytes;
}

}  // namespace lcqi

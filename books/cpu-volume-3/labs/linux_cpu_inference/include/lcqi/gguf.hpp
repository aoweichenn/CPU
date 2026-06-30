#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace lcqi {

enum class GgmlType : std::int32_t {
    f32 = 0,
    f16 = 1,
    q4_0 = 2,
    q4_1 = 3,
    q5_0 = 6,
    q5_1 = 7,
    q8_0 = 8,
    q8_1 = 9,
    q2_k = 10,
    q3_k = 11,
    q4_k = 12,
    q5_k = 13,
    q6_k = 14,
    q8_k = 15,
    i8 = 24,
    i16 = 25,
    i32 = 26,
    i64 = 27,
    f64 = 28,
    bf16 = 30,
    q1_0 = 41,
};

struct GgmlTypeLayout {
    std::int64_t block_size = 1;
    std::uint64_t type_size = 0;
    const char* name = "";
    bool quantized = false;
};

struct GgufMetadataEntry {
    std::string key;
    std::string type;
    std::string value_preview;
    std::vector<std::string> string_values;
};

struct GgufTensorInfo {
    std::string name;
    std::vector<std::int64_t> shape;
    GgmlType type = GgmlType::f32;
    std::uint64_t relative_offset = 0;
    std::uint64_t absolute_offset = 0;
    std::uint64_t byte_size = 0;

    [[nodiscard]] std::uint64_t element_count() const;
};

struct GgufManifest {
    std::uint32_t version = 0;
    std::uint64_t tensor_count = 0;
    std::uint64_t metadata_count = 0;
    std::uint64_t alignment = 32;
    std::uint64_t data_offset = 0;
    std::vector<GgufMetadataEntry> metadata;
    std::vector<GgufTensorInfo> tensors;

    [[nodiscard]] const GgufTensorInfo* find_tensor(std::string_view name) const;
    [[nodiscard]] const GgufMetadataEntry* find_metadata(std::string_view key) const;
};

[[nodiscard]] GgmlTypeLayout ggml_type_layout(GgmlType type);

[[nodiscard]] const char* ggml_type_name(GgmlType type);

[[nodiscard]] std::uint64_t ggml_tensor_byte_size(GgmlType type,
                                                  std::span<const std::int64_t> shape);

[[nodiscard]] GgufManifest load_gguf_manifest(const std::filesystem::path& path);

[[nodiscard]] std::vector<std::uint8_t> read_gguf_tensor_bytes(
    const std::filesystem::path& path,
    const GgufTensorInfo& tensor);

}  // namespace lcqi

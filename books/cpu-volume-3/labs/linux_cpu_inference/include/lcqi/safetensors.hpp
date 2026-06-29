#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lcqi {

struct SafeTensorEntry {
    std::string name;
    std::string dtype;
    std::vector<std::int64_t> shape;
    std::uint64_t data_begin = 0;
    std::uint64_t data_end = 0;

    [[nodiscard]] std::uint64_t byte_size() const;
};

struct SafeTensorManifest {
    std::uint64_t header_size = 0;
    std::uint64_t data_start_offset = 0;
    std::vector<std::pair<std::string, std::string>> metadata;
    std::vector<SafeTensorEntry> tensors;

    [[nodiscard]] const SafeTensorEntry* find_tensor(std::string_view name) const;
};

struct SafeTensorFile {
    std::filesystem::path path;
    SafeTensorManifest manifest;
    std::vector<std::uint8_t> bytes;
};

[[nodiscard]] SafeTensorManifest load_safetensors_manifest(
    const std::filesystem::path& path);

[[nodiscard]] SafeTensorFile load_safetensors_file(const std::filesystem::path& path);

[[nodiscard]] std::vector<float> read_safetensor_f32_tensor(
    const SafeTensorFile& file,
    std::string_view name);

[[nodiscard]] std::span<const std::uint8_t> read_safetensor_tensor_bytes(
    const SafeTensorFile& file,
    std::string_view name);

}  // namespace lcqi

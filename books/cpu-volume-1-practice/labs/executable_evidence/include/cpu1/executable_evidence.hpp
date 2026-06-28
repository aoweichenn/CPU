#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace cpu1 {

struct InspectionConfig {
    bool include_symbols{true};
    std::uint64_t max_symbols{128};
};

struct SectionSummary {
    std::string name;
    std::uint32_t type{};
    std::uint64_t flags{};
    std::uint64_t address{};
    std::uint64_t offset{};
    std::uint64_t size{};
};

struct SymbolSummary {
    std::string name;
    std::string section_name;
    std::uint8_t type{};
    std::uint8_t binding{};
    std::uint16_t section_index{};
    std::uint64_t value{};
    std::uint64_t size{};
};

struct ElfReport {
    std::string source_name;
    bool valid_magic{};
    bool is_elf64{};
    bool is_little_endian{};
    std::uint16_t object_type{};
    std::uint16_t machine{};
    std::uint64_t entry_address{};
    std::uint64_t checksum{};
    std::uint64_t byte_size{};
    std::vector<SectionSummary> sections;
    std::vector<SymbolSummary> symbols;
    std::vector<std::string> diagnostics;
};

std::vector<std::uint8_t> read_binary_file(const std::filesystem::path& path);
std::uint64_t checksum_bytes(std::span<const std::uint8_t> bytes);
std::string classify_machine(std::uint16_t machine);
ElfReport inspect_elf(
    std::span<const std::uint8_t> bytes,
    std::string_view source_name,
    const InspectionConfig& config = {});
std::string format_report(const ElfReport& report);
std::string format_sections_csv(const ElfReport& report);

}  // namespace cpu1

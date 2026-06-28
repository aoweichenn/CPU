#include <cpu1/executable_evidence.hpp>

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace cpu1 {
namespace {

constexpr std::uint8_t ELF_MAGIC_0 = 0x7F;
constexpr std::uint8_t ELF_MAGIC_1 = 'E';
constexpr std::uint8_t ELF_MAGIC_2 = 'L';
constexpr std::uint8_t ELF_MAGIC_3 = 'F';
constexpr std::size_t ELF_IDENT_SIZE = 16;
constexpr std::size_t ELF_IDENT_CLASS_OFFSET = 4;
constexpr std::size_t ELF_IDENT_DATA_OFFSET = 5;
constexpr std::uint8_t ELF_CLASS_64 = 2;
constexpr std::uint8_t ELF_DATA_LITTLE_ENDIAN = 1;

constexpr std::size_t ELF64_HEADER_MIN_SIZE = 64;
constexpr std::size_t ELF64_TYPE_OFFSET = 16;
constexpr std::size_t ELF64_MACHINE_OFFSET = 18;
constexpr std::size_t ELF64_ENTRY_OFFSET = 24;
constexpr std::size_t ELF64_SECTION_HEADER_OFFSET = 40;
constexpr std::size_t ELF64_HEADER_SECTION_ENTRY_SIZE_OFFSET = 58;
constexpr std::size_t ELF64_SECTION_COUNT_OFFSET = 60;
constexpr std::size_t ELF64_SECTION_NAME_INDEX_OFFSET = 62;

constexpr std::size_t ELF64_SECTION_NAME_OFFSET = 0;
constexpr std::size_t ELF64_SECTION_TYPE_OFFSET = 4;
constexpr std::size_t ELF64_SECTION_FLAGS_OFFSET = 8;
constexpr std::size_t ELF64_SECTION_ADDRESS_OFFSET = 16;
constexpr std::size_t ELF64_SECTION_FILE_OFFSET = 24;
constexpr std::size_t ELF64_SECTION_SIZE_OFFSET = 32;
constexpr std::size_t ELF64_SECTION_LINK_OFFSET = 40;
constexpr std::size_t ELF64_SECTION_ENTRY_SIZE_OFFSET = 56;
constexpr std::size_t ELF64_SECTION_HEADER_SIZE = 64;

constexpr std::size_t ELF64_SYMBOL_NAME_OFFSET = 0;
constexpr std::size_t ELF64_SYMBOL_INFO_OFFSET = 4;
constexpr std::size_t ELF64_SYMBOL_SECTION_INDEX_OFFSET = 6;
constexpr std::size_t ELF64_SYMBOL_VALUE_OFFSET = 8;
constexpr std::size_t ELF64_SYMBOL_SIZE_OFFSET = 16;
constexpr std::size_t ELF64_SYMBOL_ENTRY_SIZE = 24;

constexpr std::uint32_t ELF_SECTION_TYPE_SYMTAB = 2;
constexpr std::uint32_t ELF_SECTION_TYPE_DYNSYM = 11;
constexpr std::uint16_t ELF_MACHINE_X86_64 = 62;
constexpr std::uint16_t ELF_MACHINE_AARCH64 = 183;
constexpr std::uint16_t ELF_MACHINE_RISCV = 243;

constexpr std::uint64_t FNV_OFFSET_BASIS = 14695981039346656037ULL;
constexpr std::uint64_t FNV_PRIME = 1099511628211ULL;
constexpr std::uint64_t HEX_FIELD_WIDTH_64 = 16;

bool has_range(std::span<const std::uint8_t> bytes, std::uint64_t offset, std::uint64_t size)
{
    const std::uint64_t byte_count = static_cast<std::uint64_t>(bytes.size());
    return offset <= byte_count && size <= byte_count - offset;
}

std::uint8_t read_u8(std::span<const std::uint8_t> bytes, std::uint64_t offset)
{
    if (!has_range(bytes, offset, sizeof(std::uint8_t))) {
        throw std::out_of_range("read_u8 outside input");
    }
    return bytes[static_cast<std::size_t>(offset)];
}

std::uint16_t read_u16_le(std::span<const std::uint8_t> bytes, std::uint64_t offset)
{
    if (!has_range(bytes, offset, sizeof(std::uint16_t))) {
        throw std::out_of_range("read_u16_le outside input");
    }
    const std::uint16_t low = bytes[static_cast<std::size_t>(offset)];
    const std::uint16_t high = bytes[static_cast<std::size_t>(offset + 1)];
    return static_cast<std::uint16_t>(low | static_cast<std::uint16_t>(high << 8U));
}

std::uint32_t read_u32_le(std::span<const std::uint8_t> bytes, std::uint64_t offset)
{
    if (!has_range(bytes, offset, sizeof(std::uint32_t))) {
        throw std::out_of_range("read_u32_le outside input");
    }
    std::uint32_t value = 0;
    constexpr std::uint32_t BYTE_BITS = 8;
    for (std::uint32_t i = 0; i < sizeof(std::uint32_t); ++i) {
        value |= static_cast<std::uint32_t>(bytes[static_cast<std::size_t>(offset + i)])
                 << (i * BYTE_BITS);
    }
    return value;
}

std::uint64_t read_u64_le(std::span<const std::uint8_t> bytes, std::uint64_t offset)
{
    if (!has_range(bytes, offset, sizeof(std::uint64_t))) {
        throw std::out_of_range("read_u64_le outside input");
    }
    std::uint64_t value = 0;
    constexpr std::uint64_t BYTE_BITS = 8;
    for (std::uint64_t i = 0; i < sizeof(std::uint64_t); ++i) {
        value |= static_cast<std::uint64_t>(bytes[static_cast<std::size_t>(offset + i)])
                 << (i * BYTE_BITS);
    }
    return value;
}

void write_hex64(std::ostream& output, std::uint64_t value)
{
    output << "0x" << std::hex << std::setfill('0') << std::setw(HEX_FIELD_WIDTH_64) << value
           << std::dec << std::setfill(' ');
}

std::string read_string(std::span<const std::uint8_t> table, std::uint32_t offset)
{
    if (offset >= table.size()) {
        return {};
    }

    std::string value;
    for (std::size_t i = offset; i < table.size() && table[i] != 0; ++i) {
        value.push_back(static_cast<char>(table[i]));
    }
    return value;
}

std::span<const std::uint8_t> slice_bytes(
    std::span<const std::uint8_t> bytes,
    std::uint64_t offset,
    std::uint64_t size)
{
    if (!has_range(bytes, offset, size)) {
        return {};
    }
    return bytes.subspan(static_cast<std::size_t>(offset), static_cast<std::size_t>(size));
}

struct RawSection {
    std::uint32_t name_offset{};
    std::uint32_t type{};
    std::uint64_t flags{};
    std::uint64_t address{};
    std::uint64_t offset{};
    std::uint64_t size{};
    std::uint32_t link{};
    std::uint64_t entry_size{};
};

RawSection read_raw_section(std::span<const std::uint8_t> bytes, std::uint64_t section_offset)
{
    return RawSection{
        .name_offset = read_u32_le(bytes, section_offset + ELF64_SECTION_NAME_OFFSET),
        .type = read_u32_le(bytes, section_offset + ELF64_SECTION_TYPE_OFFSET),
        .flags = read_u64_le(bytes, section_offset + ELF64_SECTION_FLAGS_OFFSET),
        .address = read_u64_le(bytes, section_offset + ELF64_SECTION_ADDRESS_OFFSET),
        .offset = read_u64_le(bytes, section_offset + ELF64_SECTION_FILE_OFFSET),
        .size = read_u64_le(bytes, section_offset + ELF64_SECTION_SIZE_OFFSET),
        .link = read_u32_le(bytes, section_offset + ELF64_SECTION_LINK_OFFSET),
        .entry_size = read_u64_le(bytes, section_offset + ELF64_SECTION_ENTRY_SIZE_OFFSET),
    };
}

std::vector<RawSection> read_raw_sections(
    std::span<const std::uint8_t> bytes,
    std::uint64_t section_header_offset,
    std::uint16_t section_count,
    std::uint16_t section_entry_size,
    std::vector<std::string>& diagnostics)
{
    std::vector<RawSection> sections;
    if (section_entry_size < ELF64_SECTION_HEADER_SIZE) {
        diagnostics.push_back("section entry size is smaller than ELF64 section header");
        return sections;
    }

    sections.reserve(section_count);
    for (std::uint16_t index = 0; index < section_count; ++index) {
        const std::uint64_t current_offset =
            section_header_offset + static_cast<std::uint64_t>(index) * section_entry_size;
        if (!has_range(bytes, current_offset, ELF64_SECTION_HEADER_SIZE)) {
            diagnostics.push_back("section header table extends beyond input");
            break;
        }
        sections.push_back(read_raw_section(bytes, current_offset));
    }
    return sections;
}

std::vector<SectionSummary> summarize_sections(
    std::span<const std::uint8_t> bytes,
    const std::vector<RawSection>& raw_sections,
    std::uint16_t section_name_index,
    std::vector<std::string>& diagnostics)
{
    if (section_name_index >= raw_sections.size()) {
        diagnostics.push_back("section-name string table index is out of range");
        return {};
    }

    const RawSection& name_section = raw_sections[section_name_index];
    const std::span<const std::uint8_t> names =
        slice_bytes(bytes, name_section.offset, name_section.size);
    if (names.empty() && name_section.size != 0) {
        diagnostics.push_back("section-name string table extends beyond input");
        return {};
    }

    std::vector<SectionSummary> summaries;
    summaries.reserve(raw_sections.size());
    for (const RawSection& raw : raw_sections) {
        summaries.push_back(SectionSummary{
            .name = read_string(names, raw.name_offset),
            .type = raw.type,
            .flags = raw.flags,
            .address = raw.address,
            .offset = raw.offset,
            .size = raw.size,
        });
    }
    return summaries;
}

std::vector<SymbolSummary> summarize_symbols(
    std::span<const std::uint8_t> bytes,
    const std::vector<RawSection>& raw_sections,
    const std::vector<SectionSummary>& sections,
    const InspectionConfig& config,
    std::vector<std::string>& diagnostics)
{
    std::vector<SymbolSummary> symbols;
    if (!config.include_symbols) {
        return symbols;
    }

    for (std::size_t section_index = 0; section_index < raw_sections.size(); ++section_index) {
        const RawSection& symbol_section = raw_sections[section_index];
        if (symbol_section.type != ELF_SECTION_TYPE_SYMTAB &&
            symbol_section.type != ELF_SECTION_TYPE_DYNSYM) {
            continue;
        }
        if (symbol_section.entry_size < ELF64_SYMBOL_ENTRY_SIZE) {
            diagnostics.push_back("symbol entry size is smaller than ELF64 symbol entry");
            continue;
        }
        if (symbol_section.link >= raw_sections.size()) {
            diagnostics.push_back("symbol string table link is out of range");
            continue;
        }

        const RawSection& string_section = raw_sections[symbol_section.link];
        const std::span<const std::uint8_t> symbol_names =
            slice_bytes(bytes, string_section.offset, string_section.size);
        const std::uint64_t symbol_count = symbol_section.size / symbol_section.entry_size;
        const std::uint64_t capped_count = std::min(symbol_count, config.max_symbols);

        for (std::uint64_t symbol_index = 0; symbol_index < capped_count; ++symbol_index) {
            const std::uint64_t symbol_offset =
                symbol_section.offset + symbol_index * symbol_section.entry_size;
            if (!has_range(bytes, symbol_offset, ELF64_SYMBOL_ENTRY_SIZE)) {
                diagnostics.push_back("symbol table extends beyond input");
                break;
            }

            const std::uint32_t name_offset =
                read_u32_le(bytes, symbol_offset + ELF64_SYMBOL_NAME_OFFSET);
            const std::uint8_t info = read_u8(bytes, symbol_offset + ELF64_SYMBOL_INFO_OFFSET);
            const std::uint16_t symbol_section_index =
                read_u16_le(bytes, symbol_offset + ELF64_SYMBOL_SECTION_INDEX_OFFSET);
            const std::string section_name =
                symbol_section_index < sections.size() ? sections[symbol_section_index].name : "";

            symbols.push_back(SymbolSummary{
                .name = read_string(symbol_names, name_offset),
                .section_name = section_name,
                .type = static_cast<std::uint8_t>(info & 0x0FU),
                .binding = static_cast<std::uint8_t>(info >> 4U),
                .section_index = symbol_section_index,
                .value = read_u64_le(bytes, symbol_offset + ELF64_SYMBOL_VALUE_OFFSET),
                .size = read_u64_le(bytes, symbol_offset + ELF64_SYMBOL_SIZE_OFFSET),
            });
        }

        if (symbol_count > config.max_symbols) {
            diagnostics.push_back("symbol table truncated by max_symbols");
        }
    }

    return symbols;
}

bool has_elf_magic(std::span<const std::uint8_t> bytes)
{
    return bytes.size() >= ELF_IDENT_SIZE && bytes[0] == ELF_MAGIC_0 && bytes[1] == ELF_MAGIC_1 &&
           bytes[2] == ELF_MAGIC_2 && bytes[3] == ELF_MAGIC_3;
}

}  // namespace

std::vector<std::uint8_t> read_binary_file(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("failed to open binary file: " + path.string());
    }

    input.seekg(0, std::ios::end);
    const std::streamoff size = input.tellg();
    if (size < 0) {
        throw std::runtime_error("failed to determine binary file size: " + path.string());
    }
    input.seekg(0, std::ios::beg);

    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    if (!bytes.empty()) {
        input.read(reinterpret_cast<char*>(bytes.data()), size);
    }
    if (!input && !input.eof()) {
        throw std::runtime_error("failed to read binary file: " + path.string());
    }
    return bytes;
}

std::uint64_t checksum_bytes(std::span<const std::uint8_t> bytes)
{
    std::uint64_t value = FNV_OFFSET_BASIS;
    for (const std::uint8_t byte : bytes) {
        value ^= byte;
        value *= FNV_PRIME;
    }
    return value;
}

std::string classify_machine(std::uint16_t machine)
{
    switch (machine) {
    case ELF_MACHINE_X86_64:
        return "x86-64";
    case ELF_MACHINE_AARCH64:
        return "AArch64";
    case ELF_MACHINE_RISCV:
        return "RISC-V";
    default:
        return "unknown";
    }
}

ElfReport inspect_elf(
    std::span<const std::uint8_t> bytes,
    std::string_view source_name,
    const InspectionConfig& config)
{
    ElfReport report;
    report.source_name = std::string(source_name);
    report.byte_size = bytes.size();
    report.checksum = checksum_bytes(bytes);
    report.valid_magic = has_elf_magic(bytes);

    if (!report.valid_magic) {
        report.diagnostics.push_back("input does not start with ELF magic");
        return report;
    }
    if (bytes.size() < ELF64_HEADER_MIN_SIZE) {
        report.diagnostics.push_back("input is shorter than ELF64 header");
        return report;
    }

    report.is_elf64 = read_u8(bytes, ELF_IDENT_CLASS_OFFSET) == ELF_CLASS_64;
    report.is_little_endian = read_u8(bytes, ELF_IDENT_DATA_OFFSET) == ELF_DATA_LITTLE_ENDIAN;
    if (!report.is_elf64 || !report.is_little_endian) {
        report.diagnostics.push_back("only ELF64 little-endian inputs are supported");
        return report;
    }

    report.object_type = read_u16_le(bytes, ELF64_TYPE_OFFSET);
    report.machine = read_u16_le(bytes, ELF64_MACHINE_OFFSET);
    report.entry_address = read_u64_le(bytes, ELF64_ENTRY_OFFSET);

    const std::uint64_t section_header_offset = read_u64_le(bytes, ELF64_SECTION_HEADER_OFFSET);
    const std::uint16_t section_entry_size =
        read_u16_le(bytes, ELF64_HEADER_SECTION_ENTRY_SIZE_OFFSET);
    const std::uint16_t section_count = read_u16_le(bytes, ELF64_SECTION_COUNT_OFFSET);
    const std::uint16_t section_name_index = read_u16_le(bytes, ELF64_SECTION_NAME_INDEX_OFFSET);

    std::vector<RawSection> raw_sections = read_raw_sections(
        bytes,
        section_header_offset,
        section_count,
        section_entry_size,
        report.diagnostics);
    report.sections = summarize_sections(bytes, raw_sections, section_name_index, report.diagnostics);
    report.symbols =
        summarize_symbols(bytes, raw_sections, report.sections, config, report.diagnostics);

    return report;
}

std::string format_report(const ElfReport& report)
{
    std::ostringstream output;
    output << "source=" << report.source_name << '\n';
    output << "bytes=" << report.byte_size << '\n';
    output << "checksum=" << report.checksum << '\n';
    output << "valid_magic=" << (report.valid_magic ? "true" : "false") << '\n';
    output << "class=" << (report.is_elf64 ? "ELF64" : "unsupported") << '\n';
    output << "endian=" << (report.is_little_endian ? "little" : "unsupported") << '\n';
    output << "machine=" << classify_machine(report.machine) << '\n';
    output << "entry=";
    write_hex64(output, report.entry_address);
    output << '\n';
    output << "sections=" << report.sections.size() << '\n';
    output << "symbols=" << report.symbols.size() << '\n';
    for (const std::string& diagnostic : report.diagnostics) {
        output << "diagnostic=" << diagnostic << '\n';
    }
    return output.str();
}

std::string format_sections_csv(const ElfReport& report)
{
    std::ostringstream output;
    output << "name,type,flags,address,offset,size\n";
    for (const SectionSummary& section : report.sections) {
        output << section.name << ',' << section.type << ',' << section.flags << ','
               << section.address << ',' << section.offset << ',' << section.size << '\n';
    }
    return output.str();
}

}  // namespace cpu1

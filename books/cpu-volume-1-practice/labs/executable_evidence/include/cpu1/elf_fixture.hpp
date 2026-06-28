#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace cpu1 {

constexpr std::size_t FIXTURE_ELF_SIZE = 704;
constexpr std::size_t FIXTURE_ELF_IDENT_SIZE = 16;
constexpr std::size_t FIXTURE_ELF_HEADER_SIZE = 64;
constexpr std::size_t FIXTURE_ELF_SH_OFFSET = 384;
constexpr std::size_t FIXTURE_ELF_SH_ENTRY_SIZE = 64;
constexpr std::size_t FIXTURE_ELF_SH_COUNT = 5;
constexpr std::size_t FIXTURE_ELF_SHSTR_INDEX = 2;
constexpr std::size_t FIXTURE_ELF_TEXT_OFFSET = 96;
constexpr std::size_t FIXTURE_ELF_TEXT_SIZE = 4;
constexpr std::size_t FIXTURE_ELF_SHSTRTAB_OFFSET = 128;
constexpr std::size_t FIXTURE_ELF_STRTAB_OFFSET = 176;
constexpr std::size_t FIXTURE_ELF_SYMTAB_OFFSET = 208;
constexpr std::size_t FIXTURE_ELF_SYMTAB_ENTRY_SIZE = 24;
constexpr std::uint64_t FIXTURE_ELF_ENTRY = 0x401000;
constexpr std::uint16_t FIXTURE_ELF_EXEC_TYPE = 2;
constexpr std::uint16_t FIXTURE_ELF_X86_64 = 62;
constexpr std::uint32_t FIXTURE_ELF_VERSION = 1;
constexpr std::uint32_t FIXTURE_ELF_PROGBITS = 1;
constexpr std::uint32_t FIXTURE_ELF_SYMTAB = 2;
constexpr std::uint32_t FIXTURE_ELF_STRTAB = 3;
constexpr std::uint64_t FIXTURE_ELF_ALLOC_EXEC_FLAGS = 6;
constexpr std::uint8_t FIXTURE_ELF64_CLASS = 2;
constexpr std::uint8_t FIXTURE_ELF_LITTLE_ENDIAN = 1;
constexpr std::uint8_t FIXTURE_ELF_MAGIC_0 = 0x7F;
constexpr std::uint8_t FIXTURE_ELF_MAGIC_1 = 'E';
constexpr std::uint8_t FIXTURE_ELF_MAGIC_2 = 'L';
constexpr std::uint8_t FIXTURE_ELF_MAGIC_3 = 'F';
constexpr std::uint8_t FIXTURE_SYMBOL_GLOBAL_FUNC = 0x12;
constexpr std::uint8_t FIXTURE_SYMBOL_LOCAL_FUNC = 0x02;
constexpr std::uint8_t FIXTURE_TEXT_BYTE_0 = 0x55;
constexpr std::uint8_t FIXTURE_TEXT_BYTE_1 = 0x48;
constexpr std::uint8_t FIXTURE_TEXT_BYTE_2 = 0x89;
constexpr std::uint8_t FIXTURE_TEXT_BYTE_3 = 0xE5;
constexpr std::size_t FIXTURE_ELF_TYPE_OFFSET = 16;
constexpr std::size_t FIXTURE_ELF_MACHINE_OFFSET = 18;
constexpr std::size_t FIXTURE_ELF_VERSION_OFFSET = 20;
constexpr std::size_t FIXTURE_ELF_ENTRY_OFFSET = 24;
constexpr std::size_t FIXTURE_ELF_SECTION_HEADER_OFFSET = 40;
constexpr std::size_t FIXTURE_ELF_HEADER_SIZE_OFFSET = 52;
constexpr std::size_t FIXTURE_ELF_SECTION_ENTRY_SIZE_OFFSET = 58;
constexpr std::size_t FIXTURE_ELF_SECTION_COUNT_OFFSET = 60;
constexpr std::size_t FIXTURE_ELF_SECTION_NAME_INDEX_OFFSET = 62;
constexpr std::size_t FIXTURE_SECTION_NAME_OFFSET = 0;
constexpr std::size_t FIXTURE_SECTION_TYPE_OFFSET = 4;
constexpr std::size_t FIXTURE_SECTION_FLAGS_OFFSET = 8;
constexpr std::size_t FIXTURE_SECTION_ADDRESS_OFFSET = 16;
constexpr std::size_t FIXTURE_SECTION_FILE_OFFSET = 24;
constexpr std::size_t FIXTURE_SECTION_SIZE_OFFSET = 32;
constexpr std::size_t FIXTURE_SECTION_LINK_OFFSET = 40;
constexpr std::size_t FIXTURE_SECTION_ENTRY_SIZE_OFFSET = 56;
constexpr std::size_t FIXTURE_SYMBOL_NAME_OFFSET = 0;
constexpr std::size_t FIXTURE_SYMBOL_INFO_OFFSET = 4;
constexpr std::size_t FIXTURE_SYMBOL_SECTION_INDEX_OFFSET = 6;
constexpr std::size_t FIXTURE_SYMBOL_VALUE_OFFSET = 8;
constexpr std::size_t FIXTURE_SYMBOL_SIZE_OFFSET = 16;

inline void put_u16(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint16_t value)
{
    bytes[offset] = static_cast<std::uint8_t>(value & 0xFFU);
    bytes[offset + 1] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
}

inline void put_u32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value)
{
    constexpr std::size_t BYTE_BITS = 8;
    for (std::size_t i = 0; i < sizeof(std::uint32_t); ++i) {
        bytes[offset + i] = static_cast<std::uint8_t>((value >> (i * BYTE_BITS)) & 0xFFU);
    }
}

inline void put_u64(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint64_t value)
{
    constexpr std::size_t BYTE_BITS = 8;
    for (std::size_t i = 0; i < sizeof(std::uint64_t); ++i) {
        bytes[offset + i] = static_cast<std::uint8_t>((value >> (i * BYTE_BITS)) & 0xFFU);
    }
}

inline void put_string(std::vector<std::uint8_t>& bytes, std::size_t offset, std::string_view value)
{
    for (std::size_t i = 0; i < value.size(); ++i) {
        bytes[offset + i] = static_cast<std::uint8_t>(value[i]);
    }
}

inline void put_section_header(
    std::vector<std::uint8_t>& bytes,
    std::size_t index,
    std::uint32_t name_offset,
    std::uint32_t type,
    std::uint64_t flags,
    std::uint64_t address,
    std::uint64_t file_offset,
    std::uint64_t size,
    std::uint32_t link,
    std::uint64_t entry_size)
{
    const std::size_t offset = FIXTURE_ELF_SH_OFFSET + index * FIXTURE_ELF_SH_ENTRY_SIZE;
    put_u32(bytes, offset + FIXTURE_SECTION_NAME_OFFSET, name_offset);
    put_u32(bytes, offset + FIXTURE_SECTION_TYPE_OFFSET, type);
    put_u64(bytes, offset + FIXTURE_SECTION_FLAGS_OFFSET, flags);
    put_u64(bytes, offset + FIXTURE_SECTION_ADDRESS_OFFSET, address);
    put_u64(bytes, offset + FIXTURE_SECTION_FILE_OFFSET, file_offset);
    put_u64(bytes, offset + FIXTURE_SECTION_SIZE_OFFSET, size);
    put_u32(bytes, offset + FIXTURE_SECTION_LINK_OFFSET, link);
    put_u64(bytes, offset + FIXTURE_SECTION_ENTRY_SIZE_OFFSET, entry_size);
}

inline void put_symbol(
    std::vector<std::uint8_t>& bytes,
    std::size_t index,
    std::uint32_t name_offset,
    std::uint8_t info,
    std::uint16_t section_index,
    std::uint64_t value,
    std::uint64_t size)
{
    const std::size_t offset = FIXTURE_ELF_SYMTAB_OFFSET + index * FIXTURE_ELF_SYMTAB_ENTRY_SIZE;
    put_u32(bytes, offset + FIXTURE_SYMBOL_NAME_OFFSET, name_offset);
    bytes[offset + FIXTURE_SYMBOL_INFO_OFFSET] = info;
    put_u16(bytes, offset + FIXTURE_SYMBOL_SECTION_INDEX_OFFSET, section_index);
    put_u64(bytes, offset + FIXTURE_SYMBOL_VALUE_OFFSET, value);
    put_u64(bytes, offset + FIXTURE_SYMBOL_SIZE_OFFSET, size);
}

inline std::vector<std::uint8_t> make_synthetic_elf()
{
    std::vector<std::uint8_t> bytes(FIXTURE_ELF_SIZE, 0);
    bytes[0] = FIXTURE_ELF_MAGIC_0;
    bytes[1] = FIXTURE_ELF_MAGIC_1;
    bytes[2] = FIXTURE_ELF_MAGIC_2;
    bytes[3] = FIXTURE_ELF_MAGIC_3;
    bytes[4] = FIXTURE_ELF64_CLASS;
    bytes[5] = FIXTURE_ELF_LITTLE_ENDIAN;
    bytes[6] = 1;

    put_u16(bytes, FIXTURE_ELF_TYPE_OFFSET, FIXTURE_ELF_EXEC_TYPE);
    put_u16(bytes, FIXTURE_ELF_MACHINE_OFFSET, FIXTURE_ELF_X86_64);
    put_u32(bytes, FIXTURE_ELF_VERSION_OFFSET, FIXTURE_ELF_VERSION);
    put_u64(bytes, FIXTURE_ELF_ENTRY_OFFSET, FIXTURE_ELF_ENTRY);
    put_u64(bytes, FIXTURE_ELF_SECTION_HEADER_OFFSET, FIXTURE_ELF_SH_OFFSET);
    put_u16(bytes, FIXTURE_ELF_HEADER_SIZE_OFFSET, FIXTURE_ELF_HEADER_SIZE);
    put_u16(bytes, FIXTURE_ELF_SECTION_ENTRY_SIZE_OFFSET, FIXTURE_ELF_SH_ENTRY_SIZE);
    put_u16(bytes, FIXTURE_ELF_SECTION_COUNT_OFFSET, FIXTURE_ELF_SH_COUNT);
    put_u16(bytes, FIXTURE_ELF_SECTION_NAME_INDEX_OFFSET, FIXTURE_ELF_SHSTR_INDEX);

    bytes[FIXTURE_ELF_TEXT_OFFSET + 0] = FIXTURE_TEXT_BYTE_0;
    bytes[FIXTURE_ELF_TEXT_OFFSET + 1] = FIXTURE_TEXT_BYTE_1;
    bytes[FIXTURE_ELF_TEXT_OFFSET + 2] = FIXTURE_TEXT_BYTE_2;
    bytes[FIXTURE_ELF_TEXT_OFFSET + 3] = FIXTURE_TEXT_BYTE_3;

    put_string(bytes, FIXTURE_ELF_SHSTRTAB_OFFSET + 1, ".text");
    put_string(bytes, FIXTURE_ELF_SHSTRTAB_OFFSET + 7, ".shstrtab");
    put_string(bytes, FIXTURE_ELF_SHSTRTAB_OFFSET + 17, ".symtab");
    put_string(bytes, FIXTURE_ELF_SHSTRTAB_OFFSET + 25, ".strtab");
    put_string(bytes, FIXTURE_ELF_STRTAB_OFFSET + 1, "_start");
    put_string(bytes, FIXTURE_ELF_STRTAB_OFFSET + 8, "helper");

    put_symbol(bytes, 1, 1, FIXTURE_SYMBOL_GLOBAL_FUNC, 1, FIXTURE_ELF_ENTRY,
               FIXTURE_ELF_TEXT_SIZE);
    put_symbol(bytes, 2, 8, FIXTURE_SYMBOL_LOCAL_FUNC, 1,
               FIXTURE_ELF_ENTRY + FIXTURE_ELF_TEXT_SIZE, 8);

    put_section_header(bytes, 1, 1, FIXTURE_ELF_PROGBITS, FIXTURE_ELF_ALLOC_EXEC_FLAGS,
                       FIXTURE_ELF_ENTRY, FIXTURE_ELF_TEXT_OFFSET, FIXTURE_ELF_TEXT_SIZE, 0, 0);
    put_section_header(bytes, 2, 7, FIXTURE_ELF_STRTAB, 0, 0, FIXTURE_ELF_SHSTRTAB_OFFSET, 33, 0,
                       0);
    put_section_header(bytes, 3, 17, FIXTURE_ELF_SYMTAB, 0, 0, FIXTURE_ELF_SYMTAB_OFFSET,
                       FIXTURE_ELF_SYMTAB_ENTRY_SIZE * 3, 4, FIXTURE_ELF_SYMTAB_ENTRY_SIZE);
    put_section_header(bytes, 4, 25, FIXTURE_ELF_STRTAB, 0, 0, FIXTURE_ELF_STRTAB_OFFSET, 15, 0,
                       0);

    return bytes;
}

}  // namespace cpu1

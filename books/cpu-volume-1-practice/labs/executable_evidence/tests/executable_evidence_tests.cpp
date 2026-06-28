#include <cpu1/elf_fixture.hpp>
#include <cpu1/executable_evidence.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

using ::testing::HasSubstr;

constexpr std::uint16_t TEST_ELF_MACHINE_AARCH64 = 183;
constexpr std::uint16_t TEST_ELF_MACHINE_RISCV = 243;
constexpr std::uint16_t TEST_ELF_MACHINE_UNKNOWN = 0;

}  // namespace

TEST(CPU1ExecutableEvidenceTest, ParsesSyntheticElfHeaderSectionsAndSymbols)
{
    const std::vector<std::uint8_t> bytes = cpu1::make_synthetic_elf();
    const cpu1::ElfReport report = cpu1::inspect_elf(bytes, "fixture.elf");

    EXPECT_TRUE(report.valid_magic);
    EXPECT_TRUE(report.is_elf64);
    EXPECT_TRUE(report.is_little_endian);
    EXPECT_EQ(report.object_type, cpu1::FIXTURE_ELF_EXEC_TYPE);
    EXPECT_EQ(report.machine, cpu1::FIXTURE_ELF_X86_64);
    EXPECT_EQ(report.entry_address, cpu1::FIXTURE_ELF_ENTRY);
    EXPECT_EQ(report.byte_size, cpu1::FIXTURE_ELF_SIZE);
    EXPECT_TRUE(report.diagnostics.empty());
    ASSERT_EQ(report.sections.size(), cpu1::FIXTURE_ELF_SH_COUNT);
    EXPECT_EQ(report.sections[1].name, ".text");
    EXPECT_EQ(report.sections[1].offset, cpu1::FIXTURE_ELF_TEXT_OFFSET);
    EXPECT_EQ(report.sections[1].size, cpu1::FIXTURE_ELF_TEXT_SIZE);
    ASSERT_EQ(report.symbols.size(), 3U);
    EXPECT_EQ(report.symbols[1].name, "_start");
    EXPECT_EQ(report.symbols[1].section_name, ".text");
    EXPECT_EQ(report.symbols[1].binding, 1U);
    EXPECT_EQ(report.symbols[1].type, 2U);
}

TEST(CPU1ExecutableEvidenceTest, FormatsStableReportAndSectionCsv)
{
    const std::vector<std::uint8_t> bytes = cpu1::make_synthetic_elf();
    const cpu1::ElfReport report = cpu1::inspect_elf(bytes, "fixture.elf");

    const std::string text = cpu1::format_report(report);
    const std::string csv = cpu1::format_sections_csv(report);

    EXPECT_THAT(text, HasSubstr("source=fixture.elf"));
    EXPECT_THAT(text, HasSubstr("machine=x86-64"));
    EXPECT_THAT(text, HasSubstr("sections=5"));
    EXPECT_THAT(text, HasSubstr("symbols=3"));
    EXPECT_THAT(csv, HasSubstr("name,type,flags,address,offset,size"));
    EXPECT_THAT(csv, HasSubstr(".text,1,6,4198400,96,4"));
}

TEST(CPU1ExecutableEvidenceTest, RejectsNonElfMagic)
{
    const std::vector<std::uint8_t> bytes{0, 1, 2, 3};
    const cpu1::ElfReport report = cpu1::inspect_elf(bytes, "bad.bin");

    EXPECT_FALSE(report.valid_magic);
    ASSERT_EQ(report.diagnostics.size(), 1U);
    EXPECT_EQ(report.diagnostics[0], "input does not start with ELF magic");
}

TEST(CPU1ExecutableEvidenceTest, RejectsShortElfHeader)
{
    std::vector<std::uint8_t> bytes(cpu1::FIXTURE_ELF_IDENT_SIZE, 0);
    bytes[0] = cpu1::FIXTURE_ELF_MAGIC_0;
    bytes[1] = cpu1::FIXTURE_ELF_MAGIC_1;
    bytes[2] = cpu1::FIXTURE_ELF_MAGIC_2;
    bytes[3] = cpu1::FIXTURE_ELF_MAGIC_3;

    const cpu1::ElfReport report = cpu1::inspect_elf(bytes, "short.elf");

    EXPECT_TRUE(report.valid_magic);
    ASSERT_EQ(report.diagnostics.size(), 1U);
    EXPECT_EQ(report.diagnostics[0], "input is shorter than ELF64 header");
}

TEST(CPU1ExecutableEvidenceTest, RejectsUnsupportedClassOrEndian)
{
    std::vector<std::uint8_t> bytes = cpu1::make_synthetic_elf();
    bytes[4] = 1;

    const cpu1::ElfReport report = cpu1::inspect_elf(bytes, "elf32.elf");

    EXPECT_TRUE(report.valid_magic);
    EXPECT_FALSE(report.is_elf64);
    ASSERT_EQ(report.diagnostics.size(), 1U);
    EXPECT_EQ(report.diagnostics[0], "only ELF64 little-endian inputs are supported");
}

TEST(CPU1ExecutableEvidenceTest, RespectsSymbolCap)
{
    const std::vector<std::uint8_t> bytes = cpu1::make_synthetic_elf();
    cpu1::InspectionConfig config;
    config.max_symbols = 1;

    const cpu1::ElfReport report = cpu1::inspect_elf(bytes, "fixture.elf", config);

    EXPECT_EQ(report.symbols.size(), 1U);
    ASSERT_EQ(report.diagnostics.size(), 1U);
    EXPECT_EQ(report.diagnostics[0], "symbol table truncated by max_symbols");
}

TEST(CPU1ExecutableEvidenceTest, ClassifiesKnownAndUnknownMachines)
{
    EXPECT_EQ(cpu1::classify_machine(cpu1::FIXTURE_ELF_X86_64), "x86-64");
    EXPECT_EQ(cpu1::classify_machine(TEST_ELF_MACHINE_AARCH64), "AArch64");
    EXPECT_EQ(cpu1::classify_machine(TEST_ELF_MACHINE_RISCV), "RISC-V");
    EXPECT_EQ(cpu1::classify_machine(TEST_ELF_MACHINE_UNKNOWN), "unknown");
}

TEST(CPU1ExecutableEvidenceTest, ReadsBinaryFileAndKeepsChecksumStable)
{
    const std::vector<std::uint8_t> bytes = cpu1::make_synthetic_elf();
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "cpu1ee_fixture.elf";
    {
        std::ofstream output(path, std::ios::binary);
        ASSERT_TRUE(output);
        output.write(reinterpret_cast<const char*>(bytes.data()),
                     static_cast<std::streamsize>(bytes.size()));
    }

    const std::vector<std::uint8_t> read_back = cpu1::read_binary_file(path);
    std::filesystem::remove(path);

    EXPECT_EQ(read_back, bytes);
    EXPECT_EQ(cpu1::checksum_bytes(read_back), cpu1::checksum_bytes(bytes));
}

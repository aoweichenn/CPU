#include <cpu1/executable_evidence.hpp>

#include <exception>
#include <iostream>
#include <string_view>

namespace {

constexpr int CLI_SUCCESS = 0;
constexpr int CLI_USAGE_ERROR = 2;
constexpr int CLI_RUNTIME_ERROR = 1;
constexpr int CLI_MIN_ARGUMENTS = 2;
constexpr int CLI_MODE_ARGUMENTS = 3;

void print_usage(std::ostream& output)
{
    output << "usage: cpu1ee_cli <elf-file> [--sections-csv]\n";
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc < CLI_MIN_ARGUMENTS || argc > CLI_MODE_ARGUMENTS) {
        print_usage(std::cerr);
        return CLI_USAGE_ERROR;
    }

    try {
        const std::vector<std::uint8_t> bytes = cpu1::read_binary_file(argv[1]);
        const cpu1::ElfReport report = cpu1::inspect_elf(bytes, argv[1]);

        if (argc == CLI_MODE_ARGUMENTS && std::string_view(argv[2]) == "--sections-csv") {
            std::cout << cpu1::format_sections_csv(report);
            return CLI_SUCCESS;
        }

        std::cout << cpu1::format_report(report);
        return CLI_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "cpu1ee_cli: " << error.what() << '\n';
        return CLI_RUNTIME_ERROR;
    }
}

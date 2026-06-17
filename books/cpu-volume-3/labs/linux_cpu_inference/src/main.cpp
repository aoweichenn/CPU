#include <lcqi/inference.hpp>
#include <lcqi/model.hpp>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr std::int32_t LCQI_DEFAULT_REPEAT = 1000;
constexpr int LCQI_EXPECTED_ARGC_MIN = 2;
constexpr double LCQI_SECONDS_TO_MICROSECONDS = 1000000.0;

std::vector<float> parse_input(int argc, char** argv, std::int32_t expected_size) {
    if (argc < LCQI_EXPECTED_ARGC_MIN + expected_size) {
        throw std::runtime_error("usage: lcqi_cli <model.txt> <input floats...> [repeat]");
    }
    std::vector<float> input(static_cast<std::size_t>(expected_size));
    for (std::int32_t i = 0; i < expected_size; ++i) {
        input[static_cast<std::size_t>(i)] =
            std::stof(argv[LCQI_EXPECTED_ARGC_MIN + i]);
    }
    return input;
}

std::int32_t parse_repeat(int argc, char** argv, std::int32_t input_size) {
    const int repeat_index = LCQI_EXPECTED_ARGC_MIN + input_size;
    if (argc <= repeat_index) {
        return LCQI_DEFAULT_REPEAT;
    }
    const std::int32_t repeat = static_cast<std::int32_t>(std::stoi(argv[repeat_index]));
    if (repeat <= 0) {
        throw std::runtime_error("repeat must be positive");
    }
    return repeat;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc < LCQI_EXPECTED_ARGC_MIN) {
            throw std::runtime_error("usage: lcqi_cli <model.txt> <input floats...> [repeat]");
        }

        const std::filesystem::path model_path = argv[1];
        const lcqi::TinyMlpModel model = lcqi::load_model(model_path);
        const std::vector<float> input = parse_input(argc, argv, model.input_size);
        const std::int32_t repeat = parse_repeat(argc, argv, model.input_size);

        const lcqi::InferenceResult result = lcqi::run_inference(model, input);
        std::cout << "predicted_class " << result.predicted_class << "\n";
        std::cout << "logits";
        for (const float value : result.logits) {
            std::cout << ' ' << value;
        }
        std::cout << "\n";

        const auto begin = std::chrono::steady_clock::now();
        std::int32_t checksum = 0;
        for (std::int32_t i = 0; i < repeat; ++i) {
            checksum += lcqi::run_inference(model, input).predicted_class;
        }
        const auto end = std::chrono::steady_clock::now();
        const std::chrono::duration<double> elapsed = end - begin;
        const double average_us =
            elapsed.count() * LCQI_SECONDS_TO_MICROSECONDS / static_cast<double>(repeat);
        std::cout << "repeat " << repeat << "\n";
        std::cout << "average_us " << average_us << "\n";
        std::cout << "checksum " << checksum << "\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << "\n";
        return EXIT_FAILURE;
    }
}

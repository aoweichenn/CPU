#include <lcqi/gguf.hpp>
#include <lcqi/q4_k.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr int LCQI_ARG_MODEL = 1;
constexpr int LCQI_ARG_TENSOR = 2;
constexpr int LCQI_ARG_REPEAT = 3;
constexpr int LCQI_DEFAULT_REPEAT = 20;
constexpr std::int64_t LCQI_MAX_AUTO_ROWS = 4096;
constexpr double LCQI_SECONDS_TO_MICROSECONDS = 1000000.0;
constexpr float LCQI_INPUT_PATTERN_SCALE = 0.015625F;
constexpr int LCQI_INPUT_PATTERN_MODULUS = 31;
constexpr int LCQI_INPUT_PATTERN_CENTER = 15;

struct SelectedTensor {
    const lcqi::GgufTensorInfo* tensor = nullptr;
    std::int64_t rows = 0;
    std::int64_t columns = 0;
};

std::int32_t parse_repeat(int argc, char** argv) {
    if (argc <= LCQI_ARG_REPEAT) {
        return LCQI_DEFAULT_REPEAT;
    }
    const std::int32_t repeat = static_cast<std::int32_t>(std::stoi(argv[LCQI_ARG_REPEAT]));
    if (repeat <= 0) {
        throw std::runtime_error("repeat must be positive");
    }
    return repeat;
}

bool is_q4_k_matrix(const lcqi::GgufTensorInfo& tensor) {
    return tensor.type == lcqi::GgmlType::q4_k && tensor.shape.size() == 2 &&
           tensor.shape[0] > 0 && tensor.shape[1] > 0 &&
           tensor.shape[0] % lcqi::LCQI_QK_K_BLOCK_VALUES == 0;
}

bool preferred_tensor_name(std::string_view name) {
    return name.find("ffn_up.weight") != std::string_view::npos ||
           name.find("ffn_gate.weight") != std::string_view::npos ||
           name.find("ffn_down.weight") != std::string_view::npos;
}

SelectedTensor select_tensor(const lcqi::GgufManifest& manifest, std::string_view requested) {
    if (!requested.empty() && requested != "auto") {
        const lcqi::GgufTensorInfo* tensor = manifest.find_tensor(requested);
        if (tensor == nullptr) {
            throw std::runtime_error("requested GGUF tensor not found");
        }
        if (!is_q4_k_matrix(*tensor)) {
            throw std::runtime_error("requested GGUF tensor is not a rank-2 Q4_K matrix");
        }
        return {tensor, tensor->shape[1], tensor->shape[0]};
    }

    const lcqi::GgufTensorInfo* best = nullptr;
    for (const lcqi::GgufTensorInfo& tensor : manifest.tensors) {
        if (!is_q4_k_matrix(tensor) || tensor.shape[1] > LCQI_MAX_AUTO_ROWS) {
            continue;
        }
        if (best == nullptr) {
            best = &tensor;
            continue;
        }
        const bool tensor_preferred = preferred_tensor_name(tensor.name);
        const bool best_preferred = preferred_tensor_name(best->name);
        if ((tensor_preferred && !best_preferred) ||
            (tensor_preferred == best_preferred &&
             tensor.element_count() > best->element_count())) {
            best = &tensor;
        }
    }
    if (best == nullptr) {
        throw std::runtime_error("no suitable rank-2 Q4_K tensor found in GGUF model");
    }
    return {best, best->shape[1], best->shape[0]};
}

std::vector<float> make_input(std::int64_t columns) {
    std::vector<float> input(static_cast<std::size_t>(columns), 0.0F);
    for (std::int64_t index = 0; index < columns; ++index) {
        input[static_cast<std::size_t>(index)] =
            static_cast<float>((index % LCQI_INPUT_PATTERN_MODULUS) -
                               LCQI_INPUT_PATTERN_CENTER) *
            LCQI_INPUT_PATTERN_SCALE;
    }
    return input;
}

float max_abs_diff(std::span<const float> lhs, std::span<const float> rhs) {
    if (lhs.size() != rhs.size()) {
        throw std::runtime_error("max_abs_diff size mismatch");
    }
    float diff = 0.0F;
    for (std::size_t index = 0; index < lhs.size(); ++index) {
        diff = std::max(diff, std::fabs(lhs[index] - rhs[index]));
    }
    return diff;
}

float checksum(std::span<const float> values) {
    float sum = 0.0F;
    for (const float value : values) {
        sum += value;
    }
    return sum;
}

void matvec_f32(std::span<const float> weights,
                std::int64_t rows,
                std::int64_t columns,
                std::span<const float> input,
                std::span<float> output) {
    if (weights.size() != static_cast<std::size_t>(rows * columns) ||
        input.size() != static_cast<std::size_t>(columns) ||
        output.size() != static_cast<std::size_t>(rows)) {
        throw std::runtime_error("F32 matvec shape mismatch");
    }
    for (std::int64_t row = 0; row < rows; ++row) {
        const float* row_weights = weights.data() + row * columns;
        float sum = 0.0F;
        for (std::int64_t column = 0; column < columns; ++column) {
            sum += row_weights[column] * input[static_cast<std::size_t>(column)];
        }
        output[static_cast<std::size_t>(row)] = sum;
    }
}

template <typename Callable>
double measure_average_us(std::int32_t repeat, Callable&& callable) {
    const auto begin = std::chrono::steady_clock::now();
    for (std::int32_t index = 0; index < repeat; ++index) {
        callable();
    }
    const auto end = std::chrono::steady_clock::now();
    const std::chrono::duration<double> elapsed = end - begin;
    return elapsed.count() * LCQI_SECONDS_TO_MICROSECONDS / static_cast<double>(repeat);
}

void print_usage(const char* program) {
    std::cerr << "usage: " << program << " <model.gguf> [tensor-name|auto] [repeat]\n";
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc <= LCQI_ARG_MODEL) {
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
        const std::filesystem::path model_path = argv[LCQI_ARG_MODEL];
        const std::string tensor_request =
            argc > LCQI_ARG_TENSOR ? argv[LCQI_ARG_TENSOR] : "auto";
        const std::int32_t repeat = parse_repeat(argc, argv);

        const lcqi::GgufManifest manifest = lcqi::load_gguf_manifest(model_path);
        const SelectedTensor selected = select_tensor(manifest, tensor_request);
        const std::vector<std::uint8_t> tensor_bytes =
            lcqi::read_gguf_tensor_bytes(model_path, *selected.tensor);
        const std::vector<float> input = make_input(selected.columns);
        const std::vector<lcqi::Q8KBlock> q8_input = lcqi::quantize_q8_k_input(input);
        std::vector<float> q4_f32_output(static_cast<std::size_t>(selected.rows), 0.0F);
        std::vector<float> q4_q8_output(q4_f32_output.size(), 0.0F);
        std::vector<float> f32_output(q4_f32_output.size(), 0.0F);

        lcqi::matvec_q4_k_f32(tensor_bytes,
                              selected.rows,
                              selected.columns,
                              input,
                              q4_f32_output);
        lcqi::matvec_q4_k_q8(tensor_bytes,
                             selected.rows,
                             selected.columns,
                             q8_input,
                             q4_q8_output);

        const std::vector<float> f32_weights =
            lcqi::dequantize_q4_k(tensor_bytes, selected.rows * selected.columns);
        matvec_f32(f32_weights, selected.rows, selected.columns, input, f32_output);

        const double f32_us = measure_average_us(
            repeat,
            [&]() { matvec_f32(f32_weights, selected.rows, selected.columns, input, f32_output); });
        const double q4_f32_us = measure_average_us(
            repeat,
            [&]() {
                lcqi::matvec_q4_k_f32(tensor_bytes,
                                      selected.rows,
                                      selected.columns,
                                      input,
                                      q4_f32_output);
            });
        const double q8_quantize_us = measure_average_us(
            repeat,
            [&]() {
                const std::vector<lcqi::Q8KBlock> temporary =
                    lcqi::quantize_q8_k_input(input);
                static_cast<void>(temporary);
            });
        const double q4_q8_us = measure_average_us(
            repeat,
            [&]() {
                lcqi::matvec_q4_k_q8(tensor_bytes,
                                     selected.rows,
                                     selected.columns,
                                     q8_input,
                                     q4_q8_output);
            });

        const double q4_q8_total_us = q8_quantize_us + q4_q8_us;
        std::cout << std::fixed << std::setprecision(3);
        std::cout << "lcqi_smollm2_q4_k_benchmark\n";
        std::cout << "model_path=" << model_path.string() << '\n';
        std::cout << "gguf_version=" << manifest.version << '\n';
        std::cout << "gguf_alignment=" << manifest.alignment << '\n';
        std::cout << "gguf_tensors=" << manifest.tensors.size() << '\n';
        std::cout << "tensor_name=" << selected.tensor->name << '\n';
        std::cout << "tensor_type=" << lcqi::ggml_type_name(selected.tensor->type) << '\n';
        std::cout << "tensor_shape_ne0_ne1=" << selected.columns << 'x' << selected.rows << '\n';
        std::cout << "tensor_bytes=" << selected.tensor->byte_size << '\n';
        std::cout << "f32_dequantized_bytes=" << f32_weights.size() * sizeof(float) << '\n';
        std::cout << "repeat=" << repeat << '\n';
        std::cout << "f32_matvec_average_us=" << f32_us << '\n';
        std::cout << "q4_f32_input_average_us=" << q4_f32_us << '\n';
        std::cout << "q8_quantize_input_average_us=" << q8_quantize_us << '\n';
        std::cout << "q4_q8_backend=" << lcqi::q4_k_q8_active_backend() << '\n';
        std::cout << "q4_q8_matvec_average_us=" << q4_q8_us << '\n';
        std::cout << "q4_q8_total_average_us=" << q4_q8_total_us << '\n';
        std::cout << "speedup_q4_q8_total_vs_f32="
                  << (q4_q8_total_us > 0.0 ? f32_us / q4_q8_total_us : 0.0) << '\n';
        std::cout << "speedup_q4_f32_input_vs_f32="
                  << (q4_f32_us > 0.0 ? f32_us / q4_f32_us : 0.0) << '\n';
        std::cout << "q4_f32_vs_f32_max_abs_diff="
                  << max_abs_diff(q4_f32_output, f32_output) << '\n';
        std::cout << "q4_q8_vs_q4_f32_max_abs_diff="
                  << max_abs_diff(q4_q8_output, q4_f32_output) << '\n';
        std::cout << "q4_q8_checksum=" << checksum(q4_q8_output) << '\n';
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}

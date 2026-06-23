#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include <lcqi/model.hpp>

namespace lcqi {

struct PackedLinearI8 {
    std::int32_t input_size = 0;
    std::int32_t output_size = 0;
    std::int32_t output_block_size = 0;
    float scale = 1.0F;
    std::vector<std::int8_t> packed_weights;
    std::vector<float> bias;
};

struct KernelBenchmarkCase {
    std::int32_t input_size = 0;
    std::int32_t output_size = 0;
    std::int32_t output_block_size = 0;
    std::int32_t repeat = 0;
};

struct KernelBackendBenchmark {
    const char* name = "";
    bool available = false;
    double average_us = 0.0;
    float max_abs_diff = 0.0F;
    float checksum = 0.0F;
};

struct KernelBenchmarkResult {
    KernelBenchmarkCase benchmark_case;
    std::vector<KernelBackendBenchmark> backends;
};

PackedLinearI8 pack_linear_i8(const QuantizedLinearLayer& layer,
                              std::int32_t output_block_size);

void linear_i8_packed(const PackedLinearI8& layer,
                      std::span<const float> input,
                      std::span<float> output);

void linear_i8_packed_with_workspace(const PackedLinearI8& layer,
                                     std::span<const float> input,
                                     std::span<float> output,
                                     std::span<float> accumulator_workspace);

bool linear_i8_packed_avx2_available() noexcept;

void linear_i8_packed_avx2(const PackedLinearI8& layer,
                           std::span<const float> input,
                           std::span<float> output);

bool linear_i8_packed_neon_available() noexcept;

void linear_i8_packed_neon(const PackedLinearI8& layer,
                           std::span<const float> input,
                           std::span<float> output);

QuantizedLinearLayer make_deterministic_i8_layer(std::int32_t input_size,
                                                 std::int32_t output_size,
                                                 float scale);

std::vector<float> make_deterministic_input(std::int32_t input_size);

float max_abs_diff(std::span<const float> lhs, std::span<const float> rhs);

KernelBenchmarkResult benchmark_linear_i8_case(const KernelBenchmarkCase& benchmark_case);

}  // namespace lcqi

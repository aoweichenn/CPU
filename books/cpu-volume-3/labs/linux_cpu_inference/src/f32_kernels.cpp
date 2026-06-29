#include <lcqi/f32_kernels.hpp>

#include <limits>
#include <stdexcept>

namespace lcqi {
namespace {

constexpr std::size_t LCQI_F32_AVX2_MIN_DOT_SIZE = 32;
constexpr std::size_t LCQI_F32_AVX2_MIN_ROW_COUNT = 4;

}  // namespace

float dot_f32(std::span<const float> lhs, std::span<const float> rhs) {
    if (lhs.size() != rhs.size()) {
        throw std::runtime_error("dot_f32 size mismatch");
    }
    return dot_f32_unchecked(lhs.data(), rhs.data(), lhs.size());
}

float dot_f32_scalar_unchecked(const float* lhs,
                               const float* rhs,
                               std::size_t size) noexcept {
    float sum = 0.0F;
    for (std::size_t index = 0; index < size; ++index) {
        sum += lhs[index] * rhs[index];
    }
    return sum;
}

float dot_f32_unchecked(const float* lhs,
                        const float* rhs,
                        std::size_t size) noexcept {
    static const bool AVX2_AVAILABLE = dot_f32_avx2_available();
    if (AVX2_AVAILABLE && size >= LCQI_F32_AVX2_MIN_DOT_SIZE) {
        return dot_f32_avx2_unchecked(lhs, rhs, size);
    }
    return dot_f32_scalar_unchecked(lhs, rhs, size);
}

void linear_f32_rows_scalar_unchecked(const float* weights,
                                      const float* input,
                                      const float* bias,
                                      std::size_t input_size,
                                      std::size_t row_begin,
                                      std::size_t row_end,
                                      float* output) noexcept {
    for (std::size_t row = row_begin; row < row_end; ++row) {
        const float* weight_row = weights + row * input_size;
        output[row] = dot_f32_scalar_unchecked(weight_row, input, input_size) +
                      (bias == nullptr ? 0.0F : bias[row]);
    }
}

void linear_f32_rows_unchecked(const float* weights,
                               const float* input,
                               const float* bias,
                               std::size_t input_size,
                               std::size_t row_begin,
                               std::size_t row_end,
                               float* output) noexcept {
    static const bool AVX2_AVAILABLE = dot_f32_avx2_available();
    if (AVX2_AVAILABLE && input_size >= LCQI_F32_AVX2_MIN_DOT_SIZE &&
        row_end - row_begin >= LCQI_F32_AVX2_MIN_ROW_COUNT) {
        linear_f32_rows_avx2_unchecked(weights,
                                       input,
                                       bias,
                                       input_size,
                                       row_begin,
                                       row_end,
                                       output);
        return;
    }
    linear_f32_rows_scalar_unchecked(weights, input, bias, input_size, row_begin, row_end, output);
}

F32RowMax max_dot_f32_rows_scalar_unchecked(const float* weights,
                                            const float* input,
                                            std::size_t input_size,
                                            std::size_t row_begin,
                                            std::size_t row_end) noexcept {
    F32RowMax best;
    best.row = row_begin;
    best.value = -std::numeric_limits<float>::infinity();
    for (std::size_t row = row_begin; row < row_end; ++row) {
        const float* weight_row = weights + row * input_size;
        const float value = dot_f32_scalar_unchecked(weight_row, input, input_size);
        if (row == row_begin || value > best.value) {
            best.row = row;
            best.value = value;
        }
    }
    return best;
}

F32RowMax max_dot_f32_rows_unchecked(const float* weights,
                                     const float* input,
                                     std::size_t input_size,
                                     std::size_t row_begin,
                                     std::size_t row_end) noexcept {
    static const bool AVX2_AVAILABLE = dot_f32_avx2_available();
    if (AVX2_AVAILABLE && input_size >= LCQI_F32_AVX2_MIN_DOT_SIZE &&
        row_end - row_begin >= LCQI_F32_AVX2_MIN_ROW_COUNT) {
        return max_dot_f32_rows_avx2_unchecked(weights,
                                               input,
                                               input_size,
                                               row_begin,
                                               row_end);
    }
    return max_dot_f32_rows_scalar_unchecked(weights, input, input_size, row_begin, row_end);
}

}  // namespace lcqi

#if !defined(LCQI_ENABLE_AVX2)
namespace lcqi {

bool dot_f32_avx2_available() noexcept {
    return false;
}

float dot_f32_avx2_unchecked(const float* lhs,
                             const float* rhs,
                             std::size_t size) noexcept {
    return dot_f32_scalar_unchecked(lhs, rhs, size);
}

void linear_f32_rows_avx2_unchecked(const float* weights,
                                    const float* input,
                                    const float* bias,
                                    std::size_t input_size,
                                    std::size_t row_begin,
                                    std::size_t row_end,
                                    float* output) noexcept {
    linear_f32_rows_scalar_unchecked(weights,
                                     input,
                                     bias,
                                     input_size,
                                     row_begin,
                                     row_end,
                                     output);
}

F32RowMax max_dot_f32_rows_avx2_unchecked(const float* weights,
                                          const float* input,
                                          std::size_t input_size,
                                          std::size_t row_begin,
                                          std::size_t row_end) noexcept {
    return max_dot_f32_rows_scalar_unchecked(weights, input, input_size, row_begin, row_end);
}

}  // namespace lcqi
#endif

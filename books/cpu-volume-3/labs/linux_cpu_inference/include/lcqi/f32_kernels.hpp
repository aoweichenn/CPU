#pragma once

#include <cstddef>
#include <span>

namespace lcqi {

struct F32RowMax {
    std::size_t row = 0;
    float value = 0.0F;
};

[[nodiscard]] float dot_f32(std::span<const float> lhs, std::span<const float> rhs);

[[nodiscard]] float dot_f32_scalar_unchecked(const float* lhs,
                                             const float* rhs,
                                             std::size_t size) noexcept;

[[nodiscard]] float dot_f32_unchecked(const float* lhs,
                                      const float* rhs,
                                      std::size_t size) noexcept;

[[nodiscard]] bool dot_f32_avx2_available() noexcept;

[[nodiscard]] float dot_f32_avx2_unchecked(const float* lhs,
                                           const float* rhs,
                                           std::size_t size) noexcept;

void linear_f32_rows_scalar_unchecked(const float* weights,
                                      const float* input,
                                      const float* bias,
                                      std::size_t input_size,
                                      std::size_t row_begin,
                                      std::size_t row_end,
                                      float* output) noexcept;

void linear_f32_rows_unchecked(const float* weights,
                               const float* input,
                               const float* bias,
                               std::size_t input_size,
                               std::size_t row_begin,
                               std::size_t row_end,
                               float* output) noexcept;

void linear_f32_rows_avx2_unchecked(const float* weights,
                                    const float* input,
                                    const float* bias,
                                    std::size_t input_size,
                                    std::size_t row_begin,
                                    std::size_t row_end,
                                    float* output) noexcept;

[[nodiscard]] F32RowMax max_dot_f32_rows_scalar_unchecked(const float* weights,
                                                          const float* input,
                                                          std::size_t input_size,
                                                          std::size_t row_begin,
                                                          std::size_t row_end) noexcept;

[[nodiscard]] F32RowMax max_dot_f32_rows_unchecked(const float* weights,
                                                   const float* input,
                                                   std::size_t input_size,
                                                   std::size_t row_begin,
                                                   std::size_t row_end) noexcept;

[[nodiscard]] F32RowMax max_dot_f32_rows_avx2_unchecked(const float* weights,
                                                        const float* input,
                                                        std::size_t input_size,
                                                        std::size_t row_begin,
                                                        std::size_t row_end) noexcept;

}  // namespace lcqi

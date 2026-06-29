#pragma once

#include <cstddef>
#include <span>

namespace lcqi {

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

}  // namespace lcqi

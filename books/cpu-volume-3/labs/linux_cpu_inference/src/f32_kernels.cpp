#include <lcqi/f32_kernels.hpp>

#include <stdexcept>

namespace lcqi {
namespace {

constexpr std::size_t LCQI_F32_AVX2_MIN_DOT_SIZE = 32;

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

}  // namespace lcqi
#endif

#include <lcqi/q5_0_packed.hpp>

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string>

namespace lcqi {
namespace {

constexpr std::uint32_t LCQI_Q5_0_PACKED_BYTE_BITS = 8U;
constexpr std::uint8_t LCQI_Q5_0_PACKED_LOW_NIBBLE_MASK = 0x0FU;
constexpr std::uint8_t LCQI_Q5_0_PACKED_HIGH_BIT_SHIFT = 4U;
constexpr std::int32_t LCQI_Q5_0_PACKED_ZERO_POINT = 16;

[[nodiscard]] std::size_t checked_size(std::int64_t value, const char* name) {
    if (value < 0 ||
        static_cast<std::uint64_t>(value) >
            static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        throw std::runtime_error(std::string(name) + " is outside size_t range");
    }
    return static_cast<std::size_t>(value);
}

void require_positive(std::int64_t value, const char* name) {
    if (value <= 0) {
        throw std::runtime_error(std::string(name) + " must be positive");
    }
}

void require_multiple(std::int64_t value, std::int64_t divisor, const char* name) {
    if (value <= 0 || value % divisor != 0) {
        throw std::runtime_error(std::string(name) + " must be a positive multiple of " +
                                 std::to_string(divisor));
    }
}

[[nodiscard]] std::uint16_t read_le_u16(const std::uint8_t* bytes) noexcept {
    return static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(bytes[0]) |
        (static_cast<std::uint16_t>(bytes[1]) << LCQI_Q5_0_PACKED_BYTE_BITS));
}

[[nodiscard]] std::uint32_t read_le_u32(const std::uint8_t* bytes) noexcept {
    std::uint32_t value = 0;
    for (std::uint32_t index = 0; index < sizeof(std::uint32_t); ++index) {
        value |= static_cast<std::uint32_t>(bytes[index]) <<
                 (LCQI_Q5_0_PACKED_BYTE_BITS * index);
    }
    return value;
}

void validate_q5_0_bytes(std::span<const std::uint8_t> bytes,
                         std::int64_t element_count) {
    require_multiple(element_count, LCQI_Q5_0_BLOCK_VALUES, "Q5_0 element_count");
    const std::int64_t expected_bytes =
        (element_count / LCQI_Q5_0_BLOCK_VALUES) * LCQI_Q5_0_BLOCK_BYTES;
    if (bytes.size() != checked_size(expected_bytes, "Q5_0 byte count")) {
        throw std::runtime_error("Q5_0 byte count does not match element count");
    }
}

void validate_q8_0_blocks(std::span<const Q8_0InputBlock> input,
                          std::int64_t column_count) {
    require_multiple(column_count, LCQI_Q8_0_INPUT_BLOCK_VALUES, "Q8_0 input column count");
    if (input.size() !=
        checked_size(column_count / LCQI_Q8_0_INPUT_BLOCK_VALUES, "Q8_0 input block count")) {
        throw std::runtime_error("Q8_0 input block count mismatch");
    }
}

void validate_packed_rows(std::span<const Q5_0PackedBlock> rows,
                          std::int64_t row_count,
                          std::int64_t column_count) {
    require_positive(row_count, "Q5_0 packed row count");
    require_multiple(column_count, LCQI_Q5_0_BLOCK_VALUES, "Q5_0 packed column count");
    const std::int64_t row_blocks = column_count / LCQI_Q5_0_BLOCK_VALUES;
    if (rows.size() != checked_size(row_blocks * row_count, "Q5_0 packed matrix blocks")) {
        throw std::runtime_error("Q5_0 packed matrix block count mismatch");
    }
}

[[nodiscard]] Q5_0PackedBlock pack_q5_0_block(const std::uint8_t* block) noexcept {
    Q5_0PackedBlock packed;
    packed.d = ggml_f16_to_f32(read_le_u16(block));
    const std::uint32_t qh = read_le_u32(block + sizeof(std::uint16_t));
    const std::uint8_t* qs = block + sizeof(std::uint16_t) + sizeof(std::uint32_t);
    for (std::int32_t index = 0; index < LCQI_Q5_0_BLOCK_VALUES / 2; ++index) {
        const std::uint8_t high_0 =
            static_cast<std::uint8_t>(((qh >> index) & 1U)
                                      << LCQI_Q5_0_PACKED_HIGH_BIT_SHIFT);
        const std::uint8_t high_1 = static_cast<std::uint8_t>(
            ((qh >> (index + LCQI_Q5_0_BLOCK_VALUES / 2)) & 1U)
            << LCQI_Q5_0_PACKED_HIGH_BIT_SHIFT);
        packed.qs[static_cast<std::size_t>(index)] =
            static_cast<std::uint8_t>((qs[index] & LCQI_Q5_0_PACKED_LOW_NIBBLE_MASK) |
                                      high_0);
        packed.qs[static_cast<std::size_t>(index + LCQI_Q5_0_BLOCK_VALUES / 2)] =
            static_cast<std::uint8_t>((qs[index] >> 4U) | high_1);
    }
    return packed;
}

[[nodiscard]] float dot_q5_0_packed_q8_0_scalar(const Q5_0PackedBlock& block,
                                                const Q8_0InputBlock& input) noexcept {
    std::int32_t weighted_sum = 0;
    for (std::int32_t index = 0; index < LCQI_Q5_0_BLOCK_VALUES; ++index) {
        weighted_sum += static_cast<std::int32_t>(block.qs[static_cast<std::size_t>(index)]) *
                        static_cast<std::int32_t>(input.qs[static_cast<std::size_t>(index)]);
    }
    weighted_sum -= LCQI_Q5_0_PACKED_ZERO_POINT * static_cast<std::int32_t>(input.sum);
    return block.d * input.d * static_cast<float>(weighted_sum);
}

}  // namespace

#if !defined(LCQI_ENABLE_AVX2)
bool q5_0_packed_q8_0_avx2_available() noexcept {
    return false;
}

const char* q5_0_packed_q8_0_active_backend() noexcept {
    return "scalar";
}
#endif

void matvec_q5_0_packed_q8_0_avx2_rows_unchecked(
    std::span<const Q5_0PackedBlock>,
    std::int64_t,
    std::int64_t,
    std::span<const Q8_0InputBlock>,
    std::span<float>,
    std::int64_t,
    std::int64_t) noexcept;

void matvec_q5_0_packed_q8_0_avx2_batch_rows_unchecked(
    std::span<const Q5_0PackedBlock>,
    std::int64_t,
    std::int64_t,
    std::span<const Q8_0InputBlock>,
    std::int64_t,
    std::span<float>,
    std::int64_t,
    std::int64_t,
    std::int64_t) noexcept;

std::vector<Q5_0PackedBlock> pack_q5_0_blocks(std::span<const std::uint8_t> bytes,
                                              std::int64_t element_count) {
    validate_q5_0_bytes(bytes, element_count);
    std::vector<Q5_0PackedBlock> output(
        checked_size(element_count / LCQI_Q5_0_BLOCK_VALUES, "Q5_0 packed block count"));
    pack_q5_0_blocks_to(bytes, output);
    return output;
}

void pack_q5_0_blocks_to(std::span<const std::uint8_t> bytes,
                         std::span<Q5_0PackedBlock> output) {
    validate_q5_0_bytes(
        bytes,
        static_cast<std::int64_t>(output.size()) * LCQI_Q5_0_BLOCK_VALUES);
    for (std::size_t block_index = 0; block_index < output.size(); ++block_index) {
        output[block_index] =
            pack_q5_0_block(bytes.data() + block_index * LCQI_Q5_0_BLOCK_BYTES);
    }
}

void matvec_q5_0_packed_q8_0(std::span<const Q5_0PackedBlock> rows,
                             std::int64_t row_count,
                             std::int64_t column_count,
                             std::span<const Q8_0InputBlock> input,
                             std::span<float> output) {
    validate_packed_rows(rows, row_count, column_count);
    validate_q8_0_blocks(input, column_count);
    if (output.size() != checked_size(row_count, "Q5_0 packed output rows")) {
        throw std::runtime_error("Q5_0 packed output size mismatch");
    }
    matvec_q5_0_packed_q8_0_rows_unchecked(rows,
                                           row_count,
                                           column_count,
                                           input,
                                           output,
                                           0,
                                           row_count);
}

void matvec_q5_0_packed_q8_0_rows_unchecked(std::span<const Q5_0PackedBlock> rows,
                                            std::int64_t row_count,
                                            std::int64_t column_count,
                                            std::span<const Q8_0InputBlock> input,
                                            std::span<float> output,
                                            std::int64_t row_begin,
                                            std::int64_t row_end) noexcept {
#if defined(LCQI_ENABLE_AVX2)
    if (q5_0_packed_q8_0_avx2_available()) {
        matvec_q5_0_packed_q8_0_avx2_rows_unchecked(rows,
                                                    row_count,
                                                    column_count,
                                                    input,
                                                    output,
                                                    row_begin,
                                                    row_end);
        return;
    }
#else
    (void)row_count;
#endif
    const std::int64_t row_blocks = column_count / LCQI_Q5_0_BLOCK_VALUES;
    for (std::int64_t row = row_begin; row < row_end; ++row) {
        float sum = 0.0F;
        const Q5_0PackedBlock* row_blocks_begin =
            rows.data() + static_cast<std::size_t>(row * row_blocks);
        for (std::int64_t block = 0; block < row_blocks; ++block) {
            sum += dot_q5_0_packed_q8_0_scalar(
                row_blocks_begin[static_cast<std::size_t>(block)],
                input[static_cast<std::size_t>(block)]);
        }
        output[static_cast<std::size_t>(row)] = sum;
    }
}

void matvec_q5_0_packed_q8_0_batch_rows_unchecked(
    std::span<const Q5_0PackedBlock> rows,
    std::int64_t row_count,
    std::int64_t column_count,
    std::span<const Q8_0InputBlock> input,
    std::int64_t batch_size,
    std::span<float> output,
    std::int64_t output_stride,
    std::int64_t row_begin,
    std::int64_t row_end) noexcept {
#if defined(LCQI_ENABLE_AVX2)
    if (q5_0_packed_q8_0_avx2_available()) {
        matvec_q5_0_packed_q8_0_avx2_batch_rows_unchecked(rows,
                                                          row_count,
                                                          column_count,
                                                          input,
                                                          batch_size,
                                                          output,
                                                          output_stride,
                                                          row_begin,
                                                          row_end);
        return;
    }
#else
    (void)row_count;
#endif
    const std::int64_t row_blocks = column_count / LCQI_Q5_0_BLOCK_VALUES;
    for (std::int64_t row = row_begin; row < row_end; ++row) {
        const Q5_0PackedBlock* row_blocks_begin =
            rows.data() + static_cast<std::size_t>(row * row_blocks);
        for (std::int64_t batch = 0; batch < batch_size; ++batch) {
            const Q8_0InputBlock* batch_input =
                input.data() + static_cast<std::size_t>(batch * row_blocks);
            float sum = 0.0F;
            for (std::int64_t block = 0; block < row_blocks; ++block) {
                sum += dot_q5_0_packed_q8_0_scalar(
                    row_blocks_begin[static_cast<std::size_t>(block)],
                    batch_input[static_cast<std::size_t>(block)]);
            }
            output[static_cast<std::size_t>(batch * output_stride + row)] = sum;
        }
    }
}

}  // namespace lcqi

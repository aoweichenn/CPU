#include <lcqi/gpt2_reference.hpp>
#include <lcqi/f32_kernels.hpp>
#include <lcqi/ggml_matvec.hpp>
#include <lcqi/ggml_tensors.hpp>
#include <lcqi/gguf.hpp>
#include <lcqi/inference.hpp>
#include <lcqi/int8_kernels.hpp>
#include <lcqi/llama_gguf.hpp>
#include <lcqi/model.hpp>
#include <lcqi/q4_k.hpp>
#include <lcqi/reference_decoder.hpp>
#include <lcqi/safetensors.hpp>
#include <lcqi/tokenizer.hpp>

#include <array>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr float LCQI_TEST_EPSILON = 1.0e-5F;
constexpr std::int32_t LCQI_TEST_INPUT_SIZE = 4;
constexpr std::int32_t LCQI_TEST_HIDDEN_SIZE = 3;
constexpr std::int32_t LCQI_TEST_OUTPUT_SIZE = 2;
constexpr std::int32_t LCQI_TEST_PREDICTED_CLASS = 1;
constexpr float LCQI_TEST_LOGIT_0 = -0.48F;
constexpr float LCQI_TEST_LOGIT_1 = 1.06F;
constexpr float LCQI_TEST_HIDDEN_0 = 0.0F;
constexpr float LCQI_TEST_HIDDEN_1 = 0.4F;
constexpr float LCQI_TEST_HIDDEN_2 = 1.4F;
constexpr float LCQI_RMS_EXPECTED_0 = 0.848528F;
constexpr float LCQI_RMS_EXPECTED_1 = 0.565685F;
constexpr float LCQI_ATTENTION_EXPECTED_0 = 2.0F;
constexpr float LCQI_ATTENTION_EXPECTED_1 = 3.0F;
constexpr float LCQI_KERNEL_MAX_DIFF = 1.0e-5F;
constexpr float LCQI_F32_KERNEL_MAX_DIFF = 1.0e-4F;
constexpr std::int32_t LCQI_SIMD_TEST_BLOCK = 8;
constexpr std::size_t LCQI_F32_DOT_TEST_SIZE = 257;
constexpr std::size_t LCQI_F32_ROW_TEST_INPUT_SIZE = 65;
constexpr std::size_t LCQI_F32_ROW_TEST_OUTPUT_SIZE = 17;
constexpr std::int32_t LCQI_REFERENCE_DECODER_PREDICTED_TOKEN = 0;
constexpr std::size_t LCQI_REFERENCE_DECODER_VOCAB_SIZE = 5;
constexpr std::array<float, LCQI_REFERENCE_DECODER_VOCAB_SIZE> LCQI_REFERENCE_DECODER_LOGITS{
    0.352516979F,
    -0.126884326F,
    0.0797837451F,
    -0.29797405F,
    0.127030417F,
};
constexpr std::int32_t LCQI_REFERENCE_TRACE_TOKEN_COUNT = 3;
constexpr std::int32_t LCQI_REFERENCE_TRACE_HIDDEN_SIZE = 4;
constexpr std::int32_t LCQI_REFERENCE_TRACE_FIRST_TOKEN = 0;
constexpr std::uint64_t LCQI_TOKENIZER_HASH_EXPECTED = 13452902845388333734ULL;
constexpr std::int32_t LCQI_SAFETENSORS_PREFIX_BYTES = 8;
constexpr std::int32_t LCQI_TEST_BYTE_BITS = 8;
constexpr std::uint64_t LCQI_SAFETENSORS_VALID_PAYLOAD_BYTES = 48;
constexpr std::uint64_t LCQI_SAFETENSORS_EMBED_BEGIN = 0;
constexpr std::uint64_t LCQI_SAFETENSORS_EMBED_END = 24;
constexpr std::uint64_t LCQI_SAFETENSORS_QPROJ_BEGIN = 24;
constexpr std::uint64_t LCQI_SAFETENSORS_QPROJ_END = 48;
constexpr unsigned int LCQI_BYTE_MASK = 0xFFU;
constexpr std::int32_t LCQI_GPT2_PREDICTED_TOKEN = 3;
constexpr std::size_t LCQI_GPT2_VOCAB_SIZE = 6;
constexpr std::array<float, LCQI_GPT2_VOCAB_SIZE> LCQI_GPT2_LOGITS{
    0.407358F,
    -0.504049F,
    -0.107834F,
    0.840337F,
    -0.450123F,
    -0.156763F,
};
constexpr std::int32_t LCQI_GPT2_GENERATED_LENGTH = 5;
constexpr std::int32_t LCQI_GPT2_TOKENIZER_HELLO_ID = 7;
constexpr std::uint16_t LCQI_TEST_F16_ONE = 0x3C00U;
constexpr std::uint16_t LCQI_TEST_F16_HALF = 0x3800U;
constexpr std::uint16_t LCQI_TEST_F16_NEGATIVE_TWO = 0xC000U;
constexpr std::uint16_t LCQI_TEST_BF16_ONE_POINT_FIVE = 0x3FC0U;
constexpr std::uint16_t LCQI_TEST_BF16_NEGATIVE_HALF = 0xBF00U;
constexpr std::uint32_t LCQI_GGUF_TEST_VERSION = 3;
constexpr std::uint32_t LCQI_GGUF_TEST_ALIGNMENT = 32;
constexpr std::int64_t LCQI_GGUF_TEST_TENSOR_COUNT = 1;
constexpr std::int64_t LCQI_GGUF_TEST_METADATA_COUNT = 3;
constexpr std::int32_t LCQI_GGUF_TYPE_UINT32 = 4;
constexpr std::int32_t LCQI_GGUF_TYPE_FLOAT32 = 6;
constexpr std::int32_t LCQI_GGUF_TYPE_STRING = 8;
constexpr std::int32_t LCQI_GGUF_TYPE_ARRAY = 9;
constexpr std::int32_t LCQI_GGUF_TENSOR_TYPE_Q4_K = 12;
constexpr std::uint64_t LCQI_GGUF_TEST_TENSOR_OFFSET = 0;
constexpr float LCQI_Q4K_TEST_BLOCK_SCALE = 0.5F;
constexpr float LCQI_Q4K_TEST_BLOCK_MIN = 0.25F;
constexpr float LCQI_Q4K_Q8_DIFF_LIMIT = 0.08F;
constexpr float LCQI_GGML_MATVEC_MAX_DIFF = 1.0e-4F;
constexpr float LCQI_GGML_Q8_MATVEC_MAX_DIFF = 0.2F;
constexpr std::int32_t LCQI_GGML_TEST_Q5_HALF_VALUES = 16;
constexpr std::int32_t LCQI_GGML_TEST_Q5_ZERO_POINT = 16;
constexpr std::int32_t LCQI_GGML_TEST_Q6_LANE = 5;
constexpr std::int32_t LCQI_LLAMA_TEST_HIDDEN = 4;
constexpr std::int32_t LCQI_LLAMA_TEST_HEADS = 2;
constexpr std::int32_t LCQI_LLAMA_TEST_KV_HEADS = 1;
constexpr std::int32_t LCQI_LLAMA_TEST_HEAD_DIM = 2;
constexpr std::int32_t LCQI_LLAMA_TEST_INTERMEDIATE = 4;
constexpr std::int32_t LCQI_LLAMA_TEST_VOCAB = 4;
constexpr std::int32_t LCQI_LLAMA_TEST_CONTEXT = 8;
constexpr std::int32_t LCQI_LLAMA_TEST_LAYER_COUNT = 1;
constexpr std::int32_t LCQI_LLAMA_TEST_EXPECTED_TOKEN = 2;
constexpr float LCQI_LLAMA_TEST_SMALL_ATTENTION_VALUE = 0.1F;
constexpr float LCQI_LLAMA_TEST_SMALL_OUTPUT_SCALE = 0.1F;
constexpr std::int32_t LCQI_LLAMA_Q4_TEST_HIDDEN = 256;
constexpr std::int32_t LCQI_LLAMA_Q4_TEST_HEADS = 1;
constexpr std::int32_t LCQI_LLAMA_Q4_TEST_KV_HEADS = 1;
constexpr std::int32_t LCQI_LLAMA_Q4_TEST_HEAD_DIM = 256;
constexpr std::int32_t LCQI_LLAMA_Q4_TEST_INTERMEDIATE = 256;
constexpr std::int32_t LCQI_LLAMA_Q4_TEST_VOCAB = 2;
constexpr std::int32_t LCQI_LLAMA_Q4_TEST_CONTEXT = 4;
constexpr std::int32_t LCQI_LLAMA_Q4_TEST_LAYER_COUNT = 1;
constexpr const char* LCQI_TEST_LLAMA_GGML_DIRECT_ENV = "LCQI_LLAMA_GGML_DIRECT";
constexpr const char* LCQI_TEST_TRUE_ENV = "1";

struct KernelShapeCase {
    std::int32_t input_size = 0;
    std::int32_t output_size = 0;
    std::int32_t output_block_size = 0;
};

struct RawTensorFixture {
    std::string name;
    std::string dtype;
    std::vector<std::int64_t> shape;
    std::vector<std::uint8_t> bytes;
};

struct FloatTensorFixture {
    std::string name;
    std::vector<std::int64_t> shape;
    std::vector<float> values;
};

struct GgufTensorFixture {
    std::string name;
    std::vector<std::int64_t> shape;
    lcqi::GgmlType type = lcqi::GgmlType::f32;
    std::vector<std::uint8_t> bytes;
    std::uint64_t relative_offset = 0;
};

struct ScopedEnvVar {
    std::string name;
    std::string old_value;
    bool had_old_value = false;

    ScopedEnvVar(const char* env_name, const char* value) : name(env_name) {
        const char* old = std::getenv(env_name);
        if (old != nullptr) {
            this->old_value = old;
            this->had_old_value = true;
        }
        if (setenv(env_name, value, 1) != 0) {
            throw std::runtime_error("failed to set test environment variable");
        }
    }

    ScopedEnvVar(const ScopedEnvVar&) = delete;
    ScopedEnvVar& operator=(const ScopedEnvVar&) = delete;

    ~ScopedEnvVar() {
        if (this->had_old_value) {
            static_cast<void>(setenv(this->name.c_str(), this->old_value.c_str(), 1));
        } else {
            static_cast<void>(unsetenv(this->name.c_str()));
        }
    }
};

constexpr std::array<KernelShapeCase, 4> LCQI_KERNEL_SHAPE_CASES{{
    {17, 19, 8},
    {33, 7, 8},
    {64, 32, 16},
    {65, 31, 16},
}};

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void require_close(float actual, float expected, const char* message) {
    if (std::fabs(actual - expected) > LCQI_TEST_EPSILON) {
        throw std::runtime_error(message);
    }
}

std::filesystem::path test_model_path() {
    return std::filesystem::path(__FILE__).parent_path().parent_path() /
           "models" / "tiny_mlp_i8.txt";
}

std::filesystem::path test_tokenizer_path() {
    return std::filesystem::path(__FILE__).parent_path().parent_path() /
           "models" / "tiny_tokenizer.txt";
}

std::filesystem::path temp_file_path(const std::string& name) {
    return std::filesystem::temp_directory_path() / name;
}

void write_le_u64(std::ofstream& output, std::uint64_t value) {
    for (std::int32_t i = 0; i < LCQI_SAFETENSORS_PREFIX_BYTES; ++i) {
        const char byte = static_cast<char>(
            (value >> (LCQI_TEST_BYTE_BITS * i)) & LCQI_BYTE_MASK);
        output.write(&byte, 1);
    }
}

void append_le_u16(std::vector<std::uint8_t>& output, std::uint16_t value) {
    output.push_back(static_cast<std::uint8_t>(value & LCQI_BYTE_MASK));
    output.push_back(static_cast<std::uint8_t>((value >> LCQI_TEST_BYTE_BITS) & LCQI_BYTE_MASK));
}

void append_le_u32(std::vector<std::uint8_t>& output, std::uint32_t value) {
    for (std::int32_t i = 0; i < 4; ++i) {
        output.push_back(static_cast<std::uint8_t>(
            (value >> (LCQI_TEST_BYTE_BITS * i)) & LCQI_BYTE_MASK));
    }
}

void append_le_i32(std::vector<std::uint8_t>& output, std::int32_t value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    append_le_u32(output, bits);
}

void append_le_u64(std::vector<std::uint8_t>& output, std::uint64_t value) {
    for (std::int32_t i = 0; i < 8; ++i) {
        output.push_back(static_cast<std::uint8_t>(
            (value >> (LCQI_TEST_BYTE_BITS * i)) & LCQI_BYTE_MASK));
    }
}

void append_le_i64(std::vector<std::uint8_t>& output, std::int64_t value) {
    std::uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    append_le_u64(output, bits);
}

void append_gguf_string(std::vector<std::uint8_t>& output, const std::string& text) {
    append_le_u64(output, static_cast<std::uint64_t>(text.size()));
    output.insert(output.end(), text.begin(), text.end());
}

void append_le_f32(std::vector<std::uint8_t>& output, float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    for (std::int32_t i = 0; i < 4; ++i) {
        output.push_back(static_cast<std::uint8_t>(
            (bits >> (LCQI_TEST_BYTE_BITS * i)) & LCQI_BYTE_MASK));
    }
}

void write_safetensors_fixture(const std::filesystem::path& path,
                               const std::string& header,
                               std::uint64_t payload_bytes) {
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        throw std::runtime_error("cannot create safetensors test fixture");
    }
    write_le_u64(output, static_cast<std::uint64_t>(header.size()));
    output.write(header.data(), static_cast<std::streamsize>(header.size()));
    for (std::uint64_t i = 0; i < payload_bytes; ++i) {
        const char byte = static_cast<char>(i & 0xFFU);
        output.write(&byte, 1);
    }
}

std::string json_shape(std::span<const std::int64_t> shape) {
    std::ostringstream stream;
    stream << '[';
    for (std::size_t index = 0; index < shape.size(); ++index) {
        if (index != 0) {
            stream << ',';
        }
        stream << shape[index];
    }
    stream << ']';
    return stream.str();
}

void write_safetensors_raw_fixture(const std::filesystem::path& path,
                                   std::span<const RawTensorFixture> tensors) {
    std::ostringstream header;
    header << "{\"__metadata__\":{\"format\":\"lcqi-test\"}";
    std::uint64_t offset = 0;
    for (const RawTensorFixture& tensor : tensors) {
        header << ",\"" << tensor.name << "\":{\"dtype\":\"" << tensor.dtype
               << "\",\"shape\":" << json_shape(tensor.shape)
               << ",\"data_offsets\":[" << offset << ','
               << offset + tensor.bytes.size() << "]}";
        offset += tensor.bytes.size();
    }
    header << '}';

    std::ofstream output(path, std::ios::binary);
    if (!output) {
        throw std::runtime_error("cannot create safetensors raw test fixture");
    }
    const std::string header_text = header.str();
    write_le_u64(output, static_cast<std::uint64_t>(header_text.size()));
    output.write(header_text.data(), static_cast<std::streamsize>(header_text.size()));
    for (const RawTensorFixture& tensor : tensors) {
        output.write(reinterpret_cast<const char*>(tensor.bytes.data()),
                     static_cast<std::streamsize>(tensor.bytes.size()));
    }
}

std::vector<std::uint8_t> f32_payload(std::span<const float> values) {
    std::vector<std::uint8_t> bytes;
    bytes.reserve(values.size() * 4);
    for (const float value : values) {
        append_le_f32(bytes, value);
    }
    return bytes;
}

std::vector<float> transpose_linear_to_hf_conv1d(const lcqi::Gpt2LinearF32& linear) {
    std::vector<float> transposed(
        static_cast<std::size_t>(linear.input_size) *
            static_cast<std::size_t>(linear.output_size),
        0.0F);
    for (std::int32_t out = 0; out < linear.output_size; ++out) {
        for (std::int32_t in = 0; in < linear.input_size; ++in) {
            transposed[static_cast<std::size_t>(in) *
                           static_cast<std::size_t>(linear.output_size) +
                       static_cast<std::size_t>(out)] =
                linear.weights[static_cast<std::size_t>(out) *
                                   static_cast<std::size_t>(linear.input_size) +
                               static_cast<std::size_t>(in)];
        }
    }
    return transposed;
}

void add_f32_tensor(std::vector<RawTensorFixture>& tensors,
                    std::string name,
                    std::vector<std::int64_t> shape,
                    std::span<const float> values) {
    tensors.push_back(RawTensorFixture{
        std::move(name),
        "F32",
        std::move(shape),
        f32_payload(values),
    });
}

void write_text_file(const std::filesystem::path& path, const std::string& text) {
    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("cannot create text fixture");
    }
    output << text;
}

std::vector<std::uint8_t> make_q4_k_test_block() {
    std::vector<std::uint8_t> block(
        static_cast<std::size_t>(lcqi::LCQI_Q4_K_BLOCK_BYTES),
        0);
    block[0] = 0x00U;
    block[1] = 0x38U;
    block[2] = 0x00U;
    block[3] = 0x34U;

    std::array<std::uint8_t, static_cast<std::size_t>(lcqi::LCQI_Q4_K_SUBBLOCKS)> scales{
        1, 2, 3, 4, 5, 6, 7, 8,
    };
    std::array<std::uint8_t, static_cast<std::size_t>(lcqi::LCQI_Q4_K_SUBBLOCKS)> mins{
        1, 1, 2, 2, 3, 3, 4, 4,
    };
    for (std::size_t index = 0; index < 4; ++index) {
        block[4 + index] = static_cast<std::uint8_t>(
            (scales[index] & 0x3FU) | ((scales[index + 4] >> 4U) << 6U));
        block[8 + index] = static_cast<std::uint8_t>(
            (mins[index] & 0x3FU) | ((mins[index + 4] >> 4U) << 6U));
        block[12 + index] = static_cast<std::uint8_t>(
            (scales[index + 4] & 0x0FU) | ((mins[index + 4] & 0x0FU) << 4U));
    }

    for (std::int32_t pair = 0; pair < 4; ++pair) {
        const std::int32_t low_subblock = pair * 2;
        const std::int32_t high_subblock = low_subblock + 1;
        for (std::int32_t index = 0; index < lcqi::LCQI_Q4_K_SUBBLOCK_VALUES; ++index) {
            const std::uint8_t low = static_cast<std::uint8_t>(
                (low_subblock + index) & 0x0F);
            const std::uint8_t high = static_cast<std::uint8_t>(
                (high_subblock + index + 1) & 0x0F);
            block[static_cast<std::size_t>(16 + pair * 32 + index)] =
                static_cast<std::uint8_t>(low | (high << 4U));
        }
    }
    return block;
}

std::vector<float> expected_q4_k_test_values() {
    const std::array<float, static_cast<std::size_t>(lcqi::LCQI_Q4_K_SUBBLOCKS)> scales{
        1, 2, 3, 4, 5, 6, 7, 8,
    };
    const std::array<float, static_cast<std::size_t>(lcqi::LCQI_Q4_K_SUBBLOCKS)> mins{
        1, 1, 2, 2, 3, 3, 4, 4,
    };
    std::vector<float> values(
        static_cast<std::size_t>(lcqi::LCQI_QK_K_BLOCK_VALUES),
        0.0F);
    for (std::int32_t subblock = 0; subblock < lcqi::LCQI_Q4_K_SUBBLOCKS; ++subblock) {
        for (std::int32_t index = 0; index < lcqi::LCQI_Q4_K_SUBBLOCK_VALUES; ++index) {
            const std::int32_t quantized =
                (subblock % 2 == 0)
                    ? ((subblock + index) & 0x0F)
                    : ((subblock + index + 1) & 0x0F);
            values[static_cast<std::size_t>(
                subblock * lcqi::LCQI_Q4_K_SUBBLOCK_VALUES + index)] =
                LCQI_Q4K_TEST_BLOCK_SCALE * scales[static_cast<std::size_t>(subblock)] *
                    static_cast<float>(quantized) -
                LCQI_Q4K_TEST_BLOCK_MIN * mins[static_cast<std::size_t>(subblock)];
        }
    }
    return values;
}

std::vector<std::uint8_t> make_q8_0_test_block() {
    std::vector<std::uint8_t> block(
        static_cast<std::size_t>(lcqi::LCQI_Q8_0_BLOCK_BYTES),
        0);
    block[0] = static_cast<std::uint8_t>(LCQI_TEST_F16_HALF & LCQI_BYTE_MASK);
    block[1] = static_cast<std::uint8_t>((LCQI_TEST_F16_HALF >> LCQI_TEST_BYTE_BITS) &
                                         LCQI_BYTE_MASK);
    for (std::int32_t index = 0; index < lcqi::LCQI_Q8_0_BLOCK_VALUES; ++index) {
        block[static_cast<std::size_t>(2 + index)] =
            static_cast<std::uint8_t>(static_cast<std::int8_t>(index - 16));
    }
    return block;
}

std::vector<std::uint8_t> make_q5_0_test_block() {
    std::vector<std::uint8_t> block(
        static_cast<std::size_t>(lcqi::LCQI_Q5_0_BLOCK_BYTES),
        0);
    block[0] = static_cast<std::uint8_t>(LCQI_TEST_F16_HALF & LCQI_BYTE_MASK);
    block[1] = static_cast<std::uint8_t>((LCQI_TEST_F16_HALF >> LCQI_TEST_BYTE_BITS) &
                                         LCQI_BYTE_MASK);

    std::uint32_t qh = 0;
    for (std::int32_t index = 0; index < lcqi::LCQI_Q5_0_BLOCK_VALUES / 2; ++index) {
        const std::int32_t first_quantized = index - LCQI_GGML_TEST_Q5_ZERO_POINT;
        const std::int32_t second_quantized = index;
        const std::uint8_t first_encoded =
            static_cast<std::uint8_t>(first_quantized + LCQI_GGML_TEST_Q5_ZERO_POINT);
        const std::uint8_t second_encoded =
            static_cast<std::uint8_t>(second_quantized + LCQI_GGML_TEST_Q5_ZERO_POINT);
        if ((first_encoded & 0x10U) != 0U) {
            qh |= (1U << index);
        }
        if ((second_encoded & 0x10U) != 0U) {
            qh |= (1U << (index + LCQI_GGML_TEST_Q5_HALF_VALUES));
        }
        const std::uint8_t low = static_cast<std::uint8_t>(first_encoded & 0x0FU);
        const std::uint8_t high = static_cast<std::uint8_t>(second_encoded & 0x0FU);
        block[static_cast<std::size_t>(6 + index)] =
            static_cast<std::uint8_t>(low | (high << 4U));
    }
    block[2] = static_cast<std::uint8_t>(qh & LCQI_BYTE_MASK);
    block[3] = static_cast<std::uint8_t>((qh >> LCQI_TEST_BYTE_BITS) & LCQI_BYTE_MASK);
    block[4] = static_cast<std::uint8_t>((qh >> (LCQI_TEST_BYTE_BITS * 2)) & LCQI_BYTE_MASK);
    block[5] = static_cast<std::uint8_t>((qh >> (LCQI_TEST_BYTE_BITS * 3)) & LCQI_BYTE_MASK);
    return block;
}

std::vector<std::uint8_t> make_q6_k_test_block() {
    std::vector<std::uint8_t> block(
        static_cast<std::size_t>(lcqi::LCQI_Q6_K_BLOCK_BYTES),
        0);
    const std::size_t ql_offset = 0;
    const std::size_t qh_offset = static_cast<std::size_t>(lcqi::LCQI_Q6_K_BLOCK_VALUES / 2);
    const std::size_t scales_offset =
        qh_offset + static_cast<std::size_t>(lcqi::LCQI_Q6_K_BLOCK_VALUES / 4);
    for (std::int32_t index = 0; index < 16; ++index) {
        block[scales_offset + static_cast<std::size_t>(index)] = 1;
    }
    block[static_cast<std::size_t>(lcqi::LCQI_Q6_K_BLOCK_BYTES - 2)] =
        static_cast<std::uint8_t>(LCQI_TEST_F16_ONE & LCQI_BYTE_MASK);
    block[static_cast<std::size_t>(lcqi::LCQI_Q6_K_BLOCK_BYTES - 1)] =
        static_cast<std::uint8_t>((LCQI_TEST_F16_ONE >> LCQI_TEST_BYTE_BITS) &
                                 LCQI_BYTE_MASK);

    const auto store_q6 = [&block, ql_offset, qh_offset](std::int32_t position,
                                                        std::uint8_t encoded) {
        const std::int32_t half = position / 128;
        const std::int32_t local = position % 128;
        const std::size_t ql_base = ql_offset + static_cast<std::size_t>(half * 64);
        const std::size_t qh_base = qh_offset + static_cast<std::size_t>(half * 32);
        const std::uint8_t low = encoded & 0x0FU;
        const std::uint8_t high = static_cast<std::uint8_t>((encoded >> 4U) & 0x03U);
        if (local < 32) {
            block[ql_base + static_cast<std::size_t>(local)] =
                static_cast<std::uint8_t>(
                    (block[ql_base + static_cast<std::size_t>(local)] & 0xF0U) | low);
            block[qh_base + static_cast<std::size_t>(local)] =
                static_cast<std::uint8_t>(
                    (block[qh_base + static_cast<std::size_t>(local)] & 0xFCU) | high);
        } else if (local < 64) {
            const std::size_t lane = static_cast<std::size_t>(local - 32);
            block[ql_base + static_cast<std::size_t>(32) + lane] =
                static_cast<std::uint8_t>(
                    (block[ql_base + static_cast<std::size_t>(32) + lane] & 0xF0U) | low);
            block[qh_base + lane] =
                static_cast<std::uint8_t>((block[qh_base + lane] & 0xF3U) | (high << 2U));
        } else if (local < 96) {
            const std::size_t lane = static_cast<std::size_t>(local - 64);
            block[ql_base + lane] =
                static_cast<std::uint8_t>(
                    (block[ql_base + lane] & 0x0FU) | (low << 4U));
            block[qh_base + lane] =
                static_cast<std::uint8_t>((block[qh_base + lane] & 0xCFU) | (high << 4U));
        } else {
            const std::size_t lane = static_cast<std::size_t>(local - 96);
            block[ql_base + static_cast<std::size_t>(32) + lane] =
                static_cast<std::uint8_t>(
                    (block[ql_base + static_cast<std::size_t>(32) + lane] & 0x0FU) |
                    (low << 4U));
            block[qh_base + lane] =
                static_cast<std::uint8_t>((block[qh_base + lane] & 0x3FU) | (high << 6U));
        }
    };

    store_q6(LCQI_GGML_TEST_Q6_LANE, 35);
    store_q6(32 + LCQI_GGML_TEST_Q6_LANE, 31);
    store_q6(64 + LCQI_GGML_TEST_Q6_LANE, 63);
    store_q6(96 + LCQI_GGML_TEST_Q6_LANE, 0);
    store_q6(128 + LCQI_GGML_TEST_Q6_LANE, 36);
    return block;
}

void write_gguf_q4_k_fixture(const std::filesystem::path& path) {
    std::vector<std::uint8_t> bytes;
    bytes.push_back('G');
    bytes.push_back('G');
    bytes.push_back('U');
    bytes.push_back('F');
    append_le_u32(bytes, LCQI_GGUF_TEST_VERSION);
    append_le_i64(bytes, LCQI_GGUF_TEST_TENSOR_COUNT);
    append_le_i64(bytes, LCQI_GGUF_TEST_METADATA_COUNT);

    append_gguf_string(bytes, "general.alignment");
    append_le_i32(bytes, LCQI_GGUF_TYPE_UINT32);
    append_le_u32(bytes, LCQI_GGUF_TEST_ALIGNMENT);
    append_gguf_string(bytes, "general.architecture");
    append_le_i32(bytes, LCQI_GGUF_TYPE_STRING);
    append_gguf_string(bytes, "lcqi-test");
    append_gguf_string(bytes, "tokenizer.ggml.tokens");
    append_le_i32(bytes, LCQI_GGUF_TYPE_ARRAY);
    append_le_i32(bytes, LCQI_GGUF_TYPE_STRING);
    append_le_u64(bytes, 2);
    append_gguf_string(bytes, "<bos>");
    append_gguf_string(bytes, "hello");

    append_gguf_string(bytes, "blk.0.ffn_up.weight");
    append_le_u32(bytes, 2);
    append_le_i64(bytes, lcqi::LCQI_QK_K_BLOCK_VALUES);
    append_le_i64(bytes, 1);
    append_le_i32(bytes, LCQI_GGUF_TENSOR_TYPE_Q4_K);
    append_le_u64(bytes, LCQI_GGUF_TEST_TENSOR_OFFSET);
    while (bytes.size() % LCQI_GGUF_TEST_ALIGNMENT != 0) {
        bytes.push_back(0);
    }
    const std::vector<std::uint8_t> block = make_q4_k_test_block();
    bytes.insert(bytes.end(), block.begin(), block.end());

    std::ofstream output(path, std::ios::binary);
    if (!output) {
        throw std::runtime_error("cannot create GGUF fixture");
    }
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
}

void append_gguf_uint32_metadata(std::vector<std::uint8_t>& bytes,
                                 const std::string& key,
                                 std::uint32_t value) {
    append_gguf_string(bytes, key);
    append_le_i32(bytes, LCQI_GGUF_TYPE_UINT32);
    append_le_u32(bytes, value);
}

void append_gguf_float32_metadata(std::vector<std::uint8_t>& bytes,
                                  const std::string& key,
                                  float value) {
    append_gguf_string(bytes, key);
    append_le_i32(bytes, LCQI_GGUF_TYPE_FLOAT32);
    append_le_f32(bytes, value);
}

void append_gguf_string_metadata(std::vector<std::uint8_t>& bytes,
                                 const std::string& key,
                                 const std::string& value) {
    append_gguf_string(bytes, key);
    append_le_i32(bytes, LCQI_GGUF_TYPE_STRING);
    append_gguf_string(bytes, value);
}

void write_gguf_fixture(const std::filesystem::path& path,
                        std::span<GgufTensorFixture> tensors,
                        const std::vector<std::uint8_t>& metadata_bytes) {
    std::uint64_t relative_offset = 0;
    for (GgufTensorFixture& tensor : tensors) {
        tensor.relative_offset = relative_offset;
        relative_offset += tensor.bytes.size();
        while (relative_offset % LCQI_GGUF_TEST_ALIGNMENT != 0) {
            ++relative_offset;
        }
    }

    std::vector<std::uint8_t> bytes;
    bytes.push_back('G');
    bytes.push_back('G');
    bytes.push_back('U');
    bytes.push_back('F');
    append_le_u32(bytes, LCQI_GGUF_TEST_VERSION);
    append_le_u64(bytes, static_cast<std::uint64_t>(tensors.size()));
    append_le_u64(bytes, 14);
    bytes.insert(bytes.end(), metadata_bytes.begin(), metadata_bytes.end());

    for (const GgufTensorFixture& tensor : tensors) {
        append_gguf_string(bytes, tensor.name);
        append_le_u32(bytes, static_cast<std::uint32_t>(tensor.shape.size()));
        for (const std::int64_t extent : tensor.shape) {
            append_le_i64(bytes, extent);
        }
        append_le_i32(bytes, static_cast<std::int32_t>(tensor.type));
        append_le_u64(bytes, tensor.relative_offset);
    }
    while (bytes.size() % LCQI_GGUF_TEST_ALIGNMENT != 0) {
        bytes.push_back(0);
    }
    std::uint64_t written_relative = 0;
    for (const GgufTensorFixture& tensor : tensors) {
        while (written_relative < tensor.relative_offset) {
            bytes.push_back(0);
            ++written_relative;
        }
        bytes.insert(bytes.end(), tensor.bytes.begin(), tensor.bytes.end());
        written_relative += tensor.bytes.size();
    }

    std::ofstream output(path, std::ios::binary);
    if (!output) {
        throw std::runtime_error("cannot create GGUF fixture");
    }
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
}

void add_gguf_f32_tensor(std::vector<GgufTensorFixture>& tensors,
                         std::string name,
                         std::vector<std::int64_t> shape,
                         std::span<const float> values) {
    tensors.push_back(GgufTensorFixture{
        std::move(name),
        std::move(shape),
        lcqi::GgmlType::f32,
        f32_payload(values),
        0,
    });
}

void add_gguf_q4_k_tensor(std::vector<GgufTensorFixture>& tensors,
                          std::string name,
                          std::vector<std::int64_t> shape,
                          const std::vector<std::uint8_t>& bytes) {
    tensors.push_back(GgufTensorFixture{
        std::move(name),
        std::move(shape),
        lcqi::GgmlType::q4_k,
        bytes,
        0,
    });
}

void add_gguf_quantized_tensor(std::vector<GgufTensorFixture>& tensors,
                               std::string name,
                               std::vector<std::int64_t> shape,
                               lcqi::GgmlType type,
                               const std::vector<std::uint8_t>& bytes) {
    tensors.push_back(GgufTensorFixture{
        std::move(name),
        std::move(shape),
        type,
        bytes,
        0,
    });
}

std::vector<std::uint8_t> repeated_ggml_block(const std::vector<std::uint8_t>& block,
                                              std::int32_t input_size,
                                              std::int32_t output_size,
                                              std::int64_t block_values) {
    const std::size_t block_count =
        static_cast<std::size_t>((input_size / block_values) * output_size);
    std::vector<std::uint8_t> bytes;
    bytes.reserve(block.size() * block_count);
    for (std::size_t index = 0; index < block_count; ++index) {
        bytes.insert(bytes.end(), block.begin(), block.end());
    }
    return bytes;
}

void write_tiny_llama_gguf_fixture(const std::filesystem::path& path) {
    std::vector<std::uint8_t> metadata;
    append_gguf_uint32_metadata(metadata, "general.alignment", LCQI_GGUF_TEST_ALIGNMENT);
    append_gguf_string_metadata(metadata, "general.architecture", "llama");
    append_gguf_string_metadata(metadata, "general.name", "lcqi tiny llama");
    append_gguf_uint32_metadata(metadata, "llama.embedding_length", LCQI_LLAMA_TEST_HIDDEN);
    append_gguf_uint32_metadata(metadata, "llama.block_count", LCQI_LLAMA_TEST_LAYER_COUNT);
    append_gguf_uint32_metadata(metadata, "llama.feed_forward_length", LCQI_LLAMA_TEST_INTERMEDIATE);
    append_gguf_uint32_metadata(metadata, "llama.attention.head_count", LCQI_LLAMA_TEST_HEADS);
    append_gguf_uint32_metadata(metadata, "llama.attention.head_count_kv", LCQI_LLAMA_TEST_KV_HEADS);
    append_gguf_uint32_metadata(metadata, "llama.rope.dimension_count", LCQI_LLAMA_TEST_HEAD_DIM);
    append_gguf_float32_metadata(metadata, "llama.rope.freq_base", 10000.0F);
    append_gguf_uint32_metadata(metadata, "llama.context_length", LCQI_LLAMA_TEST_CONTEXT);
    append_gguf_uint32_metadata(metadata, "llama.vocab_size", LCQI_LLAMA_TEST_VOCAB);
    append_gguf_float32_metadata(metadata, "llama.attention.layer_norm_rms_epsilon", 1.0e-5F);
    append_gguf_uint32_metadata(metadata, "tokenizer.ggml.eos_token_id", 3);

    std::vector<GgufTensorFixture> tensors;
    const std::vector<float> embedding{
        1.0F, 0.0F, 0.0F, 0.0F,
        0.0F, 1.0F, 0.0F, 0.0F,
        0.0F, 2.0F, 0.0F, 0.0F,
        0.0F, 0.0F, 1.0F, 0.0F,
    };
    add_gguf_f32_tensor(tensors,
                        "token_embd.weight",
                        {LCQI_LLAMA_TEST_HIDDEN, LCQI_LLAMA_TEST_VOCAB},
                        embedding);
    add_gguf_f32_tensor(tensors,
                        "output_norm.weight",
                        {LCQI_LLAMA_TEST_HIDDEN},
                        std::vector<float>(LCQI_LLAMA_TEST_HIDDEN, 1.0F));
    add_gguf_f32_tensor(tensors,
                        "blk.0.attn_norm.weight",
                        {LCQI_LLAMA_TEST_HIDDEN},
                        std::vector<float>(LCQI_LLAMA_TEST_HIDDEN, 1.0F));
    add_gguf_f32_tensor(tensors,
                        "blk.0.ffn_norm.weight",
                        {LCQI_LLAMA_TEST_HIDDEN},
                        std::vector<float>(LCQI_LLAMA_TEST_HIDDEN, 1.0F));
    const std::vector<float> hidden_to_hidden(
        static_cast<std::size_t>(LCQI_LLAMA_TEST_HIDDEN * LCQI_LLAMA_TEST_HIDDEN),
        0.0F);
    std::vector<float> hidden_to_kv(
        static_cast<std::size_t>(LCQI_LLAMA_TEST_HIDDEN * LCQI_LLAMA_TEST_HEAD_DIM),
        0.0F);
    hidden_to_kv[1] = LCQI_LLAMA_TEST_SMALL_ATTENTION_VALUE;
    std::vector<float> attention_output = hidden_to_hidden;
    attention_output[0] = LCQI_LLAMA_TEST_SMALL_OUTPUT_SCALE;
    attention_output[10] = LCQI_LLAMA_TEST_SMALL_OUTPUT_SCALE;
    const std::vector<float> hidden_to_intermediate(
        static_cast<std::size_t>(LCQI_LLAMA_TEST_HIDDEN * LCQI_LLAMA_TEST_INTERMEDIATE),
        0.0F);
    const std::vector<float> intermediate_to_hidden(
        static_cast<std::size_t>(LCQI_LLAMA_TEST_INTERMEDIATE * LCQI_LLAMA_TEST_HIDDEN),
        0.0F);
    add_gguf_f32_tensor(tensors,
                        "blk.0.attn_q.weight",
                        {LCQI_LLAMA_TEST_HIDDEN, LCQI_LLAMA_TEST_HIDDEN},
                        hidden_to_hidden);
    add_gguf_f32_tensor(tensors,
                        "blk.0.attn_k.weight",
                        {LCQI_LLAMA_TEST_HIDDEN, LCQI_LLAMA_TEST_HEAD_DIM},
                        hidden_to_kv);
    add_gguf_f32_tensor(tensors,
                        "blk.0.attn_v.weight",
                        {LCQI_LLAMA_TEST_HIDDEN, LCQI_LLAMA_TEST_HEAD_DIM},
                        hidden_to_kv);
    add_gguf_f32_tensor(tensors,
                        "blk.0.attn_output.weight",
                        {LCQI_LLAMA_TEST_HIDDEN, LCQI_LLAMA_TEST_HIDDEN},
                        attention_output);
    add_gguf_f32_tensor(tensors,
                        "blk.0.ffn_gate.weight",
                        {LCQI_LLAMA_TEST_HIDDEN, LCQI_LLAMA_TEST_INTERMEDIATE},
                        hidden_to_intermediate);
    add_gguf_f32_tensor(tensors,
                        "blk.0.ffn_up.weight",
                        {LCQI_LLAMA_TEST_HIDDEN, LCQI_LLAMA_TEST_INTERMEDIATE},
                        hidden_to_intermediate);
    add_gguf_f32_tensor(tensors,
                        "blk.0.ffn_down.weight",
                        {LCQI_LLAMA_TEST_INTERMEDIATE, LCQI_LLAMA_TEST_HIDDEN},
                        intermediate_to_hidden);
    write_gguf_fixture(path, tensors, metadata);
}

void write_tiny_llama_ggml_direct_gguf_fixture(const std::filesystem::path& path) {
    std::vector<std::uint8_t> metadata;
    append_gguf_uint32_metadata(metadata, "general.alignment", LCQI_GGUF_TEST_ALIGNMENT);
    append_gguf_string_metadata(metadata, "general.architecture", "llama");
    append_gguf_string_metadata(metadata, "general.name", "lcqi ggml direct tiny llama");
    append_gguf_uint32_metadata(metadata, "llama.embedding_length", LCQI_LLAMA_Q4_TEST_HIDDEN);
    append_gguf_uint32_metadata(metadata, "llama.block_count", LCQI_LLAMA_Q4_TEST_LAYER_COUNT);
    append_gguf_uint32_metadata(metadata,
                                "llama.feed_forward_length",
                                LCQI_LLAMA_Q4_TEST_INTERMEDIATE);
    append_gguf_uint32_metadata(metadata, "llama.attention.head_count", LCQI_LLAMA_Q4_TEST_HEADS);
    append_gguf_uint32_metadata(metadata,
                                "llama.attention.head_count_kv",
                                LCQI_LLAMA_Q4_TEST_KV_HEADS);
    append_gguf_uint32_metadata(metadata,
                                "llama.rope.dimension_count",
                                LCQI_LLAMA_Q4_TEST_HEAD_DIM);
    append_gguf_float32_metadata(metadata, "llama.rope.freq_base", 10000.0F);
    append_gguf_uint32_metadata(metadata, "llama.context_length", LCQI_LLAMA_Q4_TEST_CONTEXT);
    append_gguf_uint32_metadata(metadata, "llama.vocab_size", LCQI_LLAMA_Q4_TEST_VOCAB);
    append_gguf_float32_metadata(metadata, "llama.attention.layer_norm_rms_epsilon", 1.0e-5F);
    append_gguf_uint32_metadata(metadata, "tokenizer.ggml.eos_token_id", 1);

    std::vector<GgufTensorFixture> tensors;
    std::vector<float> embedding(
        static_cast<std::size_t>(LCQI_LLAMA_Q4_TEST_HIDDEN * LCQI_LLAMA_Q4_TEST_VOCAB),
        0.0F);
    embedding[0] = 1.0F;
    embedding[static_cast<std::size_t>(LCQI_LLAMA_Q4_TEST_HIDDEN)] = 0.5F;
    add_gguf_f32_tensor(tensors,
                        "token_embd.weight",
                        {LCQI_LLAMA_Q4_TEST_HIDDEN, LCQI_LLAMA_Q4_TEST_VOCAB},
                        embedding);
    add_gguf_f32_tensor(tensors,
                        "output_norm.weight",
                        {LCQI_LLAMA_Q4_TEST_HIDDEN},
                        std::vector<float>(LCQI_LLAMA_Q4_TEST_HIDDEN, 1.0F));
    add_gguf_f32_tensor(tensors,
                        "blk.0.attn_norm.weight",
                        {LCQI_LLAMA_Q4_TEST_HIDDEN},
                        std::vector<float>(LCQI_LLAMA_Q4_TEST_HIDDEN, 1.0F));
    add_gguf_f32_tensor(tensors,
                        "blk.0.ffn_norm.weight",
                        {LCQI_LLAMA_Q4_TEST_HIDDEN},
                        std::vector<float>(LCQI_LLAMA_Q4_TEST_HIDDEN, 1.0F));

    const std::vector<std::uint8_t> q5_matrix = repeated_ggml_block(
        std::vector<std::uint8_t>(static_cast<std::size_t>(lcqi::LCQI_Q5_0_BLOCK_BYTES), 0),
        LCQI_LLAMA_Q4_TEST_HIDDEN,
        LCQI_LLAMA_Q4_TEST_HIDDEN,
        lcqi::LCQI_Q5_0_BLOCK_VALUES);
    const std::vector<std::uint8_t> q8_matrix = repeated_ggml_block(
        std::vector<std::uint8_t>(static_cast<std::size_t>(lcqi::LCQI_Q8_0_BLOCK_BYTES), 0),
        LCQI_LLAMA_Q4_TEST_HIDDEN,
        LCQI_LLAMA_Q4_TEST_HIDDEN,
        lcqi::LCQI_Q8_0_BLOCK_VALUES);
    const std::vector<std::uint8_t> q6_matrix = repeated_ggml_block(
        std::vector<std::uint8_t>(static_cast<std::size_t>(lcqi::LCQI_Q6_K_BLOCK_BYTES), 0),
        LCQI_LLAMA_Q4_TEST_INTERMEDIATE,
        LCQI_LLAMA_Q4_TEST_HIDDEN,
        lcqi::LCQI_Q6_K_BLOCK_VALUES);

    add_gguf_quantized_tensor(tensors,
                              "blk.0.attn_q.weight",
                              {LCQI_LLAMA_Q4_TEST_HIDDEN, LCQI_LLAMA_Q4_TEST_HIDDEN},
                              lcqi::GgmlType::q5_0,
                              q5_matrix);
    add_gguf_quantized_tensor(tensors,
                              "blk.0.attn_k.weight",
                              {LCQI_LLAMA_Q4_TEST_HIDDEN, LCQI_LLAMA_Q4_TEST_HEAD_DIM},
                              lcqi::GgmlType::q5_0,
                              q5_matrix);
    add_gguf_quantized_tensor(tensors,
                              "blk.0.attn_v.weight",
                              {LCQI_LLAMA_Q4_TEST_HIDDEN, LCQI_LLAMA_Q4_TEST_HEAD_DIM},
                              lcqi::GgmlType::q8_0,
                              q8_matrix);
    add_gguf_quantized_tensor(tensors,
                              "blk.0.attn_output.weight",
                              {LCQI_LLAMA_Q4_TEST_HIDDEN, LCQI_LLAMA_Q4_TEST_HIDDEN},
                              lcqi::GgmlType::q5_0,
                              q5_matrix);
    add_gguf_quantized_tensor(tensors,
                              "blk.0.ffn_gate.weight",
                              {LCQI_LLAMA_Q4_TEST_HIDDEN, LCQI_LLAMA_Q4_TEST_INTERMEDIATE},
                              lcqi::GgmlType::q5_0,
                              q5_matrix);
    add_gguf_quantized_tensor(tensors,
                              "blk.0.ffn_up.weight",
                              {LCQI_LLAMA_Q4_TEST_HIDDEN, LCQI_LLAMA_Q4_TEST_INTERMEDIATE},
                              lcqi::GgmlType::q8_0,
                              q8_matrix);
    add_gguf_quantized_tensor(tensors,
                              "blk.0.ffn_down.weight",
                              {LCQI_LLAMA_Q4_TEST_INTERMEDIATE, LCQI_LLAMA_Q4_TEST_HIDDEN},
                              lcqi::GgmlType::q6_k,
                              q6_matrix);
    write_gguf_fixture(path, tensors, metadata);
}

void write_tiny_llama_q4_direct_gguf_fixture(const std::filesystem::path& path) {
    std::vector<std::uint8_t> metadata;
    append_gguf_uint32_metadata(metadata, "general.alignment", LCQI_GGUF_TEST_ALIGNMENT);
    append_gguf_string_metadata(metadata, "general.architecture", "llama");
    append_gguf_string_metadata(metadata, "general.name", "lcqi q4 direct tiny llama");
    append_gguf_uint32_metadata(metadata, "llama.embedding_length", LCQI_LLAMA_Q4_TEST_HIDDEN);
    append_gguf_uint32_metadata(metadata, "llama.block_count", LCQI_LLAMA_Q4_TEST_LAYER_COUNT);
    append_gguf_uint32_metadata(metadata, "llama.feed_forward_length", LCQI_LLAMA_Q4_TEST_INTERMEDIATE);
    append_gguf_uint32_metadata(metadata, "llama.attention.head_count", LCQI_LLAMA_Q4_TEST_HEADS);
    append_gguf_uint32_metadata(metadata, "llama.attention.head_count_kv", LCQI_LLAMA_Q4_TEST_KV_HEADS);
    append_gguf_uint32_metadata(metadata, "llama.rope.dimension_count", LCQI_LLAMA_Q4_TEST_HEAD_DIM);
    append_gguf_float32_metadata(metadata, "llama.rope.freq_base", 10000.0F);
    append_gguf_uint32_metadata(metadata, "llama.context_length", LCQI_LLAMA_Q4_TEST_CONTEXT);
    append_gguf_uint32_metadata(metadata, "llama.vocab_size", LCQI_LLAMA_Q4_TEST_VOCAB);
    append_gguf_float32_metadata(metadata, "llama.attention.layer_norm_rms_epsilon", 1.0e-5F);
    append_gguf_uint32_metadata(metadata, "tokenizer.ggml.eos_token_id", 1);

    std::vector<GgufTensorFixture> tensors;
    std::vector<float> embedding(
        static_cast<std::size_t>(LCQI_LLAMA_Q4_TEST_HIDDEN * LCQI_LLAMA_Q4_TEST_VOCAB),
        0.0F);
    embedding[0] = 1.0F;
    embedding[static_cast<std::size_t>(LCQI_LLAMA_Q4_TEST_HIDDEN)] = 0.5F;
    add_gguf_f32_tensor(tensors,
                        "token_embd.weight",
                        {LCQI_LLAMA_Q4_TEST_HIDDEN, LCQI_LLAMA_Q4_TEST_VOCAB},
                        embedding);
    add_gguf_f32_tensor(tensors,
                        "output_norm.weight",
                        {LCQI_LLAMA_Q4_TEST_HIDDEN},
                        std::vector<float>(LCQI_LLAMA_Q4_TEST_HIDDEN, 1.0F));
    add_gguf_f32_tensor(tensors,
                        "blk.0.attn_norm.weight",
                        {LCQI_LLAMA_Q4_TEST_HIDDEN},
                        std::vector<float>(LCQI_LLAMA_Q4_TEST_HIDDEN, 1.0F));
    add_gguf_f32_tensor(tensors,
                        "blk.0.ffn_norm.weight",
                        {LCQI_LLAMA_Q4_TEST_HIDDEN},
                        std::vector<float>(LCQI_LLAMA_Q4_TEST_HIDDEN, 1.0F));

    const std::vector<std::uint8_t> zero_q4_matrix(
        static_cast<std::size_t>(LCQI_LLAMA_Q4_TEST_HIDDEN) *
            static_cast<std::size_t>(lcqi::LCQI_Q4_K_BLOCK_BYTES),
        0);
    add_gguf_q4_k_tensor(tensors,
                         "blk.0.attn_q.weight",
                         {LCQI_LLAMA_Q4_TEST_HIDDEN, LCQI_LLAMA_Q4_TEST_HIDDEN},
                         zero_q4_matrix);
    add_gguf_q4_k_tensor(tensors,
                         "blk.0.attn_k.weight",
                         {LCQI_LLAMA_Q4_TEST_HIDDEN, LCQI_LLAMA_Q4_TEST_HEAD_DIM},
                         zero_q4_matrix);
    add_gguf_q4_k_tensor(tensors,
                         "blk.0.attn_v.weight",
                         {LCQI_LLAMA_Q4_TEST_HIDDEN, LCQI_LLAMA_Q4_TEST_HEAD_DIM},
                         zero_q4_matrix);
    add_gguf_q4_k_tensor(tensors,
                         "blk.0.attn_output.weight",
                         {LCQI_LLAMA_Q4_TEST_HIDDEN, LCQI_LLAMA_Q4_TEST_HIDDEN},
                         zero_q4_matrix);
    add_gguf_q4_k_tensor(tensors,
                         "blk.0.ffn_gate.weight",
                         {LCQI_LLAMA_Q4_TEST_HIDDEN, LCQI_LLAMA_Q4_TEST_INTERMEDIATE},
                         zero_q4_matrix);
    add_gguf_q4_k_tensor(tensors,
                         "blk.0.ffn_up.weight",
                         {LCQI_LLAMA_Q4_TEST_HIDDEN, LCQI_LLAMA_Q4_TEST_INTERMEDIATE},
                         zero_q4_matrix);
    add_gguf_q4_k_tensor(tensors,
                         "blk.0.ffn_down.weight",
                         {LCQI_LLAMA_Q4_TEST_INTERMEDIATE, LCQI_LLAMA_Q4_TEST_HIDDEN},
                         zero_q4_matrix);
    write_gguf_fixture(path, tensors, metadata);
}

void write_tiny_gpt2_safetensors(const std::filesystem::path& path,
                                 const lcqi::Gpt2ReferenceModel& model) {
    std::vector<RawTensorFixture> tensors;
    add_f32_tensor(tensors,
                   "transformer.wte.weight",
                   {model.config.vocab_size, model.config.hidden_size},
                   model.token_embedding);
    add_f32_tensor(tensors,
                   "transformer.wpe.weight",
                   {model.config.max_positions, model.config.hidden_size},
                   model.position_embedding);
    for (std::int32_t layer_id = 0; layer_id < model.config.layer_count; ++layer_id) {
        const lcqi::Gpt2LayerWeightsF32& layer =
            model.layers[static_cast<std::size_t>(layer_id)];
        const std::string prefix = "transformer.h." + std::to_string(layer_id) + ".";
        add_f32_tensor(tensors, prefix + "ln_1.weight", {model.config.hidden_size}, layer.ln_1_weight);
        add_f32_tensor(tensors, prefix + "ln_1.bias", {model.config.hidden_size}, layer.ln_1_bias);
        const std::vector<float> c_attn_hf = transpose_linear_to_hf_conv1d(layer.c_attn);
        add_f32_tensor(tensors,
                       prefix + "attn.c_attn.weight",
                       {model.config.hidden_size,
                        model.config.hidden_size * 3},
                       c_attn_hf);
        add_f32_tensor(tensors,
                       prefix + "attn.c_attn.bias",
                       {model.config.hidden_size * 3},
                       layer.c_attn.bias);
        const std::vector<float> c_proj_hf = transpose_linear_to_hf_conv1d(layer.c_proj);
        add_f32_tensor(tensors,
                       prefix + "attn.c_proj.weight",
                       {model.config.hidden_size, model.config.hidden_size},
                       c_proj_hf);
        add_f32_tensor(tensors,
                       prefix + "attn.c_proj.bias",
                       {model.config.hidden_size},
                       layer.c_proj.bias);
        add_f32_tensor(tensors, prefix + "ln_2.weight", {model.config.hidden_size}, layer.ln_2_weight);
        add_f32_tensor(tensors, prefix + "ln_2.bias", {model.config.hidden_size}, layer.ln_2_bias);
        const std::vector<float> c_fc_hf = transpose_linear_to_hf_conv1d(layer.c_fc);
        add_f32_tensor(tensors,
                       prefix + "mlp.c_fc.weight",
                       {model.config.hidden_size, model.config.intermediate_size},
                       c_fc_hf);
        add_f32_tensor(tensors,
                       prefix + "mlp.c_fc.bias",
                       {model.config.intermediate_size},
                       layer.c_fc.bias);
        const std::vector<float> mlp_proj_hf =
            transpose_linear_to_hf_conv1d(layer.mlp_c_proj);
        add_f32_tensor(tensors,
                       prefix + "mlp.c_proj.weight",
                       {model.config.intermediate_size, model.config.hidden_size},
                       mlp_proj_hf);
        add_f32_tensor(tensors,
                       prefix + "mlp.c_proj.bias",
                       {model.config.hidden_size},
                       layer.mlp_c_proj.bias);
    }
    add_f32_tensor(tensors,
                   "transformer.ln_f.weight",
                   {model.config.hidden_size},
                   model.final_ln_weight);
    add_f32_tensor(tensors,
                   "transformer.ln_f.bias",
                   {model.config.hidden_size},
                   model.final_ln_bias);
    write_safetensors_raw_fixture(path, tensors);
}

void test_tiny_mlp() {
    const lcqi::TinyMlpModel model = lcqi::load_model(test_model_path());
    require(model.input_size == LCQI_TEST_INPUT_SIZE, "input size mismatch");
    require(model.hidden_size == LCQI_TEST_HIDDEN_SIZE, "hidden size mismatch");
    require(model.output_size == LCQI_TEST_OUTPUT_SIZE, "output size mismatch");

    const std::vector<float> input{1.0F, 2.0F, -1.0F, 0.5F};
    const lcqi::InferenceResult result = lcqi::run_inference(model, input);

    require(result.predicted_class == LCQI_TEST_PREDICTED_CLASS, "unexpected predicted class");
    require(result.logits.size() == static_cast<std::size_t>(LCQI_TEST_OUTPUT_SIZE),
            "unexpected logits size");
    require_close(result.logits[0], LCQI_TEST_LOGIT_0, "logit 0 mismatch");
    require_close(result.logits[1], LCQI_TEST_LOGIT_1, "logit 1 mismatch");

    std::vector<float> hidden(static_cast<std::size_t>(LCQI_TEST_HIDDEN_SIZE), 0.0F);
    lcqi::linear_i8(model.hidden, input, hidden);
    lcqi::relu_inplace(hidden);
    require_close(hidden[0], LCQI_TEST_HIDDEN_0, "hidden 0 mismatch");
    require_close(hidden[1], LCQI_TEST_HIDDEN_1, "hidden 1 mismatch");
    require_close(hidden[2], LCQI_TEST_HIDDEN_2, "hidden 2 mismatch");
}

void test_tokenizer_contract() {
    const lcqi::TokenizerModel tokenizer = lcqi::load_tokenizer(test_tokenizer_path());
    require(tokenizer.bos_id == 0, "tokenizer BOS id mismatch");
    require(tokenizer.eos_id == 4, "tokenizer EOS id mismatch");
    require(tokenizer.unk_id == -1, "tokenizer UNK id mismatch");
    require(tokenizer.chat_template_hash == "tiny-chat-template-v1",
            "tokenizer template hash mismatch");

    const std::vector<std::int32_t> token_ids =
        lcqi::encode_prompt(tokenizer, "tok1 tok2 tok3");
    const std::vector<std::int32_t> expected{0, 1, 2, 3, 4};
    require(token_ids == expected, "tokenizer prompt ids mismatch");
    require(lcqi::tokenizer_contract_hash(tokenizer) == LCQI_TOKENIZER_HASH_EXPECTED,
            "tokenizer contract hash mismatch");
}

void test_tokenizer_rejects_unknown_without_unk() {
    const lcqi::TokenizerModel tokenizer = lcqi::load_tokenizer(test_tokenizer_path());
    bool threw = false;
    try {
        static_cast<void>(lcqi::encode_prompt(tokenizer, "tok1 missing_token"));
    } catch (const std::runtime_error&) {
        threw = true;
    }
    require(threw, "tokenizer should reject unknown token when unk id is absent");
}

void test_safetensors_manifest_contract() {
    const std::filesystem::path path =
        temp_file_path("lcqi_safetensors_manifest_contract.safetensors");
    const std::string header =
        R"({"__metadata__":{"format":"lcqi-fixture"},"model.embed_tokens.weight":{"dtype":"F32","shape":[2,3],"data_offsets":[0,24]},"model.layers.0.attn.q_proj.weight":{"dtype":"F16","shape":[4,3],"data_offsets":[24,48]}})";
    write_safetensors_fixture(path, header, LCQI_SAFETENSORS_VALID_PAYLOAD_BYTES);

    const lcqi::SafeTensorManifest manifest = lcqi::load_safetensors_manifest(path);
    std::filesystem::remove(path);

    require(manifest.header_size == static_cast<std::uint64_t>(header.size()),
            "safetensors header size mismatch");
    require(manifest.data_start_offset ==
                static_cast<std::uint64_t>(LCQI_SAFETENSORS_PREFIX_BYTES) +
                    static_cast<std::uint64_t>(header.size()),
            "safetensors data start mismatch");
    require(manifest.metadata.size() == 1, "safetensors metadata count mismatch");
    require(manifest.metadata[0].first == "format" &&
                manifest.metadata[0].second == "lcqi-fixture",
            "safetensors metadata mismatch");
    require(manifest.tensors.size() == 2, "safetensors tensor count mismatch");

    const lcqi::SafeTensorEntry* embedding =
        manifest.find_tensor("model.embed_tokens.weight");
    require(embedding != nullptr, "safetensors embedding tensor missing");
    require(embedding->dtype == "F32", "safetensors embedding dtype mismatch");
    require(embedding->shape == std::vector<std::int64_t>({2, 3}),
            "safetensors embedding shape mismatch");
    require(embedding->data_begin == LCQI_SAFETENSORS_EMBED_BEGIN &&
                embedding->data_end == LCQI_SAFETENSORS_EMBED_END,
            "safetensors embedding offset mismatch");
    require(embedding->byte_size() ==
                LCQI_SAFETENSORS_EMBED_END - LCQI_SAFETENSORS_EMBED_BEGIN,
            "safetensors embedding byte size mismatch");

    const lcqi::SafeTensorEntry* q_proj =
        manifest.find_tensor("model.layers.0.attn.q_proj.weight");
    require(q_proj != nullptr, "safetensors q projection tensor missing");
    require(q_proj->dtype == "F16", "safetensors q projection dtype mismatch");
    require(q_proj->data_begin == LCQI_SAFETENSORS_QPROJ_BEGIN &&
                q_proj->data_end == LCQI_SAFETENSORS_QPROJ_END,
            "safetensors q projection offset mismatch");
}

void test_safetensors_rejects_bad_offsets() {
    const std::filesystem::path path =
        temp_file_path("lcqi_safetensors_bad_offsets.safetensors");
    const std::string header =
        R"({"tensor":{"dtype":"F32","shape":[4],"data_offsets":[0,32]}})";
    write_safetensors_fixture(path, header, 4);

    bool threw = false;
    try {
        static_cast<void>(lcqi::load_safetensors_manifest(path));
    } catch (const std::runtime_error&) {
        threw = true;
    }
    std::filesystem::remove(path);
    require(threw, "safetensors parser should reject offsets beyond payload");
}

void test_safetensors_rejects_bad_dtype_shape_size() {
    const std::filesystem::path path =
        temp_file_path("lcqi_safetensors_bad_dtype_shape_size.safetensors");
    const std::string header =
        R"({"tensor":{"dtype":"F32","shape":[4],"data_offsets":[0,8]}})";
    write_safetensors_fixture(path, header, 8);

    bool threw = false;
    try {
        static_cast<void>(lcqi::load_safetensors_manifest(path));
    } catch (const std::runtime_error&) {
        threw = true;
    }
    std::filesystem::remove(path);
    require(threw, "safetensors parser should reject dtype/shape byte mismatch");
}

void test_safetensors_reads_float_tensors() {
    const std::filesystem::path path =
        temp_file_path("lcqi_safetensors_float_read.safetensors");
    std::vector<std::uint8_t> f16_bytes;
    append_le_u16(f16_bytes, LCQI_TEST_F16_ONE);
    append_le_u16(f16_bytes, LCQI_TEST_F16_NEGATIVE_TWO);
    std::vector<std::uint8_t> bf16_bytes;
    append_le_u16(bf16_bytes, LCQI_TEST_BF16_ONE_POINT_FIVE);
    append_le_u16(bf16_bytes, LCQI_TEST_BF16_NEGATIVE_HALF);

    const std::array<RawTensorFixture, 3> tensors{{
        {"f32", "F32", {2}, f32_payload(std::array<float, 2>{1.25F, -2.5F})},
        {"f16", "F16", {2}, f16_bytes},
        {"bf16", "BF16", {2}, bf16_bytes},
    }};
    write_safetensors_raw_fixture(path, tensors);

    const lcqi::SafeTensorFile file = lcqi::load_safetensors_file(path);
    std::filesystem::remove(path);
    const std::vector<float> f32 = lcqi::read_safetensor_f32_tensor(file, "f32");
    const std::vector<float> f16 = lcqi::read_safetensor_f32_tensor(file, "f16");
    const std::vector<float> bf16 = lcqi::read_safetensor_f32_tensor(file, "bf16");

    require_close(f32[0], 1.25F, "safetensors F32 value 0 mismatch");
    require_close(f32[1], -2.5F, "safetensors F32 value 1 mismatch");
    require_close(f16[0], 1.0F, "safetensors F16 value 0 mismatch");
    require_close(f16[1], -2.0F, "safetensors F16 value 1 mismatch");
    require_close(bf16[0], 1.5F, "safetensors BF16 value 0 mismatch");
    require_close(bf16[1], -0.5F, "safetensors BF16 value 1 mismatch");
}

void test_gguf_manifest_reads_q4_k_tensor() {
    const std::filesystem::path path = temp_file_path("lcqi_q4_k_fixture.gguf");
    write_gguf_q4_k_fixture(path);

    const lcqi::GgufManifest manifest = lcqi::load_gguf_manifest(path);
    const lcqi::GgufTensorInfo* tensor =
        manifest.find_tensor("blk.0.ffn_up.weight");
    require(tensor != nullptr, "GGUF Q4_K tensor missing");
    const std::vector<std::uint8_t> bytes =
        lcqi::read_gguf_tensor_bytes(path, *tensor);
    std::filesystem::remove(path);

    require(manifest.version == LCQI_GGUF_TEST_VERSION, "GGUF version mismatch");
    require(manifest.alignment == LCQI_GGUF_TEST_ALIGNMENT, "GGUF alignment mismatch");
    require(manifest.metadata.size() == LCQI_GGUF_TEST_METADATA_COUNT,
            "GGUF metadata count mismatch");
    const lcqi::GgufMetadataEntry* tokens = manifest.find_metadata("tokenizer.ggml.tokens");
    require(tokens != nullptr, "GGUF tokenizer tokens metadata missing");
    require(tokens->string_values == std::vector<std::string>({"<bos>", "hello"}),
            "GGUF tokenizer string array mismatch");
    require(manifest.tensors.size() == LCQI_GGUF_TEST_TENSOR_COUNT,
            "GGUF tensor count mismatch");
    require(tensor->type == lcqi::GgmlType::q4_k, "GGUF tensor type mismatch");
    require(tensor->shape == std::vector<std::int64_t>({lcqi::LCQI_QK_K_BLOCK_VALUES, 1}),
            "GGUF tensor shape mismatch");
    require(tensor->byte_size == lcqi::LCQI_Q4_K_BLOCK_BYTES,
            "GGUF tensor byte size mismatch");
    require(bytes == make_q4_k_test_block(), "GGUF tensor payload mismatch");
}

void test_q4_k_dequant_and_dot_paths() {
    const std::vector<std::uint8_t> block = make_q4_k_test_block();
    const std::vector<float> expected = expected_q4_k_test_values();
    const std::vector<float> decoded =
        lcqi::dequantize_q4_k(block, lcqi::LCQI_QK_K_BLOCK_VALUES);

    require(decoded.size() == expected.size(), "Q4_K decoded size mismatch");
    for (std::size_t index = 0; index < expected.size(); ++index) {
        require_close(decoded[index], expected[index], "Q4_K decoded value mismatch");
    }

    std::vector<float> input(expected.size(), 0.0F);
    for (std::size_t index = 0; index < input.size(); ++index) {
        input[index] = static_cast<float>(static_cast<std::int32_t>(index % 11U) - 5) *
                       0.125F;
    }
    float expected_dot = 0.0F;
    for (std::size_t index = 0; index < expected.size(); ++index) {
        expected_dot += expected[index] * input[index];
    }
    const float q4_f32_dot = lcqi::dot_q4_k_f32(block, input);
    require_close(q4_f32_dot, expected_dot, "Q4_K F32 dot mismatch");

    const std::vector<lcqi::Q8KBlock> q8_input = lcqi::quantize_q8_k_input(input);
    const float q4_q8_dot = lcqi::dot_q4_k_q8(block, q8_input);
    require(std::fabs(q4_q8_dot - q4_f32_dot) <= LCQI_Q4K_Q8_DIFF_LIMIT,
            "Q4_K Q8 dot drift is too large");

    std::vector<float> matvec_output(1, 0.0F);
    lcqi::matvec_q4_k_q8(block, 1, lcqi::LCQI_QK_K_BLOCK_VALUES, q8_input, matvec_output);
    require_close(matvec_output[0], q4_q8_dot, "Q4_K Q8 matvec mismatch");
}

void test_ggml_mixed_dequantizers() {
    std::vector<std::uint8_t> f32_bytes;
    append_le_f32(f32_bytes, 1.25F);
    append_le_f32(f32_bytes, -2.5F);
    const std::vector<float> f32 =
        lcqi::dequantize_ggml_tensor(lcqi::GgmlType::f32, f32_bytes, 2);
    require_close(f32[0], 1.25F, "GGML F32 decoded value 0 mismatch");
    require_close(f32[1], -2.5F, "GGML F32 decoded value 1 mismatch");

    const std::vector<std::uint8_t> q8 = make_q8_0_test_block();
    const std::vector<float> q8_values =
        lcqi::dequantize_ggml_tensor(lcqi::GgmlType::q8_0,
                                     q8,
                                     lcqi::LCQI_Q8_0_BLOCK_VALUES);
    require_close(q8_values[0], -8.0F, "Q8_0 decoded value 0 mismatch");
    require_close(q8_values[16], 0.0F, "Q8_0 decoded value 16 mismatch");
    require_close(q8_values[31], 7.5F, "Q8_0 decoded value 31 mismatch");

    const std::vector<std::uint8_t> q5 = make_q5_0_test_block();
    const std::vector<float> q5_values =
        lcqi::dequantize_ggml_tensor(lcqi::GgmlType::q5_0,
                                     q5,
                                     lcqi::LCQI_Q5_0_BLOCK_VALUES);
    require_close(q5_values[0], -8.0F, "Q5_0 decoded value 0 mismatch");
    require_close(q5_values[1], -7.5F, "Q5_0 decoded value 1 mismatch");
    require_close(q5_values[15], -0.5F, "Q5_0 decoded value 15 mismatch");
    require_close(q5_values[16], 0.0F, "Q5_0 decoded value 16 mismatch");
    require_close(q5_values[31], 7.5F, "Q5_0 decoded value 31 mismatch");

    const std::vector<std::uint8_t> q6 = make_q6_k_test_block();
    const std::vector<float> q6_values =
        lcqi::dequantize_ggml_tensor(lcqi::GgmlType::q6_k,
                                     q6,
                                     lcqi::LCQI_Q6_K_BLOCK_VALUES);
    require_close(q6_values[LCQI_GGML_TEST_Q6_LANE], 3.0F, "Q6_K q1 mismatch");
    require_close(q6_values[32 + LCQI_GGML_TEST_Q6_LANE], -1.0F, "Q6_K q2 mismatch");
    require_close(q6_values[64 + LCQI_GGML_TEST_Q6_LANE], 31.0F, "Q6_K q3 mismatch");
    require_close(q6_values[96 + LCQI_GGML_TEST_Q6_LANE], -32.0F, "Q6_K q4 mismatch");
    require_close(q6_values[128 + LCQI_GGML_TEST_Q6_LANE], 4.0F, "Q6_K second half mismatch");

    bool rejected_bad_size = false;
    try {
        static_cast<void>(lcqi::dequantize_ggml_tensor(
            lcqi::GgmlType::q8_0,
            std::span<const std::uint8_t>(q8.data(), q8.size() - 1U),
            lcqi::LCQI_Q8_0_BLOCK_VALUES));
    } catch (const std::runtime_error&) {
        rejected_bad_size = true;
    }
    require(rejected_bad_size, "GGML dequantizer accepted a truncated Q8_0 block");
}

void test_ggml_quantized_f32_matvec_paths() {
    const std::vector<float> input32 = [] {
        std::vector<float> values(static_cast<std::size_t>(lcqi::LCQI_Q8_0_BLOCK_VALUES), 0.0F);
        for (std::size_t index = 0; index < values.size(); ++index) {
            values[index] =
                static_cast<float>(static_cast<std::int32_t>(index % 9U) - 4) * 0.125F;
        }
        return values;
    }();
    const std::vector<float> input256 = [] {
        std::vector<float> values(static_cast<std::size_t>(lcqi::LCQI_Q6_K_BLOCK_VALUES), 0.0F);
        for (std::size_t index = 0; index < values.size(); ++index) {
            values[index] =
                static_cast<float>(static_cast<std::int32_t>(index % 13U) - 6) * 0.0625F;
        }
        return values;
    }();

    const auto assert_matvec_matches_dequantized =
        [](lcqi::GgmlType type,
           const std::vector<std::uint8_t>& block,
           std::span<const float> input,
           const char* message) {
            const std::int64_t columns = static_cast<std::int64_t>(input.size());
            const std::vector<std::uint8_t> matrix = [&] {
                std::vector<std::uint8_t> bytes;
                bytes.reserve(block.size() * 2U);
                bytes.insert(bytes.end(), block.begin(), block.end());
                bytes.insert(bytes.end(), block.begin(), block.end());
                return bytes;
            }();
            std::vector<float> direct(2, 0.0F);
            lcqi::matvec_ggml_quantized_f32(type, matrix, 2, columns, input, direct);
            const std::vector<lcqi::Q8_0InputBlock> q8_0_input =
                lcqi::quantize_q8_0_input(input);
            std::vector<float> q8_0_direct(2, 0.0F);
            lcqi::matvec_ggml_quantized_q8_0(type,
                                             matrix,
                                             2,
                                             columns,
                                             q8_0_input,
                                             q8_0_direct);
            const std::vector<float> weights =
                lcqi::dequantize_ggml_tensor(type, matrix, columns * 2);
            std::array<float, 2> reference{0.0F, 0.0F};
            for (std::size_t row = 0; row < reference.size(); ++row) {
                for (std::size_t column = 0; column < input.size(); ++column) {
                    reference[row] +=
                        weights[row * input.size() + column] * input[column];
                }
                if (std::fabs(reference[row] - direct[row]) > LCQI_GGML_MATVEC_MAX_DIFF) {
                    throw std::runtime_error(message);
                }
                if (std::fabs(reference[row] - q8_0_direct[row]) >
                    LCQI_GGML_Q8_MATVEC_MAX_DIFF) {
                    throw std::runtime_error(message);
                }
            }
        };

    require(lcqi::ggml_type_has_f32_direct_matvec(lcqi::GgmlType::q5_0),
            "Q5_0 direct matvec support flag mismatch");
    require(lcqi::ggml_type_has_f32_direct_matvec(lcqi::GgmlType::q6_k),
            "Q6_K direct matvec support flag mismatch");
    require(lcqi::ggml_type_has_f32_direct_matvec(lcqi::GgmlType::q8_0),
            "Q8_0 direct matvec support flag mismatch");
    require(!lcqi::ggml_type_has_f32_direct_matvec(lcqi::GgmlType::f32),
            "F32 should not be reported as quantized direct matvec");
    require(lcqi::ggml_type_has_q8_0_direct_matvec(lcqi::GgmlType::q5_0),
            "Q5_0 Q8_0 direct matvec support flag mismatch");
    require(lcqi::ggml_type_has_q8_0_direct_matvec(lcqi::GgmlType::q6_k),
            "Q6_K Q8_0 direct matvec support flag mismatch");
    require(lcqi::ggml_type_has_q8_0_direct_matvec(lcqi::GgmlType::q8_0),
            "Q8_0 Q8_0 direct matvec support flag mismatch");
    require(!lcqi::ggml_type_has_q8_0_direct_matvec(lcqi::GgmlType::f32),
            "F32 should not be reported as Q8_0 quantized direct matvec");

    assert_matvec_matches_dequantized(lcqi::GgmlType::q8_0,
                                      make_q8_0_test_block(),
                                      input32,
                                      "Q8_0 direct matvec mismatch");
    assert_matvec_matches_dequantized(lcqi::GgmlType::q5_0,
                                      make_q5_0_test_block(),
                                      input32,
                                      "Q5_0 direct matvec mismatch");
    assert_matvec_matches_dequantized(lcqi::GgmlType::q6_k,
                                      make_q6_k_test_block(),
                                      input256,
                                      "Q6_K direct matvec mismatch");
}

void test_llama_gguf_loader_and_generation() {
    const std::filesystem::path path = temp_file_path("lcqi_tiny_llama_fixture.gguf");
    write_tiny_llama_gguf_fixture(path);

    const lcqi::LlamaGgufLoadedModel loaded =
        lcqi::load_llama_gguf_reference_model(path);
    const std::vector<std::int32_t> prompt{1};
    const lcqi::LlamaGgufGenerationResult result =
        lcqi::llama_gguf_generate_greedy(loaded.model, prompt, 1);
    std::filesystem::remove(path);

    require(loaded.model.architecture == "llama", "LLaMA GGUF architecture mismatch");
    require(loaded.model.decoder.config.hidden_size == LCQI_LLAMA_TEST_HIDDEN,
            "LLaMA GGUF hidden size mismatch");
    require(loaded.model.decoder.layers.size() == LCQI_LLAMA_TEST_LAYER_COUNT,
            "LLaMA GGUF layer count mismatch");
    require(loaded.model.lm_head_tied_to_embedding,
            "LLaMA GGUF tiny fixture should tie lm head to embeddings");
    require(result.generated_ids == std::vector<std::int32_t>({1, LCQI_LLAMA_TEST_EXPECTED_TOKEN}),
            "LLaMA GGUF generated ids mismatch");
    require(result.predicted_first_token == LCQI_LLAMA_TEST_EXPECTED_TOKEN,
            "LLaMA GGUF predicted token mismatch");
    require(loaded.report.tensors_loaded == 11, "LLaMA GGUF tensor load count mismatch");
    require(loaded.report.f32_weight_bytes == loaded.report.quantized_weight_bytes,
            "LLaMA GGUF F32 fixture byte accounting mismatch");
    require(loaded.report.q4_k_direct_tensors == 0,
            "LLaMA GGUF F32 fixture should not use direct Q4_K tensors");
    require(loaded.report.ggml_direct_tensors == 0,
            "LLaMA GGUF F32 fixture should not use direct GGML tensors");
    require(result.hotspots.f32_fallback_calls == 7,
            "LLaMA GGUF F32 fixture linear fallback call count mismatch");
    require(result.hotspots.q4_k_direct_calls == 0,
            "LLaMA GGUF F32 fixture should not call Q4_K direct matvec");
    require(result.hotspots.ggml_direct_calls == 0,
            "LLaMA GGUF F32 fixture should not call GGML direct matvec");
}

void test_llama_gguf_q4_direct_generation() {
    const std::filesystem::path path = temp_file_path("lcqi_tiny_llama_q4_direct.gguf");
    write_tiny_llama_q4_direct_gguf_fixture(path);

    const lcqi::LlamaGgufLoadedModel loaded =
        lcqi::load_llama_gguf_reference_model(path);
    const std::vector<std::int32_t> prompt{0};
    const lcqi::LlamaGgufGenerationResult result =
        lcqi::llama_gguf_generate_greedy(loaded.model, prompt, 1);
    std::filesystem::remove(path);

    require(loaded.model.decoder.config.hidden_size == LCQI_LLAMA_Q4_TEST_HIDDEN,
            "LLaMA Q4_K fixture hidden size mismatch");
    require(loaded.report.tensors_loaded == 11, "LLaMA Q4_K fixture tensor load count mismatch");
    require(loaded.report.q4_k_direct_tensors == 7,
            "LLaMA Q4_K fixture direct tensor count mismatch");
    require(loaded.report.ggml_direct_tensors == 0,
            "LLaMA Q4_K fixture should not use GGML direct tensors");
    require(loaded.report.f32_fallback_tensors == 4,
            "LLaMA Q4_K fixture fallback tensor count mismatch");
    require(loaded.report.direct_quantized_weight_bytes >
                loaded.report.fallback_dequantized_weight_bytes,
            "LLaMA Q4_K fixture should keep direct quantized matrix bytes");
    require(result.hotspots.q4_k_direct_calls == 7,
            "LLaMA Q4_K fixture direct matvec call count mismatch");
    require(result.hotspots.ggml_direct_calls == 0,
            "LLaMA Q4_K fixture should not call GGML direct matvec");
    require(result.hotspots.f32_fallback_calls == 0,
            "LLaMA Q4_K fixture should not call fallback linear matvec");
    require(result.generated_ids == std::vector<std::int32_t>({0, 0}),
            "LLaMA Q4_K fixture generated ids mismatch");
}

void test_llama_gguf_ggml_direct_generation() {
    const ScopedEnvVar enable_ggml_direct(LCQI_TEST_LLAMA_GGML_DIRECT_ENV,
                                          LCQI_TEST_TRUE_ENV);
    const std::filesystem::path path = temp_file_path("lcqi_tiny_llama_ggml_direct.gguf");
    write_tiny_llama_ggml_direct_gguf_fixture(path);

    const lcqi::LlamaGgufLoadedModel loaded =
        lcqi::load_llama_gguf_reference_model(path);
    const std::vector<std::int32_t> prompt{0};
    const lcqi::LlamaGgufGenerationResult result =
        lcqi::llama_gguf_generate_greedy(loaded.model, prompt, 1);
    std::filesystem::remove(path);

    require(loaded.model.decoder.config.hidden_size == LCQI_LLAMA_Q4_TEST_HIDDEN,
            "LLaMA GGML fixture hidden size mismatch");
    require(loaded.report.tensors_loaded == 11, "LLaMA GGML fixture tensor load count mismatch");
    require(loaded.report.q4_k_direct_tensors == 0,
            "LLaMA GGML fixture should not use Q4_K direct tensors");
    require(loaded.report.ggml_direct_tensors == 7,
            "LLaMA GGML fixture direct tensor count mismatch");
    require(loaded.report.f32_fallback_tensors == 4,
            "LLaMA GGML fixture fallback tensor count mismatch");
    require(result.hotspots.ggml_direct_calls == 7,
            "LLaMA GGML fixture direct matvec call count mismatch");
    require(result.hotspots.q4_k_direct_calls == 0,
            "LLaMA GGML fixture should not call Q4_K direct matvec");
    require(result.hotspots.f32_fallback_calls == 0,
            "LLaMA GGML fixture should not call fallback linear matvec");
    require(result.generated_ids == std::vector<std::int32_t>({0, 0}),
            "LLaMA GGML fixture generated ids mismatch");
}

void test_gpt2_tiny_forward_and_generation() {
    const lcqi::Gpt2ReferenceModel model = lcqi::make_tiny_gpt2_reference_model();
    const std::vector<std::int32_t> tokens{1, 2, 3};
    const lcqi::Gpt2ForwardResult result = lcqi::run_gpt2_forward(model, tokens);

    require(result.logits.size() == LCQI_GPT2_VOCAB_SIZE, "GPT-2 logits size mismatch");
    require(result.predicted_token == LCQI_GPT2_PREDICTED_TOKEN,
            "GPT-2 predicted token mismatch");
    for (std::size_t index = 0; index < LCQI_GPT2_LOGITS.size(); ++index) {
        require_close(result.logits[index], LCQI_GPT2_LOGITS[index], "GPT-2 logit mismatch");
    }

    const std::vector<std::int32_t> generated =
        lcqi::gpt2_generate_greedy(model, tokens, 2);
    const std::vector<std::int32_t> expected{1, 2, 3, 3, 3};
    require(generated == expected, "GPT-2 greedy generation mismatch");
    require(static_cast<std::int32_t>(generated.size()) == LCQI_GPT2_GENERATED_LENGTH,
            "GPT-2 generated length mismatch");

    lcqi::Gpt2KvCache cache(model.config);
    lcqi::Gpt2ForwardResult cached_result;
    for (const std::int32_t token : tokens) {
        cached_result = lcqi::run_gpt2_forward_cached(model, cache, token);
    }
    require(cache.filled_tokens() == static_cast<std::int32_t>(tokens.size()),
            "GPT-2 KV cache filled token count mismatch");
    require(cache.byte_size() > 0, "GPT-2 KV cache byte size should be non-zero");
    require(cached_result.predicted_token == result.predicted_token,
            "cached GPT-2 predicted token mismatch");
    require(cached_result.logits.size() == result.logits.size(),
            "cached GPT-2 logits size mismatch");
    for (std::size_t index = 0; index < result.logits.size(); ++index) {
        require_close(cached_result.logits[index],
                      result.logits[index],
                      "cached GPT-2 logit mismatch");
    }

    lcqi::Gpt2CachedGreedyDecoder decoder_with_logits(model);
    lcqi::Gpt2ForwardResult decoder_result;
    for (const std::int32_t token : tokens) {
        decoder_result = decoder_with_logits.step_with_logits(token);
    }
    require(decoder_with_logits.filled_tokens() == static_cast<std::int32_t>(tokens.size()),
            "GPT-2 optimized decoder filled token count mismatch");
    require(decoder_with_logits.kv_cache_bytes() == cache.byte_size(),
            "GPT-2 optimized decoder KV byte size mismatch");
    require(decoder_result.predicted_token == result.predicted_token,
            "optimized cached GPT-2 predicted token mismatch");
    require(decoder_result.logits.size() == result.logits.size(),
            "optimized cached GPT-2 logits size mismatch");
    for (std::size_t index = 0; index < result.logits.size(); ++index) {
        require_close(decoder_result.logits[index],
                      result.logits[index],
                      "optimized cached GPT-2 logit mismatch");
    }

    lcqi::Gpt2ExecutionOptions parallel_options;
    parallel_options.worker_count = 2;
    parallel_options.parallel_min_rows = 1;
    lcqi::Gpt2CachedGreedyDecoder parallel_decoder(model, nullptr, parallel_options);
    lcqi::Gpt2ForwardResult parallel_result;
    for (const std::int32_t token : tokens) {
        parallel_result = parallel_decoder.step_with_logits(token);
    }
    require(parallel_decoder.worker_count() == parallel_options.worker_count,
            "parallel GPT-2 decoder worker count mismatch");
    require(parallel_result.predicted_token == result.predicted_token,
            "parallel cached GPT-2 predicted token mismatch");
    require(parallel_result.logits.size() == result.logits.size(),
            "parallel cached GPT-2 logits size mismatch");
    for (std::size_t index = 0; index < result.logits.size(); ++index) {
        require_close(parallel_result.logits[index],
                      result.logits[index],
                      "parallel cached GPT-2 logit mismatch");
    }

    lcqi::Gpt2CachedGreedyDecoder greedy_decoder(model);
    std::int32_t greedy_predicted = 0;
    for (const std::int32_t token : tokens) {
        greedy_predicted = greedy_decoder.step(token);
    }
    require(greedy_predicted == result.predicted_token,
            "optimized logits-free GPT-2 predicted token mismatch");

    lcqi::Gpt2CachedGreedyDecoder prefill_decoder(model);
    for (std::size_t index = 0; index + 1 < tokens.size(); ++index) {
        prefill_decoder.advance_without_prediction(tokens[index]);
    }
    require(prefill_decoder.filled_tokens() ==
                static_cast<std::int32_t>(tokens.size() - 1U),
            "GPT-2 prefill-only decoder filled token count mismatch");
    const std::int32_t prefill_predicted = prefill_decoder.step(tokens.back());
    require(prefill_decoder.filled_tokens() == static_cast<std::int32_t>(tokens.size()),
            "GPT-2 prefill decoder final filled token count mismatch");
    require(prefill_predicted == result.predicted_token,
            "GPT-2 prefill-skipped predicted token mismatch");

    lcqi::Gpt2HotspotProfile hotspot_profile;
    lcqi::Gpt2CachedGreedyDecoder profiled_decoder(model, &hotspot_profile);
    std::int32_t profiled_predicted = 0;
    for (const std::int32_t token : tokens) {
        profiled_predicted = profiled_decoder.step(token);
    }
    require(profiled_predicted == result.predicted_token,
            "profiled GPT-2 predicted token mismatch");
    require(hotspot_profile.decoder_steps == static_cast<std::int64_t>(tokens.size()),
            "profiled GPT-2 decoder step count mismatch");
    require(hotspot_profile.layer_steps ==
                static_cast<std::int64_t>(tokens.size()) * model.config.layer_count,
            "profiled GPT-2 layer step count mismatch");
    require(hotspot_profile.total_step_ms >= hotspot_profile.lm_head_ms,
            "profiled GPT-2 total time should include lm head time");

    const std::vector<std::int32_t> cached_generated =
        lcqi::gpt2_generate_greedy_cached(model, tokens, 2);
    require(cached_generated == expected, "cached GPT-2 greedy generation mismatch");
}

void test_gpt2_byte_bpe_tokenizer() {
    const std::filesystem::path vocab_path = temp_file_path("lcqi_gpt2_vocab.json");
    const std::filesystem::path merges_path = temp_file_path("lcqi_gpt2_merges.txt");
    const std::string space_marker = "\xC4\xA0";
    const std::string vocab =
        "{\"Hello\":7,\",\":8,\"" + space_marker + "world\":9,\"!\":10}";
    const std::string merges =
        "#version: 0.2\n"
        "H e\n"
        "He l\n"
        "Hel l\n"
        "Hell o\n" +
        space_marker + " w\n" +
        space_marker + "w o\n" +
        space_marker + "wo r\n" +
        space_marker + "wor l\n" +
        space_marker + "worl d\n";
    write_text_file(vocab_path, vocab);
    write_text_file(merges_path, merges);

    const lcqi::Gpt2Tokenizer tokenizer =
        lcqi::load_gpt2_tokenizer(vocab_path, merges_path, -1, -1);
    std::filesystem::remove(vocab_path);
    std::filesystem::remove(merges_path);

    const std::vector<std::int32_t> ids =
        lcqi::gpt2_encode(tokenizer, "Hello, world!");
    const std::vector<std::int32_t> expected{
        LCQI_GPT2_TOKENIZER_HELLO_ID,
        8,
        9,
        10,
    };
    require(ids == expected, "GPT-2 BPE ids mismatch");
    require(lcqi::gpt2_decode(tokenizer, ids) == "Hello, world!",
            "GPT-2 BPE decode mismatch");
}

void test_gpt2_loads_hf_style_tiny_checkpoint() {
    const std::filesystem::path directory =
        temp_file_path("lcqi_gpt2_tiny_checkpoint_dir");
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);

    const lcqi::Gpt2ReferenceModel original = lcqi::make_tiny_gpt2_reference_model();
    write_text_file(
        directory / "config.json",
        R"({"model_type":"gpt2","n_embd":4,"n_head":2,"n_layer":1,"n_positions":8,"n_ctx":8,"vocab_size":6,"n_inner":8,"layer_norm_epsilon":0.00001,"activation_function":"gelu_new","bos_token_id":0,"eos_token_id":5})");
    write_tiny_gpt2_safetensors(directory / "model.safetensors", original);

    const lcqi::Gpt2ReferenceModel loaded = lcqi::load_gpt2_from_directory(directory);
    const std::vector<std::int32_t> tokens{1, 2, 3};
    const lcqi::Gpt2ForwardResult result = lcqi::run_gpt2_forward(loaded, tokens);
    std::filesystem::remove_all(directory);

    require(result.predicted_token == LCQI_GPT2_PREDICTED_TOKEN,
            "loaded GPT-2 predicted token mismatch");
    for (std::size_t index = 0; index < LCQI_GPT2_LOGITS.size(); ++index) {
        require_close(result.logits[index],
                      LCQI_GPT2_LOGITS[index],
                      "loaded GPT-2 logit mismatch");
    }
}

void test_gpt2_loader_rejects_bad_shape() {
    const std::filesystem::path directory =
        temp_file_path("lcqi_gpt2_bad_shape_dir");
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);

    const lcqi::Gpt2ReferenceModel original = lcqi::make_tiny_gpt2_reference_model();
    write_text_file(
        directory / "config.json",
        R"({"model_type":"gpt2","n_embd":5,"n_head":1,"n_layer":1,"n_positions":8,"n_ctx":8,"vocab_size":6,"n_inner":8,"layer_norm_epsilon":0.00001,"activation_function":"gelu_new","bos_token_id":0,"eos_token_id":5})");
    write_tiny_gpt2_safetensors(directory / "model.safetensors", original);

    bool threw = false;
    try {
        static_cast<void>(lcqi::load_gpt2_from_directory(directory));
    } catch (const std::runtime_error&) {
        threw = true;
    }
    std::filesystem::remove_all(directory);
    require(threw, "GPT-2 loader should reject bad tensor shape");
}

void test_reference_rms_norm() {
    const std::vector<float> input{3.0F, 4.0F};
    const std::vector<float> weight{1.0F, 0.5F};
    std::vector<float> output(2, 0.0F);
    lcqi::rms_norm(input, weight, 0.0F, output);
    require_close(output[0], LCQI_RMS_EXPECTED_0, "RMSNorm output 0 mismatch");
    require_close(output[1], LCQI_RMS_EXPECTED_1, "RMSNorm output 1 mismatch");
}

void test_reference_rope() {
    std::vector<float> heads{1.0F, 0.0F, 0.0F, 1.0F};
    lcqi::apply_rope(heads, 1, 4, 1, 10000.0F);
    require_close(heads[0], std::cos(1.0F), "RoPE pair 0 cosine mismatch");
    require_close(heads[1], std::sin(1.0F), "RoPE pair 0 sine mismatch");
    require_close(heads[2], -std::sin(0.01F), "RoPE pair 1 negative sine mismatch");
    require_close(heads[3], std::cos(0.01F), "RoPE pair 1 cosine mismatch");
}

void test_reference_kv_attention() {
    lcqi::DecoderConfig config;
    config.hidden_size = 4;
    config.query_heads = 2;
    config.kv_heads = 1;
    config.head_dim = 2;
    config.intermediate_size = 4;
    config.vocab_size = 4;
    config.max_context = 4;

    lcqi::ReferenceKVCache cache(config, 1);
    const std::vector<float> key{1.0F, 0.0F};
    const std::vector<float> value{LCQI_ATTENTION_EXPECTED_0, LCQI_ATTENTION_EXPECTED_1};
    cache.append(0, 0, key, value);
    require(cache.filled_tokens() == 1, "KV filled token count mismatch");
    require_close(cache.key(0, 0, 0)[0], 1.0F, "KV key read mismatch");

    const std::vector<float> query{1.0F, 0.0F, 0.0F, 1.0F};
    std::vector<float> output(4, 0.0F);
    lcqi::attention_decode(config, cache, 0, 0, query, output);
    require_close(output[0], LCQI_ATTENTION_EXPECTED_0, "attention head 0 dim 0 mismatch");
    require_close(output[1], LCQI_ATTENTION_EXPECTED_1, "attention head 0 dim 1 mismatch");
    require_close(output[2], LCQI_ATTENTION_EXPECTED_0, "attention head 1 dim 0 mismatch");
    require_close(output[3], LCQI_ATTENTION_EXPECTED_1, "attention head 1 dim 1 mismatch");
}

void test_reference_decoder_end_to_end() {
    const lcqi::ReferenceDecoderModel model = lcqi::make_tiny_reference_decoder_model();
    const std::vector<std::int32_t> tokens{1, 2, 3};
    const lcqi::ReferenceDecodeResult result = lcqi::run_reference_decode(model, tokens);
    require(result.logits.size() == LCQI_REFERENCE_DECODER_VOCAB_SIZE,
            "reference decoder logits size mismatch");
    require(result.predicted_token == LCQI_REFERENCE_DECODER_PREDICTED_TOKEN,
            "reference decoder predicted token mismatch");
    for (std::size_t index = 0; index < LCQI_REFERENCE_DECODER_LOGITS.size(); ++index) {
        require_close(result.logits[index],
                      LCQI_REFERENCE_DECODER_LOGITS[index],
                      "reference decoder golden logit mismatch");
    }
}

void test_reference_decoder_trace_contract() {
    const lcqi::ReferenceDecoderModel model = lcqi::make_tiny_reference_decoder_model();
    const std::vector<std::int32_t> tokens{1, 2, 3};
    const lcqi::ReferenceDecodeTraceResult trace =
        lcqi::run_reference_decode_with_trace(model, tokens);

    require(trace.result.predicted_token == LCQI_REFERENCE_DECODER_PREDICTED_TOKEN,
            "reference trace predicted token mismatch");
    require(!trace.checkpoints.empty(), "reference trace checkpoints missing");

    const lcqi::ReferenceTensorCheckpoint& first = trace.checkpoints.front();
    require(first.name == "embedding", "first checkpoint should be embedding");
    require(first.token_position == LCQI_REFERENCE_TRACE_FIRST_TOKEN,
            "first checkpoint token position mismatch");
    require(first.dtype == "f32", "first checkpoint dtype mismatch");
    require(first.layout == "row_major", "first checkpoint layout mismatch");
    require(first.shape.size() == 1 &&
                first.shape[0] == LCQI_REFERENCE_TRACE_HIDDEN_SIZE,
            "first checkpoint shape mismatch");
    require(first.stride.size() == 1 && first.stride[0] == 1,
            "first checkpoint stride mismatch");

    bool saw_logits = false;
    bool saw_kv_slot = false;
    for (const lcqi::ReferenceTensorCheckpoint& checkpoint : trace.checkpoints) {
        if (checkpoint.name == "kv_cache_key_slot") {
            saw_kv_slot = true;
            require(checkpoint.shape.size() == 4, "KV checkpoint rank mismatch");
            require(checkpoint.layout == "kv_contiguous", "KV checkpoint layout mismatch");
        }
        if (checkpoint.name == "logits") {
            saw_logits = true;
            require(checkpoint.token_position == LCQI_REFERENCE_TRACE_TOKEN_COUNT - 1,
                    "logits checkpoint token position mismatch");
            require(checkpoint.shape.size() == 1 &&
                        checkpoint.shape[0] ==
                            static_cast<std::int32_t>(LCQI_REFERENCE_DECODER_VOCAB_SIZE),
                    "logits checkpoint shape mismatch");
            require(checkpoint.values.size() == LCQI_REFERENCE_DECODER_LOGITS.size(),
                    "logits checkpoint value size mismatch");
            for (std::size_t index = 0; index < LCQI_REFERENCE_DECODER_LOGITS.size(); ++index) {
                require_close(checkpoint.values[index],
                              LCQI_REFERENCE_DECODER_LOGITS[index],
                              "trace logits checkpoint mismatch");
            }
        }
    }
    require(saw_kv_slot, "KV checkpoint missing");
    require(saw_logits, "logits checkpoint missing");
}

void test_packed_i8_kernel_matches_scalar() {
    for (const KernelShapeCase& shape_case : LCQI_KERNEL_SHAPE_CASES) {
        const lcqi::QuantizedLinearLayer layer =
            lcqi::make_deterministic_i8_layer(shape_case.input_size,
                                             shape_case.output_size,
                                             0.03125F);
        const lcqi::PackedLinearI8 packed =
            lcqi::pack_linear_i8(layer, shape_case.output_block_size);
        const std::vector<float> input = lcqi::make_deterministic_input(layer.input_size);
        std::vector<float> scalar_output(static_cast<std::size_t>(layer.output_size), 0.0F);
        std::vector<float> packed_output(static_cast<std::size_t>(layer.output_size), 0.0F);
        lcqi::linear_i8(layer, input, scalar_output);
        lcqi::linear_i8_packed(packed, input, packed_output);
        require(lcqi::max_abs_diff(scalar_output, packed_output) <= LCQI_KERNEL_MAX_DIFF,
                "packed int8 kernel output differs from scalar");

        const lcqi::PackedLinearI8 simd_packed =
            lcqi::pack_linear_i8(layer, LCQI_SIMD_TEST_BLOCK);
        if (lcqi::linear_i8_packed_avx2_available()) {
            std::vector<float> avx2_output(static_cast<std::size_t>(layer.output_size), 0.0F);
            lcqi::linear_i8_packed_avx2(simd_packed, input, avx2_output);
            require(lcqi::max_abs_diff(scalar_output, avx2_output) <= LCQI_KERNEL_MAX_DIFF,
                    "AVX2 int8 kernel output differs from scalar");
        }
    }
}

void test_f32_dot_kernel_matches_scalar() {
    std::vector<float> lhs(LCQI_F32_DOT_TEST_SIZE, 0.0F);
    std::vector<float> rhs(LCQI_F32_DOT_TEST_SIZE, 0.0F);
    for (std::size_t index = 0; index < LCQI_F32_DOT_TEST_SIZE; ++index) {
        lhs[index] = static_cast<float>(static_cast<std::int32_t>(index % 19U) + 1) *
                     0.03125F;
        rhs[index] = static_cast<float>(static_cast<std::int32_t>(index % 23U) - 11) *
                     0.0625F;
    }

    const float scalar =
        lcqi::dot_f32_scalar_unchecked(lhs.data(), rhs.data(), lhs.size());
    const float dispatched = lcqi::dot_f32(lhs, rhs);
    require(std::fabs(scalar - dispatched) <= LCQI_F32_KERNEL_MAX_DIFF,
            "F32 dot kernel output differs from scalar");

    if (lcqi::dot_f32_avx2_available()) {
        const float avx2 =
            lcqi::dot_f32_avx2_unchecked(lhs.data(), rhs.data(), lhs.size());
        require(std::fabs(scalar - avx2) <= LCQI_F32_KERNEL_MAX_DIFF,
                "AVX2 F32 dot kernel output differs from scalar");
    }

    bool rejected_mismatch = false;
    try {
        (void)lcqi::dot_f32(std::span<const float>(lhs.data(), lhs.size() - 1U), rhs);
    } catch (const std::runtime_error&) {
        rejected_mismatch = true;
    }
    require(rejected_mismatch, "F32 dot kernel accepted mismatched sizes");
}

void test_f32_row_kernels_match_scalar() {
    std::vector<float> weights(LCQI_F32_ROW_TEST_INPUT_SIZE *
                                   LCQI_F32_ROW_TEST_OUTPUT_SIZE,
                               0.0F);
    std::vector<float> input(LCQI_F32_ROW_TEST_INPUT_SIZE, 0.0F);
    std::vector<float> bias(LCQI_F32_ROW_TEST_OUTPUT_SIZE, 0.0F);
    for (std::size_t index = 0; index < weights.size(); ++index) {
        weights[index] = static_cast<float>(static_cast<std::int32_t>(index % 29U) - 14) *
                         0.015625F;
    }
    for (std::size_t index = 0; index < input.size(); ++index) {
        input[index] = static_cast<float>(static_cast<std::int32_t>(index % 13U) - 6) *
                       0.03125F;
    }
    for (std::size_t index = 0; index < bias.size(); ++index) {
        bias[index] = static_cast<float>(static_cast<std::int32_t>(index % 7U) - 3) *
                      0.125F;
    }

    std::vector<float> scalar_output(LCQI_F32_ROW_TEST_OUTPUT_SIZE, 0.0F);
    std::vector<float> dispatched_output(LCQI_F32_ROW_TEST_OUTPUT_SIZE, 0.0F);
    lcqi::linear_f32_rows_scalar_unchecked(weights.data(),
                                           input.data(),
                                           bias.data(),
                                           input.size(),
                                           0,
                                           LCQI_F32_ROW_TEST_OUTPUT_SIZE,
                                           scalar_output.data());
    lcqi::linear_f32_rows_unchecked(weights.data(),
                                    input.data(),
                                    bias.data(),
                                    input.size(),
                                    0,
                                    LCQI_F32_ROW_TEST_OUTPUT_SIZE,
                                    dispatched_output.data());
    for (std::size_t index = 0; index < scalar_output.size(); ++index) {
        require(std::fabs(scalar_output[index] - dispatched_output[index]) <=
                    LCQI_F32_KERNEL_MAX_DIFF,
                "F32 linear row kernel output differs from scalar");
    }

    const lcqi::F32RowMax scalar_max =
        lcqi::max_dot_f32_rows_scalar_unchecked(weights.data(),
                                                input.data(),
                                                input.size(),
                                                0,
                                                LCQI_F32_ROW_TEST_OUTPUT_SIZE);
    const lcqi::F32RowMax dispatched_max =
        lcqi::max_dot_f32_rows_unchecked(weights.data(),
                                         input.data(),
                                         input.size(),
                                         0,
                                         LCQI_F32_ROW_TEST_OUTPUT_SIZE);
    require(scalar_max.row == dispatched_max.row,
            "F32 row max kernel selected a different row");
    require(std::fabs(scalar_max.value - dispatched_max.value) <= LCQI_F32_KERNEL_MAX_DIFF,
            "F32 row max kernel value differs from scalar");

    if (lcqi::dot_f32_avx2_available()) {
        std::vector<float> avx2_output(LCQI_F32_ROW_TEST_OUTPUT_SIZE, 0.0F);
        lcqi::linear_f32_rows_avx2_unchecked(weights.data(),
                                             input.data(),
                                             bias.data(),
                                             input.size(),
                                             0,
                                             LCQI_F32_ROW_TEST_OUTPUT_SIZE,
                                             avx2_output.data());
        for (std::size_t index = 0; index < scalar_output.size(); ++index) {
            require(std::fabs(scalar_output[index] - avx2_output[index]) <=
                        LCQI_F32_KERNEL_MAX_DIFF,
                    "AVX2 F32 linear row kernel output differs from scalar");
        }

        const lcqi::F32RowMax avx2_max =
            lcqi::max_dot_f32_rows_avx2_unchecked(weights.data(),
                                                  input.data(),
                                                  input.size(),
                                                  0,
                                                  LCQI_F32_ROW_TEST_OUTPUT_SIZE);
        require(scalar_max.row == avx2_max.row,
                "AVX2 F32 row max kernel selected a different row");
        require(std::fabs(scalar_max.value - avx2_max.value) <= LCQI_F32_KERNEL_MAX_DIFF,
                "AVX2 F32 row max kernel value differs from scalar");
    }
}

}  // namespace

int main() {
    try {
        test_tiny_mlp();
        test_tokenizer_contract();
        test_tokenizer_rejects_unknown_without_unk();
        test_safetensors_manifest_contract();
        test_safetensors_rejects_bad_offsets();
        test_safetensors_rejects_bad_dtype_shape_size();
        test_safetensors_reads_float_tensors();
        test_gguf_manifest_reads_q4_k_tensor();
        test_q4_k_dequant_and_dot_paths();
        test_ggml_mixed_dequantizers();
        test_ggml_quantized_f32_matvec_paths();
        test_llama_gguf_loader_and_generation();
        test_llama_gguf_q4_direct_generation();
        test_llama_gguf_ggml_direct_generation();
        test_gpt2_tiny_forward_and_generation();
        test_gpt2_byte_bpe_tokenizer();
        test_gpt2_loads_hf_style_tiny_checkpoint();
        test_gpt2_loader_rejects_bad_shape();
        test_reference_rms_norm();
        test_reference_rope();
        test_reference_kv_attention();
        test_reference_decoder_end_to_end();
        test_reference_decoder_trace_contract();
        test_packed_i8_kernel_matches_scalar();
        test_f32_dot_kernel_matches_scalar();
        test_f32_row_kernels_match_scalar();

        std::cout << "lcqi tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "lcqi test failure: " << error.what() << "\n";
        return EXIT_FAILURE;
    }
}

#include <lcqi/inference.hpp>
#include <lcqi/int8_kernels.hpp>
#include <lcqi/model.hpp>
#include <lcqi/reference_decoder.hpp>

#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <span>
#include <stdexcept>
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
constexpr std::int32_t LCQI_SIMD_TEST_BLOCK = 8;
constexpr std::int32_t LCQI_REFERENCE_DECODER_PREDICTED_TOKEN = 0;
constexpr std::size_t LCQI_REFERENCE_DECODER_VOCAB_SIZE = 5;
constexpr std::array<float, LCQI_REFERENCE_DECODER_VOCAB_SIZE> LCQI_REFERENCE_DECODER_LOGITS{
    0.352516979F,
    -0.126884326F,
    0.0797837451F,
    -0.29797405F,
    0.127030417F,
};

struct KernelShapeCase {
    std::int32_t input_size = 0;
    std::int32_t output_size = 0;
    std::int32_t output_block_size = 0;
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
        if (lcqi::linear_i8_packed_neon_available()) {
            std::vector<float> neon_output(static_cast<std::size_t>(layer.output_size), 0.0F);
            lcqi::linear_i8_packed_neon(simd_packed, input, neon_output);
            require(lcqi::max_abs_diff(scalar_output, neon_output) <= LCQI_KERNEL_MAX_DIFF,
                    "NEON int8 kernel output differs from scalar");
        }
    }
}

}  // namespace

int main() {
    try {
        test_tiny_mlp();
        test_reference_rms_norm();
        test_reference_rope();
        test_reference_kv_attention();
        test_reference_decoder_end_to_end();
        test_packed_i8_kernel_matches_scalar();

        std::cout << "lcqi tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "lcqi test failure: " << error.what() << "\n";
        return EXIT_FAILURE;
    }
}

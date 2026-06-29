#include <lcqi/gpt2_reference.hpp>
#include <lcqi/f32_kernels.hpp>
#include <lcqi/inference.hpp>
#include <lcqi/int8_kernels.hpp>
#include <lcqi/model.hpp>
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
constexpr std::uint16_t LCQI_TEST_F16_NEGATIVE_TWO = 0xC000U;
constexpr std::uint16_t LCQI_TEST_BF16_ONE_POINT_FIVE = 0x3FC0U;
constexpr std::uint16_t LCQI_TEST_BF16_NEGATIVE_HALF = 0xBF00U;

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

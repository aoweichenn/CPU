#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace lcqi {

struct Gpt2Config {
    std::int32_t hidden_size = 0;
    std::int32_t head_count = 0;
    std::int32_t layer_count = 0;
    std::int32_t max_positions = 0;
    std::int32_t vocab_size = 0;
    std::int32_t intermediate_size = 0;
    std::int32_t bos_token_id = -1;
    std::int32_t eos_token_id = -1;
    float layer_norm_epsilon = 1.0e-5F;
    std::string activation_function = "gelu_new";
};

struct Gpt2LinearF32 {
    std::int32_t input_size = 0;
    std::int32_t output_size = 0;
    std::vector<float> weights;
    std::vector<float> bias;
};

struct Gpt2LayerWeightsF32 {
    std::vector<float> ln_1_weight;
    std::vector<float> ln_1_bias;
    Gpt2LinearF32 c_attn;
    Gpt2LinearF32 c_proj;
    std::vector<float> ln_2_weight;
    std::vector<float> ln_2_bias;
    Gpt2LinearF32 c_fc;
    Gpt2LinearF32 mlp_c_proj;
};

struct Gpt2ReferenceModel {
    Gpt2Config config;
    std::vector<float> token_embedding;
    std::vector<float> position_embedding;
    std::vector<Gpt2LayerWeightsF32> layers;
    std::vector<float> final_ln_weight;
    std::vector<float> final_ln_bias;
    std::vector<float> lm_head_weight;
    bool tie_lm_head_to_embedding = true;
};

struct Gpt2ForwardResult {
    std::vector<float> logits;
    std::int32_t predicted_token = 0;
};

class Gpt2KvCache {
public:
    explicit Gpt2KvCache(const Gpt2Config& config);

    void append(std::int32_t layer_id,
                std::int32_t model_position,
                std::span<const float> key,
                std::span<const float> value);

    [[nodiscard]] std::span<const float> key(std::int32_t layer_id,
                                             std::int32_t model_position,
                                             std::int32_t head) const;

    [[nodiscard]] std::span<const float> value(std::int32_t layer_id,
                                               std::int32_t model_position,
                                               std::int32_t head) const;

    [[nodiscard]] std::int32_t filled_tokens() const noexcept;
    [[nodiscard]] std::size_t byte_size() const noexcept;

private:
    [[nodiscard]] std::size_t base_offset(std::int32_t layer_id,
                                          std::int32_t model_position,
                                          std::int32_t head) const;
    void validate_address(std::int32_t layer_id,
                          std::int32_t model_position,
                          std::int32_t head) const;
    void validate_written(std::int32_t layer_id,
                          std::int32_t model_position) const;

    Gpt2Config config_;
    std::int32_t filled_tokens_ = 0;
    std::vector<float> keys_;
    std::vector<float> values_;
    std::vector<std::uint8_t> written_;
};

struct Gpt2Tokenizer {
    std::int32_t bos_token_id = -1;
    std::int32_t eos_token_id = -1;
    std::unordered_map<std::string, std::int32_t> vocab;
    std::vector<std::string> id_to_token;
    std::unordered_map<std::string, std::int32_t> bpe_ranks;
};

[[nodiscard]] Gpt2Tokenizer load_gpt2_tokenizer(
    const std::filesystem::path& vocab_json,
    const std::filesystem::path& merges_txt,
    std::int32_t bos_token_id,
    std::int32_t eos_token_id);

[[nodiscard]] std::vector<std::int32_t> gpt2_encode(
    const Gpt2Tokenizer& tokenizer,
    std::string_view text);

[[nodiscard]] std::string gpt2_decode(
    const Gpt2Tokenizer& tokenizer,
    std::span<const std::int32_t> token_ids);

[[nodiscard]] Gpt2Config load_gpt2_config(const std::filesystem::path& config_json);

[[nodiscard]] Gpt2ReferenceModel load_gpt2_reference_model(
    const std::filesystem::path& config_json,
    const std::filesystem::path& safetensors_path);

[[nodiscard]] Gpt2ReferenceModel load_gpt2_from_directory(
    const std::filesystem::path& model_directory);

[[nodiscard]] Gpt2ForwardResult run_gpt2_forward(
    const Gpt2ReferenceModel& model,
    std::span<const std::int32_t> token_ids);

[[nodiscard]] Gpt2ForwardResult run_gpt2_forward_cached(
    const Gpt2ReferenceModel& model,
    Gpt2KvCache& cache,
    std::int32_t token_id);

[[nodiscard]] std::vector<std::int32_t> gpt2_generate_greedy(
    const Gpt2ReferenceModel& model,
    std::span<const std::int32_t> prompt_token_ids,
    std::int32_t max_new_tokens);

[[nodiscard]] std::vector<std::int32_t> gpt2_generate_greedy_cached(
    const Gpt2ReferenceModel& model,
    std::span<const std::int32_t> prompt_token_ids,
    std::int32_t max_new_tokens);

[[nodiscard]] Gpt2ReferenceModel make_tiny_gpt2_reference_model();

}  // namespace lcqi

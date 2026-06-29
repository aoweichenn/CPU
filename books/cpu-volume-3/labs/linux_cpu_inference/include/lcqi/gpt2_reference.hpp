#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
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

struct Gpt2HotspotProfile {
    std::int64_t decoder_steps = 0;
    std::int64_t layer_steps = 0;
    double total_step_ms = 0.0;
    double embedding_ms = 0.0;
    double layer_norm_ms = 0.0;
    double final_norm_ms = 0.0;
    double qkv_projection_ms = 0.0;
    double kv_append_ms = 0.0;
    double attention_ms = 0.0;
    double attention_projection_ms = 0.0;
    double residual_add_ms = 0.0;
    double mlp_fc_ms = 0.0;
    double gelu_ms = 0.0;
    double mlp_projection_ms = 0.0;
    double lm_head_ms = 0.0;
    double logits_result_ms = 0.0;
};

struct Gpt2ExecutionOptions {
    std::int32_t worker_count = 0;
    std::int32_t parallel_min_rows = 512;
};

class Gpt2KvCache;

namespace detail {
class Gpt2ParallelWorkerPool;

void attend_cached_position(const Gpt2KvCache& cache,
                            std::int32_t layer_id,
                            std::int32_t model_position,
                            std::span<const float> query,
                            std::span<float> scores,
                            std::span<float> output);
}  // namespace detail

class Gpt2ForwardWorkspace {
public:
    explicit Gpt2ForwardWorkspace(const Gpt2Config& config);

    void reset_for_config(const Gpt2Config& config);

    [[nodiscard]] std::span<float> hidden() noexcept;
    [[nodiscard]] std::span<float> normed() noexcept;
    [[nodiscard]] std::span<float> query() noexcept;
    [[nodiscard]] std::span<float> key() noexcept;
    [[nodiscard]] std::span<float> value() noexcept;
    [[nodiscard]] std::span<float> attention() noexcept;
    [[nodiscard]] std::span<float> projected() noexcept;
    [[nodiscard]] std::span<float> qkv_packed() noexcept;
    [[nodiscard]] std::span<float> mlp_fc() noexcept;
    [[nodiscard]] std::span<float> mlp_out() noexcept;
    [[nodiscard]] std::span<float> logits() noexcept;
    [[nodiscard]] std::span<float> scores_prefix(std::int32_t count);

private:
    Gpt2Config config_;
    std::vector<float> hidden_;
    std::vector<float> normed_;
    std::vector<float> query_;
    std::vector<float> key_;
    std::vector<float> value_;
    std::vector<float> attention_;
    std::vector<float> projected_;
    std::vector<float> qkv_packed_;
    std::vector<float> mlp_fc_;
    std::vector<float> mlp_out_;
    std::vector<float> logits_;
    std::vector<float> scores_;
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
    friend void detail::attend_cached_position(const Gpt2KvCache& cache,
                                               std::int32_t layer_id,
                                               std::int32_t model_position,
                                               std::span<const float> query,
                                               std::span<float> scores,
                                               std::span<float> output);

    [[nodiscard]] std::size_t base_offset(std::int32_t layer_id,
                                          std::int32_t model_position,
                                          std::int32_t head) const;
    [[nodiscard]] std::size_t base_offset_unchecked(std::int32_t layer_id,
                                                    std::int32_t model_position,
                                                    std::int32_t head) const noexcept;
    [[nodiscard]] std::span<const float> key_unchecked(std::int32_t layer_id,
                                                       std::int32_t model_position,
                                                       std::int32_t head) const noexcept;
    [[nodiscard]] std::span<const float> value_unchecked(std::int32_t layer_id,
                                                         std::int32_t model_position,
                                                         std::int32_t head) const noexcept;
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

class Gpt2CachedGreedyDecoder {
public:
    explicit Gpt2CachedGreedyDecoder(const Gpt2ReferenceModel& model);
    Gpt2CachedGreedyDecoder(const Gpt2ReferenceModel& model,
                            Gpt2HotspotProfile* hotspot_profile);
    Gpt2CachedGreedyDecoder(const Gpt2ReferenceModel& model,
                            Gpt2HotspotProfile* hotspot_profile,
                            const Gpt2ExecutionOptions& execution_options);
    ~Gpt2CachedGreedyDecoder();

    Gpt2CachedGreedyDecoder(const Gpt2CachedGreedyDecoder&) = delete;
    Gpt2CachedGreedyDecoder& operator=(const Gpt2CachedGreedyDecoder&) = delete;
    Gpt2CachedGreedyDecoder(Gpt2CachedGreedyDecoder&&) noexcept;
    Gpt2CachedGreedyDecoder& operator=(Gpt2CachedGreedyDecoder&&) noexcept;

    [[nodiscard]] std::int32_t step(std::int32_t token_id);
    [[nodiscard]] Gpt2ForwardResult step_with_logits(std::int32_t token_id);
    [[nodiscard]] const Gpt2KvCache& cache() const noexcept;
    [[nodiscard]] std::int32_t filled_tokens() const noexcept;
    [[nodiscard]] std::size_t kv_cache_bytes() const noexcept;
    [[nodiscard]] std::int32_t worker_count() const noexcept;

private:
    const Gpt2ReferenceModel* model_;
    Gpt2HotspotProfile* hotspot_profile_;
    Gpt2ExecutionOptions execution_options_;
    Gpt2KvCache cache_;
    Gpt2ForwardWorkspace workspace_;
    std::unique_ptr<detail::Gpt2ParallelWorkerPool> worker_pool_;
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

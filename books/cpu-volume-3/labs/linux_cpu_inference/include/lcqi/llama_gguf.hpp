#pragma once

#include <lcqi/gpt2_reference.hpp>
#include <lcqi/gguf.hpp>
#include <lcqi/reference_decoder.hpp>

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace lcqi {

enum class LlamaGgufLinearStorage : std::uint8_t {
    f32,
    q4_k,
};

struct LlamaGgufLinearWeights {
    std::int32_t input_size = 0;
    std::int32_t output_size = 0;
    GgmlType source_type = GgmlType::f32;
    LlamaGgufLinearStorage storage = LlamaGgufLinearStorage::f32;
    std::vector<float> f32_weights;
    std::vector<std::uint8_t> q4_k_bytes;
};

struct LlamaGgufDecoderLayerWeights {
    std::vector<float> rms_attention_weight;
    LlamaGgufLinearWeights wq;
    LlamaGgufLinearWeights wk;
    LlamaGgufLinearWeights wv;
    LlamaGgufLinearWeights wo;
    std::vector<float> rms_mlp_weight;
    LlamaGgufLinearWeights w_gate;
    LlamaGgufLinearWeights w_up;
    LlamaGgufLinearWeights w_down;
};

struct LlamaGgufDecoderModel {
    DecoderConfig config;
    std::vector<float> token_embedding;
    std::vector<LlamaGgufDecoderLayerWeights> layers;
    std::vector<float> final_norm_weight;
    LlamaGgufLinearWeights lm_head;
};

struct LlamaGgufModel {
    LlamaGgufDecoderModel decoder;
    std::string architecture;
    std::string name;
    std::int32_t bos_token_id = -1;
    std::int32_t eos_token_id = -1;
    std::int32_t unknown_token_id = -1;
    bool lm_head_tied_to_embedding = true;
};

struct LlamaGgufLoadReport {
    double manifest_ms = 0.0;
    double weights_ms = 0.0;
    std::uint64_t quantized_weight_bytes = 0;
    std::uint64_t f32_weight_bytes = 0;
    std::uint64_t direct_quantized_weight_bytes = 0;
    std::uint64_t fallback_dequantized_weight_bytes = 0;
    std::int64_t tensors_loaded = 0;
    std::int64_t q4_k_direct_tensors = 0;
    std::int64_t f32_fallback_tensors = 0;
};

struct LlamaGgufLoadedModel {
    LlamaGgufModel model;
    LlamaGgufLoadReport report;
};

struct LlamaGgufHotspotReport {
    double rms_norm_ms = 0.0;
    double attention_ms = 0.0;
    double rope_ms = 0.0;
    double wq_ms = 0.0;
    double wk_ms = 0.0;
    double wv_ms = 0.0;
    double wo_ms = 0.0;
    double w_gate_ms = 0.0;
    double w_up_ms = 0.0;
    double w_down_ms = 0.0;
    double lm_head_ms = 0.0;
    double q4_k_direct_ms = 0.0;
    double f32_fallback_ms = 0.0;
    std::uint64_t q4_k_direct_calls = 0;
    std::uint64_t f32_fallback_calls = 0;
};

struct LlamaGgufGenerationResult {
    std::vector<std::int32_t> prompt_ids;
    std::vector<std::int32_t> generated_ids;
    LlamaGgufHotspotReport hotspots;
    double load_ms = 0.0;
    double prefill_ms = 0.0;
    double decode_ms = 0.0;
    double total_ms = 0.0;
    std::size_t kv_cache_bytes = 0;
    std::size_t prefill_steps = 0;
    std::size_t decode_steps = 0;
    std::int32_t predicted_first_token = -1;
};

[[nodiscard]] LlamaGgufLoadedModel load_llama_gguf_reference_model(
    const std::filesystem::path& gguf_path);

[[nodiscard]] Gpt2Tokenizer load_gpt2_tokenizer_from_gguf(
    const std::filesystem::path& gguf_path);

[[nodiscard]] std::vector<std::int32_t> llama_gguf_chat_prompt_ids(
    const Gpt2Tokenizer& tokenizer,
    std::string_view user_prompt,
    std::int32_t bos_token_id);

[[nodiscard]] LlamaGgufGenerationResult llama_gguf_generate_greedy(
    const LlamaGgufModel& model,
    std::span<const std::int32_t> prompt_ids,
    std::int32_t max_new_tokens);

}  // namespace lcqi

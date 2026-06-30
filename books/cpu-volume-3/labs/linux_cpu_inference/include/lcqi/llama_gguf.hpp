#pragma once

#include <lcqi/gpt2_reference.hpp>
#include <lcqi/reference_decoder.hpp>

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace lcqi {

struct LlamaGgufModel {
    ReferenceDecoderModel decoder;
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
    std::int64_t tensors_loaded = 0;
};

struct LlamaGgufLoadedModel {
    LlamaGgufModel model;
    LlamaGgufLoadReport report;
};

struct LlamaGgufGenerationResult {
    std::vector<std::int32_t> prompt_ids;
    std::vector<std::int32_t> generated_ids;
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

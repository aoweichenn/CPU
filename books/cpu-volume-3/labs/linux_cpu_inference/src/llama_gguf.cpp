#include <lcqi/llama_gguf.hpp>

#include <lcqi/f32_kernels.hpp>
#include <lcqi/ggml_tensors.hpp>
#include <lcqi/gguf.hpp>
#include <lcqi/q4_k.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

namespace lcqi {
namespace {

constexpr std::int32_t LCQI_LLAMA_ROPE_PAIR_WIDTH = 2;
constexpr std::int32_t LCQI_LLAMA_DEFAULT_BOS_TOKEN = 1;
constexpr std::int32_t LCQI_LLAMA_DEFAULT_EOS_TOKEN = 2;
constexpr std::int32_t LCQI_LLAMA_DEFAULT_UNK_TOKEN = 0;
constexpr const char* LCQI_LLAMA_ARCHITECTURE_KEY = "general.architecture";
constexpr const char* LCQI_LLAMA_NAME_KEY = "general.name";
constexpr const char* LCQI_LLAMA_EMBEDDING_LENGTH_KEY = "llama.embedding_length";
constexpr const char* LCQI_LLAMA_BLOCK_COUNT_KEY = "llama.block_count";
constexpr const char* LCQI_LLAMA_FEED_FORWARD_LENGTH_KEY = "llama.feed_forward_length";
constexpr const char* LCQI_LLAMA_HEAD_COUNT_KEY = "llama.attention.head_count";
constexpr const char* LCQI_LLAMA_KV_HEAD_COUNT_KEY = "llama.attention.head_count_kv";
constexpr const char* LCQI_LLAMA_ROPE_DIMENSION_KEY = "llama.rope.dimension_count";
constexpr const char* LCQI_LLAMA_ROPE_BASE_KEY = "llama.rope.freq_base";
constexpr const char* LCQI_LLAMA_CONTEXT_LENGTH_KEY = "llama.context_length";
constexpr const char* LCQI_LLAMA_VOCAB_SIZE_KEY = "llama.vocab_size";
constexpr const char* LCQI_LLAMA_RMS_EPSILON_KEY = "llama.attention.layer_norm_rms_epsilon";
constexpr const char* LCQI_LLAMA_TOKENIZER_TOKENS_KEY = "tokenizer.ggml.tokens";
constexpr const char* LCQI_LLAMA_TOKENIZER_MERGES_KEY = "tokenizer.ggml.merges";
constexpr const char* LCQI_LLAMA_BOS_TOKEN_KEY = "tokenizer.ggml.bos_token_id";
constexpr const char* LCQI_LLAMA_EOS_TOKEN_KEY = "tokenizer.ggml.eos_token_id";
constexpr const char* LCQI_LLAMA_UNKNOWN_TOKEN_KEY = "tokenizer.ggml.unknown_token_id";
constexpr const char* LCQI_LLAMA_Q4_DIRECT_ENV = "LCQI_LLAMA_Q4_DIRECT";
constexpr const char* LCQI_LLAMA_FALSE_ENV = "0";

using Clock = std::chrono::steady_clock;

enum class LlamaGgufLinearOp : std::uint8_t {
    wq,
    wk,
    wv,
    wo,
    w_gate,
    w_up,
    w_down,
    lm_head,
};

[[nodiscard]] double elapsed_ms(Clock::time_point begin, Clock::time_point end) {
    return std::chrono::duration<double, std::milli>(end - begin).count();
}

[[nodiscard]] std::size_t checked_size(std::int64_t value, const char* name) {
    if (value < 0 ||
        static_cast<std::uint64_t>(value) >
            static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        throw std::runtime_error(std::string(name) + " is outside size_t range");
    }
    return static_cast<std::size_t>(value);
}

void require_positive(std::int32_t value, const char* name) {
    if (value <= 0) {
        throw std::runtime_error(std::string(name) + " must be positive");
    }
}

[[nodiscard]] const GgufMetadataEntry& require_metadata(const GgufManifest& manifest,
                                                        std::string_view key) {
    const GgufMetadataEntry* entry = manifest.find_metadata(key);
    if (entry == nullptr) {
        throw std::runtime_error("missing GGUF metadata key: " + std::string(key));
    }
    return *entry;
}

[[nodiscard]] std::string metadata_string(const GgufManifest& manifest, std::string_view key) {
    return require_metadata(manifest, key).value_preview;
}

[[nodiscard]] std::int32_t metadata_i32(const GgufManifest& manifest, std::string_view key) {
    const std::int64_t value = std::stoll(require_metadata(manifest, key).value_preview);
    if (value < std::numeric_limits<std::int32_t>::min() ||
        value > std::numeric_limits<std::int32_t>::max()) {
        throw std::runtime_error("GGUF metadata int32 out of range: " + std::string(key));
    }
    return static_cast<std::int32_t>(value);
}

[[nodiscard]] std::int32_t optional_metadata_i32(const GgufManifest& manifest,
                                                 std::string_view key,
                                                 std::int32_t fallback) {
    const GgufMetadataEntry* entry = manifest.find_metadata(key);
    if (entry == nullptr) {
        return fallback;
    }
    const std::int64_t value = std::stoll(entry->value_preview);
    if (value < std::numeric_limits<std::int32_t>::min() ||
        value > std::numeric_limits<std::int32_t>::max()) {
        throw std::runtime_error("GGUF metadata int32 out of range: " + std::string(key));
    }
    return static_cast<std::int32_t>(value);
}

[[nodiscard]] float metadata_f32(const GgufManifest& manifest, std::string_view key) {
    return std::stof(require_metadata(manifest, key).value_preview);
}

void validate_shape(const GgufTensorInfo& tensor,
                    std::span<const std::int64_t> expected_shape) {
    if (tensor.shape.size() != expected_shape.size()) {
        throw std::runtime_error("GGUF tensor rank mismatch: " + tensor.name);
    }
    for (std::size_t index = 0; index < expected_shape.size(); ++index) {
        if (tensor.shape[index] != expected_shape[index]) {
            throw std::runtime_error("GGUF tensor shape mismatch: " + tensor.name);
        }
    }
}

[[nodiscard]] bool can_use_q4_k_direct(const GgufTensorInfo& tensor) {
    return tensor.type == GgmlType::q4_k && tensor.shape.size() == 2 &&
           tensor.shape[0] > 0 && tensor.shape[1] > 0 &&
           tensor.shape[0] % LCQI_QK_K_BLOCK_VALUES == 0;
}

[[nodiscard]] bool q4_k_direct_enabled() noexcept {
    const char* enabled = std::getenv(LCQI_LLAMA_Q4_DIRECT_ENV);
    return enabled == nullptr || std::string_view(enabled) != LCQI_LLAMA_FALSE_ENV;
}

void account_f32_tensor_load(const GgufTensorInfo& tensor,
                             std::span<const float> values,
                             LlamaGgufLoadReport& report) {
    report.quantized_weight_bytes += tensor.byte_size;
    const std::uint64_t f32_bytes =
        static_cast<std::uint64_t>(values.size()) * static_cast<std::uint64_t>(sizeof(float));
    report.f32_weight_bytes += f32_bytes;
    report.fallback_dequantized_weight_bytes += f32_bytes;
    ++report.tensors_loaded;
    ++report.f32_fallback_tensors;
}

[[nodiscard]] std::vector<float> read_tensor_f32(const std::filesystem::path& gguf_path,
                                                 const GgufManifest& manifest,
                                                 std::string_view name,
                                                 std::span<const std::int64_t> expected_shape,
                                                 LlamaGgufLoadReport& report) {
    const GgufTensorInfo* tensor = manifest.find_tensor(name);
    if (tensor == nullptr) {
        throw std::runtime_error("missing LLaMA GGUF tensor: " + std::string(name));
    }
    validate_shape(*tensor, expected_shape);
    const std::vector<std::uint8_t> bytes = read_gguf_tensor_bytes(gguf_path, *tensor);
    std::vector<float> values =
        dequantize_ggml_tensor(tensor->type,
                               bytes,
                               static_cast<std::int64_t>(tensor->element_count()));
    account_f32_tensor_load(*tensor, values, report);
    return values;
}

void account_q4_k_direct_tensor_load(const GgufTensorInfo& tensor,
                                     LlamaGgufLoadReport& report) {
    report.quantized_weight_bytes += tensor.byte_size;
    report.direct_quantized_weight_bytes += tensor.byte_size;
    ++report.tensors_loaded;
    ++report.q4_k_direct_tensors;
}

[[nodiscard]] LlamaGgufLinearWeights make_linear_from_gguf(
    const std::filesystem::path& gguf_path,
    const GgufManifest& manifest,
    std::string_view name,
    std::int32_t input_size,
    std::int32_t output_size,
    LlamaGgufLoadReport& report) {
    const std::array<std::int64_t, 2> shape{input_size, output_size};
    const GgufTensorInfo* tensor = manifest.find_tensor(name);
    if (tensor == nullptr) {
        throw std::runtime_error("missing LLaMA GGUF tensor: " + std::string(name));
    }
    validate_shape(*tensor, shape);

    LlamaGgufLinearWeights linear;
    linear.input_size = input_size;
    linear.output_size = output_size;
    linear.source_type = tensor->type;
    const std::vector<std::uint8_t> bytes = read_gguf_tensor_bytes(gguf_path, *tensor);
    if (q4_k_direct_enabled() && can_use_q4_k_direct(*tensor)) {
        linear.storage = LlamaGgufLinearStorage::q4_k;
        linear.q4_k_bytes = bytes;
        account_q4_k_direct_tensor_load(*tensor, report);
        return linear;
    }

    linear.storage = LlamaGgufLinearStorage::f32;
    linear.f32_weights =
        dequantize_ggml_tensor(tensor->type,
                               bytes,
                               static_cast<std::int64_t>(tensor->element_count()));
    account_f32_tensor_load(*tensor, linear.f32_weights, report);
    return linear;
}

[[nodiscard]] std::string layer_tensor_name(std::int32_t layer_id, std::string_view suffix) {
    return "blk." + std::to_string(layer_id) + "." + std::string(suffix);
}

void validate_llama_config(const DecoderConfig& config, std::int32_t layer_count) {
    require_positive(layer_count, "layer_count");
    require_positive(config.hidden_size, "hidden_size");
    require_positive(config.query_heads, "query_heads");
    require_positive(config.kv_heads, "kv_heads");
    require_positive(config.head_dim, "head_dim");
    require_positive(config.intermediate_size, "intermediate_size");
    require_positive(config.vocab_size, "vocab_size");
    require_positive(config.max_context, "max_context");
    if (config.query_heads % config.kv_heads != 0) {
        throw std::runtime_error("LLaMA query_heads must be divisible by kv_heads");
    }
    if (config.hidden_size != config.query_heads * config.head_dim) {
        throw std::runtime_error("LLaMA hidden_size must equal query_heads * head_dim");
    }
    if (config.head_dim % LCQI_LLAMA_ROPE_PAIR_WIDTH != 0) {
        throw std::runtime_error("LLaMA head_dim must be even for RoPE");
    }
}

[[nodiscard]] std::span<const float> row_span(std::span<const float> values,
                                              std::int32_t row,
                                              std::int32_t row_width) {
    return values.subspan(checked_size(row, "row") * checked_size(row_width, "row_width"),
                          checked_size(row_width, "row_width"));
}

void add_inplace(std::span<float> target, std::span<const float> source) {
    if (target.size() != source.size()) {
        throw std::runtime_error("LLaMA residual add size mismatch");
    }
    for (std::size_t index = 0; index < target.size(); ++index) {
        target[index] += source[index];
    }
}

[[nodiscard]] float silu(float value) {
    return value / (1.0F + std::exp(-value));
}

template <typename Func>
double measure_ms(Func&& func) {
    const Clock::time_point begin = Clock::now();
    std::forward<Func>(func)();
    return elapsed_ms(begin, Clock::now());
}

[[nodiscard]] double& linear_hotspot_field(LlamaGgufHotspotReport& hotspots,
                                           LlamaGgufLinearOp operation) {
    switch (operation) {
        case LlamaGgufLinearOp::wq:
            return hotspots.wq_ms;
        case LlamaGgufLinearOp::wk:
            return hotspots.wk_ms;
        case LlamaGgufLinearOp::wv:
            return hotspots.wv_ms;
        case LlamaGgufLinearOp::wo:
            return hotspots.wo_ms;
        case LlamaGgufLinearOp::w_gate:
            return hotspots.w_gate_ms;
        case LlamaGgufLinearOp::w_up:
            return hotspots.w_up_ms;
        case LlamaGgufLinearOp::w_down:
            return hotspots.w_down_ms;
        case LlamaGgufLinearOp::lm_head:
            return hotspots.lm_head_ms;
    }
    throw std::runtime_error("unknown LLaMA linear hotspot operation");
}

void validate_linear_shape(const LlamaGgufLinearWeights& layer,
                           std::span<const float> input,
                           std::span<float> output) {
    if (input.size() != checked_size(layer.input_size, "linear input size") ||
        output.size() != checked_size(layer.output_size, "linear output size")) {
        throw std::runtime_error("LLaMA GGUF linear input/output size mismatch");
    }
}

void linear_f32_fallback(const LlamaGgufLinearWeights& layer,
                         std::span<const float> input,
                         std::span<float> output) {
    if (layer.f32_weights.size() !=
        checked_size(layer.input_size, "linear input size") *
            checked_size(layer.output_size, "linear output size")) {
        throw std::runtime_error("LLaMA GGUF F32 linear weight size mismatch");
    }
    linear_f32_rows_unchecked(layer.f32_weights.data(),
                              input.data(),
                              nullptr,
                              checked_size(layer.input_size, "linear input size"),
                              0,
                              checked_size(layer.output_size, "linear output size"),
                              output.data());
}

void linear_q4_k_direct(const LlamaGgufLinearWeights& layer,
                        std::span<const Q8KBlock> q8_input,
                        std::span<float> output) {
    if (q8_input.size() !=
        checked_size(layer.input_size / LCQI_QK_K_BLOCK_VALUES, "Q8_K input block count")) {
        throw std::runtime_error("LLaMA GGUF Q4_K scratch block count mismatch");
    }
    matvec_q4_k_q8(layer.q4_k_bytes,
                   layer.output_size,
                   layer.input_size,
                   q8_input,
                   output);
}

void linear_gguf(const LlamaGgufLinearWeights& layer,
                 std::span<const float> input,
                 std::span<const Q8KBlock> q8_input,
                 std::span<float> output,
                 LlamaGgufLinearOp operation,
                 LlamaGgufHotspotReport& hotspots) {
    validate_linear_shape(layer, input, output);
    double elapsed = 0.0;
    switch (layer.storage) {
        case LlamaGgufLinearStorage::q4_k:
            elapsed = measure_ms([&]() { linear_q4_k_direct(layer, q8_input, output); });
            hotspots.q4_k_direct_ms += elapsed;
            ++hotspots.q4_k_direct_calls;
            break;
        case LlamaGgufLinearStorage::f32:
            elapsed = measure_ms([&]() { linear_f32_fallback(layer, input, output); });
            hotspots.f32_fallback_ms += elapsed;
            ++hotspots.f32_fallback_calls;
            break;
    }
    linear_hotspot_field(hotspots, operation) += elapsed;
}

[[nodiscard]] bool uses_q4_k_direct(const LlamaGgufLinearWeights& layer) noexcept {
    return layer.storage == LlamaGgufLinearStorage::q4_k;
}

[[nodiscard]] std::size_t q8_scratch_blocks(std::int32_t input_size) {
    if (input_size <= 0 || input_size % LCQI_QK_K_BLOCK_VALUES != 0) {
        return 0;
    }
    return checked_size(input_size / LCQI_QK_K_BLOCK_VALUES, "Q8_K scratch blocks");
}

void prepare_q8_if_needed(std::span<const float> input,
                          std::span<Q8KBlock> scratch,
                          bool needed) {
    if (!needed) {
        return;
    }
    quantize_q8_k_input_to(input, scratch);
}

struct LlamaWorkspace {
    std::vector<float> hidden;
    std::vector<float> normed;
    std::vector<float> query;
    std::vector<float> key;
    std::vector<float> value;
    std::vector<float> attention;
    std::vector<float> projected;
    std::vector<float> gate;
    std::vector<float> up;
    std::vector<float> mlp_hidden;
    std::vector<float> mlp_out;
    std::vector<float> final_norm;
    std::vector<Q8KBlock> normed_q8;
    std::vector<Q8KBlock> attention_q8;
    std::vector<Q8KBlock> mlp_hidden_q8;

    explicit LlamaWorkspace(const DecoderConfig& config)
        : hidden(checked_size(config.hidden_size, "hidden_size"), 0.0F),
          normed(hidden.size(), 0.0F),
          query(hidden.size(), 0.0F),
          key(checked_size(config.kv_heads * config.head_dim, "kv_width"), 0.0F),
          value(key.size(), 0.0F),
          attention(hidden.size(), 0.0F),
          projected(hidden.size(), 0.0F),
          gate(checked_size(config.intermediate_size, "intermediate_size"), 0.0F),
          up(gate.size(), 0.0F),
          mlp_hidden(gate.size(), 0.0F),
          mlp_out(hidden.size(), 0.0F),
          final_norm(hidden.size(), 0.0F),
          normed_q8(q8_scratch_blocks(config.hidden_size)),
          attention_q8(normed_q8.size()),
          mlp_hidden_q8(q8_scratch_blocks(config.intermediate_size)) {}
};

void forward_llama_layer(const DecoderConfig& config,
                         const LlamaGgufDecoderLayerWeights& layer,
                         std::int32_t layer_id,
                         std::int32_t model_position,
                         ReferenceKVCache& cache,
                         LlamaWorkspace& workspace,
                         LlamaGgufHotspotReport& hotspots) {
    hotspots.rms_norm_ms += measure_ms([&]() {
        rms_norm(workspace.hidden,
                 layer.rms_attention_weight,
                 config.rms_epsilon,
                 workspace.normed);
    });
    prepare_q8_if_needed(workspace.normed,
                         workspace.normed_q8,
                         uses_q4_k_direct(layer.wq) || uses_q4_k_direct(layer.wk) ||
                             uses_q4_k_direct(layer.wv));
    linear_gguf(layer.wq,
                workspace.normed,
                workspace.normed_q8,
                workspace.query,
                LlamaGgufLinearOp::wq,
                hotspots);
    linear_gguf(layer.wk,
                workspace.normed,
                workspace.normed_q8,
                workspace.key,
                LlamaGgufLinearOp::wk,
                hotspots);
    linear_gguf(layer.wv,
                workspace.normed,
                workspace.normed_q8,
                workspace.value,
                LlamaGgufLinearOp::wv,
                hotspots);
    hotspots.rope_ms += measure_ms([&]() {
        apply_rope(workspace.query,
                   config.query_heads,
                   config.head_dim,
                   model_position,
                   config.rope_base);
        apply_rope(workspace.key,
                   config.kv_heads,
                   config.head_dim,
                   model_position,
                   config.rope_base);
    });
    cache.append(layer_id, model_position, workspace.key, workspace.value);
    hotspots.attention_ms += measure_ms([&]() {
        attention_decode(config,
                         cache,
                         layer_id,
                         model_position,
                         workspace.query,
                         workspace.attention);
    });
    prepare_q8_if_needed(workspace.attention,
                         workspace.attention_q8,
                         uses_q4_k_direct(layer.wo));
    linear_gguf(layer.wo,
                workspace.attention,
                workspace.attention_q8,
                workspace.projected,
                LlamaGgufLinearOp::wo,
                hotspots);
    add_inplace(workspace.hidden, workspace.projected);

    hotspots.rms_norm_ms += measure_ms([&]() {
        rms_norm(workspace.hidden, layer.rms_mlp_weight, config.rms_epsilon, workspace.normed);
    });
    prepare_q8_if_needed(workspace.normed,
                         workspace.normed_q8,
                         uses_q4_k_direct(layer.w_gate) || uses_q4_k_direct(layer.w_up));
    linear_gguf(layer.w_gate,
                workspace.normed,
                workspace.normed_q8,
                workspace.gate,
                LlamaGgufLinearOp::w_gate,
                hotspots);
    linear_gguf(layer.w_up,
                workspace.normed,
                workspace.normed_q8,
                workspace.up,
                LlamaGgufLinearOp::w_up,
                hotspots);
    for (std::size_t index = 0; index < workspace.mlp_hidden.size(); ++index) {
        workspace.mlp_hidden[index] = silu(workspace.gate[index]) * workspace.up[index];
    }
    prepare_q8_if_needed(workspace.mlp_hidden,
                         workspace.mlp_hidden_q8,
                         uses_q4_k_direct(layer.w_down));
    linear_gguf(layer.w_down,
                workspace.mlp_hidden,
                workspace.mlp_hidden_q8,
                workspace.mlp_out,
                LlamaGgufLinearOp::w_down,
                hotspots);
    add_inplace(workspace.hidden, workspace.mlp_out);
}

[[nodiscard]] std::int32_t predict_next_token(const LlamaGgufModel& model,
                                              std::span<const float> final_hidden,
                                              LlamaGgufHotspotReport& hotspots) {
    const DecoderConfig& config = model.decoder.config;
    F32RowMax best;
    if (model.lm_head_tied_to_embedding) {
        const std::span<const float> weights = model.decoder.token_embedding;
        hotspots.lm_head_ms += measure_ms([&]() {
            best = max_dot_f32_rows_unchecked(weights.data(),
                                              final_hidden.data(),
                                              checked_size(config.hidden_size, "hidden_size"),
                                              0,
                                              checked_size(config.vocab_size, "vocab_size"));
        });
        return static_cast<std::int32_t>(best.row);
    }

    std::vector<float> logits(checked_size(config.vocab_size, "vocab_size"), 0.0F);
    const std::vector<Q8KBlock> final_hidden_q8 =
        uses_q4_k_direct(model.decoder.lm_head)
            ? quantize_q8_k_input(final_hidden)
            : std::vector<Q8KBlock>{};
    linear_gguf(model.decoder.lm_head,
                final_hidden,
                final_hidden_q8,
                logits,
                LlamaGgufLinearOp::lm_head,
                hotspots);
    best.row = 0;
    best.value = logits.front();
    for (std::size_t row = 1; row < logits.size(); ++row) {
        if (logits[row] > best.value) {
            best.row = row;
            best.value = logits[row];
        }
    }
    return static_cast<std::int32_t>(best.row);
}

[[nodiscard]] std::int32_t step_llama_decoder(const LlamaGgufModel& model,
                                              const DecoderConfig& active_config,
                                              ReferenceKVCache& cache,
                                              LlamaWorkspace& workspace,
                                              std::int32_t token_id,
                                              bool predict,
                                              LlamaGgufHotspotReport& hotspots) {
    const DecoderConfig& original_config = model.decoder.config;
    if (token_id < 0 || token_id >= original_config.vocab_size) {
        throw std::runtime_error("LLaMA token id out of vocabulary");
    }
    const std::int32_t model_position = cache.filled_tokens();
    if (model_position >= active_config.max_context) {
        throw std::runtime_error("LLaMA generation exceeded active context");
    }

    const std::span<const float> embedding =
        row_span(model.decoder.token_embedding, token_id, original_config.hidden_size);
    std::copy(embedding.begin(), embedding.end(), workspace.hidden.begin());
    for (std::int32_t layer_id = 0;
         layer_id < static_cast<std::int32_t>(model.decoder.layers.size());
         ++layer_id) {
        forward_llama_layer(active_config,
                            model.decoder.layers[checked_size(layer_id, "layer_id")],
                            layer_id,
                            model_position,
                            cache,
                            workspace,
                            hotspots);
    }
    if (!predict) {
        return -1;
    }
    hotspots.rms_norm_ms += measure_ms([&]() {
        rms_norm(workspace.hidden,
                 model.decoder.final_norm_weight,
                 active_config.rms_epsilon,
                 workspace.final_norm);
    });
    return predict_next_token(model, workspace.final_norm, hotspots);
}

[[nodiscard]] std::size_t kv_cache_bytes(const DecoderConfig& config, std::int32_t layer_count) {
    return checked_size(layer_count, "layer_count") *
           checked_size(config.max_context, "max_context") *
           checked_size(config.kv_heads, "kv_heads") *
           checked_size(config.head_dim, "head_dim") * sizeof(float) * 2U;
}

[[nodiscard]] std::unordered_map<std::string, std::int32_t> build_vocab_from_tokens(
    std::span<const std::string> tokens) {
    std::unordered_map<std::string, std::int32_t> vocab;
    for (std::size_t index = 0; index < tokens.size(); ++index) {
        vocab.emplace(tokens[index], static_cast<std::int32_t>(index));
    }
    return vocab;
}

[[nodiscard]] std::unordered_map<std::string, std::int32_t> build_merge_ranks(
    std::span<const std::string> merges) {
    std::unordered_map<std::string, std::int32_t> ranks;
    for (std::size_t index = 0; index < merges.size(); ++index) {
        std::istringstream line_stream(merges[index]);
        std::string left;
        std::string right;
        if (!(line_stream >> left >> right)) {
            throw std::runtime_error("invalid GGUF tokenizer merge: " + merges[index]);
        }
        ranks.emplace(left + "\n" + right, static_cast<std::int32_t>(index));
    }
    return ranks;
}

[[nodiscard]] bool starts_with(std::string_view text,
                               std::size_t offset,
                               std::string_view needle) {
    return offset + needle.size() <= text.size() &&
           text.substr(offset, needle.size()) == needle;
}

[[nodiscard]] std::vector<std::pair<std::string, std::int32_t>> special_tokens(
    const Gpt2Tokenizer& tokenizer) {
    std::vector<std::pair<std::string, std::int32_t>> result;
    for (const auto& [token, id] : tokenizer.vocab) {
        if (token.size() >= 4 && token.front() == '<' && token.find('|') != std::string::npos) {
            result.emplace_back(token, id);
        }
    }
    std::sort(result.begin(),
              result.end(),
              [](const auto& lhs, const auto& rhs) {
                  return lhs.first.size() > rhs.first.size();
              });
    return result;
}

[[nodiscard]] std::vector<std::int32_t> encode_with_special_tokens(
    const Gpt2Tokenizer& tokenizer,
    std::string_view text) {
    const std::vector<std::pair<std::string, std::int32_t>> specials = special_tokens(tokenizer);
    std::vector<std::int32_t> ids;
    std::size_t offset = 0;
    while (offset < text.size()) {
        bool matched_special = false;
        for (const auto& [token, id] : specials) {
            if (starts_with(text, offset, token)) {
                ids.push_back(id);
                offset += token.size();
                matched_special = true;
                break;
            }
        }
        if (matched_special) {
            continue;
        }

        std::size_t next_special = text.size();
        for (const auto& [token, id] : specials) {
            (void)id;
            const std::size_t found = text.find(token, offset);
            if (found != std::string_view::npos) {
                next_special = std::min(next_special, found);
            }
        }
        const std::vector<std::int32_t> chunk =
            gpt2_encode(tokenizer, text.substr(offset, next_special - offset));
        ids.insert(ids.end(), chunk.begin(), chunk.end());
        offset = next_special;
    }
    return ids;
}

}  // namespace

LlamaGgufLoadedModel load_llama_gguf_reference_model(const std::filesystem::path& gguf_path) {
    LlamaGgufLoadedModel loaded;
    const Clock::time_point manifest_begin = Clock::now();
    const GgufManifest manifest = load_gguf_manifest(gguf_path);
    loaded.report.manifest_ms = elapsed_ms(manifest_begin, Clock::now());

    const Clock::time_point weights_begin = Clock::now();
    loaded.model.architecture = metadata_string(manifest, LCQI_LLAMA_ARCHITECTURE_KEY);
    loaded.model.name = metadata_string(manifest, LCQI_LLAMA_NAME_KEY);
    DecoderConfig& config = loaded.model.decoder.config;
    config.hidden_size = metadata_i32(manifest, LCQI_LLAMA_EMBEDDING_LENGTH_KEY);
    const std::int32_t layer_count = metadata_i32(manifest, LCQI_LLAMA_BLOCK_COUNT_KEY);
    config.intermediate_size = metadata_i32(manifest, LCQI_LLAMA_FEED_FORWARD_LENGTH_KEY);
    config.query_heads = metadata_i32(manifest, LCQI_LLAMA_HEAD_COUNT_KEY);
    config.kv_heads = metadata_i32(manifest, LCQI_LLAMA_KV_HEAD_COUNT_KEY);
    config.head_dim = config.hidden_size / config.query_heads;
    const std::int32_t rope_dimension = metadata_i32(manifest, LCQI_LLAMA_ROPE_DIMENSION_KEY);
    if (rope_dimension != config.head_dim) {
        throw std::runtime_error("LLaMA rope dimension does not match head_dim");
    }
    config.rope_base = metadata_f32(manifest, LCQI_LLAMA_ROPE_BASE_KEY);
    config.max_context = metadata_i32(manifest, LCQI_LLAMA_CONTEXT_LENGTH_KEY);
    config.vocab_size = metadata_i32(manifest, LCQI_LLAMA_VOCAB_SIZE_KEY);
    config.rms_epsilon = metadata_f32(manifest, LCQI_LLAMA_RMS_EPSILON_KEY);
    loaded.model.bos_token_id =
        optional_metadata_i32(manifest, LCQI_LLAMA_BOS_TOKEN_KEY, LCQI_LLAMA_DEFAULT_BOS_TOKEN);
    loaded.model.eos_token_id =
        optional_metadata_i32(manifest, LCQI_LLAMA_EOS_TOKEN_KEY, LCQI_LLAMA_DEFAULT_EOS_TOKEN);
    loaded.model.unknown_token_id = optional_metadata_i32(
        manifest,
        LCQI_LLAMA_UNKNOWN_TOKEN_KEY,
        LCQI_LLAMA_DEFAULT_UNK_TOKEN);
    validate_llama_config(config, layer_count);

    const std::array<std::int64_t, 2> embedding_shape{config.hidden_size, config.vocab_size};
    loaded.model.decoder.token_embedding = read_tensor_f32(gguf_path,
                                                           manifest,
                                                           "token_embd.weight",
                                                           embedding_shape,
                                                           loaded.report);
    const std::array<std::int64_t, 1> hidden_shape{config.hidden_size};
    loaded.model.decoder.final_norm_weight = read_tensor_f32(gguf_path,
                                                             manifest,
                                                             "output_norm.weight",
                                                             hidden_shape,
                                                             loaded.report);
    loaded.model.decoder.layers.resize(checked_size(layer_count, "layer_count"));
    const std::int32_t kv_width = config.kv_heads * config.head_dim;
    for (std::int32_t layer_id = 0; layer_id < layer_count; ++layer_id) {
        LlamaGgufDecoderLayerWeights& layer =
            loaded.model.decoder.layers[checked_size(layer_id, "layer_id")];
        layer.rms_attention_weight = read_tensor_f32(
            gguf_path,
            manifest,
            layer_tensor_name(layer_id, "attn_norm.weight"),
            hidden_shape,
            loaded.report);
        layer.rms_mlp_weight = read_tensor_f32(gguf_path,
                                               manifest,
                                               layer_tensor_name(layer_id, "ffn_norm.weight"),
                                               hidden_shape,
                                               loaded.report);
        layer.wq = make_linear_from_gguf(gguf_path,
                                         manifest,
                                         layer_tensor_name(layer_id, "attn_q.weight"),
                                         config.hidden_size,
                                         config.hidden_size,
                                         loaded.report);
        layer.wk = make_linear_from_gguf(gguf_path,
                                         manifest,
                                         layer_tensor_name(layer_id, "attn_k.weight"),
                                         config.hidden_size,
                                         kv_width,
                                         loaded.report);
        layer.wv = make_linear_from_gguf(gguf_path,
                                         manifest,
                                         layer_tensor_name(layer_id, "attn_v.weight"),
                                         config.hidden_size,
                                         kv_width,
                                         loaded.report);
        layer.wo = make_linear_from_gguf(gguf_path,
                                         manifest,
                                         layer_tensor_name(layer_id, "attn_output.weight"),
                                         config.hidden_size,
                                         config.hidden_size,
                                         loaded.report);
        layer.w_gate = make_linear_from_gguf(gguf_path,
                                             manifest,
                                             layer_tensor_name(layer_id, "ffn_gate.weight"),
                                             config.hidden_size,
                                             config.intermediate_size,
                                             loaded.report);
        layer.w_up = make_linear_from_gguf(gguf_path,
                                           manifest,
                                           layer_tensor_name(layer_id, "ffn_up.weight"),
                                           config.hidden_size,
                                           config.intermediate_size,
                                           loaded.report);
        layer.w_down = make_linear_from_gguf(gguf_path,
                                             manifest,
                                             layer_tensor_name(layer_id, "ffn_down.weight"),
                                             config.intermediate_size,
                                             config.hidden_size,
                                             loaded.report);
    }

    if (manifest.find_tensor("output.weight") != nullptr) {
        loaded.model.lm_head_tied_to_embedding = false;
        loaded.model.decoder.lm_head = make_linear_from_gguf(gguf_path,
                                                             manifest,
                                                             "output.weight",
                                                             config.hidden_size,
                                                             config.vocab_size,
                                                             loaded.report);
    } else {
        loaded.model.lm_head_tied_to_embedding = true;
        loaded.model.decoder.lm_head.input_size = config.hidden_size;
        loaded.model.decoder.lm_head.output_size = config.vocab_size;
    }
    loaded.report.weights_ms = elapsed_ms(weights_begin, Clock::now());
    return loaded;
}

Gpt2Tokenizer load_gpt2_tokenizer_from_gguf(const std::filesystem::path& gguf_path) {
    const GgufManifest manifest = load_gguf_manifest(gguf_path);
    const GgufMetadataEntry& tokens_entry = require_metadata(manifest,
                                                             LCQI_LLAMA_TOKENIZER_TOKENS_KEY);
    const GgufMetadataEntry& merges_entry = require_metadata(manifest,
                                                             LCQI_LLAMA_TOKENIZER_MERGES_KEY);
    if (tokens_entry.string_values.empty() || merges_entry.string_values.empty()) {
        throw std::runtime_error("GGUF tokenizer tokens or merges are empty");
    }
    Gpt2Tokenizer tokenizer;
    tokenizer.bos_token_id =
        optional_metadata_i32(manifest, LCQI_LLAMA_BOS_TOKEN_KEY, LCQI_LLAMA_DEFAULT_BOS_TOKEN);
    tokenizer.eos_token_id =
        optional_metadata_i32(manifest, LCQI_LLAMA_EOS_TOKEN_KEY, LCQI_LLAMA_DEFAULT_EOS_TOKEN);
    tokenizer.id_to_token = tokens_entry.string_values;
    tokenizer.vocab = build_vocab_from_tokens(tokenizer.id_to_token);
    tokenizer.bpe_ranks = build_merge_ranks(merges_entry.string_values);
    return tokenizer;
}

std::vector<std::int32_t> llama_gguf_chat_prompt_ids(const Gpt2Tokenizer& tokenizer,
                                                     std::string_view user_prompt,
                                                     std::int32_t bos_token_id) {
    std::string prompt;
    prompt += "<|im_start|>system\n";
    prompt += "You are a helpful AI assistant named SmolLM, trained by Hugging Face";
    prompt += "<|im_end|>\n";
    prompt += "<|im_start|>user\n";
    prompt += user_prompt;
    prompt += "<|im_end|>\n";
    prompt += "<|im_start|>assistant\n";
    std::vector<std::int32_t> ids = encode_with_special_tokens(tokenizer, prompt);
    if (ids.empty() && bos_token_id >= 0) {
        ids.push_back(bos_token_id);
    }
    return ids;
}

LlamaGgufGenerationResult llama_gguf_generate_greedy(const LlamaGgufModel& model,
                                                     std::span<const std::int32_t> prompt_ids,
                                                     std::int32_t max_new_tokens) {
    if (prompt_ids.empty()) {
        throw std::runtime_error("LLaMA generation requires at least one prompt token");
    }
    if (max_new_tokens < 0) {
        throw std::runtime_error("max_new_tokens cannot be negative");
    }
    const std::int32_t layer_count = static_cast<std::int32_t>(model.decoder.layers.size());
    validate_llama_config(model.decoder.config, layer_count);
    const std::int32_t active_context =
        static_cast<std::int32_t>(prompt_ids.size()) + std::max(max_new_tokens, 1);
    if (active_context > model.decoder.config.max_context) {
        throw std::runtime_error("LLaMA prompt plus generation exceeds model context");
    }

    DecoderConfig active_config = model.decoder.config;
    active_config.max_context = active_context;
    ReferenceKVCache cache(active_config, layer_count);
    LlamaWorkspace workspace(active_config);

    LlamaGgufGenerationResult result;
    result.prompt_ids.assign(prompt_ids.begin(), prompt_ids.end());
    result.generated_ids.assign(prompt_ids.begin(), prompt_ids.end());
    result.kv_cache_bytes = kv_cache_bytes(active_config, layer_count);
    if (max_new_tokens == 0) {
        return result;
    }

    const Clock::time_point prefill_begin = Clock::now();
    for (std::size_t index = 0; index + 1 < prompt_ids.size(); ++index) {
        static_cast<void>(step_llama_decoder(model,
                                             active_config,
                                             cache,
                                             workspace,
                                             prompt_ids[index],
                                             false,
                                             result.hotspots));
    }
    std::int32_t predicted = step_llama_decoder(model,
                                                active_config,
                                                cache,
                                                workspace,
                                                prompt_ids.back(),
                                                true,
                                                result.hotspots);
    result.predicted_first_token = predicted;
    result.prefill_steps = prompt_ids.size();
    result.prefill_ms = elapsed_ms(prefill_begin, Clock::now());

    const Clock::time_point decode_begin = Clock::now();
    for (std::int32_t step = 0; step < max_new_tokens; ++step) {
        result.generated_ids.push_back(predicted);
        if (predicted == model.eos_token_id) {
            break;
        }
        if (step + 1 < max_new_tokens) {
            predicted = step_llama_decoder(model,
                                           active_config,
                                           cache,
                                           workspace,
                                           predicted,
                                           true,
                                           result.hotspots);
            ++result.decode_steps;
        }
    }
    result.decode_ms = elapsed_ms(decode_begin, Clock::now());
    result.total_ms = result.prefill_ms + result.decode_ms;
    return result;
}

}  // namespace lcqi

#include <lcqi/reference_decoder.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

namespace lcqi {
namespace {

constexpr float LCQI_SOFTMAX_NEGATIVE_INFINITY = -std::numeric_limits<float>::infinity();
constexpr std::int32_t LCQI_ROPE_PAIR_WIDTH = 2;
constexpr const char* LCQI_TRACE_DTYPE_F32 = "f32";
constexpr const char* LCQI_TRACE_LAYOUT_CONTIGUOUS = "contiguous";
constexpr const char* LCQI_TRACE_LAYOUT_ROW_MAJOR = "row_major";
constexpr const char* LCQI_TRACE_LAYOUT_KV_CONTIGUOUS = "kv_contiguous";

void require_positive(std::int32_t value, const char* name) {
    if (value <= 0) {
        throw std::runtime_error(std::string(name) + " must be positive");
    }
}

void validate_config(const DecoderConfig& config) {
    require_positive(config.hidden_size, "hidden_size");
    require_positive(config.query_heads, "query_heads");
    require_positive(config.kv_heads, "kv_heads");
    require_positive(config.head_dim, "head_dim");
    require_positive(config.intermediate_size, "intermediate_size");
    require_positive(config.vocab_size, "vocab_size");
    require_positive(config.max_context, "max_context");
    if (config.query_heads % config.kv_heads != 0) {
        throw std::runtime_error("query_heads must be divisible by kv_heads");
    }
    if (config.hidden_size != config.query_heads * config.head_dim) {
        throw std::runtime_error("hidden_size must equal query_heads * head_dim");
    }
    if (config.head_dim % LCQI_ROPE_PAIR_WIDTH != 0) {
        throw std::runtime_error("head_dim must be even for RoPE");
    }
    if (config.rope_base <= 1.0F) {
        throw std::runtime_error("rope_base must be greater than 1");
    }
}

std::size_t checked_size(std::int32_t value, const char* name) {
    if (value < 0) {
        throw std::runtime_error(std::string(name) + " cannot be negative");
    }
    return static_cast<std::size_t>(value);
}

std::int32_t kv_width(const DecoderConfig& config) {
    return config.kv_heads * config.head_dim;
}

void validate_linear(const LinearWeightsF32& layer, const char* name) {
    require_positive(layer.input_size, "linear input_size");
    require_positive(layer.output_size, "linear output_size");
    const std::size_t expected_weights =
        checked_size(layer.input_size, name) * checked_size(layer.output_size, name);
    if (layer.weights.size() != expected_weights) {
        throw std::runtime_error(std::string(name) + " weight size mismatch");
    }
    if (!layer.bias.empty() &&
        layer.bias.size() != checked_size(layer.output_size, name)) {
        throw std::runtime_error(std::string(name) + " bias size mismatch");
    }
}

void validate_span_size(std::size_t actual, std::int32_t expected, const char* name) {
    if (actual != checked_size(expected, name)) {
        throw std::runtime_error(std::string(name) + " size mismatch");
    }
}

std::int32_t argmax(std::span<const float> values) {
    if (values.empty()) {
        throw std::runtime_error("cannot take argmax of empty values");
    }
    std::int32_t best_index = 0;
    float best_value = values[0];
    for (std::int32_t index = 1; index < static_cast<std::int32_t>(values.size()); ++index) {
        const float candidate = values[static_cast<std::size_t>(index)];
        if (candidate > best_value) {
            best_value = candidate;
            best_index = index;
        }
    }
    return best_index;
}

float silu(float value) {
    return value / (1.0F + std::exp(-value));
}

std::span<const float> row_span(std::span<const float> values,
                                std::int32_t row,
                                std::int32_t row_width) {
    const std::size_t begin = checked_size(row, "row") * checked_size(row_width, "row_width");
    return values.subspan(begin, checked_size(row_width, "row_width"));
}

void add_inplace(std::span<float> target, std::span<const float> source) {
    if (target.size() != source.size()) {
        throw std::runtime_error("residual add size mismatch");
    }
    for (std::size_t index = 0; index < target.size(); ++index) {
        target[index] += source[index];
    }
}

void swiglu_mlp(const DecoderLayerWeightsF32& layer,
                std::span<const float> input,
                std::span<float> output) {
    std::vector<float> gate(checked_size(layer.w_gate.output_size, "gate"), 0.0F);
    std::vector<float> up(checked_size(layer.w_up.output_size, "up"), 0.0F);
    std::vector<float> hidden(gate.size(), 0.0F);
    linear_f32(layer.w_gate, input, gate);
    linear_f32(layer.w_up, input, up);
    if (gate.size() != up.size()) {
        throw std::runtime_error("SwiGLU gate/up size mismatch");
    }
    for (std::size_t index = 0; index < hidden.size(); ++index) {
        hidden[index] = silu(gate[index]) * up[index];
    }
    linear_f32(layer.w_down, hidden, output);
}

float checksum(std::span<const float> values) {
    float sum = 0.0F;
    for (const float value : values) {
        sum += value;
    }
    return sum;
}

float max_abs(std::span<const float> values) {
    float result = 0.0F;
    for (const float value : values) {
        result = std::max(result, std::fabs(value));
    }
    return result;
}

std::vector<std::int32_t> contiguous_stride(std::span<const std::int32_t> shape) {
    std::vector<std::int32_t> stride(shape.size(), 1);
    std::int32_t running = 1;
    for (std::int32_t index = static_cast<std::int32_t>(shape.size()) - 1; index >= 0; --index) {
        stride[checked_size(index, "stride index")] = running;
        running *= shape[checked_size(index, "shape index")];
    }
    return stride;
}

void add_checkpoint(std::vector<ReferenceTensorCheckpoint>* checkpoints,
                    const char* name,
                    std::int32_t token_position,
                    std::int32_t layer_id,
                    std::span<const std::int32_t> shape,
                    const char* layout,
                    std::span<const float> values) {
    if (checkpoints == nullptr) {
        return;
    }
    ReferenceTensorCheckpoint checkpoint;
    checkpoint.name = name;
    checkpoint.token_position = token_position;
    checkpoint.layer_id = layer_id;
    checkpoint.shape.assign(shape.begin(), shape.end());
    checkpoint.stride = contiguous_stride(shape);
    checkpoint.dtype = LCQI_TRACE_DTYPE_F32;
    checkpoint.layout = layout;
    checkpoint.checksum = checksum(values);
    checkpoint.max_abs = max_abs(values);
    checkpoint.values.assign(values.begin(), values.end());
    checkpoints->push_back(std::move(checkpoint));
}

void swiglu_mlp_with_trace(const DecoderLayerWeightsF32& layer,
                           std::span<const float> input,
                           std::span<float> output,
                           std::int32_t token_position,
                           std::int32_t layer_id,
                           std::vector<ReferenceTensorCheckpoint>* checkpoints) {
    std::vector<float> gate(checked_size(layer.w_gate.output_size, "gate"), 0.0F);
    std::vector<float> up(checked_size(layer.w_up.output_size, "up"), 0.0F);
    std::vector<float> hidden(gate.size(), 0.0F);
    linear_f32(layer.w_gate, input, gate);
    linear_f32(layer.w_up, input, up);
    if (gate.size() != up.size()) {
        throw std::runtime_error("SwiGLU gate/up size mismatch");
    }
    const std::array<std::int32_t, 1> intermediate_shape{layer.w_gate.output_size};
    add_checkpoint(checkpoints,
                   "mlp_gate",
                   token_position,
                   layer_id,
                   intermediate_shape,
                   LCQI_TRACE_LAYOUT_CONTIGUOUS,
                   gate);
    add_checkpoint(checkpoints,
                   "mlp_up",
                   token_position,
                   layer_id,
                   intermediate_shape,
                   LCQI_TRACE_LAYOUT_CONTIGUOUS,
                   up);
    for (std::size_t index = 0; index < hidden.size(); ++index) {
        hidden[index] = silu(gate[index]) * up[index];
    }
    add_checkpoint(checkpoints,
                   "mlp_hidden",
                   token_position,
                   layer_id,
                   intermediate_shape,
                   LCQI_TRACE_LAYOUT_CONTIGUOUS,
                   hidden);
    linear_f32(layer.w_down, hidden, output);
}

void forward_layer(const DecoderConfig& config,
                   const DecoderLayerWeightsF32& layer,
                   std::int32_t layer_id,
                   std::int32_t model_position,
                   ReferenceKVCache& cache,
                   std::span<float> hidden,
                   std::vector<ReferenceTensorCheckpoint>* checkpoints = nullptr) {
    validate_span_size(hidden.size(), config.hidden_size, "hidden");

    std::vector<float> normed(checked_size(config.hidden_size, "hidden_size"), 0.0F);
    std::vector<float> query(checked_size(config.hidden_size, "hidden_size"), 0.0F);
    std::vector<float> key(checked_size(kv_width(config), "kv_width"), 0.0F);
    std::vector<float> value(checked_size(kv_width(config), "kv_width"), 0.0F);
    std::vector<float> attention(checked_size(config.hidden_size, "hidden_size"), 0.0F);
    std::vector<float> attention_projected(checked_size(config.hidden_size, "hidden_size"), 0.0F);
    std::vector<float> mlp(checked_size(config.hidden_size, "hidden_size"), 0.0F);

    const std::array<std::int32_t, 1> hidden_shape{config.hidden_size};
    const std::array<std::int32_t, 2> query_shape{config.query_heads, config.head_dim};
    const std::array<std::int32_t, 2> kv_shape{config.kv_heads, config.head_dim};
    const std::array<std::int32_t, 4> kv_slot_shape{
        1, 1, config.kv_heads, config.head_dim};

    rms_norm(hidden, layer.rms_attention_weight, config.rms_epsilon, normed);
    add_checkpoint(checkpoints,
                   "attn_norm",
                   model_position,
                   layer_id,
                   hidden_shape,
                   LCQI_TRACE_LAYOUT_CONTIGUOUS,
                   normed);
    linear_f32(layer.wq, normed, query);
    linear_f32(layer.wk, normed, key);
    linear_f32(layer.wv, normed, value);
    add_checkpoint(checkpoints,
                   "q_before_rope",
                   model_position,
                   layer_id,
                   query_shape,
                   LCQI_TRACE_LAYOUT_CONTIGUOUS,
                   query);
    add_checkpoint(checkpoints,
                   "k_before_rope",
                   model_position,
                   layer_id,
                   kv_shape,
                   LCQI_TRACE_LAYOUT_CONTIGUOUS,
                   key);
    add_checkpoint(checkpoints,
                   "v",
                   model_position,
                   layer_id,
                   kv_shape,
                   LCQI_TRACE_LAYOUT_CONTIGUOUS,
                   value);
    apply_rope(query, config.query_heads, config.head_dim, model_position, config.rope_base);
    apply_rope(key, config.kv_heads, config.head_dim, model_position, config.rope_base);
    add_checkpoint(checkpoints,
                   "q_after_rope",
                   model_position,
                   layer_id,
                   query_shape,
                   LCQI_TRACE_LAYOUT_CONTIGUOUS,
                   query);
    add_checkpoint(checkpoints,
                   "k_after_rope",
                   model_position,
                   layer_id,
                   kv_shape,
                   LCQI_TRACE_LAYOUT_CONTIGUOUS,
                   key);

    cache.append(layer_id, model_position, key, value);
    add_checkpoint(checkpoints,
                   "kv_cache_key_slot",
                   model_position,
                   layer_id,
                   kv_slot_shape,
                   LCQI_TRACE_LAYOUT_KV_CONTIGUOUS,
                   key);
    attention_decode(config, cache, layer_id, model_position, query, attention);
    add_checkpoint(checkpoints,
                   "attention_out",
                   model_position,
                   layer_id,
                   hidden_shape,
                   LCQI_TRACE_LAYOUT_CONTIGUOUS,
                   attention);
    linear_f32(layer.wo, attention, attention_projected);
    add_inplace(hidden, attention_projected);
    add_checkpoint(checkpoints,
                   "post_attention_residual",
                   model_position,
                   layer_id,
                   hidden_shape,
                   LCQI_TRACE_LAYOUT_CONTIGUOUS,
                   hidden);

    rms_norm(hidden, layer.rms_mlp_weight, config.rms_epsilon, normed);
    add_checkpoint(checkpoints,
                   "mlp_norm",
                   model_position,
                   layer_id,
                   hidden_shape,
                   LCQI_TRACE_LAYOUT_CONTIGUOUS,
                   normed);
    if (checkpoints == nullptr) {
        swiglu_mlp(layer, normed, mlp);
    } else {
        swiglu_mlp_with_trace(layer, normed, mlp, model_position, layer_id, checkpoints);
    }
    add_inplace(hidden, mlp);
    add_checkpoint(checkpoints,
                   "layer_out",
                   model_position,
                   layer_id,
                   hidden_shape,
                   LCQI_TRACE_LAYOUT_CONTIGUOUS,
                   hidden);
}

LinearWeightsF32 make_linear(std::int32_t input_size,
                             std::int32_t output_size,
                             std::vector<float> weights,
                             std::vector<float> bias = {}) {
    LinearWeightsF32 layer;
    layer.input_size = input_size;
    layer.output_size = output_size;
    layer.weights = std::move(weights);
    layer.bias = std::move(bias);
    validate_linear(layer, "tiny linear");
    return layer;
}

std::vector<float> zeros(std::int32_t count) {
    return std::vector<float>(checked_size(count, "zero count"), 0.0F);
}

}  // namespace

ReferenceDecodeTraceResult run_reference_decode_with_trace(
    const ReferenceDecoderModel& model,
    std::span<const std::int32_t> token_ids) {
    validate_config(model.config);
    if (token_ids.empty()) {
        throw std::runtime_error("reference decode needs at least one token");
    }
    if (token_ids.size() > checked_size(model.config.max_context, "max_context")) {
        throw std::runtime_error("token_ids exceed max_context");
    }
    if (model.token_embedding.size() !=
        checked_size(model.config.vocab_size, "vocab_size") *
            checked_size(model.config.hidden_size, "hidden_size")) {
        throw std::runtime_error("token embedding size mismatch");
    }
    if (model.final_norm_weight.size() != checked_size(model.config.hidden_size, "hidden_size")) {
        throw std::runtime_error("final norm weight size mismatch");
    }
    validate_linear(model.lm_head, "lm_head");

    ReferenceDecodeTraceResult trace;
    ReferenceKVCache cache(model.config, static_cast<std::int32_t>(model.layers.size()));
    std::vector<float> hidden(checked_size(model.config.hidden_size, "hidden_size"), 0.0F);
    const std::array<std::int32_t, 1> hidden_shape{model.config.hidden_size};
    for (std::int32_t position = 0; position < static_cast<std::int32_t>(token_ids.size());
         ++position) {
        const std::int32_t token_id = token_ids[checked_size(position, "position")];
        if (token_id < 0 || token_id >= model.config.vocab_size) {
            throw std::runtime_error("token id out of vocabulary");
        }
        const std::span<const float> embedding =
            row_span(model.token_embedding, token_id, model.config.hidden_size);
        std::copy(embedding.begin(), embedding.end(), hidden.begin());
        add_checkpoint(&trace.checkpoints,
                       "embedding",
                       position,
                       -1,
                       hidden_shape,
                       LCQI_TRACE_LAYOUT_ROW_MAJOR,
                       hidden);
        for (std::int32_t layer_id = 0; layer_id < static_cast<std::int32_t>(model.layers.size());
             ++layer_id) {
            forward_layer(model.config,
                          model.layers[checked_size(layer_id, "layer_id")],
                          layer_id,
                          position,
                          cache,
                          hidden,
                          &trace.checkpoints);
        }
    }

    std::vector<float> normed(checked_size(model.config.hidden_size, "hidden_size"), 0.0F);
    std::vector<float> logits(checked_size(model.config.vocab_size, "vocab_size"), 0.0F);
    const std::array<std::int32_t, 1> logits_shape{model.config.vocab_size};
    const std::int32_t last_position = static_cast<std::int32_t>(token_ids.size()) - 1;
    rms_norm(hidden, model.final_norm_weight, model.config.rms_epsilon, normed);
    add_checkpoint(&trace.checkpoints,
                   "final_norm",
                   last_position,
                   -1,
                   hidden_shape,
                   LCQI_TRACE_LAYOUT_CONTIGUOUS,
                   normed);
    linear_f32(model.lm_head, normed, logits);
    add_checkpoint(&trace.checkpoints,
                   "logits",
                   last_position,
                   -1,
                   logits_shape,
                   LCQI_TRACE_LAYOUT_CONTIGUOUS,
                   logits);

    trace.result.predicted_token = argmax(logits);
    trace.result.logits = std::move(logits);
    return trace;
}

ReferenceKVCache::ReferenceKVCache(const DecoderConfig& config, std::int32_t layer_count)
    : config_(config),
      layer_count_(layer_count) {
    validate_config(this->config_);
    require_positive(this->layer_count_, "layer_count");
    const std::size_t element_count =
        checked_size(this->layer_count_, "layer_count") *
        checked_size(this->config_.max_context, "max_context") *
        checked_size(this->config_.kv_heads, "kv_heads") *
        checked_size(this->config_.head_dim, "head_dim");
    this->keys_.assign(element_count, 0.0F);
    this->values_.assign(element_count, 0.0F);
    this->written_.assign(
        checked_size(this->layer_count_, "layer_count") *
            checked_size(this->config_.max_context, "max_context"),
        0);
}

void ReferenceKVCache::append(std::int32_t layer_id,
                              std::int32_t model_position,
                              std::span<const float> key,
                              std::span<const float> value) {
    validate_span_size(key.size(), kv_width(this->config_), "KV key");
    validate_span_size(value.size(), kv_width(this->config_), "KV value");
    this->validate_address(layer_id, model_position, 0);
    for (std::int32_t kv_head = 0; kv_head < this->config_.kv_heads; ++kv_head) {
        const std::size_t target = this->base_offset(layer_id, model_position, kv_head);
        const std::size_t source =
            checked_size(kv_head, "kv_head") * checked_size(this->config_.head_dim, "head_dim");
        std::copy_n(key.begin() + static_cast<std::ptrdiff_t>(source),
                    this->config_.head_dim,
                    this->keys_.begin() + static_cast<std::ptrdiff_t>(target));
        std::copy_n(value.begin() + static_cast<std::ptrdiff_t>(source),
                    this->config_.head_dim,
                    this->values_.begin() + static_cast<std::ptrdiff_t>(target));
    }
    const std::size_t written_index =
        checked_size(layer_id, "layer_id") * checked_size(this->config_.max_context, "max_context") +
        checked_size(model_position, "model_position");
    this->written_[written_index] = 1;
    this->filled_tokens_ = std::max(this->filled_tokens_, model_position + 1);
}

std::span<const float> ReferenceKVCache::key(std::int32_t layer_id,
                                             std::int32_t model_position,
                                             std::int32_t kv_head) const {
    this->validate_address(layer_id, model_position, kv_head);
    const std::size_t offset = this->base_offset(layer_id, model_position, kv_head);
    return std::span<const float>(this->keys_.data() + offset,
                                  checked_size(this->config_.head_dim, "head_dim"));
}

std::span<const float> ReferenceKVCache::value(std::int32_t layer_id,
                                               std::int32_t model_position,
                                               std::int32_t kv_head) const {
    this->validate_address(layer_id, model_position, kv_head);
    const std::size_t offset = this->base_offset(layer_id, model_position, kv_head);
    return std::span<const float>(this->values_.data() + offset,
                                  checked_size(this->config_.head_dim, "head_dim"));
}

std::int32_t ReferenceKVCache::layer_count() const noexcept {
    return this->layer_count_;
}

std::int32_t ReferenceKVCache::filled_tokens() const noexcept {
    return this->filled_tokens_;
}

std::size_t ReferenceKVCache::base_offset(std::int32_t layer_id,
                                          std::int32_t model_position,
                                          std::int32_t kv_head) const {
    return (((checked_size(layer_id, "layer_id") *
              checked_size(this->config_.max_context, "max_context")) +
             checked_size(model_position, "model_position")) *
                checked_size(this->config_.kv_heads, "kv_heads") +
            checked_size(kv_head, "kv_head")) *
           checked_size(this->config_.head_dim, "head_dim");
}

void ReferenceKVCache::validate_address(std::int32_t layer_id,
                                        std::int32_t model_position,
                                        std::int32_t kv_head) const {
    if (layer_id < 0 || layer_id >= this->layer_count_) {
        throw std::runtime_error("KV layer_id out of range");
    }
    if (model_position < 0 || model_position >= this->config_.max_context) {
        throw std::runtime_error("KV model_position out of range");
    }
    if (kv_head < 0 || kv_head >= this->config_.kv_heads) {
        throw std::runtime_error("KV head out of range");
    }
}

void rms_norm(std::span<const float> input,
              std::span<const float> weight,
              float epsilon,
              std::span<float> output) {
    if (input.size() != weight.size() || input.size() != output.size()) {
        throw std::runtime_error("RMSNorm size mismatch");
    }
    if (input.empty()) {
        throw std::runtime_error("RMSNorm input is empty");
    }
    float sum_square = 0.0F;
    for (const float value : input) {
        sum_square += value * value;
    }
    const float mean_square = sum_square / static_cast<float>(input.size());
    const float scale = 1.0F / std::sqrt(mean_square + epsilon);
    for (std::size_t index = 0; index < input.size(); ++index) {
        output[index] = input[index] * scale * weight[index];
    }
}

void linear_f32(const LinearWeightsF32& layer,
                std::span<const float> input,
                std::span<float> output) {
    validate_linear(layer, "linear_f32");
    validate_span_size(input.size(), layer.input_size, "linear input");
    validate_span_size(output.size(), layer.output_size, "linear output");
    for (std::int32_t out = 0; out < layer.output_size; ++out) {
        const std::size_t row_base =
            checked_size(out, "out") * checked_size(layer.input_size, "input_size");
        float accumulator = layer.bias.empty() ? 0.0F : layer.bias[checked_size(out, "out")];
        for (std::int32_t in = 0; in < layer.input_size; ++in) {
            accumulator += layer.weights[row_base + checked_size(in, "in")] *
                           input[checked_size(in, "in")];
        }
        output[checked_size(out, "out")] = accumulator;
    }
}

void apply_rope(std::span<float> heads,
                std::int32_t head_count,
                std::int32_t head_dim,
                std::int32_t model_position,
                float rope_base) {
    require_positive(head_count, "head_count");
    require_positive(head_dim, "head_dim");
    validate_span_size(heads.size(), head_count * head_dim, "RoPE heads");
    if (head_dim % LCQI_ROPE_PAIR_WIDTH != 0) {
        throw std::runtime_error("RoPE head_dim must be even");
    }
    if (model_position < 0) {
        throw std::runtime_error("RoPE model_position cannot be negative");
    }
    if (rope_base <= 1.0F) {
        throw std::runtime_error("RoPE base must be greater than 1");
    }
    for (std::int32_t head = 0; head < head_count; ++head) {
        for (std::int32_t offset = 0; offset < head_dim; offset += LCQI_ROPE_PAIR_WIDTH) {
            const float exponent = static_cast<float>(offset) / static_cast<float>(head_dim);
            const float theta = 1.0F / std::pow(rope_base, exponent);
            const float angle = static_cast<float>(model_position) * theta;
            const float cosine = std::cos(angle);
            const float sine = std::sin(angle);
            const std::size_t first =
                checked_size(head, "head") * checked_size(head_dim, "head_dim") +
                checked_size(offset, "offset");
            const float x0 = heads[first];
            const float x1 = heads[first + 1];
            heads[first] = x0 * cosine - x1 * sine;
            heads[first + 1] = x0 * sine + x1 * cosine;
        }
    }
}

void attention_decode(const DecoderConfig& config,
                      const ReferenceKVCache& cache,
                      std::int32_t layer_id,
                      std::int32_t model_position,
                      std::span<const float> query,
                      std::span<float> output) {
    validate_config(config);
    validate_span_size(query.size(), config.hidden_size, "attention query");
    validate_span_size(output.size(), config.hidden_size, "attention output");
    if (model_position < 0 || model_position >= cache.filled_tokens()) {
        throw std::runtime_error("attention model_position has not been written");
    }
    std::fill(output.begin(), output.end(), 0.0F);

    const std::int32_t group_size = config.query_heads / config.kv_heads;
    const float score_scale = 1.0F / std::sqrt(static_cast<float>(config.head_dim));
    std::vector<float> scores(checked_size(model_position + 1, "score count"),
                              LCQI_SOFTMAX_NEGATIVE_INFINITY);
    std::vector<float> probabilities(scores.size(), 0.0F);

    for (std::int32_t query_head = 0; query_head < config.query_heads; ++query_head) {
        const std::int32_t kv_head = query_head / group_size;
        const std::size_t query_base =
            checked_size(query_head, "query_head") * checked_size(config.head_dim, "head_dim");
        float max_score = LCQI_SOFTMAX_NEGATIVE_INFINITY;
        for (std::int32_t position = 0; position <= model_position; ++position) {
            const std::span<const float> key = cache.key(layer_id, position, kv_head);
            float dot = 0.0F;
            for (std::int32_t dim = 0; dim < config.head_dim; ++dim) {
                dot += query[query_base + checked_size(dim, "dim")] *
                       key[checked_size(dim, "dim")];
            }
            const float score = dot * score_scale;
            scores[checked_size(position, "position")] = score;
            max_score = std::max(max_score, score);
        }

        float probability_sum = 0.0F;
        for (std::int32_t position = 0; position <= model_position; ++position) {
            const std::size_t index = checked_size(position, "position");
            const float unnormalized = std::exp(scores[index] - max_score);
            probabilities[index] = unnormalized;
            probability_sum += unnormalized;
        }
        if (probability_sum <= 0.0F) {
            throw std::runtime_error("attention probability sum is invalid");
        }
        for (std::int32_t position = 0; position <= model_position; ++position) {
            const float probability =
                probabilities[checked_size(position, "position")] / probability_sum;
            const std::span<const float> value = cache.value(layer_id, position, kv_head);
            for (std::int32_t dim = 0; dim < config.head_dim; ++dim) {
                output[query_base + checked_size(dim, "dim")] +=
                    probability * value[checked_size(dim, "dim")];
            }
        }
    }
}

ReferenceDecodeResult run_reference_decode(const ReferenceDecoderModel& model,
                                           std::span<const std::int32_t> token_ids) {
    validate_config(model.config);
    if (token_ids.empty()) {
        throw std::runtime_error("reference decode needs at least one token");
    }
    if (token_ids.size() > checked_size(model.config.max_context, "max_context")) {
        throw std::runtime_error("token_ids exceed max_context");
    }
    if (model.token_embedding.size() !=
        checked_size(model.config.vocab_size, "vocab_size") *
            checked_size(model.config.hidden_size, "hidden_size")) {
        throw std::runtime_error("token embedding size mismatch");
    }
    if (model.final_norm_weight.size() != checked_size(model.config.hidden_size, "hidden_size")) {
        throw std::runtime_error("final norm weight size mismatch");
    }
    validate_linear(model.lm_head, "lm_head");

    ReferenceKVCache cache(model.config, static_cast<std::int32_t>(model.layers.size()));
    std::vector<float> hidden(checked_size(model.config.hidden_size, "hidden_size"), 0.0F);
    for (std::int32_t position = 0; position < static_cast<std::int32_t>(token_ids.size());
         ++position) {
        const std::int32_t token_id = token_ids[checked_size(position, "position")];
        if (token_id < 0 || token_id >= model.config.vocab_size) {
            throw std::runtime_error("token id out of vocabulary");
        }
        const std::span<const float> embedding =
            row_span(model.token_embedding, token_id, model.config.hidden_size);
        std::copy(embedding.begin(), embedding.end(), hidden.begin());
        for (std::int32_t layer_id = 0; layer_id < static_cast<std::int32_t>(model.layers.size());
             ++layer_id) {
            forward_layer(model.config,
                          model.layers[checked_size(layer_id, "layer_id")],
                          layer_id,
                          position,
                          cache,
                          hidden);
        }
    }

    std::vector<float> normed(checked_size(model.config.hidden_size, "hidden_size"), 0.0F);
    std::vector<float> logits(checked_size(model.config.vocab_size, "vocab_size"), 0.0F);
    rms_norm(hidden, model.final_norm_weight, model.config.rms_epsilon, normed);
    linear_f32(model.lm_head, normed, logits);

    ReferenceDecodeResult result;
    result.predicted_token = argmax(logits);
    result.logits = std::move(logits);
    return result;
}

ReferenceDecoderModel make_tiny_reference_decoder_model() {
    ReferenceDecoderModel model;
    model.config.hidden_size = 4;
    model.config.query_heads = 2;
    model.config.kv_heads = 1;
    model.config.head_dim = 2;
    model.config.intermediate_size = 6;
    model.config.vocab_size = 5;
    model.config.max_context = 8;
    model.config.rms_epsilon = 1.0e-5F;
    model.config.rope_base = 10000.0F;

    model.token_embedding = {
        0.20F, -0.10F, 0.05F, 0.30F,
        -0.40F, 0.10F, 0.25F, -0.20F,
        0.15F, 0.35F, -0.30F, 0.05F,
        0.50F, -0.25F, 0.10F, -0.15F,
        -0.05F, 0.20F, 0.40F, -0.35F,
    };

    DecoderLayerWeightsF32 layer;
    layer.rms_attention_weight = {1.0F, 0.9F, 1.1F, 1.0F};
    layer.wq = make_linear(4,
                           4,
                           {
                               0.20F, -0.10F, 0.05F, 0.30F,
                               -0.15F, 0.25F, 0.10F, -0.05F,
                               0.05F, 0.10F, -0.20F, 0.15F,
                               0.30F, -0.05F, 0.20F, 0.10F,
                           });
    layer.wk = make_linear(4,
                           2,
                           {
                               0.10F, 0.20F, -0.10F, 0.05F,
                               -0.05F, 0.15F, 0.25F, -0.20F,
                           });
    layer.wv = make_linear(4,
                           2,
                           {
                               0.25F, -0.05F, 0.10F, 0.15F,
                               -0.10F, 0.30F, 0.05F, 0.20F,
                           });
    layer.wo = make_linear(4,
                           4,
                           {
                               0.20F, 0.10F, -0.05F, 0.15F,
                               -0.10F, 0.25F, 0.20F, -0.05F,
                               0.05F, -0.20F, 0.30F, 0.10F,
                               0.15F, 0.05F, -0.10F, 0.25F,
                           });
    layer.rms_mlp_weight = {0.95F, 1.05F, 1.0F, 0.9F};
    layer.w_gate = make_linear(4,
                               6,
                               {
                                   0.20F, -0.10F, 0.05F, 0.10F,
                                   -0.05F, 0.15F, 0.20F, -0.10F,
                                   0.10F, 0.05F, -0.15F, 0.25F,
                                   -0.20F, 0.10F, 0.15F, 0.05F,
                                   0.05F, -0.25F, 0.10F, 0.20F,
                                   0.15F, 0.05F, 0.05F, -0.15F,
                               });
    layer.w_up = make_linear(4,
                             6,
                             {
                                 0.10F, 0.20F, -0.05F, 0.05F,
                                 -0.15F, 0.05F, 0.25F, 0.10F,
                                 0.20F, -0.10F, 0.05F, 0.15F,
                                 0.05F, 0.10F, -0.20F, 0.25F,
                                 -0.05F, 0.15F, 0.10F, -0.10F,
                                 0.25F, -0.05F, 0.05F, 0.10F,
                             });
    layer.w_down = make_linear(6,
                               4,
                               {
                                   0.10F, -0.05F, 0.15F, 0.20F, -0.10F, 0.05F,
                                   -0.20F, 0.10F, 0.05F, -0.05F, 0.15F, 0.20F,
                                   0.05F, 0.15F, -0.10F, 0.10F, 0.20F, -0.05F,
                                   0.20F, 0.05F, 0.10F, -0.15F, -0.05F, 0.10F,
                               });
    model.layers.push_back(std::move(layer));
    model.final_norm_weight = {1.0F, 0.95F, 1.05F, 1.0F};
    model.lm_head = make_linear(4,
                                5,
                                {
                                    0.20F, -0.10F, 0.05F, 0.15F,
                                    -0.05F, 0.25F, 0.10F, -0.20F,
                                    0.15F, 0.05F, -0.25F, 0.10F,
                                    -0.10F, 0.20F, 0.15F, 0.05F,
                                    0.05F, -0.15F, 0.20F, 0.25F,
                                },
                                zeros(5));
    return model;
}

}  // namespace lcqi

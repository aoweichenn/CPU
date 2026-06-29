#include <lcqi/gpt2_reference.hpp>

#include <lcqi/safetensors.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace lcqi {
namespace {

constexpr std::int32_t LCQI_GPT2_DEFAULT_BOS_EOS = 50256;
constexpr std::int32_t LCQI_GPT2_ATTENTION_PROJECTIONS = 3;
constexpr std::int32_t LCQI_GPT2_PAIR_TOKEN_COUNT = 2;
constexpr std::int32_t LCQI_GPT2_BYTE_COUNT = 256;
constexpr std::int32_t LCQI_GPT2_UNICODE_PRIVATE_BASE = 256;
constexpr std::int32_t LCQI_GPT2_ASCII_EXCLAMATION = 33;
constexpr std::int32_t LCQI_GPT2_ASCII_TILDE = 126;
constexpr std::int32_t LCQI_GPT2_LATIN_INVERTED_EXCLAMATION = 161;
constexpr std::int32_t LCQI_GPT2_LATIN_NOT_SIGN = 172;
constexpr std::int32_t LCQI_GPT2_LATIN_REGISTERED = 174;
constexpr std::int32_t LCQI_GPT2_LATIN_Y_DIAERESIS = 255;
constexpr std::int32_t LCQI_GPT2_UTF8_CONTINUATION_MASK = 0x3F;
constexpr std::int32_t LCQI_GPT2_UTF8_CONTINUATION_BITS = 0x80;
constexpr std::int32_t LCQI_GPT2_UTF8_TWO_BYTE_HEAD = 0xC0;
constexpr std::int32_t LCQI_GPT2_UTF8_THREE_BYTE_HEAD = 0xE0;
constexpr std::int32_t LCQI_GPT2_UTF8_FOUR_BYTE_HEAD = 0xF0;
constexpr std::int32_t LCQI_GPT2_UTF8_ONE_BYTE_LIMIT = 0x80;
constexpr std::int32_t LCQI_GPT2_UTF8_TWO_BYTE_LIMIT = 0x800;
constexpr std::int32_t LCQI_GPT2_UTF8_THREE_BYTE_LIMIT = 0x10000;
constexpr unsigned char LCQI_GPT2_ASCII_APOSTROPHE = 0x27U;
constexpr float LCQI_GPT2_GELU_SQRT_2_OVER_PI = 0.7978845608028654F;
constexpr float LCQI_GPT2_GELU_CUBIC_COEFF = 0.044715F;
constexpr float LCQI_GPT2_SOFTMAX_NEGATIVE_INFINITY =
    -std::numeric_limits<float>::infinity();

class JsonCursor {
public:
    explicit JsonCursor(std::string_view text) : text_(text) {}

    void expect(char expected) {
        this->skip_ws();
        if (this->eof() || this->text_[this->position_] != expected) {
            throw std::runtime_error("invalid JSON syntax");
        }
        ++this->position_;
    }

    [[nodiscard]] bool consume(char expected) {
        this->skip_ws();
        if (!this->eof() && this->text_[this->position_] == expected) {
            ++this->position_;
            return true;
        }
        return false;
    }

    [[nodiscard]] std::string parse_string() {
        this->skip_ws();
        this->expect('"');
        std::string result;
        while (!this->eof()) {
            const char ch = this->text_[this->position_++];
            if (ch == '"') {
                return result;
            }
            if (ch == '\\') {
                result += this->parse_escape();
            } else {
                result.push_back(ch);
            }
        }
        throw std::runtime_error("unterminated JSON string");
    }

    [[nodiscard]] std::int64_t parse_i64() {
        this->skip_ws();
        bool negative = false;
        if (this->consume('-')) {
            negative = true;
        }
        if (this->eof() || this->text_[this->position_] < '0' ||
            this->text_[this->position_] > '9') {
            throw std::runtime_error("expected JSON integer");
        }
        std::int64_t value = 0;
        while (!this->eof() && this->text_[this->position_] >= '0' &&
               this->text_[this->position_] <= '9') {
            const std::int64_t digit = this->text_[this->position_] - '0';
            if (value >
                (std::numeric_limits<std::int64_t>::max() - digit) / 10) {
                throw std::runtime_error("JSON integer overflow");
            }
            value = value * 10 + digit;
            ++this->position_;
        }
        return negative ? -value : value;
    }

    [[nodiscard]] double parse_number() {
        this->skip_ws();
        const std::size_t begin = this->position_;
        if (!this->eof() && this->text_[this->position_] == '-') {
            ++this->position_;
        }
        while (!this->eof() && this->text_[this->position_] >= '0' &&
               this->text_[this->position_] <= '9') {
            ++this->position_;
        }
        if (!this->eof() && this->text_[this->position_] == '.') {
            ++this->position_;
            while (!this->eof() && this->text_[this->position_] >= '0' &&
                   this->text_[this->position_] <= '9') {
                ++this->position_;
            }
        }
        if (!this->eof() &&
            (this->text_[this->position_] == 'e' || this->text_[this->position_] == 'E')) {
            ++this->position_;
            if (!this->eof() &&
                (this->text_[this->position_] == '+' || this->text_[this->position_] == '-')) {
                ++this->position_;
            }
            while (!this->eof() && this->text_[this->position_] >= '0' &&
                   this->text_[this->position_] <= '9') {
                ++this->position_;
            }
        }
        if (begin == this->position_) {
            throw std::runtime_error("expected JSON number");
        }
        return std::stod(std::string(this->text_.substr(begin, this->position_ - begin)));
    }

    void skip_value() {
        this->skip_ws();
        if (this->eof()) {
            throw std::runtime_error("unexpected end of JSON");
        }
        const char ch = this->text_[this->position_];
        if (ch == '"') {
            static_cast<void>(this->parse_string());
            return;
        }
        if (ch == '{') {
            this->expect('{');
            if (this->consume('}')) {
                return;
            }
            while (true) {
                static_cast<void>(this->parse_string());
                this->expect(':');
                this->skip_value();
                if (this->consume('}')) {
                    return;
                }
                this->expect(',');
            }
        }
        if (ch == '[') {
            this->expect('[');
            if (this->consume(']')) {
                return;
            }
            while (true) {
                this->skip_value();
                if (this->consume(']')) {
                    return;
                }
                this->expect(',');
            }
        }
        if (ch == 't') {
            this->expect_literal("true");
            return;
        }
        if (ch == 'f') {
            this->expect_literal("false");
            return;
        }
        if (ch == 'n') {
            this->expect_literal("null");
            return;
        }
        static_cast<void>(this->parse_number());
    }

    [[nodiscard]] bool eof_after_ws() {
        this->skip_ws();
        return this->eof();
    }

private:
    std::string_view text_;
    std::size_t position_ = 0;

    [[nodiscard]] bool eof() const {
        return this->position_ >= this->text_.size();
    }

    void skip_ws() {
        while (!this->eof() &&
               (this->text_[this->position_] == ' ' ||
                this->text_[this->position_] == '\n' ||
                this->text_[this->position_] == '\r' ||
                this->text_[this->position_] == '\t')) {
            ++this->position_;
        }
    }

    void expect_literal(std::string_view literal) {
        for (const char expected : literal) {
            if (this->eof() || this->text_[this->position_] != expected) {
                throw std::runtime_error("invalid JSON literal");
            }
            ++this->position_;
        }
    }

    [[nodiscard]] std::string parse_escape() {
        if (this->eof()) {
            throw std::runtime_error("unterminated JSON escape");
        }
        const char escaped = this->text_[this->position_++];
        switch (escaped) {
            case '"':
            case '\\':
            case '/':
                return std::string(1, escaped);
            case 'b':
                return "\b";
            case 'f':
                return "\f";
            case 'n':
                return "\n";
            case 'r':
                return "\r";
            case 't':
                return "\t";
            case 'u':
                return this->parse_unicode_escape();
            default:
                throw std::runtime_error("unsupported JSON escape");
        }
    }

    [[nodiscard]] std::string parse_unicode_escape() {
        std::uint32_t value = 0;
        for (std::int32_t i = 0; i < 4; ++i) {
            if (this->eof()) {
                throw std::runtime_error("unterminated JSON unicode escape");
            }
            const char ch = this->text_[this->position_++];
            value <<= 4U;
            if (ch >= '0' && ch <= '9') {
                value += static_cast<std::uint32_t>(ch - '0');
            } else if (ch >= 'a' && ch <= 'f') {
                value += static_cast<std::uint32_t>(10 + ch - 'a');
            } else if (ch >= 'A' && ch <= 'F') {
                value += static_cast<std::uint32_t>(10 + ch - 'A');
            } else {
                throw std::runtime_error("invalid JSON unicode escape");
            }
        }
        std::string encoded;
        if (value < LCQI_GPT2_UTF8_ONE_BYTE_LIMIT) {
            encoded.push_back(static_cast<char>(value));
        } else if (value < LCQI_GPT2_UTF8_TWO_BYTE_LIMIT) {
            encoded.push_back(static_cast<char>(
                LCQI_GPT2_UTF8_TWO_BYTE_HEAD | (value >> 6U)));
            encoded.push_back(static_cast<char>(
                LCQI_GPT2_UTF8_CONTINUATION_BITS |
                (value & LCQI_GPT2_UTF8_CONTINUATION_MASK)));
        } else {
            encoded.push_back(static_cast<char>(
                LCQI_GPT2_UTF8_THREE_BYTE_HEAD | (value >> 12U)));
            encoded.push_back(static_cast<char>(
                LCQI_GPT2_UTF8_CONTINUATION_BITS |
                ((value >> 6U) & LCQI_GPT2_UTF8_CONTINUATION_MASK)));
            encoded.push_back(static_cast<char>(
                LCQI_GPT2_UTF8_CONTINUATION_BITS |
                (value & LCQI_GPT2_UTF8_CONTINUATION_MASK)));
        }
        return encoded;
    }
};

std::string read_text_file(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("cannot open text file: " + path.string());
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::size_t checked_size(std::int64_t value, const char* name) {
    if (value < 0) {
        throw std::runtime_error(std::string(name) + " cannot be negative");
    }
    return static_cast<std::size_t>(value);
}

std::string encode_codepoint(std::uint32_t value) {
    std::string encoded;
    if (value < LCQI_GPT2_UTF8_ONE_BYTE_LIMIT) {
        encoded.push_back(static_cast<char>(value));
    } else if (value < LCQI_GPT2_UTF8_TWO_BYTE_LIMIT) {
        encoded.push_back(static_cast<char>(
            LCQI_GPT2_UTF8_TWO_BYTE_HEAD | (value >> 6U)));
        encoded.push_back(static_cast<char>(
            LCQI_GPT2_UTF8_CONTINUATION_BITS |
            (value & LCQI_GPT2_UTF8_CONTINUATION_MASK)));
    } else if (value < LCQI_GPT2_UTF8_THREE_BYTE_LIMIT) {
        encoded.push_back(static_cast<char>(
            LCQI_GPT2_UTF8_THREE_BYTE_HEAD | (value >> 12U)));
        encoded.push_back(static_cast<char>(
            LCQI_GPT2_UTF8_CONTINUATION_BITS |
            ((value >> 6U) & LCQI_GPT2_UTF8_CONTINUATION_MASK)));
        encoded.push_back(static_cast<char>(
            LCQI_GPT2_UTF8_CONTINUATION_BITS |
            (value & LCQI_GPT2_UTF8_CONTINUATION_MASK)));
    } else {
        encoded.push_back(static_cast<char>(
            LCQI_GPT2_UTF8_FOUR_BYTE_HEAD | (value >> 18U)));
        encoded.push_back(static_cast<char>(
            LCQI_GPT2_UTF8_CONTINUATION_BITS |
            ((value >> 12U) & LCQI_GPT2_UTF8_CONTINUATION_MASK)));
        encoded.push_back(static_cast<char>(
            LCQI_GPT2_UTF8_CONTINUATION_BITS |
            ((value >> 6U) & LCQI_GPT2_UTF8_CONTINUATION_MASK)));
        encoded.push_back(static_cast<char>(
            LCQI_GPT2_UTF8_CONTINUATION_BITS |
            (value & LCQI_GPT2_UTF8_CONTINUATION_MASK)));
    }
    return encoded;
}

void require_positive(std::int32_t value, const char* name) {
    if (value <= 0) {
        throw std::runtime_error(std::string(name) + " must be positive");
    }
}

void validate_config(const Gpt2Config& config) {
    require_positive(config.hidden_size, "hidden_size");
    require_positive(config.head_count, "head_count");
    require_positive(config.layer_count, "layer_count");
    require_positive(config.max_positions, "max_positions");
    require_positive(config.vocab_size, "vocab_size");
    require_positive(config.intermediate_size, "intermediate_size");
    if (config.hidden_size % config.head_count != 0) {
        throw std::runtime_error("hidden_size must be divisible by head_count");
    }
    if (config.layer_norm_epsilon <= 0.0F) {
        throw std::runtime_error("layer_norm_epsilon must be positive");
    }
}

std::int32_t head_dim(const Gpt2Config& config) {
    return config.hidden_size / config.head_count;
}

std::size_t matrix_size(std::int32_t rows, std::int32_t columns) {
    return checked_size(rows, "rows") * checked_size(columns, "columns");
}

void validate_vector_size(std::size_t actual, std::int32_t expected, const char* name) {
    if (actual != checked_size(expected, name)) {
        throw std::runtime_error(std::string(name) + " size mismatch");
    }
}

void validate_linear(const Gpt2LinearF32& linear, const char* name) {
    require_positive(linear.input_size, "linear input_size");
    require_positive(linear.output_size, "linear output_size");
    if (linear.weights.size() != matrix_size(linear.output_size, linear.input_size)) {
        throw std::runtime_error(std::string(name) + " weight size mismatch");
    }
    if (!linear.bias.empty() &&
        linear.bias.size() != checked_size(linear.output_size, "linear output_size")) {
        throw std::runtime_error(std::string(name) + " bias size mismatch");
    }
}

void validate_model(const Gpt2ReferenceModel& model) {
    validate_config(model.config);
    if (model.layers.size() != checked_size(model.config.layer_count, "layer_count")) {
        throw std::runtime_error("GPT-2 layer count mismatch");
    }
    if (model.token_embedding.size() !=
        matrix_size(model.config.vocab_size, model.config.hidden_size)) {
        throw std::runtime_error("GPT-2 token embedding size mismatch");
    }
    if (model.position_embedding.size() !=
        matrix_size(model.config.max_positions, model.config.hidden_size)) {
        throw std::runtime_error("GPT-2 position embedding size mismatch");
    }
    validate_vector_size(model.final_ln_weight.size(), model.config.hidden_size, "ln_f weight");
    validate_vector_size(model.final_ln_bias.size(), model.config.hidden_size, "ln_f bias");
    if (model.tie_lm_head_to_embedding) {
        if (!model.lm_head_weight.empty()) {
            throw std::runtime_error("tied GPT-2 lm_head should not carry separate weights");
        }
    } else if (model.lm_head_weight.size() !=
               matrix_size(model.config.vocab_size, model.config.hidden_size)) {
        throw std::runtime_error("GPT-2 lm_head size mismatch");
    }
    for (const Gpt2LayerWeightsF32& layer : model.layers) {
        validate_vector_size(layer.ln_1_weight.size(), model.config.hidden_size, "ln_1 weight");
        validate_vector_size(layer.ln_1_bias.size(), model.config.hidden_size, "ln_1 bias");
        validate_linear(layer.c_attn, "c_attn");
        validate_linear(layer.c_proj, "c_proj");
        validate_vector_size(layer.ln_2_weight.size(), model.config.hidden_size, "ln_2 weight");
        validate_vector_size(layer.ln_2_bias.size(), model.config.hidden_size, "ln_2 bias");
        validate_linear(layer.c_fc, "c_fc");
        validate_linear(layer.mlp_c_proj, "mlp_c_proj");
    }
}

std::span<const float> row_span(std::span<const float> values,
                                std::int32_t row,
                                std::int32_t row_width) {
    return values.subspan(checked_size(row, "row") * checked_size(row_width, "row_width"),
                          checked_size(row_width, "row_width"));
}

void add_inplace(std::span<float> target, std::span<const float> source) {
    if (target.size() != source.size()) {
        throw std::runtime_error("GPT-2 residual add size mismatch");
    }
    for (std::size_t index = 0; index < target.size(); ++index) {
        target[index] += source[index];
    }
}

void layer_norm(std::span<const float> input,
                std::span<const float> weight,
                std::span<const float> bias,
                float epsilon,
                std::span<float> output) {
    if (input.empty() || input.size() != weight.size() || input.size() != bias.size() ||
        input.size() != output.size()) {
        throw std::runtime_error("GPT-2 LayerNorm size mismatch");
    }
    float mean = 0.0F;
    for (const float value : input) {
        mean += value;
    }
    mean /= static_cast<float>(input.size());

    float variance = 0.0F;
    for (const float value : input) {
        const float centered = value - mean;
        variance += centered * centered;
    }
    variance /= static_cast<float>(input.size());
    const float scale = 1.0F / std::sqrt(variance + epsilon);

    for (std::size_t index = 0; index < input.size(); ++index) {
        output[index] = (input[index] - mean) * scale * weight[index] + bias[index];
    }
}

void linear_f32(const Gpt2LinearF32& linear,
                std::span<const float> input,
                std::span<float> output) {
    validate_linear(linear, "GPT-2 linear");
    if (input.size() != checked_size(linear.input_size, "linear input") ||
        output.size() != checked_size(linear.output_size, "linear output")) {
        throw std::runtime_error("GPT-2 linear input/output size mismatch");
    }
    const std::size_t input_size = checked_size(linear.input_size, "linear input");
    const std::size_t output_size = checked_size(linear.output_size, "linear output");
    for (std::size_t out = 0; out < output_size; ++out) {
        float sum = linear.bias.empty() ? 0.0F : linear.bias[out];
        const std::size_t row_base = out * input_size;
        for (std::size_t in = 0; in < input_size; ++in) {
            sum += linear.weights[row_base + in] * input[in];
        }
        output[out] = sum;
    }
}

void linear_f32_unchecked(const Gpt2LinearF32& linear,
                          std::span<const float> input,
                          std::span<float> output) {
    const std::size_t input_size = static_cast<std::size_t>(linear.input_size);
    const std::size_t output_size = static_cast<std::size_t>(linear.output_size);
    for (std::size_t out = 0; out < output_size; ++out) {
        float sum = linear.bias.empty() ? 0.0F : linear.bias[out];
        const std::size_t row_base = out * input_size;
        for (std::size_t in = 0; in < input_size; ++in) {
            sum += linear.weights[row_base + in] * input[in];
        }
        output[out] = sum;
    }
}

float gelu_new(float value) {
    const float cubic = value * value * value;
    return 0.5F * value *
           (1.0F + std::tanh(LCQI_GPT2_GELU_SQRT_2_OVER_PI *
                             (value + LCQI_GPT2_GELU_CUBIC_COEFF * cubic)));
}

std::int32_t argmax(std::span<const float> values) {
    if (values.empty()) {
        throw std::runtime_error("cannot take argmax of empty logits");
    }
    std::int32_t best_index = 0;
    float best_value = values.front();
    for (std::int32_t index = 1; index < static_cast<std::int32_t>(values.size()); ++index) {
        const float value = values[checked_size(index, "argmax index")];
        if (value > best_value) {
            best_value = value;
            best_index = index;
        }
    }
    return best_index;
}

void project_qkv(const Gpt2Config& config,
                 const Gpt2LayerWeightsF32& layer,
                 std::span<const float> hidden,
                 std::span<float> q,
                 std::span<float> k,
                 std::span<float> v) {
    std::vector<float> packed(matrix_size(LCQI_GPT2_ATTENTION_PROJECTIONS,
                                          config.hidden_size),
                              0.0F);
    linear_f32(layer.c_attn, hidden, packed);
    const std::size_t width = checked_size(config.hidden_size, "hidden_size");
    std::copy_n(packed.begin(), config.hidden_size, q.begin());
    std::copy_n(packed.begin() + static_cast<std::ptrdiff_t>(width),
                config.hidden_size,
                k.begin());
    std::copy_n(packed.begin() + static_cast<std::ptrdiff_t>(width * 2),
                config.hidden_size,
                v.begin());
}

void project_qkv_reuse_workspace(const Gpt2Config& config,
                                 const Gpt2LayerWeightsF32& layer,
                                 std::span<const float> hidden,
                                 std::span<float> packed,
                                 std::span<float> q,
                                 std::span<float> k,
                                 std::span<float> v) {
    linear_f32_unchecked(layer.c_attn, hidden, packed);
    const std::size_t width = checked_size(config.hidden_size, "hidden_size");
    std::copy_n(packed.begin(), config.hidden_size, q.begin());
    std::copy_n(packed.begin() + static_cast<std::ptrdiff_t>(width),
                config.hidden_size,
                k.begin());
    std::copy_n(packed.begin() + static_cast<std::ptrdiff_t>(width * 2),
                config.hidden_size,
                v.begin());
}

void attention_full_sequence(const Gpt2Config& config,
                             std::span<const float> q_by_pos,
                             std::span<const float> k_by_pos,
                             std::span<const float> v_by_pos,
                             std::int32_t sequence_length,
                             std::span<float> output_by_pos) {
    const std::int32_t local_head_dim = head_dim(config);
    const float score_scale = 1.0F / std::sqrt(static_cast<float>(local_head_dim));
    std::fill(output_by_pos.begin(), output_by_pos.end(), 0.0F);
    std::vector<float> scores(checked_size(sequence_length, "sequence_length"),
                              LCQI_GPT2_SOFTMAX_NEGATIVE_INFINITY);

    for (std::int32_t position = 0; position < sequence_length; ++position) {
        for (std::int32_t head = 0; head < config.head_count; ++head) {
            float max_score = LCQI_GPT2_SOFTMAX_NEGATIVE_INFINITY;
            for (std::int32_t past = 0; past <= position; ++past) {
                float dot = 0.0F;
                for (std::int32_t dim = 0; dim < local_head_dim; ++dim) {
                    const std::size_t q_index =
                        matrix_size(position, config.hidden_size) +
                        matrix_size(head, local_head_dim) + checked_size(dim, "head dim");
                    const std::size_t k_index =
                        matrix_size(past, config.hidden_size) +
                        matrix_size(head, local_head_dim) + checked_size(dim, "head dim");
                    dot += q_by_pos[q_index] * k_by_pos[k_index];
                }
                const float score = dot * score_scale;
                scores[checked_size(past, "past")] = score;
                max_score = std::max(max_score, score);
            }

            float denominator = 0.0F;
            for (std::int32_t past = 0; past <= position; ++past) {
                const std::size_t index = checked_size(past, "past");
                scores[index] = std::exp(scores[index] - max_score);
                denominator += scores[index];
            }
            if (denominator <= 0.0F) {
                throw std::runtime_error("GPT-2 attention denominator is invalid");
            }
            for (std::int32_t past = 0; past <= position; ++past) {
                const float probability = scores[checked_size(past, "past")] / denominator;
                for (std::int32_t dim = 0; dim < local_head_dim; ++dim) {
                    const std::size_t output_index =
                        matrix_size(position, config.hidden_size) +
                        matrix_size(head, local_head_dim) + checked_size(dim, "head dim");
                    const std::size_t value_index =
                        matrix_size(past, config.hidden_size) +
                        matrix_size(head, local_head_dim) + checked_size(dim, "head dim");
                    output_by_pos[output_index] += probability * v_by_pos[value_index];
                }
            }
        }
    }
}

void forward_gpt2_layer(const Gpt2Config& config,
                        const Gpt2LayerWeightsF32& layer,
                        std::int32_t sequence_length,
                        std::vector<float>& hidden_by_pos) {
    std::vector<float> normed(checked_size(config.hidden_size, "hidden_size"), 0.0F);
    std::vector<float> q_by_pos(matrix_size(sequence_length, config.hidden_size), 0.0F);
    std::vector<float> k_by_pos(q_by_pos.size(), 0.0F);
    std::vector<float> v_by_pos(q_by_pos.size(), 0.0F);
    std::vector<float> attention_by_pos(q_by_pos.size(), 0.0F);
    std::vector<float> projected(checked_size(config.hidden_size, "hidden_size"), 0.0F);
    std::vector<float> mlp_fc(checked_size(config.intermediate_size, "intermediate_size"), 0.0F);
    std::vector<float> mlp_out(checked_size(config.hidden_size, "hidden_size"), 0.0F);

    for (std::int32_t position = 0; position < sequence_length; ++position) {
        const std::span<float> hidden = std::span<float>(
            hidden_by_pos.data() + matrix_size(position, config.hidden_size),
            checked_size(config.hidden_size, "hidden_size"));
        layer_norm(hidden,
                   layer.ln_1_weight,
                   layer.ln_1_bias,
                   config.layer_norm_epsilon,
                   normed);
        std::span<float> q = std::span<float>(
            q_by_pos.data() + matrix_size(position, config.hidden_size),
            checked_size(config.hidden_size, "hidden_size"));
        std::span<float> k = std::span<float>(
            k_by_pos.data() + matrix_size(position, config.hidden_size),
            checked_size(config.hidden_size, "hidden_size"));
        std::span<float> v = std::span<float>(
            v_by_pos.data() + matrix_size(position, config.hidden_size),
            checked_size(config.hidden_size, "hidden_size"));
        project_qkv(config, layer, normed, q, k, v);
    }

    attention_full_sequence(config, q_by_pos, k_by_pos, v_by_pos, sequence_length, attention_by_pos);

    for (std::int32_t position = 0; position < sequence_length; ++position) {
        const std::span<float> hidden = std::span<float>(
            hidden_by_pos.data() + matrix_size(position, config.hidden_size),
            checked_size(config.hidden_size, "hidden_size"));
        const std::span<const float> attention = std::span<const float>(
            attention_by_pos.data() + matrix_size(position, config.hidden_size),
            checked_size(config.hidden_size, "hidden_size"));
        linear_f32(layer.c_proj, attention, projected);
        add_inplace(hidden, projected);

        layer_norm(hidden,
                   layer.ln_2_weight,
                   layer.ln_2_bias,
                   config.layer_norm_epsilon,
                   normed);
        linear_f32(layer.c_fc, normed, mlp_fc);
        for (float& value : mlp_fc) {
            value = gelu_new(value);
        }
        linear_f32(layer.mlp_c_proj, mlp_fc, mlp_out);
        add_inplace(hidden, mlp_out);
    }
}

void forward_gpt2_layer_cached(const Gpt2Config& config,
                               const Gpt2LayerWeightsF32& layer,
                               std::int32_t layer_id,
                               std::int32_t model_position,
                               Gpt2KvCache& cache,
                               Gpt2ForwardWorkspace& workspace,
                               std::span<float> hidden) {
    std::span<float> normed = workspace.normed();
    std::span<float> query = workspace.query();
    std::span<float> key = workspace.key();
    std::span<float> value = workspace.value();
    std::span<float> attention = workspace.attention();
    std::span<float> projected = workspace.projected();
    std::span<float> qkv_packed = workspace.qkv_packed();
    std::span<float> mlp_fc = workspace.mlp_fc();
    std::span<float> mlp_out = workspace.mlp_out();
    std::span<float> scores = workspace.scores_prefix(model_position + 1);

    layer_norm(hidden,
               layer.ln_1_weight,
               layer.ln_1_bias,
               config.layer_norm_epsilon,
               normed);
    project_qkv_reuse_workspace(config, layer, normed, qkv_packed, query, key, value);
    cache.append(layer_id, model_position, key, value);
    detail::attend_cached_position(cache,
                                   layer_id,
                                   model_position,
                                   query,
                                   scores,
                                   attention);

    linear_f32_unchecked(layer.c_proj, attention, projected);
    add_inplace(hidden, projected);

    layer_norm(hidden,
               layer.ln_2_weight,
               layer.ln_2_bias,
               config.layer_norm_epsilon,
               normed);
    linear_f32_unchecked(layer.c_fc, normed, mlp_fc);
    for (float& value_ref : mlp_fc) {
        value_ref = gelu_new(value_ref);
    }
    linear_f32_unchecked(layer.mlp_c_proj, mlp_fc, mlp_out);
    add_inplace(hidden, mlp_out);
}

void compute_logits(const Gpt2ReferenceModel& model,
                    std::span<const float> hidden,
                    std::span<float> logits) {
    if (logits.size() != checked_size(model.config.vocab_size, "vocab_size")) {
        throw std::runtime_error("GPT-2 logits size mismatch");
    }
    const std::span<const float> weight =
        model.tie_lm_head_to_embedding ? std::span<const float>(model.token_embedding)
                                       : std::span<const float>(model.lm_head_weight);
    const std::size_t hidden_size = checked_size(model.config.hidden_size, "hidden_size");
    const std::size_t vocab_size = checked_size(model.config.vocab_size, "vocab_size");
    const float* weight_data = weight.data();
    const float* hidden_data = hidden.data();
    for (std::size_t token = 0; token < vocab_size; ++token) {
        float sum = 0.0F;
        const float* row = weight_data + token * hidden_size;
        for (std::size_t dim = 0; dim < hidden_size; ++dim) {
            sum += row[dim] * hidden_data[dim];
        }
        logits[token] = sum;
    }
}

std::int32_t compute_predicted_token(const Gpt2ReferenceModel& model,
                                     std::span<const float> hidden) {
    const std::span<const float> weight =
        model.tie_lm_head_to_embedding ? std::span<const float>(model.token_embedding)
                                       : std::span<const float>(model.lm_head_weight);
    const std::size_t hidden_size = checked_size(model.config.hidden_size, "hidden_size");
    const std::size_t vocab_size = checked_size(model.config.vocab_size, "vocab_size");
    const float* weight_data = weight.data();
    const float* hidden_data = hidden.data();
    std::int32_t best_token = 0;
    float best_value = -std::numeric_limits<float>::infinity();
    for (std::size_t token = 0; token < vocab_size; ++token) {
        float sum = 0.0F;
        const float* row = weight_data + token * hidden_size;
        for (std::size_t dim = 0; dim < hidden_size; ++dim) {
            sum += row[dim] * hidden_data[dim];
        }
        if (token == 0 || sum > best_value) {
            best_value = sum;
            best_token = static_cast<std::int32_t>(token);
        }
    }
    return best_token;
}

Gpt2ForwardResult run_gpt2_cached_step_unchecked(const Gpt2ReferenceModel& model,
                                                 Gpt2KvCache& cache,
                                                 Gpt2ForwardWorkspace& workspace,
                                                 std::int32_t token_id,
                                                 bool need_logits) {
    if (token_id < 0 || token_id >= model.config.vocab_size) {
        throw std::runtime_error("GPT-2 token id out of range");
    }
    const std::int32_t model_position = cache.filled_tokens();
    if (model_position >= model.config.max_positions) {
        throw std::runtime_error("GPT-2 cached forward would exceed max_positions");
    }

    std::span<float> hidden = workspace.hidden();
    const std::span<const float> token =
        row_span(model.token_embedding, token_id, model.config.hidden_size);
    const std::span<const float> position_embedding =
        row_span(model.position_embedding, model_position, model.config.hidden_size);
    for (std::int32_t dim = 0; dim < model.config.hidden_size; ++dim) {
        hidden[checked_size(dim, "dim")] =
            token[checked_size(dim, "dim")] + position_embedding[checked_size(dim, "dim")];
    }

    for (std::int32_t layer_id = 0;
         layer_id < static_cast<std::int32_t>(model.layers.size());
         ++layer_id) {
        forward_gpt2_layer_cached(model.config,
                                  model.layers[checked_size(layer_id, "layer_id")],
                                  layer_id,
                                  model_position,
                                  cache,
                                  workspace,
                                  hidden);
    }

    std::span<float> normed = workspace.normed();
    layer_norm(hidden,
               model.final_ln_weight,
               model.final_ln_bias,
               model.config.layer_norm_epsilon,
               normed);

    Gpt2ForwardResult result;
    if (need_logits) {
        std::span<float> logits = workspace.logits();
        compute_logits(model, normed, logits);
        result.logits.assign(logits.begin(), logits.end());
        result.predicted_token = argmax(result.logits);
    } else {
        result.predicted_token = compute_predicted_token(model, normed);
    }
    return result;
}

std::vector<std::string> gpt2_bytes_to_unicode() {
    std::vector<std::int32_t> bytes;
    for (std::int32_t value = LCQI_GPT2_ASCII_EXCLAMATION;
         value <= LCQI_GPT2_ASCII_TILDE;
         ++value) {
        bytes.push_back(value);
    }
    for (std::int32_t value = LCQI_GPT2_LATIN_INVERTED_EXCLAMATION;
         value <= LCQI_GPT2_LATIN_NOT_SIGN;
         ++value) {
        bytes.push_back(value);
    }
    for (std::int32_t value = LCQI_GPT2_LATIN_REGISTERED;
         value <= LCQI_GPT2_LATIN_Y_DIAERESIS;
         ++value) {
        bytes.push_back(value);
    }

    std::unordered_set<std::int32_t> byte_set(bytes.begin(), bytes.end());
    std::vector<std::int32_t> chars = bytes;
    std::int32_t private_offset = 0;
    for (std::int32_t byte = 0; byte < LCQI_GPT2_BYTE_COUNT; ++byte) {
        if (!byte_set.contains(byte)) {
            bytes.push_back(byte);
            chars.push_back(LCQI_GPT2_UNICODE_PRIVATE_BASE + private_offset);
            ++private_offset;
        }
    }

    std::vector<std::string> mapping(LCQI_GPT2_BYTE_COUNT);
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        mapping[checked_size(bytes[index], "byte unicode index")] =
            encode_codepoint(static_cast<std::uint32_t>(chars[index]));
    }
    return mapping;
}

std::unordered_map<std::string, unsigned char> gpt2_unicode_to_bytes(
    const std::vector<std::string>& byte_encoder) {
    std::unordered_map<std::string, unsigned char> result;
    for (std::size_t byte = 0; byte < byte_encoder.size(); ++byte) {
        result.emplace(byte_encoder[byte], static_cast<unsigned char>(byte));
    }
    return result;
}

std::string bytes_to_bpe_alphabet(std::string_view text) {
    static const std::vector<std::string> BYTE_ENCODER = gpt2_bytes_to_unicode();
    std::string result;
    for (const unsigned char byte : text) {
        result += BYTE_ENCODER[byte];
    }
    return result;
}

std::vector<std::string> utf8_symbols(std::string_view text) {
    std::vector<std::string> symbols;
    std::size_t offset = 0;
    while (offset < text.size()) {
        const unsigned char lead = static_cast<unsigned char>(text[offset]);
        std::size_t width = 1;
        if ((lead & 0xE0U) == LCQI_GPT2_UTF8_TWO_BYTE_HEAD) {
            width = 2;
        } else if ((lead & 0xF0U) == LCQI_GPT2_UTF8_THREE_BYTE_HEAD) {
            width = 3;
        } else if ((lead & 0xF8U) == LCQI_GPT2_UTF8_FOUR_BYTE_HEAD) {
            width = 4;
        }
        if (offset + width > text.size()) {
            throw std::runtime_error("invalid UTF-8/BPE symbol boundary");
        }
        symbols.emplace_back(text.substr(offset, width));
        offset += width;
    }
    return symbols;
}

std::string pair_key(std::string_view left, std::string_view right) {
    std::string key(left);
    key.push_back('\n');
    key += right;
    return key;
}

std::vector<std::string> bpe_apply(const Gpt2Tokenizer& tokenizer, std::string_view token) {
    std::vector<std::string> word = utf8_symbols(token);
    if (word.size() <= 1) {
        return word;
    }

    while (true) {
        std::optional<std::int32_t> best_rank;
        std::string best_left;
        std::string best_right;
        for (std::size_t index = 0; index + 1 < word.size(); ++index) {
            const auto rank = tokenizer.bpe_ranks.find(pair_key(word[index], word[index + 1]));
            if (rank != tokenizer.bpe_ranks.end() &&
                (!best_rank.has_value() || rank->second < *best_rank)) {
                best_rank = rank->second;
                best_left = word[index];
                best_right = word[index + 1];
            }
        }
        if (!best_rank.has_value()) {
            return word;
        }

        std::vector<std::string> merged;
        std::size_t index = 0;
        while (index < word.size()) {
            if (index + 1 < word.size() && word[index] == best_left &&
                word[index + 1] == best_right) {
                merged.push_back(best_left + best_right);
                index += LCQI_GPT2_PAIR_TOKEN_COUNT;
            } else {
                merged.push_back(word[index]);
                ++index;
            }
        }
        word = std::move(merged);
        if (word.size() == 1) {
            return word;
        }
    }
}

bool is_ascii_letter(unsigned char value) {
    return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z');
}

bool is_ascii_digit(unsigned char value) {
    return value >= '0' && value <= '9';
}

bool is_ascii_space(unsigned char value) {
    return value == ' ' || value == '\t' || value == '\n' ||
           value == '\r' || value == '\f' || value == '\v';
}

bool is_gpt2_punctuation(unsigned char value) {
    return !is_ascii_letter(value) && !is_ascii_digit(value) &&
           !is_ascii_space(value);
}

std::size_t consume_utf8_codepoint(std::string_view text, std::size_t offset) {
    const unsigned char lead = static_cast<unsigned char>(text[offset]);
    std::size_t width = 1;
    if ((lead & 0xE0U) == LCQI_GPT2_UTF8_TWO_BYTE_HEAD) {
        width = 2;
    } else if ((lead & 0xF0U) == LCQI_GPT2_UTF8_THREE_BYTE_HEAD) {
        width = 3;
    } else if ((lead & 0xF8U) == LCQI_GPT2_UTF8_FOUR_BYTE_HEAD) {
        width = 4;
    }
    if (offset + width > text.size()) {
        throw std::runtime_error("invalid UTF-8 text for GPT-2 tokenizer");
    }
    return offset + width;
}

bool starts_with_contraction(std::string_view text,
                             std::size_t offset,
                             std::string_view contraction) {
    if (offset + contraction.size() > text.size()) {
        return false;
    }
    for (std::size_t index = 0; index < contraction.size(); ++index) {
        char left = text[offset + index];
        const char right = contraction[index];
        if (left >= 'A' && left <= 'Z') {
            left = static_cast<char>(left - 'A' + 'a');
        }
        if (left != right) {
            return false;
        }
    }
    return true;
}

std::vector<std::string> gpt2_pretokenize(std::string_view text) {
    std::vector<std::string> pieces;
    std::size_t offset = 0;
    const std::array<std::string_view, 7> contractions{
        "'s", "'t", "'re", "'ve", "'m", "'ll", "'d",
    };
    while (offset < text.size()) {
        bool emitted_contraction = false;
        for (const std::string_view contraction : contractions) {
            if (starts_with_contraction(text, offset, contraction)) {
                pieces.emplace_back(text.substr(offset, contraction.size()));
                offset += contraction.size();
                emitted_contraction = true;
                break;
            }
        }
        if (emitted_contraction) {
            continue;
        }

        const std::size_t begin = offset;
        bool has_prefix_space = false;
        if (text[offset] == ' ' && offset + 1 < text.size() &&
            !is_ascii_space(static_cast<unsigned char>(text[offset + 1]))) {
            has_prefix_space = true;
            ++offset;
        }

        if (offset >= text.size()) {
            pieces.emplace_back(text.substr(begin, offset - begin));
            continue;
        }

        const unsigned char ch = static_cast<unsigned char>(text[offset]);
        if (is_ascii_letter(ch)) {
            while (offset < text.size() &&
                   is_ascii_letter(static_cast<unsigned char>(text[offset]))) {
                ++offset;
            }
        } else if (is_ascii_digit(ch)) {
            while (offset < text.size() &&
                   is_ascii_digit(static_cast<unsigned char>(text[offset]))) {
                ++offset;
            }
        } else if (is_gpt2_punctuation(ch) && ch != LCQI_GPT2_ASCII_APOSTROPHE) {
            while (offset < text.size() &&
                   is_gpt2_punctuation(static_cast<unsigned char>(text[offset])) &&
                   static_cast<unsigned char>(text[offset]) != LCQI_GPT2_ASCII_APOSTROPHE) {
                offset = consume_utf8_codepoint(text, offset);
            }
        } else if (is_ascii_space(ch)) {
            while (offset < text.size() &&
                   is_ascii_space(static_cast<unsigned char>(text[offset]))) {
                ++offset;
            }
        } else {
            offset = consume_utf8_codepoint(text, offset);
        }

        if (has_prefix_space || offset > begin) {
            pieces.emplace_back(text.substr(begin, offset - begin));
        } else {
            ++offset;
        }
    }
    return pieces;
}

std::int32_t parse_i32_field(JsonCursor& cursor) {
    const std::int64_t value = cursor.parse_i64();
    if (value < std::numeric_limits<std::int32_t>::min() ||
        value > std::numeric_limits<std::int32_t>::max()) {
        throw std::runtime_error("JSON int32 field out of range");
    }
    return static_cast<std::int32_t>(value);
}

std::unordered_map<std::string, std::int32_t> parse_vocab_json(std::string_view text) {
    JsonCursor cursor(text);
    std::unordered_map<std::string, std::int32_t> vocab;
    cursor.expect('{');
    if (cursor.consume('}')) {
        return vocab;
    }
    while (true) {
        const std::string key = cursor.parse_string();
        cursor.expect(':');
        const std::int32_t id = parse_i32_field(cursor);
        vocab.emplace(key, id);
        if (cursor.consume('}')) {
            break;
        }
        cursor.expect(',');
    }
    if (!cursor.eof_after_ws()) {
        throw std::runtime_error("trailing data after vocab JSON");
    }
    return vocab;
}

std::vector<std::string> build_id_to_token(
    const std::unordered_map<std::string, std::int32_t>& vocab) {
    std::int32_t max_id = -1;
    for (const auto& [token, id] : vocab) {
        if (id < 0) {
            throw std::runtime_error("GPT-2 vocab id cannot be negative");
        }
        max_id = std::max(max_id, id);
    }
    std::vector<std::string> result(checked_size(max_id + 1, "max vocab id"));
    for (const auto& [token, id] : vocab) {
        std::string& slot = result[checked_size(id, "vocab id")];
        if (!slot.empty()) {
            throw std::runtime_error("duplicate GPT-2 vocab id");
        }
        slot = token;
    }
    return result;
}

std::unordered_map<std::string, std::int32_t> parse_merges(std::string_view text) {
    std::unordered_map<std::string, std::int32_t> ranks;
    std::istringstream input{std::string(text)};
    std::string line;
    std::int32_t rank = 0;
    while (std::getline(input, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        std::istringstream line_stream(line);
        std::string left;
        std::string right;
        if (!(line_stream >> left >> right)) {
            throw std::runtime_error("invalid GPT-2 merges line");
        }
        ranks.emplace(pair_key(left, right), rank);
        ++rank;
    }
    return ranks;
}

void transpose_hf_conv1d(std::vector<float>& values,
                         std::int32_t input_size,
                         std::int32_t output_size) {
    if (values.size() != matrix_size(input_size, output_size)) {
        throw std::runtime_error("GPT-2 Conv1D tensor size mismatch");
    }
    std::vector<float> transposed(matrix_size(output_size, input_size), 0.0F);
    for (std::int32_t input = 0; input < input_size; ++input) {
        for (std::int32_t output = 0; output < output_size; ++output) {
            transposed[matrix_size(output, input_size) + checked_size(input, "input")] =
                values[matrix_size(input, output_size) + checked_size(output, "output")];
        }
    }
    values = std::move(transposed);
}

std::vector<float> require_tensor(const SafeTensorFile& file,
                                  std::string_view name,
                                  std::span<const std::int64_t> expected_shape) {
    const SafeTensorEntry* entry = file.manifest.find_tensor(name);
    if (entry == nullptr) {
        throw std::runtime_error("missing GPT-2 tensor: " + std::string(name));
    }
    if (entry->shape.size() != expected_shape.size()) {
        throw std::runtime_error("GPT-2 tensor rank mismatch: " + std::string(name));
    }
    for (std::size_t index = 0; index < expected_shape.size(); ++index) {
        if (entry->shape[index] != expected_shape[index]) {
            throw std::runtime_error("GPT-2 tensor shape mismatch: " + std::string(name));
        }
    }
    return read_safetensor_f32_tensor(file, name);
}

std::vector<float> require_first_tensor(const SafeTensorFile& file,
                                        std::span<const std::string_view> names,
                                        std::span<const std::int64_t> expected_shape) {
    for (const std::string_view name : names) {
        if (file.manifest.find_tensor(name) != nullptr) {
            return require_tensor(file, name, expected_shape);
        }
    }
    throw std::runtime_error("missing GPT-2 tensor: " + std::string(names.front()));
}

Gpt2LinearF32 make_linear(std::int32_t input_size,
                          std::int32_t output_size,
                          std::vector<float> weights,
                          std::vector<float> bias = {}) {
    Gpt2LinearF32 linear;
    linear.input_size = input_size;
    linear.output_size = output_size;
    linear.weights = std::move(weights);
    linear.bias = std::move(bias);
    validate_linear(linear, "make GPT-2 linear");
    return linear;
}

std::vector<float> zeros(std::int32_t count) {
    return std::vector<float>(checked_size(count, "zero count"), 0.0F);
}

std::vector<float> ones(std::int32_t count) {
    return std::vector<float>(checked_size(count, "one count"), 1.0F);
}

std::string tensor_name(std::int32_t layer, std::string_view suffix) {
    return "transformer.h." + std::to_string(layer) + "." + std::string(suffix);
}

std::string bare_tensor_name(std::int32_t layer, std::string_view suffix) {
    return "h." + std::to_string(layer) + "." + std::string(suffix);
}

std::array<std::string_view, 2> embedding_names(std::string_view bare) {
    if (bare == "wte.weight") {
        return {"transformer.wte.weight", "wte.weight"};
    }
    if (bare == "wpe.weight") {
        return {"transformer.wpe.weight", "wpe.weight"};
    }
    if (bare == "ln_f.weight") {
        return {"transformer.ln_f.weight", "ln_f.weight"};
    }
    if (bare == "ln_f.bias") {
        return {"transformer.ln_f.bias", "ln_f.bias"};
    }
    throw std::runtime_error("unsupported GPT-2 embedding tensor alias");
}

std::array<std::string, 2> layer_names(std::int32_t layer, std::string_view suffix) {
    return {tensor_name(layer, suffix), bare_tensor_name(layer, suffix)};
}

std::vector<float> require_layer_tensor(const SafeTensorFile& file,
                                        std::int32_t layer,
                                        std::string_view suffix,
                                        std::span<const std::int64_t> expected_shape) {
    const std::array<std::string, 2> names = layer_names(layer, suffix);
    const std::array<std::string_view, 2> views{names[0], names[1]};
    return require_first_tensor(file, views, expected_shape);
}

std::filesystem::path find_gpt2_weight_file(const std::filesystem::path& model_directory) {
    const std::array<const char*, 2> candidates{
        "model.safetensors",
        "pytorch_model.safetensors",
    };
    for (const char* candidate : candidates) {
        const std::filesystem::path path = model_directory / candidate;
        if (std::filesystem::exists(path)) {
            return path;
        }
    }
    throw std::runtime_error("cannot find model.safetensors in GPT-2 directory");
}

}  // namespace

Gpt2Tokenizer load_gpt2_tokenizer(const std::filesystem::path& vocab_json,
                                  const std::filesystem::path& merges_txt,
                                  std::int32_t bos_token_id,
                                  std::int32_t eos_token_id) {
    Gpt2Tokenizer tokenizer;
    tokenizer.bos_token_id = bos_token_id;
    tokenizer.eos_token_id = eos_token_id;
    tokenizer.vocab = parse_vocab_json(read_text_file(vocab_json));
    tokenizer.id_to_token = build_id_to_token(tokenizer.vocab);
    tokenizer.bpe_ranks = parse_merges(read_text_file(merges_txt));
    if (tokenizer.vocab.empty() || tokenizer.bpe_ranks.empty()) {
        throw std::runtime_error("GPT-2 tokenizer assets are empty");
    }
    return tokenizer;
}

std::vector<std::int32_t> gpt2_encode(const Gpt2Tokenizer& tokenizer,
                                      std::string_view text) {
    std::vector<std::int32_t> ids;
    for (const std::string& piece : gpt2_pretokenize(text)) {
        const std::string byte_piece = bytes_to_bpe_alphabet(piece);
        const std::vector<std::string> bpe_tokens = bpe_apply(tokenizer, byte_piece);
        for (const std::string& token : bpe_tokens) {
            const auto found = tokenizer.vocab.find(token);
            if (found == tokenizer.vocab.end()) {
                throw std::runtime_error("GPT-2 BPE token is missing from vocab");
            }
            ids.push_back(found->second);
        }
    }
    return ids;
}

std::string gpt2_decode(const Gpt2Tokenizer& tokenizer,
                        std::span<const std::int32_t> token_ids) {
    static const std::vector<std::string> BYTE_ENCODER = gpt2_bytes_to_unicode();
    static const std::unordered_map<std::string, unsigned char> BYTE_DECODER =
        gpt2_unicode_to_bytes(BYTE_ENCODER);

    std::string token_text;
    for (const std::int32_t token_id : token_ids) {
        if (token_id < 0 || token_id >= static_cast<std::int32_t>(tokenizer.id_to_token.size())) {
            throw std::runtime_error("GPT-2 token id out of range");
        }
        token_text += tokenizer.id_to_token[checked_size(token_id, "token id")];
    }

    std::string result;
    const std::vector<std::string> symbols = utf8_symbols(token_text);
    for (const std::string& symbol : symbols) {
        const auto found = BYTE_DECODER.find(symbol);
        if (found == BYTE_DECODER.end()) {
            throw std::runtime_error("GPT-2 token cannot be decoded to byte");
        }
        result.push_back(static_cast<char>(found->second));
    }
    return result;
}

Gpt2Config load_gpt2_config(const std::filesystem::path& config_json) {
    const std::string config_text = read_text_file(config_json);
    JsonCursor cursor(config_text);
    Gpt2Config config;
    config.bos_token_id = LCQI_GPT2_DEFAULT_BOS_EOS;
    config.eos_token_id = LCQI_GPT2_DEFAULT_BOS_EOS;

    cursor.expect('{');
    if (cursor.consume('}')) {
        throw std::runtime_error("GPT-2 config is empty");
    }
    while (true) {
        const std::string key = cursor.parse_string();
        cursor.expect(':');
        if (key == "n_embd") {
            config.hidden_size = parse_i32_field(cursor);
        } else if (key == "n_head") {
            config.head_count = parse_i32_field(cursor);
        } else if (key == "n_layer") {
            config.layer_count = parse_i32_field(cursor);
        } else if (key == "n_positions" || key == "n_ctx") {
            const std::int32_t value = parse_i32_field(cursor);
            config.max_positions = std::max(config.max_positions, value);
        } else if (key == "vocab_size") {
            config.vocab_size = parse_i32_field(cursor);
        } else if (key == "n_inner") {
            config.intermediate_size = parse_i32_field(cursor);
        } else if (key == "layer_norm_epsilon") {
            config.layer_norm_epsilon = static_cast<float>(cursor.parse_number());
        } else if (key == "activation_function") {
            config.activation_function = cursor.parse_string();
        } else if (key == "bos_token_id") {
            config.bos_token_id = parse_i32_field(cursor);
        } else if (key == "eos_token_id") {
            config.eos_token_id = parse_i32_field(cursor);
        } else {
            cursor.skip_value();
        }
        if (cursor.consume('}')) {
            break;
        }
        cursor.expect(',');
    }
    if (!cursor.eof_after_ws()) {
        throw std::runtime_error("trailing data after GPT-2 config JSON");
    }
    if (config.intermediate_size == 0 && config.hidden_size > 0) {
        config.intermediate_size = config.hidden_size * 4;
    }
    validate_config(config);
    return config;
}

Gpt2ReferenceModel load_gpt2_reference_model(const std::filesystem::path& config_json,
                                             const std::filesystem::path& safetensors_path) {
    Gpt2ReferenceModel model;
    model.config = load_gpt2_config(config_json);
    const SafeTensorFile file = load_safetensors_file(safetensors_path);
    const Gpt2Config& config = model.config;
    const std::array<std::int64_t, 2> wte_shape{config.vocab_size, config.hidden_size};
    const std::array<std::int64_t, 2> wpe_shape{config.max_positions, config.hidden_size};
    const std::array<std::int64_t, 1> hidden_shape{config.hidden_size};

    const std::array<std::string_view, 2> wte_names = embedding_names("wte.weight");
    const std::array<std::string_view, 2> wpe_names = embedding_names("wpe.weight");
    model.token_embedding = require_first_tensor(file, wte_names, wte_shape);
    model.position_embedding = require_first_tensor(file, wpe_names, wpe_shape);
    model.layers.resize(checked_size(config.layer_count, "layer_count"));

    for (std::int32_t layer_id = 0; layer_id < config.layer_count; ++layer_id) {
        Gpt2LayerWeightsF32& layer = model.layers[checked_size(layer_id, "layer_id")];
        layer.ln_1_weight = require_layer_tensor(file, layer_id, "ln_1.weight", hidden_shape);
        layer.ln_1_bias = require_layer_tensor(file, layer_id, "ln_1.bias", hidden_shape);
        layer.ln_2_weight = require_layer_tensor(file, layer_id, "ln_2.weight", hidden_shape);
        layer.ln_2_bias = require_layer_tensor(file, layer_id, "ln_2.bias", hidden_shape);

        const std::array<std::int64_t, 2> c_attn_shape{
            config.hidden_size,
            config.hidden_size * LCQI_GPT2_ATTENTION_PROJECTIONS};
        std::vector<float> c_attn_weight =
            require_layer_tensor(file, layer_id, "attn.c_attn.weight", c_attn_shape);
        transpose_hf_conv1d(c_attn_weight,
                            config.hidden_size,
                            config.hidden_size * LCQI_GPT2_ATTENTION_PROJECTIONS);
        const std::array<std::int64_t, 1> c_attn_bias_shape{
            config.hidden_size * LCQI_GPT2_ATTENTION_PROJECTIONS};
        layer.c_attn = make_linear(config.hidden_size,
                                   config.hidden_size * LCQI_GPT2_ATTENTION_PROJECTIONS,
                                   std::move(c_attn_weight),
                                   require_layer_tensor(file,
                                                        layer_id,
                                                        "attn.c_attn.bias",
                                                        c_attn_bias_shape));

        const std::array<std::int64_t, 2> c_proj_shape{config.hidden_size, config.hidden_size};
        std::vector<float> c_proj_weight =
            require_layer_tensor(file, layer_id, "attn.c_proj.weight", c_proj_shape);
        transpose_hf_conv1d(c_proj_weight, config.hidden_size, config.hidden_size);
        layer.c_proj = make_linear(config.hidden_size,
                                   config.hidden_size,
                                   std::move(c_proj_weight),
                                   require_layer_tensor(file,
                                                        layer_id,
                                                        "attn.c_proj.bias",
                                                        hidden_shape));

        const std::array<std::int64_t, 2> c_fc_shape{
            config.hidden_size,
            config.intermediate_size};
        std::vector<float> c_fc_weight =
            require_layer_tensor(file, layer_id, "mlp.c_fc.weight", c_fc_shape);
        transpose_hf_conv1d(c_fc_weight, config.hidden_size, config.intermediate_size);
        const std::array<std::int64_t, 1> intermediate_shape{config.intermediate_size};
        layer.c_fc = make_linear(config.hidden_size,
                                 config.intermediate_size,
                                 std::move(c_fc_weight),
                                 require_layer_tensor(file,
                                                      layer_id,
                                                      "mlp.c_fc.bias",
                                                      intermediate_shape));

        const std::array<std::int64_t, 2> mlp_proj_shape{
            config.intermediate_size,
            config.hidden_size};
        std::vector<float> mlp_proj_weight =
            require_layer_tensor(file, layer_id, "mlp.c_proj.weight", mlp_proj_shape);
        transpose_hf_conv1d(mlp_proj_weight, config.intermediate_size, config.hidden_size);
        layer.mlp_c_proj = make_linear(config.intermediate_size,
                                       config.hidden_size,
                                       std::move(mlp_proj_weight),
                                       require_layer_tensor(file,
                                                            layer_id,
                                                            "mlp.c_proj.bias",
                                                            hidden_shape));
    }

    const std::array<std::string_view, 2> ln_weight_names = embedding_names("ln_f.weight");
    const std::array<std::string_view, 2> ln_bias_names = embedding_names("ln_f.bias");
    model.final_ln_weight = require_first_tensor(file, ln_weight_names, hidden_shape);
    model.final_ln_bias = require_first_tensor(file, ln_bias_names, hidden_shape);
    if (file.manifest.find_tensor("lm_head.weight") != nullptr) {
        model.tie_lm_head_to_embedding = false;
        model.lm_head_weight = require_tensor(file, "lm_head.weight", wte_shape);
    } else {
        model.tie_lm_head_to_embedding = true;
    }
    validate_model(model);
    return model;
}

Gpt2ReferenceModel load_gpt2_from_directory(const std::filesystem::path& model_directory) {
    return load_gpt2_reference_model(model_directory / "config.json",
                                     find_gpt2_weight_file(model_directory));
}

Gpt2ForwardWorkspace::Gpt2ForwardWorkspace(const Gpt2Config& config) : config_(config) {
    this->reset_for_config(config);
}

void Gpt2ForwardWorkspace::reset_for_config(const Gpt2Config& config) {
    validate_config(config);
    this->config_ = config;
    const std::size_t hidden_size = checked_size(this->config_.hidden_size, "hidden_size");
    const std::size_t intermediate_size =
        checked_size(this->config_.intermediate_size, "intermediate_size");
    this->hidden_.assign(hidden_size, 0.0F);
    this->normed_.assign(hidden_size, 0.0F);
    this->query_.assign(hidden_size, 0.0F);
    this->key_.assign(hidden_size, 0.0F);
    this->value_.assign(hidden_size, 0.0F);
    this->attention_.assign(hidden_size, 0.0F);
    this->projected_.assign(hidden_size, 0.0F);
    this->qkv_packed_.assign(
        matrix_size(LCQI_GPT2_ATTENTION_PROJECTIONS, this->config_.hidden_size),
        0.0F);
    this->mlp_fc_.assign(intermediate_size, 0.0F);
    this->mlp_out_.assign(hidden_size, 0.0F);
    this->logits_.assign(checked_size(this->config_.vocab_size, "vocab_size"), 0.0F);
    this->scores_.assign(checked_size(this->config_.max_positions, "max_positions"), 0.0F);
}

std::span<float> Gpt2ForwardWorkspace::hidden() noexcept {
    return this->hidden_;
}

std::span<float> Gpt2ForwardWorkspace::normed() noexcept {
    return this->normed_;
}

std::span<float> Gpt2ForwardWorkspace::query() noexcept {
    return this->query_;
}

std::span<float> Gpt2ForwardWorkspace::key() noexcept {
    return this->key_;
}

std::span<float> Gpt2ForwardWorkspace::value() noexcept {
    return this->value_;
}

std::span<float> Gpt2ForwardWorkspace::attention() noexcept {
    return this->attention_;
}

std::span<float> Gpt2ForwardWorkspace::projected() noexcept {
    return this->projected_;
}

std::span<float> Gpt2ForwardWorkspace::qkv_packed() noexcept {
    return this->qkv_packed_;
}

std::span<float> Gpt2ForwardWorkspace::mlp_fc() noexcept {
    return this->mlp_fc_;
}

std::span<float> Gpt2ForwardWorkspace::mlp_out() noexcept {
    return this->mlp_out_;
}

std::span<float> Gpt2ForwardWorkspace::logits() noexcept {
    return this->logits_;
}

std::span<float> Gpt2ForwardWorkspace::scores_prefix(std::int32_t count) {
    if (count < 0 || count > this->config_.max_positions) {
        throw std::runtime_error("GPT-2 attention scores count out of range");
    }
    return std::span<float>(this->scores_.data(), checked_size(count, "scores count"));
}

Gpt2KvCache::Gpt2KvCache(const Gpt2Config& config) : config_(config) {
    validate_config(this->config_);
    const std::size_t element_count =
        checked_size(this->config_.layer_count, "layer_count") *
        checked_size(this->config_.max_positions, "max_positions") *
        checked_size(this->config_.head_count, "head_count") *
        checked_size(head_dim(this->config_), "head_dim");
    this->keys_.assign(element_count, 0.0F);
    this->values_.assign(element_count, 0.0F);
    this->written_.assign(
        checked_size(this->config_.layer_count, "layer_count") *
            checked_size(this->config_.max_positions, "max_positions"),
        0);
}

void Gpt2KvCache::append(std::int32_t layer_id,
                         std::int32_t model_position,
                         std::span<const float> key,
                         std::span<const float> value) {
    const std::size_t hidden_size = checked_size(this->config_.hidden_size, "hidden_size");
    if (key.size() != hidden_size || value.size() != hidden_size) {
        throw std::runtime_error("GPT-2 KV key/value size mismatch");
    }
    this->validate_address(layer_id, model_position, 0);
    const std::int32_t local_head_dim = head_dim(this->config_);
    for (std::int32_t head = 0; head < this->config_.head_count; ++head) {
        const std::size_t source =
            checked_size(head, "head") * checked_size(local_head_dim, "head_dim");
        const std::size_t target = this->base_offset(layer_id, model_position, head);
        std::copy_n(key.begin() + static_cast<std::ptrdiff_t>(source),
                    local_head_dim,
                    this->keys_.begin() + static_cast<std::ptrdiff_t>(target));
        std::copy_n(value.begin() + static_cast<std::ptrdiff_t>(source),
                    local_head_dim,
                    this->values_.begin() + static_cast<std::ptrdiff_t>(target));
    }
    const std::size_t written_index =
        checked_size(layer_id, "layer_id") *
            checked_size(this->config_.max_positions, "max_positions") +
        checked_size(model_position, "model_position");
    this->written_[written_index] = 1;
    this->filled_tokens_ = std::max(this->filled_tokens_, model_position + 1);
}

std::span<const float> Gpt2KvCache::key(std::int32_t layer_id,
                                        std::int32_t model_position,
                                        std::int32_t head) const {
    this->validate_address(layer_id, model_position, head);
    this->validate_written(layer_id, model_position);
    return this->key_unchecked(layer_id, model_position, head);
}

std::span<const float> Gpt2KvCache::value(std::int32_t layer_id,
                                          std::int32_t model_position,
                                          std::int32_t head) const {
    this->validate_address(layer_id, model_position, head);
    this->validate_written(layer_id, model_position);
    return this->value_unchecked(layer_id, model_position, head);
}

void detail::attend_cached_position(const Gpt2KvCache& cache,
                                    std::int32_t layer_id,
                                    std::int32_t model_position,
                                    std::span<const float> query,
                                    std::span<float> scores,
                                    std::span<float> output) {
    cache.validate_address(layer_id, model_position, 0);
    cache.validate_written(layer_id, model_position);
    const std::int32_t local_head_dim = head_dim(cache.config_);
    const std::size_t hidden_size = checked_size(cache.config_.hidden_size, "hidden_size");
    const std::size_t local_head_dim_size = checked_size(local_head_dim, "head_dim");
    const std::size_t model_position_size =
        checked_size(model_position, "model_position");
    if (query.size() != hidden_size || output.size() != hidden_size ||
        scores.size() < model_position_size + 1) {
        throw std::runtime_error("GPT-2 cached attention workspace size mismatch");
    }

    const float score_scale = 1.0F / std::sqrt(static_cast<float>(local_head_dim));
    std::fill(output.begin(), output.end(), 0.0F);
    for (std::int32_t head = 0; head < cache.config_.head_count; ++head) {
        const std::size_t head_base =
            checked_size(head, "head") * local_head_dim_size;
        float max_score = LCQI_GPT2_SOFTMAX_NEGATIVE_INFINITY;
        for (std::size_t past = 0; past <= model_position_size; ++past) {
            const std::int32_t past_i32 = static_cast<std::int32_t>(past);
            const std::span<const float> key =
                cache.key_unchecked(layer_id, past_i32, head);
            float dot = 0.0F;
            for (std::size_t dim = 0; dim < local_head_dim_size; ++dim) {
                dot += query[head_base + dim] * key[dim];
            }
            const float score = dot * score_scale;
            scores[past] = score;
            max_score = std::max(max_score, score);
        }

        float denominator = 0.0F;
        for (std::size_t past = 0; past <= model_position_size; ++past) {
            scores[past] = std::exp(scores[past] - max_score);
            denominator += scores[past];
        }
        if (denominator <= 0.0F) {
            throw std::runtime_error("GPT-2 cached attention denominator is invalid");
        }

        for (std::size_t past = 0; past <= model_position_size; ++past) {
            const std::int32_t past_i32 = static_cast<std::int32_t>(past);
            const float probability = scores[past] / denominator;
            const std::span<const float> value =
                cache.value_unchecked(layer_id, past_i32, head);
            for (std::size_t dim = 0; dim < local_head_dim_size; ++dim) {
                output[head_base + dim] += probability * value[dim];
            }
        }
    }
}

std::span<const float> Gpt2KvCache::key_unchecked(std::int32_t layer_id,
                                                  std::int32_t model_position,
                                                  std::int32_t head) const noexcept {
    const std::size_t offset = this->base_offset_unchecked(layer_id, model_position, head);
    return std::span<const float>(
        this->keys_.data() + offset,
        static_cast<std::size_t>(head_dim(this->config_)));
}

std::span<const float> Gpt2KvCache::value_unchecked(std::int32_t layer_id,
                                                    std::int32_t model_position,
                                                    std::int32_t head) const noexcept {
    const std::size_t offset = this->base_offset_unchecked(layer_id, model_position, head);
    return std::span<const float>(
        this->values_.data() + offset,
        static_cast<std::size_t>(head_dim(this->config_)));
}

std::int32_t Gpt2KvCache::filled_tokens() const noexcept {
    return this->filled_tokens_;
}

std::size_t Gpt2KvCache::byte_size() const noexcept {
    return (this->keys_.size() + this->values_.size()) * sizeof(float) +
           this->written_.size() * sizeof(std::uint8_t);
}

std::size_t Gpt2KvCache::base_offset(std::int32_t layer_id,
                                     std::int32_t model_position,
                                     std::int32_t head) const {
    return (((checked_size(layer_id, "layer_id") *
              checked_size(this->config_.max_positions, "max_positions")) +
             checked_size(model_position, "model_position")) *
                checked_size(this->config_.head_count, "head_count") +
            checked_size(head, "head")) *
           checked_size(head_dim(this->config_), "head_dim");
}

std::size_t Gpt2KvCache::base_offset_unchecked(std::int32_t layer_id,
                                               std::int32_t model_position,
                                               std::int32_t head) const noexcept {
    return (((static_cast<std::size_t>(layer_id) *
              static_cast<std::size_t>(this->config_.max_positions)) +
             static_cast<std::size_t>(model_position)) *
                static_cast<std::size_t>(this->config_.head_count) +
            static_cast<std::size_t>(head)) *
           static_cast<std::size_t>(head_dim(this->config_));
}

void Gpt2KvCache::validate_address(std::int32_t layer_id,
                                   std::int32_t model_position,
                                   std::int32_t head) const {
    if (layer_id < 0 || layer_id >= this->config_.layer_count) {
        throw std::runtime_error("GPT-2 KV layer_id out of range");
    }
    if (model_position < 0 || model_position >= this->config_.max_positions) {
        throw std::runtime_error("GPT-2 KV model_position out of range");
    }
    if (head < 0 || head >= this->config_.head_count) {
        throw std::runtime_error("GPT-2 KV head out of range");
    }
}

void Gpt2KvCache::validate_written(std::int32_t layer_id,
                                   std::int32_t model_position) const {
    const std::size_t written_index =
        checked_size(layer_id, "layer_id") *
            checked_size(this->config_.max_positions, "max_positions") +
        checked_size(model_position, "model_position");
    if (written_index >= this->written_.size() || this->written_[written_index] == 0) {
        throw std::runtime_error("GPT-2 KV slot has not been written");
    }
}

Gpt2CachedGreedyDecoder::Gpt2CachedGreedyDecoder(const Gpt2ReferenceModel& model)
    : model_(&model),
      cache_(model.config),
      workspace_(model.config) {
    validate_model(*this->model_);
}

std::int32_t Gpt2CachedGreedyDecoder::step(std::int32_t token_id) {
    return run_gpt2_cached_step_unchecked(*this->model_,
                                          this->cache_,
                                          this->workspace_,
                                          token_id,
                                          false)
        .predicted_token;
}

Gpt2ForwardResult Gpt2CachedGreedyDecoder::step_with_logits(std::int32_t token_id) {
    return run_gpt2_cached_step_unchecked(*this->model_,
                                          this->cache_,
                                          this->workspace_,
                                          token_id,
                                          true);
}

const Gpt2KvCache& Gpt2CachedGreedyDecoder::cache() const noexcept {
    return this->cache_;
}

std::int32_t Gpt2CachedGreedyDecoder::filled_tokens() const noexcept {
    return this->cache_.filled_tokens();
}

std::size_t Gpt2CachedGreedyDecoder::kv_cache_bytes() const noexcept {
    return this->cache_.byte_size();
}

Gpt2ForwardResult run_gpt2_forward(const Gpt2ReferenceModel& model,
                                   std::span<const std::int32_t> token_ids) {
    validate_model(model);
    if (token_ids.empty()) {
        throw std::runtime_error("GPT-2 forward needs at least one token");
    }
    if (token_ids.size() > checked_size(model.config.max_positions, "max_positions")) {
        throw std::runtime_error("GPT-2 token_ids exceed max_positions");
    }

    const std::int32_t sequence_length = static_cast<std::int32_t>(token_ids.size());
    std::vector<float> hidden_by_pos(matrix_size(sequence_length, model.config.hidden_size), 0.0F);
    for (std::int32_t position = 0; position < sequence_length; ++position) {
        const std::int32_t token_id = token_ids[checked_size(position, "position")];
        if (token_id < 0 || token_id >= model.config.vocab_size) {
            throw std::runtime_error("GPT-2 token id out of range");
        }
        const std::span<const float> token =
            row_span(model.token_embedding, token_id, model.config.hidden_size);
        const std::span<const float> position_embedding =
            row_span(model.position_embedding, position, model.config.hidden_size);
        std::span<float> hidden = std::span<float>(
            hidden_by_pos.data() + matrix_size(position, model.config.hidden_size),
            checked_size(model.config.hidden_size, "hidden_size"));
        for (std::int32_t dim = 0; dim < model.config.hidden_size; ++dim) {
            hidden[checked_size(dim, "dim")] =
                token[checked_size(dim, "dim")] + position_embedding[checked_size(dim, "dim")];
        }
    }

    for (const Gpt2LayerWeightsF32& layer : model.layers) {
        forward_gpt2_layer(model.config, layer, sequence_length, hidden_by_pos);
    }

    std::vector<float> normed(checked_size(model.config.hidden_size, "hidden_size"), 0.0F);
    const std::span<const float> final_hidden = std::span<const float>(
        hidden_by_pos.data() + matrix_size(sequence_length - 1, model.config.hidden_size),
        checked_size(model.config.hidden_size, "hidden_size"));
    layer_norm(final_hidden,
               model.final_ln_weight,
               model.final_ln_bias,
               model.config.layer_norm_epsilon,
               normed);

    Gpt2ForwardResult result;
    result.logits.assign(checked_size(model.config.vocab_size, "vocab_size"), 0.0F);
    compute_logits(model, normed, result.logits);
    result.predicted_token = argmax(result.logits);
    return result;
}

Gpt2ForwardResult run_gpt2_forward_cached(const Gpt2ReferenceModel& model,
                                          Gpt2KvCache& cache,
                                          std::int32_t token_id) {
    validate_model(model);
    Gpt2ForwardWorkspace workspace(model.config);
    return run_gpt2_cached_step_unchecked(model, cache, workspace, token_id, true);
}

std::vector<std::int32_t> gpt2_generate_greedy(const Gpt2ReferenceModel& model,
                                               std::span<const std::int32_t> prompt_token_ids,
                                               std::int32_t max_new_tokens) {
    if (max_new_tokens < 0) {
        throw std::runtime_error("max_new_tokens cannot be negative");
    }
    std::vector<std::int32_t> tokens(prompt_token_ids.begin(), prompt_token_ids.end());
    for (std::int32_t step = 0; step < max_new_tokens; ++step) {
        if (tokens.size() >= checked_size(model.config.max_positions, "max_positions")) {
            throw std::runtime_error("GPT-2 generation would exceed max_positions");
        }
        const Gpt2ForwardResult result = run_gpt2_forward(model, tokens);
        tokens.push_back(result.predicted_token);
        if (model.config.eos_token_id >= 0 && result.predicted_token == model.config.eos_token_id) {
            break;
        }
    }
    return tokens;
}

std::vector<std::int32_t> gpt2_generate_greedy_cached(
    const Gpt2ReferenceModel& model,
    std::span<const std::int32_t> prompt_token_ids,
    std::int32_t max_new_tokens) {
    if (prompt_token_ids.empty()) {
        throw std::runtime_error("GPT-2 generation needs at least one prompt token");
    }
    if (max_new_tokens < 0) {
        throw std::runtime_error("max_new_tokens cannot be negative");
    }
    if (prompt_token_ids.size() > checked_size(model.config.max_positions, "max_positions")) {
        throw std::runtime_error("GPT-2 prompt exceeds max_positions");
    }

    Gpt2CachedGreedyDecoder decoder(model);
    std::vector<std::int32_t> tokens(prompt_token_ids.begin(), prompt_token_ids.end());
    if (max_new_tokens == 0) {
        return tokens;
    }
    std::int32_t predicted_token = 0;
    for (const std::int32_t token_id : prompt_token_ids) {
        predicted_token = decoder.step(token_id);
    }
    for (std::int32_t step = 0; step < max_new_tokens; ++step) {
        if (tokens.size() >= checked_size(model.config.max_positions, "max_positions")) {
            throw std::runtime_error("GPT-2 generation would exceed max_positions");
        }
        tokens.push_back(predicted_token);
        if (model.config.eos_token_id >= 0 && predicted_token == model.config.eos_token_id) {
            break;
        }
        if (step + 1 < max_new_tokens) {
            predicted_token = decoder.step(predicted_token);
        }
    }
    return tokens;
}

Gpt2ReferenceModel make_tiny_gpt2_reference_model() {
    Gpt2ReferenceModel model;
    model.config.hidden_size = 4;
    model.config.head_count = 2;
    model.config.layer_count = 1;
    model.config.max_positions = 8;
    model.config.vocab_size = 6;
    model.config.intermediate_size = 8;
    model.config.bos_token_id = 0;
    model.config.eos_token_id = 5;
    model.config.layer_norm_epsilon = 1.0e-5F;
    model.config.activation_function = "gelu_new";

    model.token_embedding = {
        0.20F, -0.10F, 0.00F, 0.10F,
        -0.10F, 0.30F, 0.20F, -0.20F,
        0.05F, 0.10F, -0.30F, 0.25F,
        0.40F, -0.20F, 0.15F, 0.05F,
        -0.30F, 0.05F, 0.35F, -0.15F,
        0.10F, 0.20F, -0.10F, 0.30F,
    };
    model.position_embedding = {
        0.01F, 0.02F, 0.03F, 0.04F,
        -0.02F, 0.01F, 0.00F, 0.03F,
        0.03F, -0.01F, 0.02F, 0.00F,
        0.00F, 0.02F, -0.02F, 0.01F,
        0.02F, 0.00F, 0.01F, -0.01F,
        -0.01F, 0.03F, 0.00F, 0.02F,
        0.01F, -0.02F, 0.03F, 0.00F,
        0.00F, 0.01F, -0.01F, 0.02F,
    };

    Gpt2LayerWeightsF32 layer;
    layer.ln_1_weight = ones(4);
    layer.ln_1_bias = zeros(4);
    layer.ln_2_weight = {0.9F, 1.1F, 1.0F, 0.95F};
    layer.ln_2_bias = {0.01F, -0.02F, 0.00F, 0.03F};
    layer.c_attn = make_linear(
        4,
        12,
        {
            0.10F, -0.20F, 0.05F, 0.15F,
            -0.05F, 0.12F, 0.20F, -0.10F,
            0.18F, 0.02F, -0.08F, 0.11F,
            -0.12F, 0.06F, 0.14F, 0.04F,
            0.07F, 0.16F, -0.09F, 0.03F,
            -0.15F, 0.05F, 0.13F, 0.10F,
            0.04F, -0.11F, 0.19F, -0.02F,
            0.09F, 0.08F, -0.06F, 0.17F,
            0.20F, -0.04F, 0.01F, 0.06F,
            -0.08F, 0.15F, 0.07F, -0.03F,
            0.05F, 0.10F, -0.14F, 0.12F,
            0.11F, -0.07F, 0.16F, 0.02F,
        },
        {
            0.01F, -0.02F, 0.00F, 0.03F,
            -0.01F, 0.02F, 0.01F, -0.02F,
            0.00F, 0.01F, -0.01F, 0.02F,
        });
    layer.c_proj = make_linear(
        4,
        4,
        {
            0.10F, 0.05F, -0.08F, 0.12F,
            -0.04F, 0.11F, 0.09F, -0.02F,
            0.07F, -0.10F, 0.13F, 0.05F,
            0.02F, 0.14F, -0.06F, 0.08F,
        },
        {0.01F, 0.00F, -0.01F, 0.02F});
    layer.c_fc = make_linear(
        4,
        8,
        {
            0.20F, -0.10F, 0.05F, 0.03F,
            -0.05F, 0.14F, 0.12F, -0.08F,
            0.09F, 0.07F, -0.11F, 0.16F,
            -0.12F, 0.05F, 0.18F, 0.02F,
            0.04F, -0.09F, 0.13F, 0.10F,
            0.11F, 0.03F, -0.07F, 0.15F,
            -0.02F, 0.17F, 0.06F, -0.04F,
            0.08F, -0.06F, 0.10F, 0.12F,
        },
        {0.01F, -0.02F, 0.00F, 0.03F, -0.01F, 0.02F, 0.01F, 0.00F});
    layer.mlp_c_proj = make_linear(
        8,
        4,
        {
            0.10F, -0.08F, 0.06F, 0.12F, -0.04F, 0.07F, 0.03F, 0.11F,
            -0.05F, 0.13F, 0.02F, -0.09F, 0.15F, 0.04F, -0.03F, 0.08F,
            0.07F, 0.01F, -0.12F, 0.10F, 0.06F, -0.05F, 0.14F, 0.02F,
            0.03F, 0.09F, 0.11F, -0.06F, 0.05F, 0.12F, -0.08F, 0.04F,
        },
        {0.00F, 0.01F, -0.01F, 0.02F});
    model.layers.push_back(std::move(layer));
    model.final_ln_weight = {1.0F, 0.95F, 1.05F, 1.0F};
    model.final_ln_bias = {0.0F, 0.01F, -0.01F, 0.02F};
    model.tie_lm_head_to_embedding = true;
    validate_model(model);
    return model;
}

}  // namespace lcqi

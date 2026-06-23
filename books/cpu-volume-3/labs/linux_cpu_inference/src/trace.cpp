#include <lcqi/reference_decoder.hpp>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <span>
#include <sstream>
#include <string>
#include <string_view>

namespace {

constexpr std::array<std::int32_t, 3> LCQI_TRACE_TOKEN_IDS{1, 2, 3};
constexpr std::string_view LCQI_TRACE_PROMPT = "tok1 tok2 tok3";
constexpr std::int32_t LCQI_TRACE_METADATA_LAYER = -1;
constexpr std::size_t LCQI_TRACE_VALUE_LIMIT = 8;

std::span<const std::int32_t> tokenize_prompt_fixture(std::string_view prompt) {
    if (prompt != LCQI_TRACE_PROMPT) {
        throw std::runtime_error("unsupported trace prompt");
    }
    return LCQI_TRACE_TOKEN_IDS;
}

std::string join_ints(std::span<const std::int32_t> values) {
    std::ostringstream stream;
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index != 0) {
            stream << 'x';
        }
        stream << values[index];
    }
    return stream.str();
}

std::string join_int_values(std::span<const std::int32_t> values) {
    std::ostringstream stream;
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index != 0) {
            stream << ';';
        }
        stream << values[index];
    }
    return stream.str();
}

std::int32_t checksum_ints(std::span<const std::int32_t> values) {
    std::int32_t result = 0;
    for (const std::int32_t value : values) {
        result += value;
    }
    return result;
}

std::int32_t max_abs_ints(std::span<const std::int32_t> values) {
    std::int32_t result = 0;
    for (const std::int32_t value : values) {
        result = std::max(result, std::abs(value));
    }
    return result;
}

std::string join_floats(std::span<const float> values) {
    std::ostringstream stream;
    const std::size_t count = std::min(values.size(), LCQI_TRACE_VALUE_LIMIT);
    for (std::size_t index = 0; index < count; ++index) {
        if (index != 0) {
            stream << ';';
        }
        stream << values[index];
    }
    if (values.size() > LCQI_TRACE_VALUE_LIMIT) {
        stream << ";...";
    }
    return stream.str();
}

void print_trace_csv(const lcqi::ReferenceDecodeTraceResult& trace) {
    std::cout << "name,token_position,layer_id,shape,stride,dtype,layout,checksum,max_abs,values\n";
    const std::span<const std::int32_t> tokens = tokenize_prompt_fixture(LCQI_TRACE_PROMPT);
    std::cout << "prompt,0," << LCQI_TRACE_METADATA_LAYER
              << ",scalar,scalar,utf8,text,0,0," << LCQI_TRACE_PROMPT << '\n';
    std::cout << "tokenizer,0," << LCQI_TRACE_METADATA_LAYER
              << ',' << tokens.size() << ",1,i32,contiguous,"
              << checksum_ints(tokens) << ','
              << max_abs_ints(tokens) << ','
              << join_int_values(tokens) << '\n';
    for (const lcqi::ReferenceTensorCheckpoint& checkpoint : trace.checkpoints) {
        std::cout << checkpoint.name << ','
                  << checkpoint.token_position << ','
                  << checkpoint.layer_id << ','
                  << join_ints(checkpoint.shape) << ','
                  << join_ints(checkpoint.stride) << ','
                  << checkpoint.dtype << ','
                  << checkpoint.layout << ','
                  << checkpoint.checksum << ','
                  << checkpoint.max_abs << ','
                  << join_floats(checkpoint.values) << '\n';
    }
    std::cout << "sampler_argmax,"
              << LCQI_TRACE_TOKEN_IDS.size() - 1 << ','
              << LCQI_TRACE_METADATA_LAYER
              << ',' << trace.result.logits.size() << ",1,f32,argmax,"
              << "0,0,token=" << trace.result.predicted_token << '\n';
    std::cout << "generated_token,"
              << LCQI_TRACE_TOKEN_IDS.size() << ','
              << LCQI_TRACE_METADATA_LAYER
              << ",scalar,scalar,i32,generated,"
              << trace.result.predicted_token << ','
              << trace.result.predicted_token << ','
              << trace.result.predicted_token << '\n';
}

}  // namespace

int main() {
    try {
        const lcqi::ReferenceDecoderModel model = lcqi::make_tiny_reference_decoder_model();
        const std::span<const std::int32_t> tokens = tokenize_prompt_fixture(LCQI_TRACE_PROMPT);
        const lcqi::ReferenceDecodeTraceResult trace =
            lcqi::run_reference_decode_with_trace(model, tokens);
        print_trace_csv(trace);
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}

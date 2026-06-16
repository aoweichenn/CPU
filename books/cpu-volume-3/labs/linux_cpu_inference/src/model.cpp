#include <lcqi/model.hpp>

#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>

namespace lcqi {
namespace {

constexpr std::int32_t LCQI_INT8_MIN_VALUE = -128;
constexpr std::int32_t LCQI_INT8_MAX_VALUE = 127;

std::string read_key(std::istream& input) {
    std::string key;
    if (!(input >> key)) {
        throw std::runtime_error("unexpected end of model file");
    }
    return key;
}

void expect_key(std::istream& input, const std::string& expected) {
    const std::string key = read_key(input);
    if (key != expected) {
        throw std::runtime_error("expected key '" + expected + "', got '" + key + "'");
    }
}

std::int32_t read_i32(std::istream& input, const std::string& key) {
    expect_key(input, key);
    std::int32_t value = 0;
    if (!(input >> value)) {
        throw std::runtime_error("invalid int32 value for key '" + key + "'");
    }
    return value;
}

float read_float(std::istream& input, const std::string& key) {
    expect_key(input, key);
    float value = 0.0F;
    if (!(input >> value)) {
        throw std::runtime_error("invalid float value for key '" + key + "'");
    }
    return value;
}

std::vector<std::int8_t> read_i8_vector(std::istream& input,
                                        const std::string& key,
                                        std::int32_t count) {
    expect_key(input, key);
    if (count < 0) {
        throw std::runtime_error("negative vector size for key '" + key + "'");
    }
    std::vector<std::int8_t> values(static_cast<std::size_t>(count));
    for (std::int32_t i = 0; i < count; ++i) {
        std::int32_t raw = 0;
        if (!(input >> raw)) {
            throw std::runtime_error("invalid int8 payload for key '" + key + "'");
        }
        if (raw < LCQI_INT8_MIN_VALUE || raw > LCQI_INT8_MAX_VALUE) {
            throw std::runtime_error("int8 payload out of range for key '" + key + "'");
        }
        values[static_cast<std::size_t>(i)] = static_cast<std::int8_t>(raw);
    }
    return values;
}

std::vector<float> read_float_vector(std::istream& input,
                                     const std::string& key,
                                     std::int32_t count) {
    expect_key(input, key);
    if (count < 0) {
        throw std::runtime_error("negative vector size for key '" + key + "'");
    }
    std::vector<float> values(static_cast<std::size_t>(count));
    for (std::int32_t i = 0; i < count; ++i) {
        if (!(input >> values[static_cast<std::size_t>(i)])) {
            throw std::runtime_error("invalid float payload for key '" + key + "'");
        }
    }
    return values;
}

void validate_layer(const QuantizedLinearLayer& layer, const std::string& name) {
    if (layer.input_size <= 0 || layer.output_size <= 0) {
        throw std::runtime_error(name + " layer has invalid shape");
    }
    const std::size_t expected_weights =
        static_cast<std::size_t>(layer.input_size) *
        static_cast<std::size_t>(layer.output_size);
    if (layer.weights.size() != expected_weights) {
        throw std::runtime_error(name + " layer weight size mismatch");
    }
    if (layer.bias.size() != static_cast<std::size_t>(layer.output_size)) {
        throw std::runtime_error(name + " layer bias size mismatch");
    }
}

std::int32_t checked_element_count(std::int32_t rows,
                                   std::int32_t columns,
                                   const std::string& name) {
    if (rows <= 0 || columns <= 0) {
        throw std::runtime_error(name + " layer has invalid shape");
    }
    const std::int64_t count =
        static_cast<std::int64_t>(rows) * static_cast<std::int64_t>(columns);
    if (count > static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max())) {
        throw std::runtime_error(name + " layer element count is too large");
    }
    return static_cast<std::int32_t>(count);
}

}  // namespace

TinyMlpModel load_model(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("cannot open model file: " + path.string());
    }

    expect_key(input, "LCQI_MODEL_V1");

    TinyMlpModel model;
    model.input_size = read_i32(input, "input_size");
    model.hidden_size = read_i32(input, "hidden_size");
    model.output_size = read_i32(input, "output_size");

    const std::int32_t hidden_weight_count =
        checked_element_count(model.input_size, model.hidden_size, "hidden");
    const std::int32_t output_weight_count =
        checked_element_count(model.hidden_size, model.output_size, "output");

    model.hidden.input_size = model.input_size;
    model.hidden.output_size = model.hidden_size;
    model.hidden.scale = read_float(input, "hidden_scale");
    model.hidden.weights = read_i8_vector(
        input,
        "hidden_weights",
        hidden_weight_count);
    model.hidden.bias = read_float_vector(input, "hidden_bias", model.hidden_size);

    model.output.input_size = model.hidden_size;
    model.output.output_size = model.output_size;
    model.output.scale = read_float(input, "output_scale");
    model.output.weights = read_i8_vector(
        input,
        "output_weights",
        output_weight_count);
    model.output.bias = read_float_vector(input, "output_bias", model.output_size);

    validate_layer(model.hidden, "hidden");
    validate_layer(model.output, "output");
    return model;
}

}  // namespace lcqi

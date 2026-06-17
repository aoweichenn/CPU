#include <lcqi/inference.hpp>
#include <lcqi/model.hpp>

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <span>
#include <stdexcept>
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

}  // namespace

int main() {
    try {
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

        std::cout << "lcqi tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "lcqi test failure: " << error.what() << "\n";
        return EXIT_FAILURE;
    }
}

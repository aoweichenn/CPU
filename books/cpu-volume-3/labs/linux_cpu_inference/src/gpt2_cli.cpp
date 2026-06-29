#include <lcqi/gpt2_reference.hpp>

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr int LCQI_GPT2_TINY_ARGC = 1;
constexpr int LCQI_GPT2_MIN_REAL_ARGC = 3;
constexpr std::int32_t LCQI_GPT2_DEFAULT_MAX_NEW_TOKENS = 8;
constexpr std::int32_t LCQI_GPT2_TINY_PROMPT_0 = 1;
constexpr std::int32_t LCQI_GPT2_TINY_PROMPT_1 = 2;
constexpr std::int32_t LCQI_GPT2_TINY_PROMPT_2 = 3;

void print_ids(std::span<const std::int32_t> ids) {
    for (std::size_t index = 0; index < ids.size(); ++index) {
        if (index != 0) {
            std::cout << ' ';
        }
        std::cout << ids[index];
    }
}

std::int32_t parse_max_new_tokens(int argc, char** argv) {
    if (argc <= 3) {
        return LCQI_GPT2_DEFAULT_MAX_NEW_TOKENS;
    }
    const std::int32_t value = static_cast<std::int32_t>(std::stoi(argv[3]));
    if (value < 0) {
        throw std::runtime_error("max_new_tokens cannot be negative");
    }
    return value;
}

int run_tiny_mode() {
    const lcqi::Gpt2ReferenceModel model = lcqi::make_tiny_gpt2_reference_model();
    const std::vector<std::int32_t> prompt{
        LCQI_GPT2_TINY_PROMPT_0,
        LCQI_GPT2_TINY_PROMPT_1,
        LCQI_GPT2_TINY_PROMPT_2,
    };
    const lcqi::Gpt2ForwardResult result = lcqi::run_gpt2_forward(model, prompt);
    const std::vector<std::int32_t> generated =
        lcqi::gpt2_generate_greedy(model, prompt, 2);
    std::cout << "mode tiny\n";
    std::cout << "prompt_ids ";
    print_ids(prompt);
    std::cout << "\n";
    std::cout << "predicted_token " << result.predicted_token << "\n";
    std::cout << "logits";
    for (const float value : result.logits) {
        std::cout << ' ' << value;
    }
    std::cout << "\n";
    std::cout << "generated_ids ";
    print_ids(generated);
    std::cout << "\n";
    return EXIT_SUCCESS;
}

int run_real_model_mode(int argc, char** argv) {
    if (argc < LCQI_GPT2_MIN_REAL_ARGC) {
        throw std::runtime_error(
            "usage: lcqi_gpt2 [model_dir prompt [max_new_tokens]]");
    }
    const std::filesystem::path model_dir = argv[1];
    const std::string prompt = argv[2];
    const std::int32_t max_new_tokens = parse_max_new_tokens(argc, argv);

    const lcqi::Gpt2ReferenceModel model = lcqi::load_gpt2_from_directory(model_dir);
    const lcqi::Gpt2Tokenizer tokenizer = lcqi::load_gpt2_tokenizer(
        model_dir / "vocab.json",
        model_dir / "merges.txt",
        model.config.bos_token_id,
        model.config.eos_token_id);
    const std::vector<std::int32_t> prompt_ids = lcqi::gpt2_encode(tokenizer, prompt);
    const std::vector<std::int32_t> generated =
        lcqi::gpt2_generate_greedy(model, prompt_ids, max_new_tokens);

    std::cout << "mode gpt2_directory\n";
    std::cout << "model_dir " << model_dir.string() << "\n";
    std::cout << "prompt_ids ";
    print_ids(prompt_ids);
    std::cout << "\n";
    std::cout << "generated_ids ";
    print_ids(generated);
    std::cout << "\n";
    std::cout << "generated_text " << lcqi::gpt2_decode(tokenizer, generated) << "\n";
    return EXIT_SUCCESS;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc == LCQI_GPT2_TINY_ARGC) {
            return run_tiny_mode();
        }
        return run_real_model_mode(argc, argv);
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << "\n";
        return EXIT_FAILURE;
    }
}

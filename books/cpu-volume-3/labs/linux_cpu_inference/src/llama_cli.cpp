#include <lcqi/llama_gguf.hpp>

#include <chrono>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::int32_t LCQI_LLAMA_DEFAULT_MAX_NEW_TOKENS = 1;
constexpr std::string_view LCQI_LLAMA_IDS_PREFIX = "--ids=";
constexpr std::string_view LCQI_LLAMA_PROMPT_PREFIX = "--prompt=";
constexpr std::string_view LCQI_LLAMA_MAX_NEW_PREFIX = "--max-new=";

using Clock = std::chrono::steady_clock;

struct LlamaCliOptions {
    std::filesystem::path gguf_path;
    std::string prompt;
    std::vector<std::int32_t> ids;
    std::int32_t max_new_tokens = LCQI_LLAMA_DEFAULT_MAX_NEW_TOKENS;
    bool use_ids = false;
    bool benchmark = false;
    bool decode_text = false;
};

[[nodiscard]] double elapsed_ms(Clock::time_point begin, Clock::time_point end) {
    return std::chrono::duration<double, std::milli>(end - begin).count();
}

void print_ids(std::string_view label, const std::vector<std::int32_t>& ids) {
    std::cout << label;
    for (const std::int32_t id : ids) {
        std::cout << ' ' << id;
    }
    std::cout << "\n";
}

[[nodiscard]] std::int32_t parse_non_negative_i32(const std::string& text,
                                                  const char* name) {
    const int value = std::stoi(text);
    if (value < 0) {
        throw std::runtime_error(std::string(name) + " cannot be negative");
    }
    return static_cast<std::int32_t>(value);
}

[[nodiscard]] std::vector<std::int32_t> parse_ids(std::string_view text) {
    std::vector<std::int32_t> ids;
    std::string normalized(text);
    for (char& ch : normalized) {
        if (ch == ',') {
            ch = ' ';
        }
    }
    std::istringstream input(normalized);
    std::int32_t id = 0;
    while (input >> id) {
        if (id < 0) {
            throw std::runtime_error("token ids cannot be negative");
        }
        ids.push_back(id);
    }
    if (ids.empty()) {
        throw std::runtime_error("--ids requires at least one token id");
    }
    return ids;
}

[[nodiscard]] bool consume_prefix(std::string_view arg,
                                  std::string_view prefix,
                                  std::string_view& value) {
    if (arg.substr(0, prefix.size()) != prefix) {
        return false;
    }
    value = arg.substr(prefix.size());
    return true;
}

[[nodiscard]] LlamaCliOptions parse_options(int argc, char** argv) {
    if (argc < 2) {
        throw std::runtime_error(
            "usage: lcqi_llama_gguf model.gguf [--prompt TEXT|--ids 1,2,3] "
            "[--max-new N] [--benchmark] [--decode-text]");
    }
    LlamaCliOptions options;
    options.gguf_path = argv[1];
    for (int index = 2; index < argc; ++index) {
        const std::string arg = argv[index];
        std::string_view value;
        if (arg == "--benchmark") {
            options.benchmark = true;
        } else if (arg == "--decode-text") {
            options.decode_text = true;
        } else if (arg == "--ids") {
            if (index + 1 >= argc) {
                throw std::runtime_error("--ids requires a value");
            }
            ++index;
            options.ids = parse_ids(argv[index]);
            options.use_ids = true;
        } else if (consume_prefix(arg, LCQI_LLAMA_IDS_PREFIX, value)) {
            options.ids = parse_ids(value);
            options.use_ids = true;
        } else if (arg == "--prompt") {
            if (index + 1 >= argc) {
                throw std::runtime_error("--prompt requires a value");
            }
            ++index;
            options.prompt = argv[index];
            options.use_ids = false;
        } else if (consume_prefix(arg, LCQI_LLAMA_PROMPT_PREFIX, value)) {
            options.prompt = std::string(value);
            options.use_ids = false;
        } else if (arg == "--max-new") {
            if (index + 1 >= argc) {
                throw std::runtime_error("--max-new requires a value");
            }
            ++index;
            options.max_new_tokens = parse_non_negative_i32(argv[index], "--max-new");
        } else if (consume_prefix(arg, LCQI_LLAMA_MAX_NEW_PREFIX, value)) {
            options.max_new_tokens =
                parse_non_negative_i32(std::string(value), "--max-new");
        } else {
            throw std::runtime_error("unknown option: " + arg);
        }
    }
    if (!options.use_ids && options.prompt.empty()) {
        options.prompt = "Hello";
    }
    return options;
}

[[nodiscard]] std::vector<std::int32_t> make_prompt_ids(const LlamaCliOptions& options,
                                                        lcqi::Gpt2Tokenizer* tokenizer,
                                                        std::int32_t bos_token_id) {
    if (options.use_ids) {
        return options.ids;
    }
    return lcqi::llama_gguf_chat_prompt_ids(*tokenizer, options.prompt, bos_token_id);
}

void print_benchmark(const lcqi::LlamaGgufLoadedModel& loaded,
                     const lcqi::LlamaGgufGenerationResult& result,
                     std::size_t prompt_size,
                     std::size_t generated_new_tokens) {
    const double first_token_tokens_per_second =
        result.prefill_ms > 0.0 ? 1000.0 / result.prefill_ms : 0.0;
    const double decode_tokens_per_second =
        result.decode_ms > 0.0
            ? static_cast<double>(result.decode_steps) * 1000.0 / result.decode_ms
            : 0.0;
    std::cout << "benchmark_manifest_ms " << loaded.report.manifest_ms << "\n";
    std::cout << "benchmark_weight_load_ms " << loaded.report.weights_ms << "\n";
    std::cout << "benchmark_load_ms " << result.load_ms << "\n";
    std::cout << "benchmark_prefill_ms " << result.prefill_ms << "\n";
    std::cout << "benchmark_decode_ms " << result.decode_ms << "\n";
    std::cout << "benchmark_total_ms " << result.total_ms << "\n";
    std::cout << "benchmark_prompt_tokens " << prompt_size << "\n";
    std::cout << "benchmark_generated_tokens " << generated_new_tokens << "\n";
    std::cout << "benchmark_prefill_steps " << result.prefill_steps << "\n";
    std::cout << "benchmark_decode_steps " << result.decode_steps << "\n";
    std::cout << "benchmark_first_token_tokens_per_second "
              << first_token_tokens_per_second << "\n";
    std::cout << "benchmark_decode_tokens_per_second " << decode_tokens_per_second << "\n";
    std::cout << "benchmark_kv_cache_bytes " << result.kv_cache_bytes << "\n";
    std::cout << "benchmark_quantized_weight_bytes "
              << loaded.report.quantized_weight_bytes << "\n";
    std::cout << "benchmark_f32_weight_bytes " << loaded.report.f32_weight_bytes << "\n";
    std::cout << "benchmark_tensors_loaded " << loaded.report.tensors_loaded << "\n";
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const Clock::time_point total_begin = Clock::now();
        const LlamaCliOptions options = parse_options(argc, argv);
        const Clock::time_point load_begin = Clock::now();
        lcqi::LlamaGgufLoadedModel loaded =
            lcqi::load_llama_gguf_reference_model(options.gguf_path);
        const double load_ms = elapsed_ms(load_begin, Clock::now());

        lcqi::Gpt2Tokenizer tokenizer;
        if (!options.use_ids || options.decode_text) {
            tokenizer = lcqi::load_gpt2_tokenizer_from_gguf(options.gguf_path);
        }
        const std::vector<std::int32_t> prompt_ids =
            make_prompt_ids(options, &tokenizer, loaded.model.bos_token_id);
        lcqi::LlamaGgufGenerationResult result =
            lcqi::llama_gguf_generate_greedy(loaded.model,
                                             prompt_ids,
                                             options.max_new_tokens);
        result.load_ms = load_ms;
        result.total_ms = elapsed_ms(total_begin, Clock::now());

        std::cout << "mode llama_gguf_reference\n";
        std::cout << "weight_execution f32_dequantized_reference\n";
        std::cout << "model_path " << options.gguf_path.string() << "\n";
        std::cout << "architecture " << loaded.model.architecture << "\n";
        std::cout << "model_name " << loaded.model.name << "\n";
        std::cout << "layers " << loaded.model.decoder.layers.size() << "\n";
        std::cout << "hidden_size " << loaded.model.decoder.config.hidden_size << "\n";
        std::cout << "query_heads " << loaded.model.decoder.config.query_heads << "\n";
        std::cout << "kv_heads " << loaded.model.decoder.config.kv_heads << "\n";
        std::cout << "head_dim " << loaded.model.decoder.config.head_dim << "\n";
        std::cout << "vocab_size " << loaded.model.decoder.config.vocab_size << "\n";
        std::cout << "tie_lm_head_to_embedding "
                  << (loaded.model.lm_head_tied_to_embedding ? 1 : 0) << "\n";
        print_ids("prompt_ids", result.prompt_ids);
        print_ids("generated_ids", result.generated_ids);
        std::cout << "predicted_first_token " << result.predicted_first_token << "\n";
        if (options.decode_text) {
            std::cout << "generated_text "
                      << lcqi::gpt2_decode(tokenizer, result.generated_ids) << "\n";
        }
        if (options.benchmark) {
            const std::size_t generated_new_tokens =
                result.generated_ids.size() >= result.prompt_ids.size()
                    ? result.generated_ids.size() - result.prompt_ids.size()
                    : 0;
            print_benchmark(loaded, result, result.prompt_ids.size(), generated_new_tokens);
        }
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << "\n";
        return EXIT_FAILURE;
    }
}

#include <lcqi/gpt2_reference.hpp>

#include <chrono>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr int LCQI_GPT2_TINY_ARGC = 1;
constexpr int LCQI_GPT2_MIN_REAL_ARGC = 3;
constexpr std::int32_t LCQI_GPT2_DEFAULT_MAX_NEW_TOKENS = 8;
constexpr std::int32_t LCQI_GPT2_TINY_PROMPT_0 = 1;
constexpr std::int32_t LCQI_GPT2_TINY_PROMPT_1 = 2;
constexpr std::int32_t LCQI_GPT2_TINY_PROMPT_2 = 3;

enum class Gpt2EngineMode {
    full_prefix,
    cached_kv,
};

struct Gpt2CliOptions {
    Gpt2EngineMode engine = Gpt2EngineMode::cached_kv;
    bool benchmark = false;
    bool profile_hotspots = false;
    std::filesystem::path model_dir;
    std::string prompt;
    std::int32_t max_new_tokens = LCQI_GPT2_DEFAULT_MAX_NEW_TOKENS;
    std::int32_t worker_count = 0;
};

struct Gpt2BenchmarkTimings {
    double load_ms = 0.0;
    double tokenize_ms = 0.0;
    double prefill_ms = 0.0;
    double decode_ms = 0.0;
    double generate_ms = 0.0;
    double total_ms = 0.0;
    std::size_t prefill_steps = 0;
    std::size_t decode_steps = 0;
    std::size_t kv_cache_bytes = 0;
    std::int32_t worker_count = 1;
};

using Clock = std::chrono::steady_clock;

void print_ids(std::span<const std::int32_t> ids) {
    for (std::size_t index = 0; index < ids.size(); ++index) {
        if (index != 0) {
            std::cout << ' ';
        }
        std::cout << ids[index];
    }
}

[[nodiscard]] double elapsed_ms(Clock::time_point start, Clock::time_point end) {
    return std::chrono::duration<double, std::milli>(end - start).count();
}

[[nodiscard]] std::size_t checked_size(std::int64_t value, const char* name) {
    if (value < 0) {
        throw std::runtime_error(std::string(name) + " cannot be negative");
    }
    return static_cast<std::size_t>(value);
}

[[nodiscard]] const char* engine_name(Gpt2EngineMode engine) {
    switch (engine) {
        case Gpt2EngineMode::full_prefix:
            return "full_prefix";
        case Gpt2EngineMode::cached_kv:
            return "cached_kv";
    }
    return "unknown";
}

[[nodiscard]] Gpt2EngineMode parse_engine(std::string_view value) {
    if (value == "full" || value == "full_prefix") {
        return Gpt2EngineMode::full_prefix;
    }
    if (value == "cached" || value == "cached_kv") {
        return Gpt2EngineMode::cached_kv;
    }
    throw std::runtime_error("unknown --engine value, expected cached or full");
}

[[nodiscard]] std::int32_t parse_non_negative_i32(const std::string& text,
                                                  const char* name) {
    const std::int32_t value = static_cast<std::int32_t>(std::stoi(text));
    if (value < 0) {
        throw std::runtime_error(std::string(name) + " cannot be negative");
    }
    return value;
}

[[nodiscard]] Gpt2CliOptions parse_options(int argc, char** argv) {
    Gpt2CliOptions options;
    std::vector<std::string> positional;
    for (int index = 1; index < argc; ++index) {
        const std::string arg = argv[index];
        if (arg == "--benchmark") {
            options.benchmark = true;
        } else if (arg == "--profile-hotspots") {
            options.profile_hotspots = true;
            options.benchmark = true;
        } else if (arg == "--engine") {
            if (index + 1 >= argc) {
                throw std::runtime_error("--engine requires a value");
            }
            ++index;
            options.engine = parse_engine(argv[index]);
        } else if (arg.rfind("--engine=", 0) == 0) {
            options.engine = parse_engine(std::string_view(arg).substr(std::string("--engine=").size()));
        } else if (arg == "--threads") {
            if (index + 1 >= argc) {
                throw std::runtime_error("--threads requires a value");
            }
            ++index;
            options.worker_count = parse_non_negative_i32(argv[index], "--threads");
        } else if (arg.rfind("--threads=", 0) == 0) {
            options.worker_count =
                parse_non_negative_i32(arg.substr(std::string("--threads=").size()), "--threads");
        } else if (arg == "--full") {
            options.engine = Gpt2EngineMode::full_prefix;
        } else if (arg == "--cached") {
            options.engine = Gpt2EngineMode::cached_kv;
        } else if (!arg.empty() && arg.front() == '-') {
            throw std::runtime_error("unknown option: " + arg);
        } else {
            positional.push_back(arg);
        }
    }

    if (positional.size() < 2 || positional.size() > 3) {
        throw std::runtime_error(
            "usage: lcqi_gpt2 [--engine cached|full] [--threads N] [--benchmark] "
            "model_dir prompt [max_new_tokens]");
    }
    options.model_dir = positional[0];
    options.prompt = positional[1];
    if (positional.size() == 3) {
        options.max_new_tokens = parse_non_negative_i32(positional[2], "max_new_tokens");
    }
    return options;
}

[[nodiscard]] std::vector<std::int32_t> generate_full_prefix(
    const lcqi::Gpt2ReferenceModel& model,
    std::span<const std::int32_t> prompt_ids,
    std::int32_t max_new_tokens,
    Gpt2BenchmarkTimings& timings) {
    const Clock::time_point generate_start = Clock::now();
    std::vector<std::int32_t> tokens(prompt_ids.begin(), prompt_ids.end());
    for (std::int32_t step = 0; step < max_new_tokens; ++step) {
        if (tokens.size() >= checked_size(model.config.max_positions, "max_positions")) {
            throw std::runtime_error("GPT-2 generation would exceed max_positions");
        }
        const lcqi::Gpt2ForwardResult result = lcqi::run_gpt2_forward(model, tokens);
        ++timings.decode_steps;
        tokens.push_back(result.predicted_token);
        if (model.config.eos_token_id >= 0 && result.predicted_token == model.config.eos_token_id) {
            break;
        }
    }
    const Clock::time_point generate_end = Clock::now();
    timings.generate_ms = elapsed_ms(generate_start, generate_end);
    timings.decode_ms = timings.generate_ms;
    return tokens;
}

[[nodiscard]] std::vector<std::int32_t> generate_cached(
    const lcqi::Gpt2ReferenceModel& model,
    std::span<const std::int32_t> prompt_ids,
    std::int32_t max_new_tokens,
    Gpt2BenchmarkTimings& timings,
    lcqi::Gpt2HotspotProfile* hotspot_profile,
    std::int32_t worker_count) {
    if (prompt_ids.empty()) {
        throw std::runtime_error("GPT-2 generation needs at least one prompt token");
    }
    lcqi::Gpt2ExecutionOptions execution_options;
    execution_options.worker_count = worker_count;
    lcqi::Gpt2CachedGreedyDecoder decoder(model, hotspot_profile, execution_options);
    timings.kv_cache_bytes = decoder.kv_cache_bytes();
    timings.worker_count = decoder.worker_count();
    std::vector<std::int32_t> tokens(prompt_ids.begin(), prompt_ids.end());
    if (max_new_tokens == 0) {
        return tokens;
    }

    const Clock::time_point generate_start = Clock::now();
    std::int32_t predicted_token = 0;
    const Clock::time_point prefill_start = Clock::now();
    for (const std::int32_t token_id : prompt_ids) {
        predicted_token = decoder.step(token_id);
        ++timings.prefill_steps;
    }
    const Clock::time_point prefill_end = Clock::now();
    timings.prefill_ms = elapsed_ms(prefill_start, prefill_end);

    const Clock::time_point decode_start = Clock::now();
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
            ++timings.decode_steps;
        }
    }
    const Clock::time_point decode_end = Clock::now();
    const Clock::time_point generate_end = Clock::now();
    timings.decode_ms = elapsed_ms(decode_start, decode_end);
    timings.generate_ms = elapsed_ms(generate_start, generate_end);
    return tokens;
}

void print_benchmark(const Gpt2BenchmarkTimings& timings,
                     std::size_t prompt_tokens,
                     std::size_t generated_tokens) {
    const double new_tokens = static_cast<double>(generated_tokens);
    const double generated_tokens_per_second =
        timings.generate_ms > 0.0 ? new_tokens * 1000.0 / timings.generate_ms : 0.0;
    const double decode_tokens_per_second =
        timings.decode_ms > 0.0
            ? static_cast<double>(timings.decode_steps) * 1000.0 / timings.decode_ms
            : 0.0;

    std::cout << "benchmark_load_ms " << timings.load_ms << "\n";
    std::cout << "benchmark_tokenize_ms " << timings.tokenize_ms << "\n";
    std::cout << "benchmark_prefill_ms " << timings.prefill_ms << "\n";
    std::cout << "benchmark_decode_ms " << timings.decode_ms << "\n";
    std::cout << "benchmark_generate_ms " << timings.generate_ms << "\n";
    std::cout << "benchmark_total_ms " << timings.total_ms << "\n";
    std::cout << "benchmark_prompt_tokens " << prompt_tokens << "\n";
    std::cout << "benchmark_generated_tokens " << generated_tokens << "\n";
    std::cout << "benchmark_prefill_steps " << timings.prefill_steps << "\n";
    std::cout << "benchmark_decode_steps " << timings.decode_steps << "\n";
    std::cout << "benchmark_worker_count " << timings.worker_count << "\n";
    std::cout << "benchmark_generate_tokens_per_second "
              << generated_tokens_per_second << "\n";
    std::cout << "benchmark_decode_tokens_per_second "
              << decode_tokens_per_second << "\n";
    std::cout << "benchmark_kv_cache_bytes " << timings.kv_cache_bytes << "\n";
}

void print_hotspot_profile(const lcqi::Gpt2HotspotProfile& profile) {
    const double total = profile.total_step_ms > 0.0 ? profile.total_step_ms : 1.0;
    const auto print_ms = [total](std::string_view name, double value) {
        std::cout << "hotspot_" << name << "_ms " << value << "\n";
        std::cout << "hotspot_" << name << "_pct " << (value * 100.0 / total) << "\n";
    };
    std::cout << "hotspot_decoder_steps " << profile.decoder_steps << "\n";
    std::cout << "hotspot_layer_steps " << profile.layer_steps << "\n";
    print_ms("total_step", profile.total_step_ms);
    print_ms("embedding", profile.embedding_ms);
    print_ms("layer_norm", profile.layer_norm_ms);
    print_ms("final_norm", profile.final_norm_ms);
    print_ms("qkv_projection", profile.qkv_projection_ms);
    print_ms("kv_append", profile.kv_append_ms);
    print_ms("attention", profile.attention_ms);
    print_ms("attention_projection", profile.attention_projection_ms);
    print_ms("residual_add", profile.residual_add_ms);
    print_ms("mlp_fc", profile.mlp_fc_ms);
    print_ms("gelu", profile.gelu_ms);
    print_ms("mlp_projection", profile.mlp_projection_ms);
    print_ms("lm_head", profile.lm_head_ms);
    print_ms("logits_result", profile.logits_result_ms);
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
            "usage: lcqi_gpt2 [--engine cached|full] [--threads N] [--benchmark] "
            "model_dir prompt [max_new_tokens]");
    }
    const Clock::time_point total_start = Clock::now();
    const Gpt2CliOptions options = parse_options(argc, argv);

    Gpt2BenchmarkTimings timings;
    const Clock::time_point load_start = Clock::now();
    const lcqi::Gpt2ReferenceModel model = lcqi::load_gpt2_from_directory(options.model_dir);
    const lcqi::Gpt2Tokenizer tokenizer = lcqi::load_gpt2_tokenizer(
        options.model_dir / "vocab.json",
        options.model_dir / "merges.txt",
        model.config.bos_token_id,
        model.config.eos_token_id);
    timings.load_ms = elapsed_ms(load_start, Clock::now());

    const Clock::time_point tokenize_start = Clock::now();
    const std::vector<std::int32_t> prompt_ids = lcqi::gpt2_encode(tokenizer, options.prompt);
    timings.tokenize_ms = elapsed_ms(tokenize_start, Clock::now());

    std::vector<std::int32_t> generated;
    lcqi::Gpt2HotspotProfile hotspot_profile;
    if (options.engine == Gpt2EngineMode::full_prefix) {
        if (options.profile_hotspots) {
            throw std::runtime_error("--profile-hotspots currently supports cached engine only");
        }
        generated = generate_full_prefix(model, prompt_ids, options.max_new_tokens, timings);
    } else {
        generated = generate_cached(model,
                                    prompt_ids,
                                    options.max_new_tokens,
                                    timings,
                                    options.profile_hotspots ? &hotspot_profile : nullptr,
                                    options.worker_count);
    }
    timings.total_ms = elapsed_ms(total_start, Clock::now());

    std::cout << "mode gpt2_directory\n";
    std::cout << "engine " << engine_name(options.engine) << "\n";
    std::cout << "model_dir " << options.model_dir.string() << "\n";
    std::cout << "prompt_ids ";
    print_ids(prompt_ids);
    std::cout << "\n";
    std::cout << "generated_ids ";
    print_ids(generated);
    std::cout << "\n";
    std::cout << "generated_text " << lcqi::gpt2_decode(tokenizer, generated) << "\n";
    if (options.benchmark) {
        const std::size_t generated_tokens = generated.size() - prompt_ids.size();
        print_benchmark(timings, prompt_ids.size(), generated_tokens);
    }
    if (options.profile_hotspots) {
        print_hotspot_profile(hotspot_profile);
    }
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

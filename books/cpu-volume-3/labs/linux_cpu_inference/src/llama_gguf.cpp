#include <lcqi/llama_gguf.hpp>

#include <lcqi/f32_kernels.hpp>
#include <lcqi/ggml_matvec.hpp>
#include <lcqi/ggml_tensors.hpp>
#include <lcqi/gguf.hpp>
#include <lcqi/q4_k.hpp>
#include <lcqi/q5_0_packed.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
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
constexpr const char* LCQI_LLAMA_Q5_0_PACKED_ENV = "LCQI_LLAMA_Q5_0_PACKED";
constexpr const char* LCQI_LLAMA_Q6_K_DIRECT_ENV = "LCQI_LLAMA_Q6_K_DIRECT";
constexpr const char* LCQI_LLAMA_Q8_0_DIRECT_ENV = "LCQI_LLAMA_Q8_0_DIRECT";
constexpr const char* LCQI_LLAMA_Q8_0_TIED_LM_HEAD_ENV =
    "LCQI_LLAMA_Q8_0_TIED_LM_HEAD";
constexpr const char* LCQI_LLAMA_GGML_DIRECT_ENV = "LCQI_LLAMA_GGML_DIRECT";
constexpr const char* LCQI_LLAMA_GGML_SELECTIVE_DIRECT_ENV =
    "LCQI_LLAMA_GGML_SELECTIVE_DIRECT";
constexpr const char* LCQI_LLAMA_BATCH_PREFILL_ENV = "LCQI_LLAMA_BATCH_PREFILL";
constexpr const char* LCQI_LLAMA_FALSE_ENV = "0";
constexpr const char* LCQI_LLAMA_TRUE_ENV = "1";
constexpr std::int32_t LCQI_LLAMA_MAX_AUTO_WORKERS = 8;
constexpr std::int32_t LCQI_LLAMA_DEFAULT_PARALLEL_MIN_ROWS = 512;
constexpr std::int32_t LCQI_LLAMA_PARALLEL_MIN_COLUMNS = 512;
constexpr std::int64_t LCQI_LLAMA_PARALLEL_MIN_OPS = 1'000'000;
constexpr std::size_t LCQI_LLAMA_PARALLEL_ROW_BLOCK_MULTIPLIER = 4;
constexpr std::size_t LCQI_LLAMA_WORKER_SPIN_PAUSES_BEFORE_YIELD = 4096;

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

void pause_worker_spin(std::size_t& pause_count) noexcept {
#if defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
    __builtin_ia32_pause();
#endif
    ++pause_count;
    if (pause_count >= LCQI_LLAMA_WORKER_SPIN_PAUSES_BEFORE_YIELD) {
        pause_count = 0;
        std::this_thread::yield();
    }
}

class LlamaParallelWorkerPool {
public:
    explicit LlamaParallelWorkerPool(std::int32_t requested_workers)
        : worker_count_(LlamaParallelWorkerPool::normalize_worker_count(requested_workers)) {
        if (this->worker_count_ <= 1) {
            return;
        }
        const std::size_t background_count =
            static_cast<std::size_t>(this->worker_count_ - 1);
        this->threads_.reserve(background_count);
        for (std::size_t index = 0; index < background_count; ++index) {
            this->threads_.emplace_back([this, index]() { this->worker_loop(index); });
        }
    }

    ~LlamaParallelWorkerPool() {
        this->stopping_.store(true, std::memory_order_release);
        this->generation_.fetch_add(1, std::memory_order_release);
        for (std::thread& thread : this->threads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
    }

    LlamaParallelWorkerPool(const LlamaParallelWorkerPool&) = delete;
    LlamaParallelWorkerPool& operator=(const LlamaParallelWorkerPool&) = delete;

    [[nodiscard]] std::int32_t worker_count() const noexcept {
        return this->worker_count_;
    }

    template <typename Function>
    void parallel_for_rows(std::size_t begin,
                           std::size_t end,
                           std::size_t grain,
                           Function&& function) {
        if (end <= begin) {
            return;
        }
        const std::size_t row_count = end - begin;
        if (this->worker_count_ <= 1 || row_count <= grain) {
            function(begin, end, 0);
            return;
        }

        const std::size_t task_count =
            std::min<std::size_t>(static_cast<std::size_t>(this->worker_count_),
                                  (row_count + grain - 1) / grain);
        if (task_count <= 1) {
            function(begin, end, 0);
            return;
        }

        auto task_context = ParallelTaskContext<Function>{function};
        this->task_begin_.store(begin, std::memory_order_release);
        this->task_end_.store(end, std::memory_order_release);
        this->task_count_.store(task_count, std::memory_order_release);
        this->task_context_.store(&task_context, std::memory_order_release);
        this->task_invoke_.store(&ParallelTaskContext<Function>::invoke,
                                 std::memory_order_release);
        this->active_workers_.store(task_count - 1, std::memory_order_release);
        this->generation_.fetch_add(1, std::memory_order_release);

        const std::size_t main_worker = task_count - 1;
        const auto [main_begin, main_end] =
            this->chunk_bounds(begin, end, task_count, main_worker);
        function(main_begin, main_end, main_worker);

        std::size_t pause_count = 0;
        while (this->active_workers_.load(std::memory_order_acquire) != 0) {
            pause_worker_spin(pause_count);
        }
        this->task_context_.store(nullptr, std::memory_order_release);
        this->task_invoke_.store(nullptr, std::memory_order_release);
    }

private:
    template <typename Function>
    struct ParallelTaskContext {
        Function& function;

        static void invoke(void* context,
                           std::size_t begin,
                           std::size_t end,
                           std::size_t worker_index) {
            auto* typed_context = static_cast<ParallelTaskContext*>(context);
            typed_context->function(begin, end, worker_index);
        }
    };

    std::int32_t worker_count_ = 1;
    std::vector<std::thread> threads_;
    std::atomic<bool> stopping_ = false;
    std::atomic<std::uint64_t> generation_ = 0;
    std::atomic<std::size_t> task_begin_ = 0;
    std::atomic<std::size_t> task_end_ = 0;
    std::atomic<std::size_t> task_count_ = 0;
    std::atomic<std::size_t> active_workers_ = 0;
    std::atomic<void*> task_context_ = nullptr;
    std::atomic<void (*)(void*, std::size_t, std::size_t, std::size_t)> task_invoke_ = nullptr;

    [[nodiscard]] static std::int32_t normalize_worker_count(std::int32_t requested_workers) {
        if (requested_workers > 0) {
            return requested_workers;
        }
        const unsigned int hardware_workers = std::thread::hardware_concurrency();
        if (hardware_workers == 0) {
            return 1;
        }
        const std::int32_t capped =
            std::min<std::int32_t>(static_cast<std::int32_t>(hardware_workers),
                                   LCQI_LLAMA_MAX_AUTO_WORKERS);
        return std::max(capped, 1);
    }

    [[nodiscard]] std::pair<std::size_t, std::size_t> chunk_bounds(
        std::size_t begin,
        std::size_t end,
        std::size_t task_count,
        std::size_t worker_index) const noexcept {
        const std::size_t row_count = end - begin;
        const std::size_t chunk_begin = begin + row_count * worker_index / task_count;
        const std::size_t chunk_end = begin + row_count * (worker_index + 1) / task_count;
        return {chunk_begin, chunk_end};
    }

    void worker_loop(std::size_t worker_index) {
        std::uint64_t observed_generation = 0;
        std::size_t pause_count = 0;
        while (true) {
            const std::uint64_t current_generation =
                this->generation_.load(std::memory_order_acquire);
            if (current_generation == observed_generation) {
                if (this->stopping_.load(std::memory_order_acquire)) {
                    return;
                }
                pause_worker_spin(pause_count);
                continue;
            }

            observed_generation = current_generation;
            pause_count = 0;
            if (this->stopping_.load(std::memory_order_acquire)) {
                return;
            }

            const std::size_t task_count = this->task_count_.load(std::memory_order_acquire);
            if (this->generation_.load(std::memory_order_acquire) != current_generation ||
                task_count == 0) {
                continue;
            }
            const std::size_t background_task_count = task_count - 1;
            if (worker_index >= background_task_count) {
                continue;
            }
            void* task_context = this->task_context_.load(std::memory_order_acquire);
            void (*task_invoke)(void*, std::size_t, std::size_t, std::size_t) =
                this->task_invoke_.load(std::memory_order_acquire);
            if (task_context == nullptr || task_invoke == nullptr) {
                continue;
            }
            const std::size_t task_begin = this->task_begin_.load(std::memory_order_acquire);
            const std::size_t task_end = this->task_end_.load(std::memory_order_acquire);
            if (this->generation_.load(std::memory_order_acquire) != current_generation) {
                continue;
            }
            const auto [begin, end] =
                this->chunk_bounds(task_begin, task_end, task_count, worker_index);
            task_invoke(task_context, begin, end, worker_index);
            this->active_workers_.fetch_sub(1, std::memory_order_acq_rel);
        }
    }
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

[[nodiscard]] bool can_use_ggml_direct(const GgufTensorInfo& tensor) {
    if (tensor.shape.size() != 2 || tensor.shape[0] <= 0 || tensor.shape[1] <= 0 ||
        !ggml_type_has_q8_0_direct_matvec(tensor.type)) {
        return false;
    }
    const GgmlTypeLayout layout = ggml_type_layout(tensor.type);
    return tensor.shape[0] % layout.block_size == 0 &&
           tensor.shape[0] % LCQI_Q8_0_INPUT_BLOCK_VALUES == 0;
}

[[nodiscard]] bool can_use_selective_ggml_direct(const GgufTensorInfo& tensor) {
    return (tensor.type == GgmlType::q5_0 || tensor.type == GgmlType::q8_0) &&
           can_use_ggml_direct(tensor);
}

[[nodiscard]] bool q4_k_direct_enabled() noexcept {
    const char* enabled = std::getenv(LCQI_LLAMA_Q4_DIRECT_ENV);
    return enabled == nullptr || std::string_view(enabled) != LCQI_LLAMA_FALSE_ENV;
}

[[nodiscard]] bool q5_0_packed_enabled() noexcept {
    const char* enabled = std::getenv(LCQI_LLAMA_Q5_0_PACKED_ENV);
    return enabled == nullptr || std::string_view(enabled) != LCQI_LLAMA_FALSE_ENV;
}

[[nodiscard]] bool q6_k_direct_enabled() noexcept {
    const char* enabled = std::getenv(LCQI_LLAMA_Q6_K_DIRECT_ENV);
    return enabled != nullptr && std::string_view(enabled) == LCQI_LLAMA_TRUE_ENV;
}

[[nodiscard]] bool q8_0_direct_enabled() noexcept {
    const char* enabled = std::getenv(LCQI_LLAMA_Q8_0_DIRECT_ENV);
    return enabled == nullptr || std::string_view(enabled) != LCQI_LLAMA_FALSE_ENV;
}

[[nodiscard]] bool q8_0_tied_lm_head_enabled() noexcept {
    const char* enabled = std::getenv(LCQI_LLAMA_Q8_0_TIED_LM_HEAD_ENV);
    return enabled == nullptr || std::string_view(enabled) != LCQI_LLAMA_FALSE_ENV;
}

[[nodiscard]] bool ggml_direct_enabled() noexcept {
    const char* enabled = std::getenv(LCQI_LLAMA_GGML_DIRECT_ENV);
    return enabled != nullptr && std::string_view(enabled) == LCQI_LLAMA_TRUE_ENV;
}

[[nodiscard]] bool ggml_selective_direct_enabled() noexcept {
    const char* enabled = std::getenv(LCQI_LLAMA_GGML_SELECTIVE_DIRECT_ENV);
    return enabled != nullptr && std::string_view(enabled) == LCQI_LLAMA_TRUE_ENV;
}

[[nodiscard]] bool batch_prefill_enabled() noexcept {
    const char* enabled = std::getenv(LCQI_LLAMA_BATCH_PREFILL_ENV);
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

void account_quantized_direct_tensor_load(const GgufTensorInfo& tensor,
                                          LlamaGgufLoadReport& report) {
    report.quantized_weight_bytes += tensor.byte_size;
    report.direct_quantized_weight_bytes += tensor.byte_size;
    ++report.tensors_loaded;
    if (tensor.type == GgmlType::q4_k) {
        ++report.q4_k_direct_tensors;
    } else {
        ++report.ggml_direct_tensors;
    }
}

void account_q5_0_packed_tensor_load(const GgufTensorInfo& tensor,
                                     const LlamaGgufLinearWeights& linear,
                                     LlamaGgufLoadReport& report) {
    report.quantized_weight_bytes += tensor.byte_size;
    report.direct_quantized_weight_bytes += tensor.byte_size;
    report.q5_0_packed_weight_bytes +=
        linear.q5_0_packed_blocks.size() * sizeof(Q5_0PackedBlock);
    const std::uint64_t batch_f32_bytes =
        static_cast<std::uint64_t>(linear.f32_weights.size()) *
        static_cast<std::uint64_t>(sizeof(float));
    report.f32_weight_bytes += batch_f32_bytes;
    report.q5_0_batch_f32_weight_bytes += batch_f32_bytes;
    ++report.tensors_loaded;
    ++report.q5_0_packed_tensors;
}

void account_q6_k_direct_tensor_load(const GgufTensorInfo& tensor,
                                     LlamaGgufLoadReport& report) {
    report.quantized_weight_bytes += tensor.byte_size;
    report.direct_quantized_weight_bytes += tensor.byte_size;
    report.q6_k_direct_weight_bytes += tensor.byte_size;
    ++report.tensors_loaded;
    ++report.q6_k_direct_tensors;
}

void account_q8_0_direct_tensor_load(const GgufTensorInfo& tensor,
                                     LlamaGgufLoadReport& report) {
    report.direct_quantized_weight_bytes += tensor.byte_size;
    report.q8_0_direct_weight_bytes += tensor.byte_size;
    ++report.q8_0_direct_tensors;
}

void account_q8_0_linear_tensor_load(const GgufTensorInfo& tensor,
                                     LlamaGgufLoadReport& report) {
    report.quantized_weight_bytes += tensor.byte_size;
    account_q8_0_direct_tensor_load(tensor, report);
    ++report.tensors_loaded;
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
        linear.quantized_bytes = bytes;
        account_quantized_direct_tensor_load(*tensor, report);
        return linear;
    }
    if (q5_0_packed_enabled() && !ggml_direct_enabled() &&
        !ggml_selective_direct_enabled() && tensor->type == GgmlType::q5_0 &&
        can_use_ggml_direct(*tensor)) {
        linear.storage = LlamaGgufLinearStorage::q5_0_packed;
        linear.q5_0_packed_blocks = pack_q5_0_blocks(
            bytes,
            static_cast<std::int64_t>(tensor->element_count()));
        linear.f32_weights =
            dequantize_ggml_tensor(tensor->type,
                                   bytes,
                                   static_cast<std::int64_t>(tensor->element_count()));
        account_q5_0_packed_tensor_load(*tensor, linear, report);
        return linear;
    }
    if (q6_k_direct_enabled() && !ggml_direct_enabled() &&
        !ggml_selective_direct_enabled() && tensor->type == GgmlType::q6_k &&
        can_use_ggml_direct(*tensor)) {
        linear.storage = LlamaGgufLinearStorage::q6_k_direct;
        linear.quantized_bytes = bytes;
        account_q6_k_direct_tensor_load(*tensor, report);
        return linear;
    }
    if (q8_0_direct_enabled() && !ggml_direct_enabled() &&
        !ggml_selective_direct_enabled() && tensor->type == GgmlType::q8_0 &&
        can_use_ggml_direct(*tensor)) {
        linear.storage = LlamaGgufLinearStorage::q8_0_direct;
        linear.quantized_bytes = bytes;
        account_q8_0_linear_tensor_load(*tensor, report);
        return linear;
    }
    if (ggml_direct_enabled() && can_use_ggml_direct(*tensor)) {
        linear.storage = LlamaGgufLinearStorage::ggml_quantized;
        linear.quantized_bytes = bytes;
        account_quantized_direct_tensor_load(*tensor, report);
        return linear;
    }
    if (ggml_selective_direct_enabled() && can_use_selective_ggml_direct(*tensor)) {
        linear.storage = LlamaGgufLinearStorage::ggml_quantized;
        linear.quantized_bytes = bytes;
        account_quantized_direct_tensor_load(*tensor, report);
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

[[nodiscard]] LlamaGgufLinearWeights make_tied_lm_head_from_embedding(
    const std::filesystem::path& gguf_path,
    const GgufManifest& manifest,
    std::int32_t hidden_size,
    std::int32_t vocab_size,
    LlamaGgufLoadReport& report) {
    LlamaGgufLinearWeights linear;
    linear.input_size = hidden_size;
    linear.output_size = vocab_size;
    const GgufTensorInfo* tensor = manifest.find_tensor("token_embd.weight");
    if (tensor == nullptr || tensor->type != GgmlType::q8_0 ||
        !q8_0_tied_lm_head_enabled() || !can_use_ggml_direct(*tensor)) {
        return linear;
    }
    validate_shape(*tensor, std::array<std::int64_t, 2>{hidden_size, vocab_size});
    linear.source_type = tensor->type;
    linear.storage = LlamaGgufLinearStorage::q8_0_direct;
    linear.quantized_bytes = read_gguf_tensor_bytes(gguf_path, *tensor);
    account_q8_0_direct_tensor_load(*tensor, report);
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

[[nodiscard]] std::span<float> mutable_row_span(std::span<float> values,
                                                std::size_t row,
                                                std::size_t row_width) {
    return values.subspan(row * row_width, row_width);
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

[[nodiscard]] bool should_parallelize_rows(const LlamaGgufExecutionOptions& options,
                                           std::size_t rows,
                                           std::size_t columns,
                                           const LlamaParallelWorkerPool* worker_pool) {
    if (worker_pool == nullptr || worker_pool->worker_count() <= 1) {
        return false;
    }
    const std::int32_t min_rows =
        options.parallel_min_rows > 0 ? options.parallel_min_rows
                                      : LCQI_LLAMA_DEFAULT_PARALLEL_MIN_ROWS;
    if (rows < checked_size(min_rows, "parallel_min_rows")) {
        return false;
    }
    if (options.worker_count > 1) {
        return true;
    }
    return columns >= checked_size(LCQI_LLAMA_PARALLEL_MIN_COLUMNS, "parallel_min_columns") ||
           rows * columns >= static_cast<std::size_t>(LCQI_LLAMA_PARALLEL_MIN_OPS);
}

[[nodiscard]] std::size_t parallel_row_grain(std::size_t rows,
                                             const LlamaParallelWorkerPool& worker_pool) {
    const std::size_t workers = checked_size(worker_pool.worker_count(), "worker_count");
    const std::size_t target_tasks = workers * LCQI_LLAMA_PARALLEL_ROW_BLOCK_MULTIPLIER;
    return std::max<std::size_t>(1, (rows + target_tasks - 1) / target_tasks);
}

[[nodiscard]] bool model_has_parallel_work(const LlamaGgufModel& model,
                                           const LlamaGgufExecutionOptions& options) {
    if (options.worker_count == 1) {
        return false;
    }
    const DecoderConfig& config = model.decoder.config;
    const std::size_t hidden_size = checked_size(config.hidden_size, "hidden_size");
    const std::size_t kv_width = checked_size(config.kv_heads * config.head_dim, "kv_width");
    const std::size_t intermediate_size =
        checked_size(config.intermediate_size, "intermediate_size");
    const std::size_t vocab_size = checked_size(config.vocab_size, "vocab_size");
    const std::int32_t min_rows =
        options.parallel_min_rows > 0 ? options.parallel_min_rows
                                      : LCQI_LLAMA_DEFAULT_PARALLEL_MIN_ROWS;
    const auto large_enough = [min_rows](std::size_t rows, std::size_t columns) {
        if (rows < checked_size(min_rows, "parallel_min_rows")) {
            return false;
        }
        return columns >= checked_size(LCQI_LLAMA_PARALLEL_MIN_COLUMNS,
                                       "parallel_min_columns") ||
               rows * columns >= static_cast<std::size_t>(LCQI_LLAMA_PARALLEL_MIN_OPS);
    };
    if (options.worker_count > 1) {
        return vocab_size >= checked_size(min_rows, "parallel_min_rows") ||
               hidden_size >= checked_size(min_rows, "parallel_min_rows") ||
               kv_width >= checked_size(min_rows, "parallel_min_rows") ||
               intermediate_size >= checked_size(min_rows, "parallel_min_rows");
    }
    return large_enough(vocab_size, hidden_size) ||
           large_enough(hidden_size, hidden_size) ||
           large_enough(kv_width, hidden_size) ||
           large_enough(intermediate_size, hidden_size) ||
           large_enough(hidden_size, intermediate_size);
}

[[nodiscard]] std::unique_ptr<LlamaParallelWorkerPool> make_worker_pool(
    const LlamaGgufModel& model,
    const LlamaGgufExecutionOptions& options) {
    if (!model_has_parallel_work(model, options)) {
        return nullptr;
    }
    return std::make_unique<LlamaParallelWorkerPool>(options.worker_count);
}

void linear_f32_fallback_parallel(const LlamaGgufLinearWeights& layer,
                                  std::span<const float> input,
                                  std::span<float> output,
                                  const LlamaGgufExecutionOptions& options,
                                  LlamaParallelWorkerPool* worker_pool) {
    if (layer.f32_weights.size() !=
        checked_size(layer.input_size, "linear input size") *
            checked_size(layer.output_size, "linear output size")) {
        throw std::runtime_error("LLaMA GGUF F32 linear weight size mismatch");
    }
    const std::size_t input_size = checked_size(layer.input_size, "linear input size");
    const std::size_t output_size = checked_size(layer.output_size, "linear output size");
    if (!should_parallelize_rows(options, output_size, input_size, worker_pool)) {
        linear_f32_fallback(layer, input, output);
        return;
    }

    const float* weights = layer.f32_weights.data();
    const float* input_data = input.data();
    float* output_data = output.data();
    const auto rows_fn = [weights, input_data, input_size, output_data](
                             std::size_t begin,
                             std::size_t end,
                             std::size_t worker_index) {
        (void)worker_index;
        linear_f32_rows_unchecked(weights,
                                  input_data,
                                  nullptr,
                                  input_size,
                                  begin,
                                  end,
                                  output_data);
    };
    worker_pool->parallel_for_rows(0,
                                   output_size,
                                   parallel_row_grain(output_size, *worker_pool),
                                   rows_fn);
}

void linear_f32_batch_parallel(std::span<const float> weights,
                               std::size_t input_size,
                               std::size_t output_size,
                               std::span<const float> input,
                               std::size_t batch_size,
                               std::span<float> output,
                               const LlamaGgufExecutionOptions& options,
                               LlamaParallelWorkerPool* worker_pool) {
    if (weights.size() != input_size * output_size) {
        throw std::runtime_error("LLaMA GGUF F32 linear weight size mismatch");
    }
    if (input.size() != batch_size * input_size || output.size() != batch_size * output_size) {
        throw std::runtime_error("LLaMA GGUF F32 batch linear shape mismatch");
    }
    if (!should_parallelize_rows(options, output_size, input_size, worker_pool)) {
        linear_f32_batch_rows_unchecked(weights.data(),
                                        input.data(),
                                        nullptr,
                                        input_size,
                                        batch_size,
                                        0,
                                        output_size,
                                        output_size,
                                        output.data());
        return;
    }

    const float* weights_data = weights.data();
    const float* input_data = input.data();
    float* output_data = output.data();
    const auto rows_fn = [weights_data, input_data, input_size, batch_size, output_size, output_data](
                             std::size_t begin,
                             std::size_t end,
                             std::size_t worker_index) {
        (void)worker_index;
        linear_f32_batch_rows_unchecked(weights_data,
                                        input_data,
                                        nullptr,
                                        input_size,
                                        batch_size,
                                        begin,
                                        end,
                                        output_size,
                                        output_data);
    };
    worker_pool->parallel_for_rows(0,
                                   output_size,
                                   parallel_row_grain(output_size, *worker_pool),
                                   rows_fn);
}

void linear_f32_fallback_batch_parallel(const LlamaGgufLinearWeights& layer,
                                        std::span<const float> input,
                                        std::size_t batch_size,
                                        std::span<float> output,
                                        const LlamaGgufExecutionOptions& options,
                                        LlamaParallelWorkerPool* worker_pool) {
    linear_f32_batch_parallel(layer.f32_weights,
                              checked_size(layer.input_size, "linear input size"),
                              checked_size(layer.output_size, "linear output size"),
                              input,
                              batch_size,
                              output,
                              options,
                              worker_pool);
}

[[nodiscard]] F32RowMax max_dot_f32_rows_parallel(const float* weights,
                                                  const float* input,
                                                  std::size_t input_size,
                                                  std::size_t row_count,
                                                  const LlamaGgufExecutionOptions& options,
                                                  LlamaParallelWorkerPool* worker_pool) {
    if (!should_parallelize_rows(options, row_count, input_size, worker_pool)) {
        return max_dot_f32_rows_unchecked(weights, input, input_size, 0, row_count);
    }

    const std::size_t worker_count = checked_size(worker_pool->worker_count(), "worker_count");
    std::vector<F32RowMax> local_best(worker_count);
    std::vector<std::uint8_t> has_result(worker_count, 0U);
    const auto rows_fn = [weights, input, input_size, &local_best, &has_result](
                             std::size_t begin,
                             std::size_t end,
                             std::size_t worker_index) {
        if (begin >= end) {
            return;
        }
        local_best[worker_index] =
            max_dot_f32_rows_unchecked(weights, input, input_size, begin, end);
        has_result[worker_index] = 1U;
    };
    worker_pool->parallel_for_rows(0,
                                   row_count,
                                   parallel_row_grain(row_count, *worker_pool),
                                   rows_fn);

    F32RowMax best;
    best.row = 0;
    best.value = -std::numeric_limits<float>::infinity();
    bool found = false;
    for (std::size_t index = 0; index < local_best.size(); ++index) {
        if (has_result[index] == 0U) {
            continue;
        }
        const F32RowMax& candidate = local_best[index];
        if (!found || candidate.value > best.value) {
            best = candidate;
            found = true;
        }
    }
    if (!found) {
        return max_dot_f32_rows_unchecked(weights, input, input_size, 0, row_count);
    }
    return best;
}

[[nodiscard]] F32RowMax max_dot_ggml_quantized_q8_0_parallel(
    const LlamaGgufLinearWeights& layer,
    std::span<const Q8_0InputBlock> q8_0_input,
    const LlamaGgufExecutionOptions& options,
    LlamaParallelWorkerPool* worker_pool) {
    if (q8_0_input.size() != checked_size(layer.input_size / LCQI_Q8_0_INPUT_BLOCK_VALUES,
                                          "Q8_0 max-dot input block count")) {
        throw std::runtime_error("LLaMA GGUF Q8_0 max-dot scratch block count mismatch");
    }
    const std::size_t input_size = checked_size(layer.input_size, "linear input size");
    const std::size_t output_size = checked_size(layer.output_size, "linear output size");
    if (!should_parallelize_rows(options, output_size, input_size, worker_pool)) {
        return max_dot_ggml_quantized_q8_0(layer.source_type,
                                           layer.quantized_bytes,
                                           layer.output_size,
                                           layer.input_size,
                                           q8_0_input);
    }

    const std::size_t worker_count = checked_size(worker_pool->worker_count(), "worker_count");
    std::vector<F32RowMax> local_best(worker_count);
    std::vector<std::uint8_t> has_result(worker_count, 0U);
    const auto rows_fn = [&layer, q8_0_input, &local_best, &has_result](
                             std::size_t begin,
                             std::size_t end,
                             std::size_t worker_index) {
        if (begin >= end) {
            return;
        }
        local_best[worker_index] =
            max_dot_ggml_quantized_q8_0_rows_unchecked(layer.source_type,
                                                       layer.quantized_bytes,
                                                       layer.output_size,
                                                       layer.input_size,
                                                       q8_0_input,
                                                       static_cast<std::int64_t>(begin),
                                                       static_cast<std::int64_t>(end));
        has_result[worker_index] = 1U;
    };
    worker_pool->parallel_for_rows(0,
                                   output_size,
                                   parallel_row_grain(output_size, *worker_pool),
                                   rows_fn);

    F32RowMax best;
    best.row = 0;
    best.value = -std::numeric_limits<float>::infinity();
    bool found = false;
    for (std::size_t index = 0; index < local_best.size(); ++index) {
        if (has_result[index] == 0U) {
            continue;
        }
        const F32RowMax& candidate = local_best[index];
        if (!found || candidate.value > best.value) {
            best = candidate;
            found = true;
        }
    }
    if (!found) {
        return max_dot_ggml_quantized_q8_0(layer.source_type,
                                           layer.quantized_bytes,
                                           layer.output_size,
                                           layer.input_size,
                                           q8_0_input);
    }
    return best;
}

void linear_q4_k_direct(const LlamaGgufLinearWeights& layer,
                        std::span<const Q8KBlock> q8_input,
                        std::span<float> output) {
    if (q8_input.size() !=
        checked_size(layer.input_size / LCQI_QK_K_BLOCK_VALUES, "Q8_K input block count")) {
        throw std::runtime_error("LLaMA GGUF Q4_K scratch block count mismatch");
    }
    matvec_q4_k_q8(layer.quantized_bytes,
                   layer.output_size,
                   layer.input_size,
                   q8_input,
                   output);
}

void linear_q4_k_direct_parallel(const LlamaGgufLinearWeights& layer,
                                 std::span<const Q8KBlock> q8_input,
                                 std::span<float> output,
                                 const LlamaGgufExecutionOptions& options,
                                 LlamaParallelWorkerPool* worker_pool) {
    if (q8_input.size() !=
        checked_size(layer.input_size / LCQI_QK_K_BLOCK_VALUES, "Q8_K input block count")) {
        throw std::runtime_error("LLaMA GGUF Q4_K scratch block count mismatch");
    }
    const std::size_t input_size = checked_size(layer.input_size, "linear input size");
    const std::size_t output_size = checked_size(layer.output_size, "linear output size");
    if (!should_parallelize_rows(options, output_size, input_size, worker_pool)) {
        linear_q4_k_direct(layer, q8_input, output);
        return;
    }

    const auto rows_fn = [&layer, q8_input, output](
                             std::size_t begin,
                             std::size_t end,
                             std::size_t worker_index) {
        (void)worker_index;
        matvec_q4_k_q8_rows_unchecked(layer.quantized_bytes,
                                      layer.output_size,
                                      layer.input_size,
                                      q8_input,
                                      output,
                                      static_cast<std::int64_t>(begin),
                                      static_cast<std::int64_t>(end));
    };
    worker_pool->parallel_for_rows(0,
                                   output_size,
                                   parallel_row_grain(output_size, *worker_pool),
                                   rows_fn);
}

void linear_q5_0_packed_direct(const LlamaGgufLinearWeights& layer,
                               std::span<const Q8_0InputBlock> q8_0_input,
                               std::span<float> output) {
    if (q8_0_input.size() != checked_size(layer.input_size / LCQI_Q8_0_INPUT_BLOCK_VALUES,
                                          "Q8_0 input block count")) {
        throw std::runtime_error("LLaMA GGUF Q5_0 packed scratch block count mismatch");
    }
    matvec_q5_0_packed_q8_0(layer.q5_0_packed_blocks,
                            layer.output_size,
                            layer.input_size,
                            q8_0_input,
                            output);
}

void linear_q5_0_packed_direct_parallel(const LlamaGgufLinearWeights& layer,
                                        std::span<const Q8_0InputBlock> q8_0_input,
                                        std::span<float> output,
                                        const LlamaGgufExecutionOptions& options,
                                        LlamaParallelWorkerPool* worker_pool) {
    if (q8_0_input.size() != checked_size(layer.input_size / LCQI_Q8_0_INPUT_BLOCK_VALUES,
                                          "Q8_0 input block count")) {
        throw std::runtime_error("LLaMA GGUF Q5_0 packed scratch block count mismatch");
    }
    const std::size_t input_size = checked_size(layer.input_size, "linear input size");
    const std::size_t output_size = checked_size(layer.output_size, "linear output size");
    if (!should_parallelize_rows(options, output_size, input_size, worker_pool)) {
        linear_q5_0_packed_direct(layer, q8_0_input, output);
        return;
    }

    const auto rows_fn = [&layer, q8_0_input, output](
                             std::size_t begin,
                             std::size_t end,
                             std::size_t worker_index) {
        (void)worker_index;
        matvec_q5_0_packed_q8_0_rows_unchecked(layer.q5_0_packed_blocks,
                                               layer.output_size,
                                               layer.input_size,
                                               q8_0_input,
                                               output,
                                               static_cast<std::int64_t>(begin),
                                               static_cast<std::int64_t>(end));
    };
    worker_pool->parallel_for_rows(0,
                                   output_size,
                                   parallel_row_grain(output_size, *worker_pool),
                                   rows_fn);
}

void quantize_q8_0_batch_to(std::span<const float> input,
                            std::size_t input_size,
                            std::size_t batch_size,
                            std::span<Q8_0InputBlock> output) {
    const std::size_t blocks_per_input =
        checked_size(static_cast<std::int64_t>(input_size) / LCQI_Q8_0_INPUT_BLOCK_VALUES,
                     "Q8_0 input block count");
    if (input.size() != batch_size * input_size ||
        output.size() != batch_size * blocks_per_input) {
        throw std::runtime_error("LLaMA GGUF Q8_0 batch quantization shape mismatch");
    }
    for (std::size_t batch = 0; batch < batch_size; ++batch) {
        quantize_q8_0_input_to(input.subspan(batch * input_size, input_size),
                               output.subspan(batch * blocks_per_input, blocks_per_input));
    }
}

void linear_ggml_quantized_direct(const LlamaGgufLinearWeights& layer,
                                  std::span<const Q8_0InputBlock> q8_0_input,
                                  std::span<float> output) {
    if (q8_0_input.size() != checked_size(layer.input_size / LCQI_Q8_0_INPUT_BLOCK_VALUES,
                                          "Q8_0 input block count")) {
        throw std::runtime_error("LLaMA GGUF Q8_0 scratch block count mismatch");
    }
    matvec_ggml_quantized_q8_0(layer.source_type,
                               layer.quantized_bytes,
                               layer.output_size,
                               layer.input_size,
                               q8_0_input,
                               output);
}

void linear_ggml_quantized_direct_parallel(const LlamaGgufLinearWeights& layer,
                                           std::span<const Q8_0InputBlock> q8_0_input,
                                           std::span<float> output,
                                           const LlamaGgufExecutionOptions& options,
                                           LlamaParallelWorkerPool* worker_pool) {
    if (q8_0_input.size() != checked_size(layer.input_size / LCQI_Q8_0_INPUT_BLOCK_VALUES,
                                          "Q8_0 input block count")) {
        throw std::runtime_error("LLaMA GGUF Q8_0 scratch block count mismatch");
    }
    const std::size_t input_size = checked_size(layer.input_size, "linear input size");
    const std::size_t output_size = checked_size(layer.output_size, "linear output size");
    if (!should_parallelize_rows(options, output_size, input_size, worker_pool)) {
        linear_ggml_quantized_direct(layer, q8_0_input, output);
        return;
    }

    const auto rows_fn = [&layer, q8_0_input, output](
                             std::size_t begin,
                             std::size_t end,
                             std::size_t worker_index) {
        (void)worker_index;
        matvec_ggml_quantized_q8_0_rows_unchecked(layer.source_type,
                                                  layer.quantized_bytes,
                                                  layer.output_size,
                                                  layer.input_size,
                                                  q8_0_input,
                                                  output,
                                                  static_cast<std::int64_t>(begin),
                                                  static_cast<std::int64_t>(end));
    };
    worker_pool->parallel_for_rows(0,
                                   output_size,
                                   parallel_row_grain(output_size, *worker_pool),
                                   rows_fn);
}

void linear_ggml_quantized_direct_batch_parallel(const LlamaGgufLinearWeights& layer,
                                                 std::span<const float> input,
                                                 std::size_t batch_size,
                                                 std::vector<Q8_0InputBlock>& q8_0_scratch,
                                                 std::span<float> output,
                                                 const LlamaGgufExecutionOptions& options,
                                                 LlamaParallelWorkerPool* worker_pool) {
    const std::size_t input_size = checked_size(layer.input_size, "linear input size");
    const std::size_t output_size = checked_size(layer.output_size, "linear output size");
    if (input.size() != batch_size * input_size || output.size() != batch_size * output_size) {
        throw std::runtime_error("LLaMA GGUF quantized batch linear shape mismatch");
    }
    if (layer.input_size <= 0 || layer.input_size % LCQI_Q8_0_INPUT_BLOCK_VALUES != 0) {
        throw std::runtime_error("LLaMA GGUF quantized batch input is not Q8_0 compatible");
    }
    const std::size_t blocks_per_input =
        checked_size(layer.input_size / LCQI_Q8_0_INPUT_BLOCK_VALUES,
                     "Q8_0 input block count");
    q8_0_scratch.resize(batch_size * blocks_per_input);
    quantize_q8_0_batch_to(input, input_size, batch_size, q8_0_scratch);

    if (!should_parallelize_rows(options, output_size, input_size, worker_pool)) {
        matvec_ggml_quantized_q8_0_batch_rows_unchecked(layer.source_type,
                                                        layer.quantized_bytes,
                                                        layer.output_size,
                                                        layer.input_size,
                                                        q8_0_scratch,
                                                        static_cast<std::int64_t>(batch_size),
                                                        output,
                                                        layer.output_size,
                                                        0,
                                                        layer.output_size);
        return;
    }

    const auto rows_fn = [&layer, &q8_0_scratch, batch_size, output](
                             std::size_t begin,
                             std::size_t end,
                             std::size_t worker_index) {
        (void)worker_index;
        matvec_ggml_quantized_q8_0_batch_rows_unchecked(
            layer.source_type,
            layer.quantized_bytes,
            layer.output_size,
            layer.input_size,
            q8_0_scratch,
            static_cast<std::int64_t>(batch_size),
            output,
            layer.output_size,
            static_cast<std::int64_t>(begin),
            static_cast<std::int64_t>(end));
    };
    worker_pool->parallel_for_rows(0,
                                   output_size,
                                   parallel_row_grain(output_size, *worker_pool),
                                   rows_fn);
}

void linear_gguf(const LlamaGgufLinearWeights& layer,
                 std::span<const float> input,
                 std::span<const Q8KBlock> q8_input,
                 std::span<const Q8_0InputBlock> q8_0_input,
                 std::span<float> output,
                 LlamaGgufLinearOp operation,
                 LlamaGgufHotspotReport& hotspots,
                 const LlamaGgufExecutionOptions& options,
                 LlamaParallelWorkerPool* worker_pool) {
    validate_linear_shape(layer, input, output);
    double elapsed = 0.0;
    switch (layer.storage) {
        case LlamaGgufLinearStorage::q4_k:
            elapsed = measure_ms([&]() {
                linear_q4_k_direct_parallel(layer, q8_input, output, options, worker_pool);
            });
            hotspots.q4_k_direct_ms += elapsed;
            ++hotspots.q4_k_direct_calls;
            break;
        case LlamaGgufLinearStorage::q5_0_packed:
            elapsed = measure_ms([&]() {
                linear_q5_0_packed_direct_parallel(layer,
                                                   q8_0_input,
                                                   output,
                                                   options,
                                                   worker_pool);
            });
            hotspots.q5_0_packed_ms += elapsed;
            ++hotspots.q5_0_packed_calls;
            break;
        case LlamaGgufLinearStorage::q6_k_direct:
            elapsed = measure_ms([&]() {
                linear_ggml_quantized_direct_parallel(layer,
                                                      q8_0_input,
                                                      output,
                                                      options,
                                                      worker_pool);
            });
            hotspots.q6_k_direct_ms += elapsed;
            ++hotspots.q6_k_direct_calls;
            break;
        case LlamaGgufLinearStorage::q8_0_direct:
            elapsed = measure_ms([&]() {
                linear_ggml_quantized_direct_parallel(layer,
                                                      q8_0_input,
                                                      output,
                                                      options,
                                                      worker_pool);
            });
            hotspots.q8_0_direct_ms += elapsed;
            ++hotspots.q8_0_direct_calls;
            break;
        case LlamaGgufLinearStorage::ggml_quantized:
            elapsed = measure_ms([&]() {
                linear_ggml_quantized_direct_parallel(layer,
                                                      q8_0_input,
                                                      output,
                                                      options,
                                                      worker_pool);
            });
            hotspots.ggml_direct_ms += elapsed;
            ++hotspots.ggml_direct_calls;
            break;
        case LlamaGgufLinearStorage::f32:
            elapsed = measure_ms([&]() {
                linear_f32_fallback_parallel(layer, input, output, options, worker_pool);
            });
            hotspots.f32_fallback_ms += elapsed;
            ++hotspots.f32_fallback_calls;
            break;
    }
    linear_hotspot_field(hotspots, operation) += elapsed;
}

[[nodiscard]] bool uses_q4_k_direct(const LlamaGgufLinearWeights& layer) noexcept;
[[nodiscard]] bool uses_q5_0_packed_direct(const LlamaGgufLinearWeights& layer) noexcept;
[[nodiscard]] bool uses_ggml_direct(const LlamaGgufLinearWeights& layer) noexcept;
[[nodiscard]] bool uses_q8_0_activation_direct(const LlamaGgufLinearWeights& layer) noexcept;
[[nodiscard]] std::size_t q8_scratch_blocks(std::int32_t input_size);
void prepare_q8_if_needed(std::span<const float> input,
                          std::span<Q8KBlock> scratch,
                          bool needed);
[[nodiscard]] std::size_t q8_0_scratch_blocks(std::int32_t input_size);
void prepare_q8_0_if_needed(std::span<const float> input,
                            std::span<Q8_0InputBlock> scratch,
                            bool needed);

void linear_gguf_batch(const LlamaGgufLinearWeights& layer,
                       std::span<const float> input,
                       std::size_t batch_size,
                       std::vector<Q8KBlock>& q8_scratch,
                       std::vector<Q8_0InputBlock>& q8_0_scratch,
                       std::span<float> output,
                       LlamaGgufLinearOp operation,
                       LlamaGgufHotspotReport& hotspots,
                       const LlamaGgufExecutionOptions& options,
                       LlamaParallelWorkerPool* worker_pool) {
    const std::size_t input_size = checked_size(layer.input_size, "linear input size");
    const std::size_t output_size = checked_size(layer.output_size, "linear output size");
    if (input.size() != batch_size * input_size || output.size() != batch_size * output_size) {
        throw std::runtime_error("LLaMA GGUF batch linear input/output size mismatch");
    }

    if (layer.storage == LlamaGgufLinearStorage::f32) {
        const double elapsed = measure_ms([&]() {
            linear_f32_fallback_batch_parallel(layer,
                                               input,
                                               batch_size,
                                               output,
                                               options,
                                               worker_pool);
        });
        hotspots.f32_fallback_ms += elapsed;
        ++hotspots.f32_fallback_calls;
        linear_hotspot_field(hotspots, operation) += elapsed;
        return;
    }

    if (layer.storage == LlamaGgufLinearStorage::q5_0_packed) {
        const double elapsed = measure_ms([&]() {
            linear_f32_fallback_batch_parallel(layer,
                                               input,
                                               batch_size,
                                               output,
                                               options,
                                               worker_pool);
        });
        hotspots.q5_0_batch_f32_ms += elapsed;
        ++hotspots.q5_0_batch_f32_calls;
        linear_hotspot_field(hotspots, operation) += elapsed;
        return;
    }

    if (layer.storage == LlamaGgufLinearStorage::q6_k_direct ||
        layer.storage == LlamaGgufLinearStorage::q8_0_direct ||
        layer.storage == LlamaGgufLinearStorage::ggml_quantized) {
        const double elapsed = measure_ms([&]() {
            linear_ggml_quantized_direct_batch_parallel(layer,
                                                       input,
                                                       batch_size,
                                                       q8_0_scratch,
                                                       output,
                                                       options,
                                                       worker_pool);
        });
        if (layer.storage == LlamaGgufLinearStorage::q6_k_direct) {
            hotspots.q6_k_direct_ms += elapsed;
            ++hotspots.q6_k_direct_calls;
        } else if (layer.storage == LlamaGgufLinearStorage::q8_0_direct) {
            hotspots.q8_0_direct_ms += elapsed;
            ++hotspots.q8_0_direct_calls;
        } else {
            hotspots.ggml_direct_ms += elapsed;
            ++hotspots.ggml_direct_calls;
        }
        linear_hotspot_field(hotspots, operation) += elapsed;
        return;
    }

    q8_scratch.resize(q8_scratch_blocks(layer.input_size));
    q8_0_scratch.resize(q8_0_scratch_blocks(layer.input_size));
    for (std::size_t batch = 0; batch < batch_size; ++batch) {
        const std::span<const float> token_input =
            input.subspan(batch * input_size, input_size);
        std::span<float> token_output =
            output.subspan(batch * output_size, output_size);
        prepare_q8_if_needed(token_input, q8_scratch, uses_q4_k_direct(layer));
        prepare_q8_0_if_needed(token_input,
                               q8_0_scratch,
                               uses_q8_0_activation_direct(layer));
        linear_gguf(layer,
                    token_input,
                    q8_scratch,
                    q8_0_scratch,
                    token_output,
                    operation,
                    hotspots,
                    options,
                    worker_pool);
    }
}

[[nodiscard]] bool uses_q4_k_direct(const LlamaGgufLinearWeights& layer) noexcept {
    return layer.storage == LlamaGgufLinearStorage::q4_k;
}

[[nodiscard]] bool uses_q5_0_packed_direct(const LlamaGgufLinearWeights& layer) noexcept {
    return layer.storage == LlamaGgufLinearStorage::q5_0_packed;
}

[[nodiscard]] bool uses_ggml_direct(const LlamaGgufLinearWeights& layer) noexcept {
    return layer.storage == LlamaGgufLinearStorage::ggml_quantized;
}

[[nodiscard]] bool uses_q8_0_activation_direct(const LlamaGgufLinearWeights& layer) noexcept {
    return uses_q5_0_packed_direct(layer) ||
           layer.storage == LlamaGgufLinearStorage::q6_k_direct ||
           layer.storage == LlamaGgufLinearStorage::q8_0_direct ||
           uses_ggml_direct(layer);
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

[[nodiscard]] std::size_t q8_0_scratch_blocks(std::int32_t input_size) {
    if (input_size <= 0 || input_size % LCQI_Q8_0_INPUT_BLOCK_VALUES != 0) {
        return 0;
    }
    return checked_size(input_size / LCQI_Q8_0_INPUT_BLOCK_VALUES, "Q8_0 scratch blocks");
}

void prepare_q8_0_if_needed(std::span<const float> input,
                            std::span<Q8_0InputBlock> scratch,
                            bool needed) {
    if (!needed) {
        return;
    }
    quantize_q8_0_input_to(input, scratch);
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
    std::vector<Q8_0InputBlock> normed_q8_0;
    std::vector<Q8_0InputBlock> attention_q8_0;
    std::vector<Q8_0InputBlock> mlp_hidden_q8_0;

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
          mlp_hidden_q8(q8_scratch_blocks(config.intermediate_size)),
          normed_q8_0(q8_0_scratch_blocks(config.hidden_size)),
          attention_q8_0(normed_q8_0.size()),
          mlp_hidden_q8_0(q8_0_scratch_blocks(config.intermediate_size)) {}
};

struct LlamaBatchWorkspace {
    std::size_t batch_size = 0;
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
    std::vector<Q8KBlock> q8_scratch;
    std::vector<Q8_0InputBlock> q8_0_scratch;

    LlamaBatchWorkspace(const DecoderConfig& config, std::size_t requested_batch_size)
        : batch_size(requested_batch_size),
          hidden(batch_size * checked_size(config.hidden_size, "hidden_size"), 0.0F),
          normed(hidden.size(), 0.0F),
          query(hidden.size(), 0.0F),
          key(batch_size * checked_size(config.kv_heads * config.head_dim, "kv_width"), 0.0F),
          value(key.size(), 0.0F),
          attention(hidden.size(), 0.0F),
          projected(hidden.size(), 0.0F),
          gate(batch_size * checked_size(config.intermediate_size, "intermediate_size"), 0.0F),
          up(gate.size(), 0.0F),
          mlp_hidden(gate.size(), 0.0F),
          mlp_out(hidden.size(), 0.0F),
          final_norm(checked_size(config.hidden_size, "hidden_size"), 0.0F) {}
};

void forward_llama_layer(const DecoderConfig& config,
                         const LlamaGgufDecoderLayerWeights& layer,
                         std::int32_t layer_id,
                         std::int32_t model_position,
                         ReferenceKVCache& cache,
                         LlamaWorkspace& workspace,
                         LlamaGgufHotspotReport& hotspots,
                         const LlamaGgufExecutionOptions& options,
                         LlamaParallelWorkerPool* worker_pool) {
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
    prepare_q8_0_if_needed(workspace.normed,
                           workspace.normed_q8_0,
                           uses_q8_0_activation_direct(layer.wq) ||
                               uses_q8_0_activation_direct(layer.wk) ||
                               uses_q8_0_activation_direct(layer.wv));
    linear_gguf(layer.wq,
                workspace.normed,
                workspace.normed_q8,
                workspace.normed_q8_0,
                workspace.query,
                LlamaGgufLinearOp::wq,
                hotspots,
                options,
                worker_pool);
    linear_gguf(layer.wk,
                workspace.normed,
                workspace.normed_q8,
                workspace.normed_q8_0,
                workspace.key,
                LlamaGgufLinearOp::wk,
                hotspots,
                options,
                worker_pool);
    linear_gguf(layer.wv,
                workspace.normed,
                workspace.normed_q8,
                workspace.normed_q8_0,
                workspace.value,
                LlamaGgufLinearOp::wv,
                hotspots,
                options,
                worker_pool);
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
    prepare_q8_0_if_needed(workspace.attention,
                           workspace.attention_q8_0,
                           uses_q8_0_activation_direct(layer.wo));
    linear_gguf(layer.wo,
                workspace.attention,
                workspace.attention_q8,
                workspace.attention_q8_0,
                workspace.projected,
                LlamaGgufLinearOp::wo,
                hotspots,
                options,
                worker_pool);
    add_inplace(workspace.hidden, workspace.projected);

    hotspots.rms_norm_ms += measure_ms([&]() {
        rms_norm(workspace.hidden, layer.rms_mlp_weight, config.rms_epsilon, workspace.normed);
    });
    prepare_q8_if_needed(workspace.normed,
                         workspace.normed_q8,
                         uses_q4_k_direct(layer.w_gate) || uses_q4_k_direct(layer.w_up));
    prepare_q8_0_if_needed(workspace.normed,
                           workspace.normed_q8_0,
                           uses_q8_0_activation_direct(layer.w_gate) ||
                               uses_q8_0_activation_direct(layer.w_up));
    linear_gguf(layer.w_gate,
                workspace.normed,
                workspace.normed_q8,
                workspace.normed_q8_0,
                workspace.gate,
                LlamaGgufLinearOp::w_gate,
                hotspots,
                options,
                worker_pool);
    linear_gguf(layer.w_up,
                workspace.normed,
                workspace.normed_q8,
                workspace.normed_q8_0,
                workspace.up,
                LlamaGgufLinearOp::w_up,
                hotspots,
                options,
                worker_pool);
    for (std::size_t index = 0; index < workspace.mlp_hidden.size(); ++index) {
        workspace.mlp_hidden[index] = silu(workspace.gate[index]) * workspace.up[index];
    }
    prepare_q8_if_needed(workspace.mlp_hidden,
                         workspace.mlp_hidden_q8,
                         uses_q4_k_direct(layer.w_down));
    prepare_q8_0_if_needed(workspace.mlp_hidden,
                           workspace.mlp_hidden_q8_0,
                           uses_q8_0_activation_direct(layer.w_down));
    linear_gguf(layer.w_down,
                workspace.mlp_hidden,
                workspace.mlp_hidden_q8,
                workspace.mlp_hidden_q8_0,
                workspace.mlp_out,
                LlamaGgufLinearOp::w_down,
                hotspots,
                options,
                worker_pool);
    add_inplace(workspace.hidden, workspace.mlp_out);
}

void forward_llama_layer_prefill_batch(const DecoderConfig& config,
                                       const LlamaGgufDecoderLayerWeights& layer,
                                       std::int32_t layer_id,
                                       std::int32_t position_begin,
                                       ReferenceKVCache& cache,
                                       LlamaBatchWorkspace& workspace,
                                       LlamaGgufHotspotReport& hotspots,
                                       const LlamaGgufExecutionOptions& options,
                                       LlamaParallelWorkerPool* worker_pool) {
    const std::size_t hidden_size = checked_size(config.hidden_size, "hidden_size");
    const std::size_t kv_width = checked_size(config.kv_heads * config.head_dim, "kv_width");
    const std::size_t intermediate_size =
        checked_size(config.intermediate_size, "intermediate_size");

    hotspots.rms_norm_ms += measure_ms([&]() {
        for (std::size_t batch = 0; batch < workspace.batch_size; ++batch) {
            rms_norm(mutable_row_span(workspace.hidden, batch, hidden_size),
                     layer.rms_attention_weight,
                     config.rms_epsilon,
                     mutable_row_span(workspace.normed, batch, hidden_size));
        }
    });
    linear_gguf_batch(layer.wq,
                      workspace.normed,
                      workspace.batch_size,
                      workspace.q8_scratch,
                      workspace.q8_0_scratch,
                      workspace.query,
                      LlamaGgufLinearOp::wq,
                      hotspots,
                      options,
                      worker_pool);
    linear_gguf_batch(layer.wk,
                      workspace.normed,
                      workspace.batch_size,
                      workspace.q8_scratch,
                      workspace.q8_0_scratch,
                      workspace.key,
                      LlamaGgufLinearOp::wk,
                      hotspots,
                      options,
                      worker_pool);
    linear_gguf_batch(layer.wv,
                      workspace.normed,
                      workspace.batch_size,
                      workspace.q8_scratch,
                      workspace.q8_0_scratch,
                      workspace.value,
                      LlamaGgufLinearOp::wv,
                      hotspots,
                      options,
                      worker_pool);
    hotspots.rope_ms += measure_ms([&]() {
        for (std::size_t batch = 0; batch < workspace.batch_size; ++batch) {
            const std::int32_t model_position =
                position_begin + static_cast<std::int32_t>(batch);
            apply_rope(mutable_row_span(workspace.query, batch, hidden_size),
                       config.query_heads,
                       config.head_dim,
                       model_position,
                       config.rope_base);
            apply_rope(mutable_row_span(workspace.key, batch, kv_width),
                       config.kv_heads,
                       config.head_dim,
                       model_position,
                       config.rope_base);
        }
    });
    for (std::size_t batch = 0; batch < workspace.batch_size; ++batch) {
        const std::int32_t model_position = position_begin + static_cast<std::int32_t>(batch);
        cache.append(layer_id,
                     model_position,
                     mutable_row_span(workspace.key, batch, kv_width),
                     mutable_row_span(workspace.value, batch, kv_width));
    }
    hotspots.attention_ms += measure_ms([&]() {
        for (std::size_t batch = 0; batch < workspace.batch_size; ++batch) {
            const std::int32_t model_position =
                position_begin + static_cast<std::int32_t>(batch);
            attention_decode(config,
                             cache,
                             layer_id,
                             model_position,
                             mutable_row_span(workspace.query, batch, hidden_size),
                             mutable_row_span(workspace.attention, batch, hidden_size));
        }
    });
    linear_gguf_batch(layer.wo,
                      workspace.attention,
                      workspace.batch_size,
                      workspace.q8_scratch,
                      workspace.q8_0_scratch,
                      workspace.projected,
                      LlamaGgufLinearOp::wo,
                      hotspots,
                      options,
                      worker_pool);
    for (std::size_t batch = 0; batch < workspace.batch_size; ++batch) {
        add_inplace(mutable_row_span(workspace.hidden, batch, hidden_size),
                    mutable_row_span(workspace.projected, batch, hidden_size));
    }

    hotspots.rms_norm_ms += measure_ms([&]() {
        for (std::size_t batch = 0; batch < workspace.batch_size; ++batch) {
            rms_norm(mutable_row_span(workspace.hidden, batch, hidden_size),
                     layer.rms_mlp_weight,
                     config.rms_epsilon,
                     mutable_row_span(workspace.normed, batch, hidden_size));
        }
    });
    linear_gguf_batch(layer.w_gate,
                      workspace.normed,
                      workspace.batch_size,
                      workspace.q8_scratch,
                      workspace.q8_0_scratch,
                      workspace.gate,
                      LlamaGgufLinearOp::w_gate,
                      hotspots,
                      options,
                      worker_pool);
    linear_gguf_batch(layer.w_up,
                      workspace.normed,
                      workspace.batch_size,
                      workspace.q8_scratch,
                      workspace.q8_0_scratch,
                      workspace.up,
                      LlamaGgufLinearOp::w_up,
                      hotspots,
                      options,
                      worker_pool);
    for (std::size_t batch = 0; batch < workspace.batch_size; ++batch) {
        std::span<float> mlp_hidden = mutable_row_span(workspace.mlp_hidden,
                                                       batch,
                                                       intermediate_size);
        std::span<float> gate = mutable_row_span(workspace.gate, batch, intermediate_size);
        std::span<float> up = mutable_row_span(workspace.up, batch, intermediate_size);
        for (std::size_t index = 0; index < intermediate_size; ++index) {
            mlp_hidden[index] = silu(gate[index]) * up[index];
        }
    }
    linear_gguf_batch(layer.w_down,
                      workspace.mlp_hidden,
                      workspace.batch_size,
                      workspace.q8_scratch,
                      workspace.q8_0_scratch,
                      workspace.mlp_out,
                      LlamaGgufLinearOp::w_down,
                      hotspots,
                      options,
                      worker_pool);
    for (std::size_t batch = 0; batch < workspace.batch_size; ++batch) {
        add_inplace(mutable_row_span(workspace.hidden, batch, hidden_size),
                    mutable_row_span(workspace.mlp_out, batch, hidden_size));
    }
}

[[nodiscard]] std::int32_t predict_next_token(const LlamaGgufModel& model,
                                              std::span<const float> final_hidden,
                                              LlamaGgufHotspotReport& hotspots,
                                              const LlamaGgufExecutionOptions& options,
                                              LlamaParallelWorkerPool* worker_pool) {
    const DecoderConfig& config = model.decoder.config;
    F32RowMax best;
    if (model.lm_head_tied_to_embedding) {
        if (model.decoder.tied_lm_head.storage == LlamaGgufLinearStorage::q8_0_direct) {
            const std::vector<Q8_0InputBlock> final_hidden_q8_0 =
                quantize_q8_0_input(final_hidden);
            const double elapsed = measure_ms([&]() {
                best = max_dot_ggml_quantized_q8_0_parallel(model.decoder.tied_lm_head,
                                                            final_hidden_q8_0,
                                                            options,
                                                            worker_pool);
            });
            hotspots.q8_0_direct_ms += elapsed;
            ++hotspots.q8_0_direct_calls;
            hotspots.lm_head_ms += elapsed;
            return static_cast<std::int32_t>(best.row);
        }
        const std::span<const float> weights = model.decoder.token_embedding;
        const std::size_t hidden_size = checked_size(config.hidden_size, "hidden_size");
        const std::size_t vocab_size = checked_size(config.vocab_size, "vocab_size");
        hotspots.lm_head_ms += measure_ms([&]() {
            best = max_dot_f32_rows_parallel(weights.data(),
                                             final_hidden.data(),
                                             hidden_size,
                                             vocab_size,
                                             options,
                                             worker_pool);
        });
        return static_cast<std::int32_t>(best.row);
    }

    std::vector<float> logits(checked_size(config.vocab_size, "vocab_size"), 0.0F);
    const std::vector<Q8KBlock> final_hidden_q8 =
        uses_q4_k_direct(model.decoder.lm_head)
            ? quantize_q8_k_input(final_hidden)
            : std::vector<Q8KBlock>{};
    const std::vector<Q8_0InputBlock> final_hidden_q8_0 =
        uses_q8_0_activation_direct(model.decoder.lm_head)
            ? quantize_q8_0_input(final_hidden)
            : std::vector<Q8_0InputBlock>{};
    linear_gguf(model.decoder.lm_head,
                final_hidden,
                final_hidden_q8,
                final_hidden_q8_0,
                logits,
                LlamaGgufLinearOp::lm_head,
                hotspots,
                options,
                worker_pool);
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
                                              LlamaGgufHotspotReport& hotspots,
                                              const LlamaGgufExecutionOptions& options,
                                              LlamaParallelWorkerPool* worker_pool) {
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
                            hotspots,
                            options,
                            worker_pool);
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
    return predict_next_token(model, workspace.final_norm, hotspots, options, worker_pool);
}

[[nodiscard]] std::int32_t prefill_llama_decoder_batch(const LlamaGgufModel& model,
                                                       const DecoderConfig& active_config,
                                                       ReferenceKVCache& cache,
                                                       std::span<const std::int32_t> prompt_ids,
                                                       LlamaGgufHotspotReport& hotspots,
                                                       const LlamaGgufExecutionOptions& options,
                                                       LlamaParallelWorkerPool* worker_pool) {
    const DecoderConfig& original_config = model.decoder.config;
    const std::size_t batch_size = prompt_ids.size();
    const std::size_t hidden_size = checked_size(original_config.hidden_size, "hidden_size");
    LlamaBatchWorkspace workspace(active_config, batch_size);
    for (std::size_t batch = 0; batch < batch_size; ++batch) {
        const std::int32_t token_id = prompt_ids[batch];
        if (token_id < 0 || token_id >= original_config.vocab_size) {
            throw std::runtime_error("LLaMA token id out of vocabulary");
        }
        const std::span<const float> embedding =
            row_span(model.decoder.token_embedding, token_id, original_config.hidden_size);
        std::copy(embedding.begin(),
                  embedding.end(),
                  workspace.hidden.begin() + static_cast<std::ptrdiff_t>(batch * hidden_size));
    }

    const std::int32_t position_begin = cache.filled_tokens();
    if (position_begin + static_cast<std::int32_t>(batch_size) > active_config.max_context) {
        throw std::runtime_error("LLaMA generation exceeded active context");
    }
    for (std::int32_t layer_id = 0;
         layer_id < static_cast<std::int32_t>(model.decoder.layers.size());
         ++layer_id) {
        forward_llama_layer_prefill_batch(active_config,
                                          model.decoder.layers[checked_size(layer_id,
                                                                            "layer_id")],
                                          layer_id,
                                          position_begin,
                                          cache,
                                          workspace,
                                          hotspots,
                                          options,
                                          worker_pool);
    }
    hotspots.rms_norm_ms += measure_ms([&]() {
        rms_norm(mutable_row_span(workspace.hidden, batch_size - 1U, hidden_size),
                 model.decoder.final_norm_weight,
                 active_config.rms_epsilon,
                 workspace.final_norm);
    });
    return predict_next_token(model, workspace.final_norm, hotspots, options, worker_pool);
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
        loaded.model.decoder.tied_lm_head =
            make_tied_lm_head_from_embedding(gguf_path,
                                             manifest,
                                             config.hidden_size,
                                             config.vocab_size,
                                             loaded.report);
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
    return llama_gguf_generate_greedy(model,
                                      prompt_ids,
                                      max_new_tokens,
                                      LlamaGgufExecutionOptions{});
}

LlamaGgufGenerationResult llama_gguf_generate_greedy(
    const LlamaGgufModel& model,
    std::span<const std::int32_t> prompt_ids,
    std::int32_t max_new_tokens,
    const LlamaGgufExecutionOptions& options) {
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
    std::unique_ptr<LlamaParallelWorkerPool> worker_pool = make_worker_pool(model, options);

    LlamaGgufGenerationResult result;
    result.prompt_ids.assign(prompt_ids.begin(), prompt_ids.end());
    result.generated_ids.assign(prompt_ids.begin(), prompt_ids.end());
    result.kv_cache_bytes = kv_cache_bytes(active_config, layer_count);
    result.worker_count = worker_pool ? worker_pool->worker_count() : 1;
    if (max_new_tokens == 0) {
        return result;
    }

    const Clock::time_point prefill_begin = Clock::now();
    std::int32_t predicted = -1;
    if (batch_prefill_enabled() && prompt_ids.size() > 1U) {
        predicted = prefill_llama_decoder_batch(model,
                                                active_config,
                                                cache,
                                                prompt_ids,
                                                result.hotspots,
                                                options,
                                                worker_pool.get());
    } else {
        for (std::size_t index = 0; index + 1 < prompt_ids.size(); ++index) {
            static_cast<void>(step_llama_decoder(model,
                                                 active_config,
                                                 cache,
                                                 workspace,
                                                 prompt_ids[index],
                                                 false,
                                                 result.hotspots,
                                                 options,
                                                 worker_pool.get()));
        }
        predicted = step_llama_decoder(model,
                                       active_config,
                                       cache,
                                       workspace,
                                       prompt_ids.back(),
                                       true,
                                       result.hotspots,
                                       options,
                                       worker_pool.get());
    }
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
                                           result.hotspots,
                                           options,
                                           worker_pool.get());
            ++result.decode_steps;
        }
    }
    result.decode_ms = elapsed_ms(decode_begin, Clock::now());
    result.total_ms = result.prefill_ms + result.decode_ms;
    return result;
}

}  // namespace lcqi

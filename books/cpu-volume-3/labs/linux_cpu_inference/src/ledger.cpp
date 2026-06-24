#include <cstdlib>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <iostream>
#include <string_view>

namespace {

constexpr double LCQI_BYTES_PER_GB = 1000.0 * 1000.0 * 1000.0;
constexpr double LCQI_BYTES_PER_MIB = 1024.0 * 1024.0;
constexpr double LCQI_FLOPS_PER_MAC = 2.0;
constexpr std::int32_t LCQI_SEVENB_LAYER_COUNT = 32;
constexpr std::int32_t LCQI_SEVENB_HIDDEN_SIZE = 4096;
constexpr std::int32_t LCQI_SEVENB_QUERY_HEADS = 32;
constexpr std::int32_t LCQI_SEVENB_KV_HEADS = 8;
constexpr std::int32_t LCQI_SEVENB_HEAD_DIM = 128;
constexpr std::int32_t LCQI_SEVENB_INTERMEDIATE_SIZE = 11008;
constexpr std::int32_t LCQI_SEVENB_CONTEXT_LENGTH = 4096;

struct DTypeLedger {
    std::string_view name;
    double weight_gb_per_token = 0.0;
    double kv_element_bytes = 0.0;
};

constexpr DTypeLedger LCQI_DTYPE_LEDGERS[] = {
    {"fp16", 14.0, 2.0},
    {"int8", 7.0, 2.0},
    {"int4", 3.7, 2.0},
};

constexpr double LCQI_BANDWIDTH_GB_PER_S[] = {
    50.0,
    100.0,
    200.0,
    600.0,
    1000.0,
    1600.0,
};

double linear_macs_per_layer() noexcept {
    const double hidden = LCQI_SEVENB_HIDDEN_SIZE;
    const double kv_width = LCQI_SEVENB_KV_HEADS * LCQI_SEVENB_HEAD_DIM;
    const double qkv = hidden * (hidden + 2.0 * kv_width);
    const double output_projection = hidden * hidden;
    const double mlp = 3.0 * hidden * LCQI_SEVENB_INTERMEDIATE_SIZE;
    return qkv + output_projection + mlp;
}

double attention_macs_per_layer() noexcept {
    return 2.0 * LCQI_SEVENB_QUERY_HEADS * LCQI_SEVENB_CONTEXT_LENGTH *
           LCQI_SEVENB_HEAD_DIM;
}

double kv_bytes_per_token_all_layers(double element_bytes) noexcept {
    return static_cast<double>(LCQI_SEVENB_LAYER_COUNT) * 2.0 *
           LCQI_SEVENB_KV_HEADS * LCQI_SEVENB_HEAD_DIM * element_bytes;
}

double kv_read_gb_per_decode_token(double element_bytes) noexcept {
    const double bytes = static_cast<double>(LCQI_SEVENB_CONTEXT_LENGTH) *
                         kv_bytes_per_token_all_layers(element_bytes);
    return bytes / LCQI_BYTES_PER_GB;
}

double kv_capacity_mib(double element_bytes, std::int32_t context_length) noexcept {
    const double bytes = static_cast<double>(context_length) *
                         kv_bytes_per_token_all_layers(element_bytes);
    return bytes / LCQI_BYTES_PER_MIB;
}

void print_model_rows() {
    const double linear_macs = linear_macs_per_layer();
    const double attention_macs = attention_macs_per_layer();
    const double total_flops =
        (linear_macs + attention_macs) * LCQI_SEVENB_LAYER_COUNT * LCQI_FLOPS_PER_MAC;
    std::cout << "section,item,value,unit,notes\n";
    std::cout << "model,layers," << LCQI_SEVENB_LAYER_COUNT << ",count,7B fixture\n";
    std::cout << "model,hidden," << LCQI_SEVENB_HIDDEN_SIZE << ",elements,H\n";
    std::cout << "model,query_heads," << LCQI_SEVENB_QUERY_HEADS << ",count,GQA query heads\n";
    std::cout << "model,kv_heads," << LCQI_SEVENB_KV_HEADS << ",count,GQA KV heads\n";
    std::cout << "model,head_dim," << LCQI_SEVENB_HEAD_DIM << ",elements,per head\n";
    std::cout << "compute,linear_macs_per_layer," << linear_macs << ",macs,decode token\n";
    std::cout << "compute,attention_macs_per_layer," << attention_macs
              << ",macs,context=4096\n";
    std::cout << "compute,total_decode_flops_per_token," << total_flops
              << ",flops,linear plus attention\n";
}

void print_kv_rows() {
    for (const std::int32_t context : {1024, 2048, 4096, 8192}) {
        std::cout << "kv,fp16_capacity_context_" << context << ','
                  << kv_capacity_mib(2.0, context)
                  << ",MiB,single session\n";
    }
}

void print_dtype_rows() {
    for (const DTypeLedger& dtype : LCQI_DTYPE_LEDGERS) {
        const double kv_read_gb = kv_read_gb_per_decode_token(dtype.kv_element_bytes);
        const double total_gb = dtype.weight_gb_per_token + kv_read_gb;
        std::cout << "decode_bytes," << dtype.name << "_weight_gb,"
                  << dtype.weight_gb_per_token << ",GB/token,weight stream\n";
        std::cout << "decode_bytes," << dtype.name << "_kv_read_gb,"
                  << kv_read_gb << ",GB/token,context=4096\n";
        std::cout << "decode_bytes," << dtype.name << "_total_gb,"
                  << total_gb << ",GB/token,weight plus KV\n";
        for (const double bandwidth : LCQI_BANDWIDTH_GB_PER_S) {
            std::cout << "bandwidth_limit," << dtype.name << "_at_"
                      << static_cast<std::int32_t>(bandwidth) << "_gbps,"
                      << bandwidth / total_gb
                      << ",tokens/s,bandwidth-only upper bound\n";
        }
    }
}

}  // namespace

int main() {
    try {
        std::cout << std::fixed << std::setprecision(3);
        print_model_rows();
        print_kv_rows();
        print_dtype_rows();
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}

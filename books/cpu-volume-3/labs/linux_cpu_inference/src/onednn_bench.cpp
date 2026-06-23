#include <dnnl.hpp>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

constexpr std::int64_t LCQI_ONEDNN_BATCH = 1;
constexpr std::int64_t LCQI_ONEDNN_K = 4096;
constexpr std::int64_t LCQI_ONEDNN_O = 4096;
constexpr std::int32_t LCQI_ONEDNN_DEFAULT_REPEAT = 20;
constexpr std::int32_t LCQI_ONEDNN_WARMUP = 3;
constexpr int LCQI_REPEAT_ARG_INDEX = 1;
constexpr double LCQI_SECONDS_TO_MICROSECONDS = 1000000.0;
constexpr float LCQI_INPUT_SCALE = 0.125F;
constexpr std::int32_t LCQI_INPUT_MODULUS = 17;
constexpr std::int32_t LCQI_INPUT_CENTER = 8;
constexpr std::int32_t LCQI_WEIGHT_MODULUS = 23;
constexpr std::int32_t LCQI_WEIGHT_CENTER = 11;
constexpr float LCQI_WEIGHT_SCALE = 0.03125F;

std::int32_t parse_repeat(int argc, char** argv) {
    if (argc <= LCQI_REPEAT_ARG_INDEX) {
        return LCQI_ONEDNN_DEFAULT_REPEAT;
    }
    const std::int32_t repeat = static_cast<std::int32_t>(std::stoi(argv[LCQI_REPEAT_ARG_INDEX]));
    if (repeat <= 0) {
        throw std::runtime_error("repeat must be positive");
    }
    return repeat;
}

std::vector<float> make_input() {
    std::vector<float> input(static_cast<std::size_t>(LCQI_ONEDNN_K));
    for (std::int64_t index = 0; index < LCQI_ONEDNN_K; ++index) {
        input[static_cast<std::size_t>(index)] =
            static_cast<float>((index % LCQI_INPUT_MODULUS) - LCQI_INPUT_CENTER) *
            LCQI_INPUT_SCALE;
    }
    return input;
}

std::vector<float> make_weights_kn() {
    std::vector<float> weights(static_cast<std::size_t>(LCQI_ONEDNN_K * LCQI_ONEDNN_O));
    for (std::int64_t k = 0; k < LCQI_ONEDNN_K; ++k) {
        for (std::int64_t out = 0; out < LCQI_ONEDNN_O; ++out) {
            const std::int64_t row_major_out_k = out * LCQI_ONEDNN_K + k;
            const float value =
                static_cast<float>((row_major_out_k % LCQI_WEIGHT_MODULUS) -
                                   LCQI_WEIGHT_CENTER) *
                LCQI_WEIGHT_SCALE;
            weights[static_cast<std::size_t>(k * LCQI_ONEDNN_O + out)] = value;
        }
    }
    return weights;
}

float checksum(const std::vector<float>& values) {
    return std::accumulate(values.begin(), values.end(), 0.0F);
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const std::int32_t repeat = parse_repeat(argc, argv);
        std::vector<float> input = make_input();
        std::vector<float> weights = make_weights_kn();
        std::vector<float> output(static_cast<std::size_t>(LCQI_ONEDNN_BATCH * LCQI_ONEDNN_O),
                                  0.0F);

        dnnl::engine engine(dnnl::engine::kind::cpu, 0);
        dnnl::stream stream(engine);

        const dnnl::memory::dims source_dims = {LCQI_ONEDNN_BATCH, LCQI_ONEDNN_K};
        const dnnl::memory::dims weight_dims = {LCQI_ONEDNN_K, LCQI_ONEDNN_O};
        const dnnl::memory::dims output_dims = {LCQI_ONEDNN_BATCH, LCQI_ONEDNN_O};

        const dnnl::memory::desc source_desc(
            source_dims, dnnl::memory::data_type::f32, dnnl::memory::format_tag::ab);
        const dnnl::memory::desc weight_desc(
            weight_dims, dnnl::memory::data_type::f32, dnnl::memory::format_tag::ab);
        const dnnl::memory::desc output_desc(
            output_dims, dnnl::memory::data_type::f32, dnnl::memory::format_tag::ab);

        dnnl::memory source_memory(source_desc, engine, input.data());
        dnnl::memory weight_memory(weight_desc, engine, weights.data());
        dnnl::memory output_memory(output_desc, engine, output.data());

        dnnl::matmul::primitive_desc matmul_desc(engine, source_desc, weight_desc, output_desc);
        dnnl::matmul matmul(matmul_desc);
        const std::unordered_map<int, dnnl::memory> arguments = {
            {DNNL_ARG_SRC, source_memory},
            {DNNL_ARG_WEIGHTS, weight_memory},
            {DNNL_ARG_DST, output_memory},
        };

        for (std::int32_t index = 0; index < LCQI_ONEDNN_WARMUP; ++index) {
            matmul.execute(stream, arguments);
            stream.wait();
        }

        const auto begin = std::chrono::steady_clock::now();
        for (std::int32_t index = 0; index < repeat; ++index) {
            matmul.execute(stream, arguments);
            stream.wait();
        }
        const auto end = std::chrono::steady_clock::now();
        const std::chrono::duration<double> elapsed = end - begin;
        const double average_us =
            elapsed.count() * LCQI_SECONDS_TO_MICROSECONDS / static_cast<double>(repeat);

        std::cout << "library,input_size,output_size,dtype,repeat,average_us,checksum\n";
        std::cout << "onednn," << LCQI_ONEDNN_K << ',' << LCQI_ONEDNN_O
                  << ",f32," << repeat << ',' << average_us << ','
                  << checksum(output) << '\n';
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}

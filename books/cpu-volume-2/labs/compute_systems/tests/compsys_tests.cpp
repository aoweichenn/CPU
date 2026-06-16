#include <compsys/parallel_reduce.hpp>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {

constexpr std::int32_t COMPSYS_TEST_SIZE = 1000;
constexpr std::int32_t COMPSYS_TEST_WORKERS = 8;
constexpr std::int32_t COMPSYS_TEST_COUNTER_THREADS = 4;
constexpr std::int32_t COMPSYS_TEST_COUNTER_STEPS = 250;
constexpr std::int32_t COMPSYS_TEST_QUEUE_CAPACITY = 3;

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::vector<std::int32_t> make_values() {
    std::vector<std::int32_t> values(static_cast<std::size_t>(COMPSYS_TEST_SIZE));
    for (std::int32_t i = 0; i < COMPSYS_TEST_SIZE; ++i) {
        values[static_cast<std::size_t>(i)] = (i % 11) - 5;
    }
    return values;
}

void test_parallel_sum() {
    const std::vector<std::int32_t> values = make_values();
    const std::int64_t sequential = compsys::sequential_sum(values);
    const std::int64_t parallel = compsys::parallel_sum(values, COMPSYS_TEST_WORKERS);
    require(sequential == parallel, "parallel sum mismatch");
}

void test_counters() {
    compsys::MutexCounter mutex_counter;
    compsys::AtomicCounter atomic_counter;
    std::vector<std::thread> threads;
    threads.reserve(static_cast<std::size_t>(COMPSYS_TEST_COUNTER_THREADS));

    for (std::int32_t worker = 0; worker < COMPSYS_TEST_COUNTER_THREADS; ++worker) {
        threads.emplace_back([&mutex_counter, &atomic_counter]() {
            for (std::int32_t i = 0; i < COMPSYS_TEST_COUNTER_STEPS; ++i) {
                mutex_counter.add(1);
                atomic_counter.add(1);
            }
        });
    }

    for (std::thread& thread : threads) {
        thread.join();
    }

    const std::int64_t expected =
        static_cast<std::int64_t>(COMPSYS_TEST_COUNTER_THREADS) *
        static_cast<std::int64_t>(COMPSYS_TEST_COUNTER_STEPS);
    require(mutex_counter.value() == expected, "mutex counter mismatch");
    require(atomic_counter.value() == expected, "atomic counter mismatch");
}

void test_queue() {
    compsys::BoundedMpmcQueue queue(COMPSYS_TEST_QUEUE_CAPACITY);
    queue.push(7);
    queue.push(9);
    require(queue.size() == 2, "queue size mismatch after push");

    const std::optional<std::int32_t> first = queue.try_pop();
    const std::optional<std::int32_t> second = queue.try_pop();
    const std::optional<std::int32_t> third = queue.try_pop();

    require(first.has_value() && first.value() == 7, "first queue value mismatch");
    require(second.has_value() && second.value() == 9, "second queue value mismatch");
    require(!third.has_value(), "empty queue should return nullopt");
}

void test_invalid_queue_capacity() {
    bool threw = false;
    try {
        compsys::BoundedMpmcQueue queue(0);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    require(threw, "invalid queue capacity should throw");
}

}  // namespace

int main() {
    try {
        test_parallel_sum();
        test_counters();
        test_queue();
        test_invalid_queue_capacity();
        std::cout << "compsys tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "compsys test failure: " << error.what() << "\n";
        return EXIT_FAILURE;
    }
}

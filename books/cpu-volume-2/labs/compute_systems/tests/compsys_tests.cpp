#include <compsys/futex_lab.hpp>
#include <compsys/parallel_reduce.hpp>
#include <compsys/sync_primitives.hpp>
#include <compsys/task_runtime.hpp>
#include <compsys/wait_channels.hpp>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {

constexpr std::int32_t COMPSYS_TEST_SIZE = 1000;
constexpr std::int32_t COMPSYS_TEST_WORKERS = 8;
constexpr std::int32_t COMPSYS_TEST_COUNTER_THREADS = 4;
constexpr std::int32_t COMPSYS_TEST_COUNTER_STEPS = 250;
constexpr std::int32_t COMPSYS_TEST_QUEUE_CAPACITY = 3;
constexpr std::int32_t COMPSYS_TEST_WAIT_SPIN_LIMIT = 100000;
constexpr std::int32_t COMPSYS_TEST_FUTEX_MUTEX_WORKERS = 4;
constexpr std::int32_t COMPSYS_TEST_FUTEX_MUTEX_INCREMENTS = 50;
constexpr std::int32_t COMPSYS_TEST_RUNTIME_WORKERS = 3;
constexpr std::int32_t COMPSYS_TEST_CONTINUATION_ROOTS = 5;
constexpr std::int64_t COMPSYS_TEST_CONTINUATION_SUM = 15;

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

template <typename Predicate>
bool wait_until(Predicate predicate) {
    for (std::int32_t i = 0; i < COMPSYS_TEST_WAIT_SPIN_LIMIT; ++i) {
        if (predicate()) {
            return true;
        }
        std::this_thread::yield();
    }
    return false;
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
    require(queue.push(7), "first push should succeed");
    require(queue.push(9), "second push should succeed");
    require(queue.size() == 2, "queue size mismatch after push");

    const std::optional<std::int32_t> first = queue.try_pop();
    const std::optional<std::int32_t> second = queue.try_pop();
    const std::optional<std::int32_t> third = queue.try_pop();

    require(first.has_value() && first.value() == 7, "first queue value mismatch");
    require(second.has_value() && second.value() == 9, "second queue value mismatch");
    require(!third.has_value(), "empty queue should return nullopt");

    const compsys::BoundedMpmcQueue::Report report = queue.report();
    require(report.push_success == 2, "queue push report mismatch");
    require(report.pop_success == 2, "queue pop report mismatch");
    require(report.max_size == 2, "queue max size report mismatch");
}

void test_queue_close_wakes_consumer() {
    compsys::BoundedMpmcQueue queue(1);
    std::optional<std::int32_t> result;

    std::thread consumer([&queue, &result]() {
        result = queue.pop();
    });

    const bool saw_wait = wait_until([&queue]() {
        return queue.report().pop_wait_count == 1;
    });
    queue.close();
    consumer.join();

    require(saw_wait, "consumer should enter pop wait before close");
    require(!result.has_value(), "closed empty queue should end pop");
    const compsys::BoundedMpmcQueue::Report report = queue.report();
    require(report.close_count == 1, "close count mismatch");
    require(report.pop_closed == 1, "closed pop report mismatch");
    require(report.pop_wait_count == 1, "closed pop wait count mismatch");
    require(report.pop_wait_ns > 0, "closed pop wait ns should be recorded");
    require(queue.closed(), "queue should be closed");
}

void test_queue_close_wakes_blocked_producer() {
    compsys::BoundedMpmcQueue queue(1);
    require(queue.push(11), "initial push should succeed");

    bool second_push_result = true;
    std::thread producer([&queue, &second_push_result]() {
        second_push_result = queue.push(22);
    });

    const bool saw_wait = wait_until([&queue]() {
        return queue.report().push_wait_count == 1;
    });
    queue.close();
    producer.join();

    require(saw_wait, "producer should enter push wait before close");
    require(!second_push_result, "push waiting on full queue should stop after close");
    const compsys::BoundedMpmcQueue::Report report = queue.report();
    require(report.push_success == 1, "push success report mismatch after close");
    require(report.push_closed == 1, "push closed report mismatch");
    require(report.push_wait_count == 1, "push wait count mismatch");
    require(report.push_wait_ns > 0, "push wait ns should be recorded");
    require(report.close_count == 1, "close report mismatch for blocked producer");
}

void test_queue_multi_producer_consumer_drain() {
    constexpr std::int32_t PRODUCERS = 3;
    constexpr std::int32_t VALUES_PER_PRODUCER = 20;
    constexpr std::int32_t CONSUMERS = 2;
    constexpr std::int32_t EXPECTED_TOTAL = PRODUCERS * VALUES_PER_PRODUCER;

    compsys::BoundedMpmcQueue queue(1);
    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;
    std::vector<std::int32_t> consumed;
    std::mutex consumed_mutex;

    producers.reserve(static_cast<std::size_t>(PRODUCERS));
    consumers.reserve(static_cast<std::size_t>(CONSUMERS));

    for (std::int32_t producer = 0; producer < PRODUCERS; ++producer) {
        producers.emplace_back([producer, &queue]() {
            for (std::int32_t i = 0; i < VALUES_PER_PRODUCER; ++i) {
                const std::int32_t value = producer * VALUES_PER_PRODUCER + i;
                if (!queue.push(value)) {
                    return;
                }
            }
        });
    }

    for (std::int32_t consumer = 0; consumer < CONSUMERS; ++consumer) {
        consumers.emplace_back([&queue, &consumed, &consumed_mutex]() {
            while (true) {
                std::optional<std::int32_t> value = queue.pop();
                if (!value.has_value()) {
                    return;
                }
                std::lock_guard<std::mutex> lock(consumed_mutex);
                consumed.push_back(value.value());
            }
        });
    }

    for (std::thread& producer : producers) {
        producer.join();
    }
    queue.close();
    for (std::thread& consumer : consumers) {
        consumer.join();
    }

    require(static_cast<std::int32_t>(consumed.size()) == EXPECTED_TOTAL,
            "drain queue consumed count mismatch");

    std::vector<bool> seen(static_cast<std::size_t>(EXPECTED_TOTAL), false);
    for (const std::int32_t value : consumed) {
        require(value >= 0 && value < EXPECTED_TOTAL, "drain queue value out of range");
        const std::size_t index = static_cast<std::size_t>(value);
        require(!seen[index], "drain queue duplicate value");
        seen[index] = true;
    }

    const compsys::BoundedMpmcQueue::Report report = queue.report();
    require(report.push_success == EXPECTED_TOTAL, "drain queue push report mismatch");
    require(report.pop_success == EXPECTED_TOTAL, "drain queue pop report mismatch");
    require(report.max_size == 1, "capacity one queue should report max size one");
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

void test_permit_limiter() {
    constexpr std::int32_t TASK_COUNT = 16;
    constexpr std::int32_t PERMIT_COUNT = 3;

    const compsys::PermitLimiterReport report =
        compsys::run_permit_limiter(TASK_COUNT, PERMIT_COUNT);

    require(report.task_count == TASK_COUNT, "permit limiter task count mismatch");
    require(report.permit_count == PERMIT_COUNT, "permit limiter permit count mismatch");
    require(report.entered_count == TASK_COUNT, "permit limiter entered count mismatch");
    require(report.max_in_flight <= PERMIT_COUNT,
            "permit limiter exceeded permit count");
    require(report.max_in_flight == PERMIT_COUNT,
            "permit limiter should observe the full permit window");
}

void test_phase_barrier() {
    constexpr std::int32_t WORKER_COUNT = 4;
    constexpr std::int32_t PHASE_COUNT = 5;

    const compsys::PhaseBarrierReport report =
        compsys::run_phase_barrier(WORKER_COUNT, PHASE_COUNT);

    require(report.worker_count == WORKER_COUNT, "barrier worker count mismatch");
    require(report.phase_count == PHASE_COUNT, "barrier phase count mismatch");
    require(report.completed_phases == PHASE_COUNT, "barrier completion count mismatch");
    require(report.arrival_count == WORKER_COUNT * PHASE_COUNT,
            "barrier arrival count mismatch");
}

void test_future_result_paths() {
    const compsys::FutureResultReport report = compsys::run_future_result_paths();

    require(report.value_ready, "future value path failed");
    require(report.value == 42, "future value mismatch");
    require(report.exception_ready, "future exception path failed");
    require(report.exception_message == "future path failed",
            "future exception message mismatch");
    require(report.broken_promise_seen, "broken promise path failed");
}

void test_atomic_wait_version_counter() {
    constexpr std::int32_t PUBLISH_COUNT = 6;
    const compsys::AtomicWaitReport report =
        compsys::run_atomic_wait_version_counter(PUBLISH_COUNT);

    require(report.final_version == PUBLISH_COUNT, "atomic wait final version mismatch");
    require(report.wake_count >= 1, "atomic wait did not observe any wake");
    require(report.observed_sum >= PUBLISH_COUNT,
            "atomic wait observed sum should include final version");
}

void test_eventfd_epoll_contract() {
    const compsys::EventFdEpollReport report = compsys::run_eventfd_epoll_contract();

    require(report.wake_write_count == 2, "eventfd wake write count mismatch");
    require(report.first_counter_read == 1, "eventfd first counter read mismatch");
    require(report.second_counter_read == 1, "eventfd second counter read mismatch");
    require(report.epoll_ready_observations == 2,
            "eventfd epoll ready observation mismatch");
    require(report.epoll_empty_observations == 2,
            "eventfd epoll empty observation mismatch");
    require(report.queue_drain_count == 3, "eventfd queue drain count mismatch");
    require(report.semaphore_read_count == 3, "eventfd semaphore read count mismatch");
    require(report.queue_fifo_ok, "eventfd queue FIFO contract failed");
    require(report.semaphore_empty_after_reads, "eventfd semaphore empty contract failed");
}

void test_futex_lab_probe() {
    const compsys::FutexMutexProbeReport mutex_report =
        compsys::run_futex_mutex_probe(COMPSYS_TEST_FUTEX_MUTEX_WORKERS,
                                       COMPSYS_TEST_FUTEX_MUTEX_INCREMENTS);
    const std::int32_t expected_final_value =
        COMPSYS_TEST_FUTEX_MUTEX_WORKERS * COMPSYS_TEST_FUTEX_MUTEX_INCREMENTS;

    require(mutex_report.worker_count == COMPSYS_TEST_FUTEX_MUTEX_WORKERS,
            "futex mutex worker count mismatch");
    require(mutex_report.increments_per_worker ==
                COMPSYS_TEST_FUTEX_MUTEX_INCREMENTS,
            "futex mutex increment count mismatch");
    require(mutex_report.final_value == expected_final_value,
            "futex mutex final value mismatch");
    require(mutex_report.contended_count > 0,
            "futex mutex should observe contention");
    require(mutex_report.wait_count > 0,
            "futex mutex should enter the wait slow path");
    require(mutex_report.wake_count > 0,
            "futex mutex should wake a waiting thread");

    const compsys::FutexContractProbeReport contract_report =
        compsys::run_futex_contract_probe();

    require(contract_report.waiter_count == 1,
            "futex contract waiter count mismatch");
    require(contract_report.wake_count > 0,
            "futex contract should wake a waiter");
    require(contract_report.eagain_count == 1,
            "futex contract EAGAIN path mismatch");
    require(contract_report.timeout_count == 1,
            "futex contract timeout path mismatch");
    require(contract_report.eintr_count == 1,
            "futex contract signal interruption path mismatch");
    require(contract_report.observed_ready_count == 1,
            "futex contract ready observation mismatch");
    require(contract_report.wait_released_on_ready,
            "futex contract ready release mismatch");
}

void test_same_pool_future_wait_probe() {
    const compsys::SamePoolFutureWaitReport report =
        compsys::run_same_pool_future_wait_probe(COMPSYS_TEST_RUNTIME_WORKERS);

    require(report.worker_count == COMPSYS_TEST_RUNTIME_WORKERS,
            "same-pool probe worker count mismatch");
    require(report.parent_count == COMPSYS_TEST_RUNTIME_WORKERS,
            "same-pool probe parent count mismatch");
    require(report.blocked_parent_count == COMPSYS_TEST_RUNTIME_WORKERS,
            "same-pool probe should block every parent");
    require(report.child_submitted_count_at_stall == COMPSYS_TEST_RUNTIME_WORKERS,
            "same-pool probe should submit every child at stall");
    require(report.queued_child_count_at_stall >= 0,
            "same-pool queued child count should be observable");
    require(report.child_started_before_release == 0,
            "same-pool child should not run while every worker is blocked");
    require(report.child_completed_count == COMPSYS_TEST_RUNTIME_WORKERS,
            "same-pool child completion mismatch after release");
    require(report.reached_stall, "same-pool probe did not reach stall");
    require(report.starved_without_extra_worker,
            "same-pool probe should expose starvation risk");
}

void test_continuation_runtime_probe() {
    const compsys::ContinuationRuntimeReport report =
        compsys::run_continuation_runtime_probe(COMPSYS_TEST_RUNTIME_WORKERS,
                                                COMPSYS_TEST_CONTINUATION_ROOTS);

    require(report.worker_count == COMPSYS_TEST_RUNTIME_WORKERS,
            "continuation probe worker count mismatch");
    require(report.root_task_count == COMPSYS_TEST_CONTINUATION_ROOTS,
            "continuation probe root count mismatch");
    require(report.completed_roots == COMPSYS_TEST_CONTINUATION_ROOTS,
            "continuation probe root completion mismatch");
    require(report.completed_continuations == COMPSYS_TEST_CONTINUATION_ROOTS,
            "continuation probe continuation completion mismatch");
    require(report.rejected_continuations == 0,
            "continuation probe should not reject continuations");
    require(report.final_sum == COMPSYS_TEST_CONTINUATION_SUM,
            "continuation probe final sum mismatch");
    require(report.max_queue_depth > 0,
            "continuation probe should observe queued work");
    require(report.wait_idle_completed,
            "continuation probe wait_idle should finish with no unfinished work");
}

void test_task_runtime_report_csv() {
    compsys::TaskRuntime runtime(COMPSYS_TEST_RUNTIME_WORKERS);
    const bool accepted = runtime.submit([]() {});
    require(accepted, "task runtime should accept first task");
    runtime.wait_idle();
    runtime.shutdown();

    const compsys::TaskRuntime::Report report = runtime.report();
    std::ostringstream output;
    compsys::write_task_runtime_report_csv(output, report);

    const std::string csv = output.str();
    require(csv.find("worker_count,active_workers,queued_tasks,max_queue_depth") == 0,
            "task runtime CSV header mismatch");
    require(csv.find(",1,") != std::string::npos,
            "task runtime CSV should include a completed count");
}

}  // namespace

int main() {
    try {
        test_parallel_sum();
        test_counters();
        test_queue();
        test_queue_close_wakes_consumer();
        test_queue_close_wakes_blocked_producer();
        test_queue_multi_producer_consumer_drain();
        test_invalid_queue_capacity();
        test_permit_limiter();
        test_phase_barrier();
        test_future_result_paths();
        test_atomic_wait_version_counter();
        test_eventfd_epoll_contract();
        test_futex_lab_probe();
        test_same_pool_future_wait_probe();
        test_continuation_runtime_probe();
        test_task_runtime_report_csv();
        std::cout << "compsys tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "compsys test failure: " << error.what() << "\n";
        return EXIT_FAILURE;
    }
}

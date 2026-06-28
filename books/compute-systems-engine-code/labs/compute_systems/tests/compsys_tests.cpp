#include <compsys/chapter_models.hpp>
#include <compsys/futex_lab.hpp>
#include <compsys/parallel_reduce.hpp>
#include <compsys/sync_primitives.hpp>
#include <compsys/task_runtime.hpp>
#include <compsys/wait_channels.hpp>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <cmath>
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
constexpr double COMPSYS_TEST_DOUBLE_TOLERANCE = 0.000001;

void require_near(double actual, double expected, const char* message) {
    if (std::fabs(actual - expected) > COMPSYS_TEST_DOUBLE_TOLERANCE) {
        throw std::runtime_error(message);
    }
}

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

void test_chapter_models_pipeline_branch_and_memory() {
    const std::vector<std::int32_t> records{4, -1, 7};
    const compsys::Chapter01PipelineReport pipeline =
        compsys::model_ch01_input_pipeline(records);
    require(pipeline.records_total == 3, "chapter 1 total mismatch");
    require(pipeline.accepted == 2, "chapter 1 accepted mismatch");
    require(pipeline.rejected == 1, "chapter 1 rejected mismatch");

    const std::vector<std::int32_t> branch_values{1, 9, 2, 10};
    const compsys::Chapter02BranchReport branch =
        compsys::model_ch02_branch_predictor(branch_values, 5);
    require(branch.iterations == 4, "chapter 2 iteration mismatch");
    require(branch.actual_taken == 2, "chapter 2 actual taken mismatch");
    require(branch.mispredictions > 0, "chapter 2 should expose branch misses");

    const std::vector<std::uint64_t> offsets{0, 64, 4096, 8192, 0};
    const compsys::Chapter03MemoryAccessReport memory =
        compsys::model_ch03_cache_tlb(offsets, 64, 4096, 2);
    require(memory.access_count == 5, "chapter 3 access count mismatch");
    require(memory.unique_cache_lines == 4, "chapter 3 cache line mismatch");
    require(memory.unique_pages == 3, "chapter 3 unique page mismatch");
    require(memory.tlb_misses == 4, "chapter 3 TLB miss mismatch");
}

void test_chapter_models_numa_roofline_and_layout() {
    const std::vector<std::int32_t> worker_nodes{0, 1, 1};
    const std::vector<std::int32_t> memory_nodes{0, 0, 1};
    const compsys::Chapter04NumaReport numa =
        compsys::model_ch04_numa_coherence(worker_nodes, memory_nodes);
    require(numa.local_accesses == 2, "chapter 4 local access mismatch");
    require(numa.remote_accesses == 1, "chapter 4 remote access mismatch");
    require(numa.coherence_transfers == 1, "chapter 4 transfer mismatch");

    const compsys::Chapter05RooflineReport roofline =
        compsys::model_ch05_roofline(200.0, 100.0, 10.0, 20.0);
    require_near(roofline.arithmetic_intensity, 2.0,
                 "chapter 5 arithmetic intensity mismatch");
    require_near(roofline.memory_bound_gflops, 40.0,
                 "chapter 5 memory roof mismatch");
    require_near(roofline.attainable_gflops, 10.0,
                 "chapter 5 attainable mismatch");
    require(!roofline.memory_bound, "chapter 5 should be compute-bound");

    const compsys::Chapter05bPmuSimdReport simd_evidence =
        compsys::model_ch05b_pmu_simd_evidence(80, 20, true);
    require(simd_evidence.retired_instructions == 100,
            "chapter 5b retired instruction mismatch");
    require_near(simd_evidence.vector_fraction, 0.2,
                 "chapter 5b vector fraction mismatch");
    require(simd_evidence.evidence_complete,
            "chapter 5b evidence should be complete");

    const compsys::Chapter06LayoutReport layout =
        compsys::model_ch06_layout_locality(10, 4, 1, 8);
    require(layout.aos_bytes_touched == 320, "chapter 6 AoS bytes mismatch");
    require(layout.soa_bytes_touched == 80, "chapter 6 SoA bytes mismatch");
    require(layout.saved_bytes == 240, "chapter 6 saved bytes mismatch");
}

void test_chapter_models_kernels_and_scheduling() {
    const compsys::Chapter07SimdReport simd =
        compsys::model_ch07_simd_ilp(10, 4);
    require(simd.vector_iterations == 2, "chapter 7 vector iteration mismatch");
    require(simd.scalar_tail == 2, "chapter 7 scalar tail mismatch");

    const std::vector<std::int32_t> kernel_values{1, 2, 3, 4};
    const compsys::Chapter08KernelReport kernel =
        compsys::model_ch08_map_filter_reduce(kernel_values);
    require(kernel.filtered_count == 2, "chapter 8 filtered count mismatch");
    require(kernel.mapped_sum == 20, "chapter 8 mapped sum mismatch");
    require(kernel.reduced_sum == 20, "chapter 8 reduced sum mismatch");

    const std::vector<std::int32_t> task_costs{5, 1, 4, 2};
    const compsys::Chapter09SchedulingReport scheduling =
        compsys::model_ch09_thread_scheduling(task_costs, 2);
    require(scheduling.task_count == 4, "chapter 9 task count mismatch");
    require(scheduling.imbalance > 0, "chapter 9 should expose imbalance");

    const std::array<bool, 3> ready_observations{false, false, true};
    const compsys::Chapter09bLinuxWaitReport wait =
        compsys::model_ch09b_linux_wait_wake(ready_observations);
    require(wait.spurious_wakeups == 2, "chapter 9b spurious wake mismatch");
    require(wait.completed, "chapter 9b should complete");
}

void test_chapter_models_sync_atomic_and_reclamation() {
    const compsys::Chapter10BackpressureReport backpressure =
        compsys::model_ch10_sync_backpressure(3, 4, 1, 5);
    require(backpressure.backpressure_events > 0,
            "chapter 10 should observe backpressure");
    require(backpressure.max_depth <= backpressure.capacity,
            "chapter 10 max depth exceeded capacity");

    const std::vector<std::int32_t> observed_versions{1, 2, 2, 3};
    const compsys::Chapter11AtomicPublicationReport atomic =
        compsys::model_ch11_atomic_publication(observed_versions, 3);
    require(atomic.stale_reads == 3, "chapter 11 stale read mismatch");
    require(atomic.monotonic_observations,
            "chapter 11 observations should be monotonic");

    const std::vector<std::int32_t> ring_operations{1, 1, 1, -1, -1, -1};
    const compsys::Chapter12LockFreeQueueReport ring =
        compsys::model_ch12_spsc_ring(ring_operations, 2);
    require(ring.pushed == 2, "chapter 12 pushed mismatch");
    require(ring.rejected_full == 1, "chapter 12 rejected mismatch");
    require(ring.popped == 2, "chapter 12 popped mismatch");
    require(ring.empty_pops == 1, "chapter 12 empty pop mismatch");

    const std::vector<std::int32_t> retired_nodes{1, 2, 3};
    const std::vector<std::int32_t> protected_nodes{2};
    const compsys::Chapter12bReclamationReport reclamation =
        compsys::model_ch12b_epoch_reclamation(retired_nodes, protected_nodes);
    require(reclamation.reclaimed_nodes == 2, "chapter 12b reclaimed mismatch");
    require(reclamation.deferred_nodes == 1, "chapter 12b deferred mismatch");
}

void test_chapter_models_runtime_io_and_distributed() {
    const std::vector<std::int32_t> scan_values{1, 2, 3};
    const compsys::Chapter13ReduceScanPartitionReport scan =
        compsys::model_ch13_reduce_scan_partition(scan_values, 2);
    require(scan.reduce_sum == 6, "chapter 13 reduce sum mismatch");
    require(scan.last_prefix == 6, "chapter 13 last prefix mismatch");
    require(scan.partition_true_count == 2, "chapter 13 partition true mismatch");

    const std::vector<std::int32_t> worker_queues{6, 0, 0};
    const compsys::Chapter14WorkStealingReport stealing =
        compsys::model_ch14_work_stealing(worker_queues);
    require(stealing.stolen_tasks > 0, "chapter 14 should steal tasks");
    require(stealing.completed_tasks == 6, "chapter 14 completed mismatch");

    const compsys::Chapter15AsyncIoReport io =
        compsys::model_ch15_async_io_backpressure(5, 2, 1);
    require(io.submitted == 5, "chapter 15 submitted mismatch");
    require(io.completed == 5, "chapter 15 completed mismatch");
    require(io.max_in_flight == 2, "chapter 15 max in-flight mismatch");
    require(io.backpressure_events > 0,
            "chapter 15 should observe backpressure");

    const std::vector<std::int32_t> task_ids{1, 1, 2, 3};
    const std::vector<std::int32_t> generations{2, 2, 1, 2};
    const compsys::Chapter16DistributedModelReport distributed =
        compsys::model_ch16_distributed_attempts(task_ids, generations, 2);
    require(distributed.logical_tasks == 2, "chapter 16 logical task mismatch");
    require(distributed.duplicate_attempts == 1,
            "chapter 16 duplicate attempt mismatch");
    require(distributed.stale_generation_attempts == 1,
            "chapter 16 stale generation mismatch");

    const std::array<bool, 3> acknowledgements{true, true, false};
    const compsys::Chapter17ReplicationReport replication =
        compsys::model_ch17_quorum_commit(3, acknowledgements);
    require(replication.quorum_size == 2, "chapter 17 quorum mismatch");
    require(replication.committed, "chapter 17 should commit");

    const compsys::Chapter18RecoveryReport recovery =
        compsys::model_ch18_checkpoint_recovery(100, 25, 63);
    require(recovery.checkpoints == 4, "chapter 18 checkpoint mismatch");
    require(recovery.replay_from_record == 50,
            "chapter 18 replay start mismatch");
    require(recovery.replay_records == 50, "chapter 18 replay count mismatch");
}

void test_chapter_model_error_paths() {
    bool saw_invalid_ring = false;
    try {
        const std::vector<std::int32_t> invalid_operations{1, 0};
        static_cast<void>(compsys::model_ch12_spsc_ring(invalid_operations, 2));
    } catch (const std::runtime_error&) {
        saw_invalid_ring = true;
    }
    require(saw_invalid_ring, "chapter 12 should reject unknown operations");

    bool saw_mismatched_spans = false;
    try {
        const std::vector<std::int32_t> task_ids{1};
        const std::vector<std::int32_t> generations{1, 1};
        static_cast<void>(
            compsys::model_ch16_distributed_attempts(task_ids, generations, 1));
    } catch (const std::runtime_error&) {
        saw_mismatched_spans = true;
    }
    require(saw_mismatched_spans, "chapter 16 should reject mismatched spans");
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
        test_chapter_models_pipeline_branch_and_memory();
        test_chapter_models_numa_roofline_and_layout();
        test_chapter_models_kernels_and_scheduling();
        test_chapter_models_sync_atomic_and_reclamation();
        test_chapter_models_runtime_io_and_distributed();
        test_chapter_model_error_paths();
        std::cout << "compsys tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "compsys test failure: " << error.what() << "\n";
        return EXIT_FAILURE;
    }
}

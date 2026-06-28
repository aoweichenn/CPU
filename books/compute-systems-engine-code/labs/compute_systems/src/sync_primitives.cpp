#include <compsys/sync_primitives.hpp>

#include <algorithm>
#include <atomic>
#include <barrier>
#include <future>
#include <latch>
#include <limits>
#include <semaphore>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace compsys {
namespace {

constexpr std::int32_t COMPSYS_MIN_SYNC_COUNT = 1;
constexpr std::int32_t COMPSYS_READY_VALUE = 42;

void require_positive(std::int32_t value, const char* name) {
    if (value < COMPSYS_MIN_SYNC_COUNT) {
        throw std::runtime_error(std::string(name) + " must be positive");
    }
}

void update_max(std::atomic<std::int32_t>& max_value, std::int32_t candidate) {
    std::int32_t current = max_value.load(std::memory_order_relaxed);
    while (candidate > current &&
           !max_value.compare_exchange_weak(current,
                                            candidate,
                                            std::memory_order_relaxed,
                                            std::memory_order_relaxed)) {
    }
}

}  // namespace

PermitLimiterReport run_permit_limiter(std::int32_t task_count,
                                       std::int32_t permit_count) {
    require_positive(task_count, "task_count");
    require_positive(permit_count, "permit_count");

    const std::int32_t expected_peak = std::min(task_count, permit_count);
    std::counting_semaphore<std::numeric_limits<std::int32_t>::max()> permits(
        permit_count);
    std::atomic<std::int32_t> in_flight{0};
    std::atomic<std::int32_t> entered_count{0};
    std::atomic<std::int32_t> max_in_flight{0};
    std::latch start_gate(1);
    std::latch release_gate(1);
    std::vector<std::thread> workers;
    workers.reserve(static_cast<std::size_t>(task_count));

    for (std::int32_t task = 0; task < task_count; ++task) {
        workers.emplace_back([&permits,
                              &in_flight,
                              &entered_count,
                              &max_in_flight,
                              &start_gate,
                              &release_gate]() {
            start_gate.wait();
            permits.acquire();
            const std::int32_t now =
                in_flight.fetch_add(1, std::memory_order_acq_rel) + 1;
            entered_count.fetch_add(1, std::memory_order_relaxed);
            update_max(max_in_flight, now);
            release_gate.wait();
            in_flight.fetch_sub(1, std::memory_order_acq_rel);
            permits.release();
        });
    }

    start_gate.count_down();
    while (entered_count.load(std::memory_order_acquire) < expected_peak) {
        std::this_thread::yield();
    }
    release_gate.count_down();

    for (std::thread& worker : workers) {
        worker.join();
    }

    PermitLimiterReport report;
    report.task_count = task_count;
    report.permit_count = permit_count;
    report.entered_count = entered_count.load(std::memory_order_relaxed);
    report.max_in_flight = max_in_flight.load(std::memory_order_relaxed);
    return report;
}

PhaseBarrierReport run_phase_barrier(std::int32_t worker_count,
                                     std::int32_t phase_count) {
    require_positive(worker_count, "worker_count");
    require_positive(phase_count, "phase_count");

    std::atomic<std::int32_t> completed_phases{0};
    std::atomic<std::int32_t> arrival_count{0};
    std::barrier phase(worker_count, [&completed_phases]() noexcept {
        completed_phases.fetch_add(1, std::memory_order_relaxed);
    });
    std::vector<std::thread> workers;
    workers.reserve(static_cast<std::size_t>(worker_count));

    for (std::int32_t worker = 0; worker < worker_count; ++worker) {
        workers.emplace_back([phase_count, &arrival_count, &phase]() {
            for (std::int32_t phase_index = 0; phase_index < phase_count; ++phase_index) {
                arrival_count.fetch_add(1, std::memory_order_relaxed);
                phase.arrive_and_wait();
            }
        });
    }

    for (std::thread& worker : workers) {
        worker.join();
    }

    PhaseBarrierReport report;
    report.worker_count = worker_count;
    report.phase_count = phase_count;
    report.completed_phases = completed_phases.load(std::memory_order_relaxed);
    report.arrival_count = arrival_count.load(std::memory_order_relaxed);
    return report;
}

FutureResultReport run_future_result_paths() {
    FutureResultReport report;

    std::promise<std::int32_t> value_promise;
    std::future<std::int32_t> value_future = value_promise.get_future();
    value_promise.set_value(COMPSYS_READY_VALUE);
    report.value = value_future.get();
    report.value_ready = report.value == COMPSYS_READY_VALUE;

    std::promise<std::int32_t> exception_promise;
    std::future<std::int32_t> exception_future = exception_promise.get_future();
    exception_promise.set_exception(std::make_exception_ptr(
        std::runtime_error("future path failed")));
    try {
        static_cast<void>(exception_future.get());
    } catch (const std::runtime_error& error) {
        report.exception_ready = true;
        report.exception_message = error.what();
    }

    std::future<std::int32_t> broken_future;
    {
        std::promise<std::int32_t> broken_promise;
        broken_future = broken_promise.get_future();
    }
    try {
        static_cast<void>(broken_future.get());
    } catch (const std::future_error& error) {
        report.broken_promise_seen =
            error.code() == std::make_error_code(std::future_errc::broken_promise);
    }

    return report;
}

AtomicWaitReport run_atomic_wait_version_counter(std::int32_t publish_count) {
    require_positive(publish_count, "publish_count");

    std::atomic<std::int32_t> version{0};
    std::atomic<std::int32_t> observed_sum{0};
    std::atomic<std::int32_t> wake_count{0};
    std::latch waiter_started(1);

    std::thread waiter([publish_count, &version, &observed_sum, &wake_count, &waiter_started]() {
        std::int32_t observed = version.load(std::memory_order_acquire);
        waiter_started.count_down();
        while (observed < publish_count) {
            version.wait(observed, std::memory_order_acquire);
            const std::int32_t next = version.load(std::memory_order_acquire);
            if (next != observed) {
                wake_count.fetch_add(1, std::memory_order_relaxed);
                observed_sum.fetch_add(next, std::memory_order_relaxed);
                observed = next;
            }
        }
    });

    waiter_started.wait();
    for (std::int32_t value = 1; value <= publish_count; ++value) {
        version.store(value, std::memory_order_release);
        version.notify_all();
    }

    waiter.join();

    AtomicWaitReport report;
    report.final_version = version.load(std::memory_order_acquire);
    report.wake_count = wake_count.load(std::memory_order_relaxed);
    report.observed_sum = observed_sum.load(std::memory_order_relaxed);
    return report;
}

}  // namespace compsys

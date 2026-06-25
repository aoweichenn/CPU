#pragma once

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace compsys {

class TaskRuntime {
public:
    struct Report {
        std::int32_t worker_count = 0;
        std::int32_t active_workers = 0;
        std::int32_t queued_tasks = 0;
        std::int32_t max_queue_depth = 0;
        std::int64_t unfinished_count = 0;
        std::int64_t submitted_count = 0;
        std::int64_t completed_count = 0;
        std::int64_t failed_count = 0;
        std::int64_t rejected_count = 0;
        bool accepting = false;
        bool stopping = false;
    };

    explicit TaskRuntime(std::int32_t worker_count);
    ~TaskRuntime();

    TaskRuntime(const TaskRuntime&) = delete;
    TaskRuntime& operator=(const TaskRuntime&) = delete;
    TaskRuntime(TaskRuntime&&) = delete;
    TaskRuntime& operator=(TaskRuntime&&) = delete;

    [[nodiscard]] bool submit(std::function<void()> task);
    void wait_idle();
    void shutdown();

    [[nodiscard]] Report report() const;
    [[nodiscard]] std::int32_t queued_task_count() const;

private:
    void worker_loop();

    std::deque<std::function<void()>> tasks_;
    std::vector<std::thread> workers_;
    mutable std::mutex mutex_;
    std::condition_variable ready_;
    std::condition_variable idle_;
    bool accepting_ = true;
    bool stopping_ = false;
    std::int32_t active_workers_ = 0;
    Report report_;
};

struct SamePoolFutureWaitReport {
    std::int32_t worker_count = 0;
    std::int32_t parent_count = 0;
    std::int32_t blocked_parent_count = 0;
    std::int32_t child_submitted_count_at_stall = 0;
    std::int32_t queued_child_count_at_stall = 0;
    std::int32_t child_started_before_release = 0;
    std::int32_t child_completed_count = 0;
    bool reached_stall = false;
    bool starved_without_extra_worker = false;
};

[[nodiscard]] SamePoolFutureWaitReport run_same_pool_future_wait_probe(
    std::int32_t worker_count);

struct ContinuationRuntimeReport {
    std::int32_t worker_count = 0;
    std::int32_t root_task_count = 0;
    std::int32_t completed_roots = 0;
    std::int32_t completed_continuations = 0;
    std::int32_t rejected_continuations = 0;
    std::int32_t max_queue_depth = 0;
    std::int64_t final_sum = 0;
    bool wait_idle_completed = false;
};

[[nodiscard]] ContinuationRuntimeReport run_continuation_runtime_probe(
    std::int32_t worker_count,
    std::int32_t root_task_count);

}  // namespace compsys

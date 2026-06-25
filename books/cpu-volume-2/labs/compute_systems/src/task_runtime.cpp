#include <compsys/task_runtime.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <future>
#include <latch>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace compsys {
namespace {

constexpr std::int32_t COMPSYS_TASK_RUNTIME_MIN_WORKERS = 1;
constexpr std::int32_t COMPSYS_TASK_RUNTIME_CHILD_VALUE = 1;
constexpr std::int32_t COMPSYS_TASK_RUNTIME_STALL_TIMEOUT_MS = 2000;

void require_positive(std::int32_t value, const char* name) {
    if (value < COMPSYS_TASK_RUNTIME_MIN_WORKERS) {
        throw std::runtime_error(std::string(name) + " must be positive");
    }
}

}  // namespace

TaskRuntime::TaskRuntime(std::int32_t worker_count) {
    require_positive(worker_count, "worker_count");
    this->report_.worker_count = worker_count;
    this->workers_.reserve(static_cast<std::size_t>(worker_count));
    for (std::int32_t worker = 0; worker < worker_count; ++worker) {
        this->workers_.emplace_back([this]() {
            this->worker_loop();
        });
    }
}

TaskRuntime::~TaskRuntime() {
    this->shutdown();
}

bool TaskRuntime::submit(std::function<void()> task) {
    if (!task) {
        throw std::runtime_error("cannot submit an empty task");
    }

    {
        std::lock_guard<std::mutex> lock(this->mutex_);
        if (!this->accepting_ || this->stopping_) {
            ++this->report_.rejected_count;
            return false;
        }
        this->tasks_.push_back(std::move(task));
        ++this->report_.submitted_count;
        ++this->report_.unfinished_count;
        this->report_.max_queue_depth = std::max(
            this->report_.max_queue_depth,
            static_cast<std::int32_t>(this->tasks_.size()));
    }
    this->ready_.notify_one();
    return true;
}

void TaskRuntime::wait_idle() {
    std::unique_lock<std::mutex> lock(this->mutex_);
    this->idle_.wait(lock, [this]() {
        return this->report_.unfinished_count == 0;
    });
}

void TaskRuntime::shutdown() {
    {
        std::lock_guard<std::mutex> lock(this->mutex_);
        this->accepting_ = false;
        this->stopping_ = true;
    }
    this->ready_.notify_all();

    for (std::thread& worker : this->workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

TaskRuntime::Report TaskRuntime::report() const {
    std::lock_guard<std::mutex> lock(this->mutex_);
    Report snapshot = this->report_;
    snapshot.active_workers = this->active_workers_;
    snapshot.queued_tasks = static_cast<std::int32_t>(this->tasks_.size());
    snapshot.accepting = this->accepting_;
    snapshot.stopping = this->stopping_;
    return snapshot;
}

std::int32_t TaskRuntime::queued_task_count() const {
    std::lock_guard<std::mutex> lock(this->mutex_);
    return static_cast<std::int32_t>(this->tasks_.size());
}

void TaskRuntime::worker_loop() {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(this->mutex_);
            this->ready_.wait(lock, [this]() {
                return this->stopping_ || !this->tasks_.empty();
            });
            if (this->tasks_.empty()) {
                if (this->stopping_) {
                    return;
                }
                continue;
            }
            task = std::move(this->tasks_.front());
            this->tasks_.pop_front();
            ++this->active_workers_;
        }

        bool failed = false;
        try {
            task();
        } catch (...) {
            failed = true;
        }

        {
            std::lock_guard<std::mutex> lock(this->mutex_);
            --this->active_workers_;
            ++this->report_.completed_count;
            if (failed) {
                ++this->report_.failed_count;
            }
            --this->report_.unfinished_count;
            if (this->report_.unfinished_count == 0) {
                this->idle_.notify_all();
            }
        }
    }
}

SamePoolFutureWaitReport run_same_pool_future_wait_probe(std::int32_t worker_count) {
    require_positive(worker_count, "worker_count");

    TaskRuntime runtime(worker_count);
    std::latch start_gate(1);
    std::mutex progress_mutex;
    std::condition_variable progress_cv;
    std::condition_variable release_cv;
    bool release_parents = false;
    std::int32_t blocked_parent_count = 0;
    std::int32_t child_submitted_count = 0;
    std::int32_t child_started_count = 0;
    std::int32_t child_completed_count = 0;

    for (std::int32_t parent = 0; parent < worker_count; ++parent) {
        const bool accepted = runtime.submit([&runtime,
                                              &start_gate,
                                              &progress_mutex,
                                              &progress_cv,
                                              &release_cv,
                                              &release_parents,
                                              &blocked_parent_count,
                                              &child_submitted_count,
                                              &child_started_count,
                                              &child_completed_count]() {
            start_gate.wait();
            auto promise = std::make_shared<std::promise<std::int32_t>>();
            std::future<std::int32_t> future = promise->get_future();
            const bool child_accepted = runtime.submit([promise,
                                                        &progress_mutex,
                                                        &progress_cv,
                                                        &child_started_count,
                                                        &child_completed_count]() {
                {
                    std::lock_guard<std::mutex> lock(progress_mutex);
                    ++child_started_count;
                    progress_cv.notify_all();
                }
                promise->set_value(COMPSYS_TASK_RUNTIME_CHILD_VALUE);
                {
                    std::lock_guard<std::mutex> lock(progress_mutex);
                    ++child_completed_count;
                    progress_cv.notify_all();
                }
            });
            if (!child_accepted) {
                throw std::runtime_error("child task should be accepted");
            }

            if (future.wait_for(std::chrono::milliseconds(0)) !=
                std::future_status::ready) {
                std::unique_lock<std::mutex> lock(progress_mutex);
                ++child_submitted_count;
                ++blocked_parent_count;
                progress_cv.notify_all();
                release_cv.wait(lock, [&release_parents]() {
                    return release_parents;
                });
            }
        });
        if (!accepted) {
            throw std::runtime_error("parent task should be accepted");
        }
    }
    start_gate.count_down();

    bool reached_stall = false;
    std::int32_t child_submitted_count_at_stall = 0;
    std::int32_t queued_child_count_at_stall = 0;
    std::int32_t child_started_before_release = 0;
    {
        std::unique_lock<std::mutex> lock(progress_mutex);
        reached_stall = progress_cv.wait_for(
            lock,
            std::chrono::milliseconds(COMPSYS_TASK_RUNTIME_STALL_TIMEOUT_MS),
            [&blocked_parent_count, worker_count]() {
                return blocked_parent_count == worker_count;
            });
        child_submitted_count_at_stall = child_submitted_count;
        queued_child_count_at_stall = runtime.queued_task_count();
        child_started_before_release = child_started_count;
        release_parents = true;
    }
    release_cv.notify_all();

    runtime.wait_idle();
    const std::int32_t final_child_completed = [&]() {
        std::lock_guard<std::mutex> lock(progress_mutex);
        return child_completed_count;
    }();
    runtime.shutdown();

    SamePoolFutureWaitReport report;
    report.worker_count = worker_count;
    report.parent_count = worker_count;
    report.blocked_parent_count = blocked_parent_count;
    report.child_submitted_count_at_stall = child_submitted_count_at_stall;
    report.queued_child_count_at_stall = queued_child_count_at_stall;
    report.child_started_before_release = child_started_before_release;
    report.child_completed_count = final_child_completed;
    report.reached_stall = reached_stall;
    report.starved_without_extra_worker =
        reached_stall &&
        blocked_parent_count == worker_count &&
        child_submitted_count_at_stall == worker_count &&
        child_started_before_release == 0;
    return report;
}

ContinuationRuntimeReport run_continuation_runtime_probe(std::int32_t worker_count,
                                                         std::int32_t root_task_count) {
    require_positive(worker_count, "worker_count");
    require_positive(root_task_count, "root_task_count");

    TaskRuntime runtime(worker_count);
    std::atomic<std::int32_t> completed_roots{0};
    std::atomic<std::int32_t> completed_continuations{0};
    std::atomic<std::int32_t> rejected_continuations{0};
    std::atomic<std::int64_t> final_sum{0};

    for (std::int32_t task_id = 0; task_id < root_task_count; ++task_id) {
        const std::int32_t value = task_id + 1;
        const bool accepted = runtime.submit([&runtime,
                                              &completed_roots,
                                              &completed_continuations,
                                              &rejected_continuations,
                                              &final_sum,
                                              value]() {
            completed_roots.fetch_add(1, std::memory_order_relaxed);
            const bool continuation_accepted = runtime.submit([&completed_continuations,
                                                               &final_sum,
                                                               value]() {
                completed_continuations.fetch_add(1, std::memory_order_relaxed);
                final_sum.fetch_add(value, std::memory_order_relaxed);
            });
            if (!continuation_accepted) {
                rejected_continuations.fetch_add(1, std::memory_order_relaxed);
            }
        });
        if (!accepted) {
            throw std::runtime_error("root task should be accepted");
        }
    }

    runtime.wait_idle();
    const TaskRuntime::Report runtime_report = runtime.report();
    runtime.shutdown();

    ContinuationRuntimeReport report;
    report.worker_count = worker_count;
    report.root_task_count = root_task_count;
    report.completed_roots = completed_roots.load(std::memory_order_relaxed);
    report.completed_continuations =
        completed_continuations.load(std::memory_order_relaxed);
    report.rejected_continuations =
        rejected_continuations.load(std::memory_order_relaxed);
    report.max_queue_depth = runtime_report.max_queue_depth;
    report.final_sum = final_sum.load(std::memory_order_relaxed);
    report.wait_idle_completed = runtime_report.unfinished_count == 0;
    return report;
}

}  // namespace compsys

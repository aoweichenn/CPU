#include <compsys/task_runtime.hpp>

#include <cstdlib>
#include <exception>
#include <iostream>

namespace {

constexpr std::int32_t COMPSYS_TASK_PROBE_WORKERS = 3;
constexpr std::int32_t COMPSYS_TASK_PROBE_ROOTS = 5;

}  // namespace

int main() {
    try {
        const compsys::SamePoolFutureWaitReport same_pool =
            compsys::run_same_pool_future_wait_probe(COMPSYS_TASK_PROBE_WORKERS);
        const compsys::ContinuationRuntimeReport continuation =
            compsys::run_continuation_runtime_probe(COMPSYS_TASK_PROBE_WORKERS,
                                                    COMPSYS_TASK_PROBE_ROOTS);

        std::cout << "task_runtime,"
                  << same_pool.worker_count << ','
                  << same_pool.blocked_parent_count << ','
                  << same_pool.child_submitted_count_at_stall << ','
                  << same_pool.child_started_before_release << ','
                  << (same_pool.starved_without_extra_worker ? 1 : 0) << ','
                  << continuation.completed_roots << ','
                  << continuation.completed_continuations << ','
                  << continuation.final_sum << ','
                  << continuation.max_queue_depth << ','
                  << (continuation.wait_idle_completed ? 1 : 0) << '\n';
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "compsys task runtime probe failure: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}

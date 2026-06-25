#pragma once

#include <cstdint>
#include <string>

namespace compsys {

struct PermitLimiterReport {
    std::int32_t task_count = 0;
    std::int32_t permit_count = 0;
    std::int32_t entered_count = 0;
    std::int32_t max_in_flight = 0;
};

[[nodiscard]] PermitLimiterReport run_permit_limiter(std::int32_t task_count,
                                                     std::int32_t permit_count);

struct PhaseBarrierReport {
    std::int32_t worker_count = 0;
    std::int32_t phase_count = 0;
    std::int32_t completed_phases = 0;
    std::int32_t arrival_count = 0;
};

[[nodiscard]] PhaseBarrierReport run_phase_barrier(std::int32_t worker_count,
                                                   std::int32_t phase_count);

struct FutureResultReport {
    bool value_ready = false;
    bool exception_ready = false;
    bool broken_promise_seen = false;
    std::int32_t value = 0;
    std::string exception_message;
};

[[nodiscard]] FutureResultReport run_future_result_paths();

struct AtomicWaitReport {
    std::int32_t final_version = 0;
    std::int32_t wake_count = 0;
    std::int32_t observed_sum = 0;
};

[[nodiscard]] AtomicWaitReport run_atomic_wait_version_counter(
    std::int32_t publish_count);

}  // namespace compsys

#pragma once

#include <cstdint>

namespace compsys {

struct FutexMutexProbeReport {
    std::int32_t worker_count = 0;
    std::int32_t increments_per_worker = 0;
    std::int32_t final_value = 0;
    std::int32_t contended_count = 0;
    std::int32_t wait_count = 0;
    std::int32_t wake_count = 0;
};

[[nodiscard]] FutexMutexProbeReport run_futex_mutex_probe(
    std::int32_t worker_count,
    std::int32_t increments_per_worker);

struct FutexContractProbeReport {
    std::int32_t waiter_count = 0;
    std::int32_t wake_count = 0;
    std::int32_t eagain_count = 0;
    std::int32_t timeout_count = 0;
    std::int32_t eintr_count = 0;
    std::int32_t observed_ready_count = 0;
    bool wait_released_on_ready = false;
};

[[nodiscard]] FutexContractProbeReport run_futex_contract_probe();

}  // namespace compsys

#pragma once

#include <cstdint>

namespace compsys {

struct EventFdEpollReport {
    std::int32_t wake_write_count = 0;
    std::uint64_t first_counter_read = 0;
    std::uint64_t second_counter_read = 0;
    std::int32_t epoll_ready_observations = 0;
    std::int32_t epoll_empty_observations = 0;
    std::int32_t queue_drain_count = 0;
    std::int32_t semaphore_read_count = 0;
    bool queue_fifo_ok = false;
    bool semaphore_empty_after_reads = false;
};

[[nodiscard]] EventFdEpollReport run_eventfd_epoll_contract();

}  // namespace compsys

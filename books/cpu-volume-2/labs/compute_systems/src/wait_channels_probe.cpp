#include <compsys/wait_channels.hpp>

#include <cstdlib>
#include <iomanip>
#include <iostream>

int main() {
    try {
        const compsys::EventFdEpollReport report = compsys::run_eventfd_epoll_contract();
        std::cout << std::fixed << std::setprecision(0);
        std::cout << "contract,"
                  << report.wake_write_count << ','
                  << report.first_counter_read << ','
                  << report.second_counter_read << ','
                  << report.epoll_ready_observations << ','
                  << report.epoll_empty_observations << ','
                  << report.queue_drain_count << ','
                  << report.semaphore_read_count << ','
                  << (report.queue_fifo_ok ? 1 : 0) << ','
                  << (report.semaphore_empty_after_reads ? 1 : 0) << '\n';
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "compsys wait channel probe failure: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}

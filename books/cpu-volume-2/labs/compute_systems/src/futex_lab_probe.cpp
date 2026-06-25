#include <compsys/futex_lab.hpp>

#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>

namespace {

constexpr std::int32_t COMPSYS_FUTEX_MUTEX_WORKERS = 4;
constexpr std::int32_t COMPSYS_FUTEX_MUTEX_INCREMENTS = 50;

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

}  // namespace

int main() {
    try {
        const compsys::FutexMutexProbeReport mutex_report =
            compsys::run_futex_mutex_probe(COMPSYS_FUTEX_MUTEX_WORKERS,
                                           COMPSYS_FUTEX_MUTEX_INCREMENTS);
        const compsys::FutexContractProbeReport contract_report =
            compsys::run_futex_contract_probe();
        const std::int32_t expected_final_value =
            COMPSYS_FUTEX_MUTEX_WORKERS * COMPSYS_FUTEX_MUTEX_INCREMENTS;

        require(mutex_report.final_value == expected_final_value,
                "futex mutex final value mismatch");
        require(mutex_report.contended_count > 0,
                "futex mutex did not observe contention");
        require(mutex_report.wait_count > 0,
                "futex mutex did not enter wait path");
        require(mutex_report.wake_count > 0,
                "futex mutex did not wake a waiter");
        require(contract_report.waiter_count == 1,
                "futex contract waiter count mismatch");
        require(contract_report.wake_count > 0,
                "futex contract wake count mismatch");
        require(contract_report.eagain_count == 1,
                "futex contract EAGAIN count mismatch");
        require(contract_report.timeout_count == 1,
                "futex contract timeout count mismatch");
        require(contract_report.eintr_count == 1,
                "futex contract EINTR count mismatch");
        require(contract_report.observed_ready_count == 1,
                "futex contract ready observation mismatch");
        require(contract_report.wait_released_on_ready,
                "futex contract ready release mismatch");

        std::cout << "futex_mutex,"
                  << mutex_report.worker_count << ','
                  << mutex_report.increments_per_worker << ','
                  << mutex_report.final_value << ','
                  << mutex_report.contended_count << ','
                  << mutex_report.wait_count << ','
                  << mutex_report.wake_count << '\n';
        std::cout << "futex_contract,"
                  << contract_report.waiter_count << ','
                  << contract_report.wake_count << ','
                  << contract_report.eagain_count << ','
                  << contract_report.timeout_count << ','
                  << contract_report.eintr_count << ','
                  << contract_report.observed_ready_count << ','
                  << (contract_report.wait_released_on_ready ? 1 : 0) << '\n';
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "compsys futex lab probe failure: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}

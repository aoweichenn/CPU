#include <compsys/futex_lab.hpp>

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <latch>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <linux/futex.h>
#include <pthread.h>
#include <signal.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace compsys {
namespace {

constexpr std::int32_t COMPSYS_FUTEX_MIN_WORKERS = 1;
constexpr std::int32_t COMPSYS_FUTEX_LOCKED = 1;
constexpr std::int32_t COMPSYS_FUTEX_CONTENDED = 2;
constexpr std::int32_t COMPSYS_FUTEX_READY_VALUE = 1;
constexpr std::int32_t COMPSYS_FUTEX_CONTRACT_TIMEOUT_MS = 100;
constexpr std::int32_t COMPSYS_FUTEX_SIGNAL_WAIT_MS = 20;
constexpr std::int32_t COMPSYS_FUTEX_MUTEX_SLEEP_MS = 10;
constexpr std::int32_t COMPSYS_FUTEX_MS_PER_SECOND = 1000;
constexpr long COMPSYS_FUTEX_NS_PER_MS = 1000000L;
constexpr std::int32_t COMPSYS_FUTEX_WAKE_SPIN_LIMIT = 100000;
constexpr std::int32_t COMPSYS_FUTEX_SIGNAL_RETRY_LIMIT = 100;
constexpr std::int32_t COMPSYS_FUTEX_SIGNAL_RETRY_SLEEP_MS = 1;
constexpr std::int32_t COMPSYS_FUTEX_STAGE_SIGNAL_WAITING = 1;
constexpr std::int32_t COMPSYS_FUTEX_STAGE_SIGNAL_INTERRUPTED = 2;
constexpr std::int32_t COMPSYS_FUTEX_STAGE_WAKE_WAITING = 3;
constexpr std::int32_t COMPSYS_FUTEX_STAGE_WAKE_DONE = 4;
constexpr std::int32_t COMPSYS_FUTEX_STAGE_READY_WAITING = 5;
constexpr int COMPSYS_FUTEX_SIGNAL = SIGUSR1;

void require_positive(std::int32_t value, const char* name) {
    if (value < COMPSYS_FUTEX_MIN_WORKERS) {
        throw std::runtime_error(std::string(name) + " must be positive");
    }
}

void futex_signal_handler(int) {}

class FutexSignalGuard {
public:
    FutexSignalGuard() {
        struct sigaction action {};
        action.sa_handler = futex_signal_handler;
        sigemptyset(&action.sa_mask);
        action.sa_flags = 0;
        if (::sigaction(COMPSYS_FUTEX_SIGNAL, &action, &this->old_action_) != 0) {
            throw std::runtime_error("failed to install futex signal handler");
        }
    }

    FutexSignalGuard(const FutexSignalGuard&) = delete;
    FutexSignalGuard& operator=(const FutexSignalGuard&) = delete;

    ~FutexSignalGuard() {
        static_cast<void>(::sigaction(COMPSYS_FUTEX_SIGNAL,
                                      &this->old_action_,
                                      nullptr));
    }

private:
    struct sigaction old_action_ {};
};

[[nodiscard]] int futex_wait_word(std::int32_t* word, std::int32_t expected) {
    return static_cast<int>(::syscall(SYS_futex,
                                      word,
                                      FUTEX_WAIT_PRIVATE,
                                      expected,
                                      nullptr,
                                      nullptr,
                                      0));
}

[[nodiscard]] int futex_wait_word_timeout(std::int32_t* word,
                                          std::int32_t expected,
                                          std::int32_t timeout_ms) {
    timespec timeout {};
    timeout.tv_sec = timeout_ms / COMPSYS_FUTEX_MS_PER_SECOND;
    timeout.tv_nsec =
        static_cast<long>(timeout_ms % COMPSYS_FUTEX_MS_PER_SECOND) *
        COMPSYS_FUTEX_NS_PER_MS;
    return static_cast<int>(::syscall(SYS_futex,
                                      word,
                                      FUTEX_WAIT_PRIVATE,
                                      expected,
                                      &timeout,
                                      nullptr,
                                      0));
}

[[nodiscard]] int futex_wake_word(std::int32_t* word, std::int32_t wake_count) {
    return static_cast<int>(::syscall(SYS_futex,
                                      word,
                                      FUTEX_WAKE_PRIVATE,
                                      wake_count,
                                      nullptr,
                                      nullptr,
                                      0));
}

class FutexMutex {
public:
    void lock() {
        std::atomic_ref<std::int32_t> state(this->state_word_);
        std::int32_t expected = 0;
        if (state.compare_exchange_strong(expected,
                                          COMPSYS_FUTEX_LOCKED,
                                          std::memory_order_acquire,
                                          std::memory_order_relaxed)) {
            return;
        }

        this->contended_count_.fetch_add(1, std::memory_order_relaxed);
        while (state.exchange(COMPSYS_FUTEX_CONTENDED,
                              std::memory_order_acquire) != 0) {
            this->wait_count_.fetch_add(1, std::memory_order_relaxed);
            const int rc =
                futex_wait_word(&this->state_word_, COMPSYS_FUTEX_CONTENDED);
            if (rc == 0 || errno == EAGAIN || errno == EINTR) {
                continue;
            }
            throw std::runtime_error("futex mutex wait failed");
        }

    }

    void unlock() {
        std::atomic_ref<std::int32_t> state(this->state_word_);
        const std::int32_t previous =
            state.exchange(0, std::memory_order_release);
        if (previous == COMPSYS_FUTEX_CONTENDED) {
            const int woke = futex_wake_word(&this->state_word_, 1);
            if (woke < 0) {
                throw std::runtime_error("futex mutex wake failed");
            }
            this->wake_count_.fetch_add(woke, std::memory_order_relaxed);
        }
    }

    [[nodiscard]] std::int32_t contended_count() const {
        return this->contended_count_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] std::int32_t wait_count() const {
        return this->wait_count_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] std::int32_t wake_count() const {
        return this->wake_count_.load(std::memory_order_relaxed);
    }

private:
    alignas(std::int32_t) std::int32_t state_word_ = 0;
    std::atomic<std::int32_t> contended_count_{0};
    std::atomic<std::int32_t> wait_count_{0};
    std::atomic<std::int32_t> wake_count_{0};
};

[[nodiscard]] bool wait_for_futex_mutex_waiter(const FutexMutex& mutex) {
    for (std::int32_t attempt = 0;
         attempt < COMPSYS_FUTEX_WAKE_SPIN_LIMIT;
         ++attempt) {
        if (mutex.wait_count() > 0) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(COMPSYS_FUTEX_MUTEX_SLEEP_MS));
            return true;
        }
        std::this_thread::yield();
    }
    return false;
}

}  // namespace

FutexMutexProbeReport run_futex_mutex_probe(std::int32_t worker_count,
                                            std::int32_t increments_per_worker) {
    require_positive(worker_count, "worker_count");
    require_positive(increments_per_worker, "increments_per_worker");

    FutexMutex mutex;
    std::int32_t shared_value = 0;
    mutex.lock();

    std::latch ready_gate(worker_count);
    std::latch start_gate(1);
    std::vector<std::thread> workers;
    workers.reserve(static_cast<std::size_t>(worker_count));

    for (std::int32_t worker = 0; worker < worker_count; ++worker) {
        workers.emplace_back([worker,
                              increments_per_worker,
                              &mutex,
                              &shared_value,
                              &ready_gate,
                              &start_gate]() {
            ready_gate.count_down();
            start_gate.wait();
            for (std::int32_t step = 0; step < increments_per_worker; ++step) {
                mutex.lock();
                ++shared_value;
                mutex.unlock();
            }
        });
    }

    ready_gate.wait();
    start_gate.count_down();

    const bool saw_waiter = wait_for_futex_mutex_waiter(mutex);
    mutex.unlock();

    for (std::thread& worker : workers) {
        worker.join();
    }

    if (!saw_waiter) {
        throw std::runtime_error("futex mutex did not observe a waiting thread");
    }

    FutexMutexProbeReport report;
    report.worker_count = worker_count;
    report.increments_per_worker = increments_per_worker;
    report.final_value = shared_value;
    report.contended_count = mutex.contended_count();
    report.wait_count = mutex.wait_count();
    report.wake_count = mutex.wake_count();
    return report;
}

FutexContractProbeReport run_futex_contract_probe() {
    FutexSignalGuard signal_guard;

    std::int32_t ready_word = 0;
    std::int32_t park_word = 0;
    std::atomic<std::int32_t> stage{0};
    std::atomic<std::int32_t> eagain_count{0};
    std::atomic<std::int32_t> timeout_count{0};
    std::atomic<std::int32_t> eintr_count{0};
    std::atomic<std::int32_t> observed_ready_count{0};
    std::atomic<std::int32_t> wake_count{0};
    std::atomic_ref<std::int32_t> ready_ref(ready_word);

    std::thread waiter([&ready_word,
                        &park_word,
                        &stage,
                        &eagain_count,
                        &timeout_count,
                        &eintr_count,
                        &observed_ready_count,
                        &ready_ref]() {
        const int eagain_rc = futex_wait_word(&ready_word, 1);
        if (eagain_rc < 0 && errno == EAGAIN) {
            eagain_count.fetch_add(1, std::memory_order_relaxed);
        }

        const int timeout_rc =
            futex_wait_word_timeout(&ready_word,
                                    0,
                                    COMPSYS_FUTEX_CONTRACT_TIMEOUT_MS);
        if (timeout_rc < 0 && errno == ETIMEDOUT) {
            timeout_count.fetch_add(1, std::memory_order_relaxed);
        }

        while (eintr_count.load(std::memory_order_relaxed) == 0) {
            stage.store(COMPSYS_FUTEX_STAGE_SIGNAL_WAITING,
                        std::memory_order_release);
            const int signal_rc =
                futex_wait_word_timeout(&ready_word,
                                        0,
                                        COMPSYS_FUTEX_SIGNAL_WAIT_MS);
            if (signal_rc == 0) {
                continue;
            }
            if (errno == EINTR) {
                eintr_count.fetch_add(1, std::memory_order_relaxed);
                stage.store(COMPSYS_FUTEX_STAGE_SIGNAL_INTERRUPTED,
                            std::memory_order_release);
                break;
            }
            if (errno == ETIMEDOUT || errno == EAGAIN) {
                continue;
            }
            throw std::runtime_error("futex signal wait failed");
        }

        stage.store(COMPSYS_FUTEX_STAGE_WAKE_WAITING,
                    std::memory_order_release);
        const int wake_rc = futex_wait_word(&park_word, 0);
        if (wake_rc < 0 && errno != EAGAIN && errno != EINTR) {
            throw std::runtime_error("futex wake-count wait failed");
        }
        stage.store(COMPSYS_FUTEX_STAGE_WAKE_DONE,
                    std::memory_order_release);

        while (true) {
            if (ready_ref.load(std::memory_order_acquire) != 0) {
                observed_ready_count.fetch_add(1, std::memory_order_relaxed);
                break;
            }

            stage.store(COMPSYS_FUTEX_STAGE_READY_WAITING,
                        std::memory_order_release);
            const int rc = futex_wait_word(&ready_word, 0);
            if (rc == 0 || errno == EAGAIN) {
                continue;
            }
            if (errno == EINTR) {
                continue;
            }
            throw std::runtime_error("futex contract wait failed");
        }
    });

    while (stage.load(std::memory_order_acquire) <
           COMPSYS_FUTEX_STAGE_SIGNAL_WAITING) {
        std::this_thread::yield();
    }

    for (std::int32_t attempt = 0;
         attempt < COMPSYS_FUTEX_SIGNAL_RETRY_LIMIT &&
         stage.load(std::memory_order_acquire) <
             COMPSYS_FUTEX_STAGE_SIGNAL_INTERRUPTED;
         ++attempt) {
        const int signal_rc =
            ::pthread_kill(waiter.native_handle(), COMPSYS_FUTEX_SIGNAL);
        if (signal_rc != 0) {
            throw std::runtime_error("failed to signal futex waiter");
        }
        std::this_thread::sleep_for(
            std::chrono::milliseconds(COMPSYS_FUTEX_SIGNAL_RETRY_SLEEP_MS));
    }

    if (stage.load(std::memory_order_acquire) <
        COMPSYS_FUTEX_STAGE_SIGNAL_INTERRUPTED) {
        throw std::runtime_error("futex signal wait was not interrupted");
    }

    std::int32_t actual_wake_count = 0;
    for (std::int32_t attempt = 0;
         attempt < COMPSYS_FUTEX_WAKE_SPIN_LIMIT && actual_wake_count == 0;
         ++attempt) {
        if (stage.load(std::memory_order_acquire) >=
            COMPSYS_FUTEX_STAGE_WAKE_WAITING) {
            const int woke = futex_wake_word(&park_word, 1);
            if (woke < 0) {
                throw std::runtime_error("futex wake failed");
            }
            actual_wake_count += woke;
        }
        std::this_thread::yield();
    }
    if (actual_wake_count == 0) {
        throw std::runtime_error("futex wake did not observe a waiting thread");
    }

    while (stage.load(std::memory_order_acquire) <
           COMPSYS_FUTEX_STAGE_READY_WAITING) {
        std::this_thread::yield();
    }

    ready_ref.store(COMPSYS_FUTEX_READY_VALUE, std::memory_order_release);
    for (std::int32_t attempt = 0;
         attempt < COMPSYS_FUTEX_WAKE_SPIN_LIMIT &&
         observed_ready_count.load(std::memory_order_relaxed) == 0;
         ++attempt) {
        const int woke = futex_wake_word(&ready_word, 1);
        if (woke < 0) {
            throw std::runtime_error("futex wake failed");
        }
        std::this_thread::yield();
    }
    if (observed_ready_count.load(std::memory_order_relaxed) == 0) {
        throw std::runtime_error("futex waiter did not observe ready state");
    }
    wake_count.store(actual_wake_count, std::memory_order_relaxed);

    waiter.join();

    FutexContractProbeReport report;
    report.waiter_count = 1;
    report.wake_count = wake_count.load(std::memory_order_relaxed);
    report.eagain_count = eagain_count.load(std::memory_order_relaxed);
    report.timeout_count = timeout_count.load(std::memory_order_relaxed);
    report.eintr_count = eintr_count.load(std::memory_order_relaxed);
    report.observed_ready_count =
        observed_ready_count.load(std::memory_order_relaxed);
    report.wait_released_on_ready =
        report.observed_ready_count == 1;
    return report;
}

}  // namespace compsys

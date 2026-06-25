#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <span>
#include <vector>

namespace compsys {

std::int64_t sequential_sum(std::span<const std::int32_t> values);

std::int64_t parallel_sum(std::span<const std::int32_t> values,
                          std::int32_t worker_count);

class MutexCounter {
public:
    void add(std::int64_t delta);
    [[nodiscard]] std::int64_t value() const;

private:
    mutable std::mutex mutex_;
    std::int64_t value_ = 0;
};

class AtomicCounter {
public:
    void add(std::int64_t delta);
    [[nodiscard]] std::int64_t value() const;

private:
    std::atomic<std::int64_t> value_{0};
};

class BoundedMpmcQueue {
public:
    explicit BoundedMpmcQueue(std::int32_t capacity);

    [[nodiscard]] bool push(std::int32_t value);
    [[nodiscard]] std::optional<std::int32_t> pop();
    [[nodiscard]] std::optional<std::int32_t> try_pop();
    void close();
    [[nodiscard]] bool closed() const;
    [[nodiscard]] std::int32_t size() const;

    struct Report {
        std::int64_t push_success = 0;
        std::int64_t push_closed = 0;
        std::int64_t push_wait_count = 0;
        std::int64_t push_wait_ns = 0;
        std::int64_t pop_success = 0;
        std::int64_t pop_closed = 0;
        std::int64_t pop_wait_count = 0;
        std::int64_t pop_wait_ns = 0;
        std::int64_t close_count = 0;
        std::int32_t max_size = 0;
    };

    [[nodiscard]] Report report() const;

private:
    std::vector<std::int32_t> buffer_;
    std::int32_t head_ = 0;
    std::int32_t tail_ = 0;
    std::int32_t size_ = 0;
    bool closed_ = false;
    Report report_;
    mutable std::mutex mutex_;
    std::condition_variable not_full_;
    std::condition_variable not_empty_;
};

}  // namespace compsys

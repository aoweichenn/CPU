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

    void push(std::int32_t value);
    [[nodiscard]] std::optional<std::int32_t> try_pop();
    [[nodiscard]] std::int32_t size() const;

private:
    std::vector<std::int32_t> buffer_;
    std::int32_t head_ = 0;
    std::int32_t tail_ = 0;
    std::int32_t size_ = 0;
    mutable std::mutex mutex_;
    std::condition_variable not_full_;
};

}  // namespace compsys

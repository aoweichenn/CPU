#include <compsys/parallel_reduce.hpp>

#include <algorithm>
#include <stdexcept>
#include <thread>

namespace compsys {
namespace {

constexpr std::int32_t COMPSYS_MIN_WORKER_COUNT = 1;

std::int64_t sum_range(std::span<const std::int32_t> values,
                       std::size_t begin,
                       std::size_t end) {
    std::int64_t sum = 0;
    for (std::size_t i = begin; i < end; ++i) {
        sum += values[i];
    }
    return sum;
}

std::int64_t sum_partials(std::span<const std::int64_t> partials) {
    std::int64_t sum = 0;
    for (const std::int64_t value : partials) {
        sum += value;
    }
    return sum;
}

std::size_t checked_queue_capacity(std::int32_t capacity) {
    if (capacity <= 0) {
        throw std::runtime_error("queue capacity must be positive");
    }
    return static_cast<std::size_t>(capacity);
}

}  // namespace

std::int64_t sequential_sum(std::span<const std::int32_t> values) {
    return sum_range(values, 0, values.size());
}

std::int64_t parallel_sum(std::span<const std::int32_t> values,
                          std::int32_t worker_count) {
    if (worker_count < COMPSYS_MIN_WORKER_COUNT) {
        throw std::runtime_error("worker_count must be positive");
    }
    if (values.empty()) {
        return 0;
    }

    const std::int32_t actual_workers = std::min<std::int32_t>(
        worker_count,
        static_cast<std::int32_t>(values.size()));
    std::vector<std::thread> threads;
    std::vector<std::int64_t> partials(static_cast<std::size_t>(actual_workers), 0);
    threads.reserve(static_cast<std::size_t>(actual_workers));

    for (std::int32_t worker = 0; worker < actual_workers; ++worker) {
        const std::size_t begin =
            values.size() * static_cast<std::size_t>(worker) /
            static_cast<std::size_t>(actual_workers);
        const std::size_t end =
            values.size() * static_cast<std::size_t>(worker + 1) /
            static_cast<std::size_t>(actual_workers);
        threads.emplace_back([&values, &partials, worker, begin, end]() {
            partials[static_cast<std::size_t>(worker)] = sum_range(values, begin, end);
        });
    }

    for (std::thread& thread : threads) {
        thread.join();
    }

    return sum_partials(partials);
}

void MutexCounter::add(std::int64_t delta) {
    std::lock_guard<std::mutex> lock(this->mutex_);
    this->value_ += delta;
}

std::int64_t MutexCounter::value() const {
    std::lock_guard<std::mutex> lock(this->mutex_);
    return this->value_;
}

void AtomicCounter::add(std::int64_t delta) {
    this->value_.fetch_add(delta, std::memory_order_relaxed);
}

std::int64_t AtomicCounter::value() const {
    return this->value_.load(std::memory_order_relaxed);
}

BoundedMpmcQueue::BoundedMpmcQueue(std::int32_t capacity)
    : buffer_(checked_queue_capacity(capacity), 0) {}

void BoundedMpmcQueue::push(std::int32_t value) {
    std::unique_lock<std::mutex> lock(this->mutex_);
    this->not_full_.wait(lock, [this]() {
        return this->size_ < static_cast<std::int32_t>(this->buffer_.size());
    });
    this->buffer_[static_cast<std::size_t>(this->tail_)] = value;
    this->tail_ = (this->tail_ + 1) % static_cast<std::int32_t>(this->buffer_.size());
    ++this->size_;
}

std::optional<std::int32_t> BoundedMpmcQueue::try_pop() {
    std::lock_guard<std::mutex> lock(this->mutex_);
    if (this->size_ == 0) {
        return std::nullopt;
    }
    const std::int32_t value = this->buffer_[static_cast<std::size_t>(this->head_)];
    this->head_ = (this->head_ + 1) % static_cast<std::int32_t>(this->buffer_.size());
    --this->size_;
    this->not_full_.notify_one();
    return value;
}

std::int32_t BoundedMpmcQueue::size() const {
    std::lock_guard<std::mutex> lock(this->mutex_);
    return this->size_;
}

}  // namespace compsys

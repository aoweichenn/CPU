#include <compsys/wait_channels.hpp>

#include <array>
#include <cstdint>
#include <deque>
#include <stdexcept>
#include <vector>

#include <cerrno>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>

namespace compsys {
namespace {

constexpr std::int32_t COMPSYS_EVENTFD_QUEUE_FIRST = 11;
constexpr std::int32_t COMPSYS_EVENTFD_QUEUE_SECOND = 22;
constexpr std::int32_t COMPSYS_EVENTFD_QUEUE_THIRD = 33;
constexpr std::uint64_t COMPSYS_EVENTFD_WAKE_VALUE = 1;
constexpr std::uint64_t COMPSYS_EVENTFD_SEMAPHORE_PULSES = 3;
constexpr std::size_t COMPSYS_EVENTFD_EXPECTED_DRAIN_COUNT = 3;

class UniqueFd {
public:
    UniqueFd() = default;
    explicit UniqueFd(int fd) noexcept : fd_(fd) {}
    ~UniqueFd() { this->reset(); }

    UniqueFd(const UniqueFd&) = delete;
    UniqueFd& operator=(const UniqueFd&) = delete;

    UniqueFd(UniqueFd&& other) noexcept : fd_(other.release()) {}

    UniqueFd& operator=(UniqueFd&& other) noexcept {
        if (this != &other) {
            this->reset(other.release());
        }
        return *this;
    }

    [[nodiscard]] int get() const noexcept {
        return this->fd_;
    }

    [[nodiscard]] bool valid() const noexcept {
        return this->fd_ >= 0;
    }

    int release() noexcept {
        const int fd = this->fd_;
        this->fd_ = -1;
        return fd;
    }

    void reset(int fd = -1) noexcept {
        if (this->fd_ >= 0) {
            ::close(this->fd_);
        }
        this->fd_ = fd;
    }

private:
    int fd_ = -1;
};

[[nodiscard]] UniqueFd make_eventfd(unsigned int flags) {
    const int fd = ::eventfd(0, flags);
    if (fd < 0) {
        throw std::runtime_error("failed to create eventfd");
    }
    return UniqueFd(fd);
}

[[nodiscard]] UniqueFd make_epollfd() {
    const int fd = ::epoll_create1(EPOLL_CLOEXEC);
    if (fd < 0) {
        throw std::runtime_error("failed to create epoll instance");
    }
    return UniqueFd(fd);
}

void write_eventfd_value(int fd, std::uint64_t value) {
    while (true) {
        const ssize_t written = ::write(fd, &value, sizeof(value));
        if (written == static_cast<ssize_t>(sizeof(value))) {
            return;
        }
        if (written < 0 && errno == EINTR) {
            continue;
        }
        throw std::runtime_error("failed to write eventfd value");
    }
}

[[nodiscard]] bool read_eventfd_value(int fd, std::uint64_t& value) {
    while (true) {
        const ssize_t read_bytes = ::read(fd, &value, sizeof(value));
        if (read_bytes == static_cast<ssize_t>(sizeof(value))) {
            return true;
        }
        if (read_bytes < 0 && errno == EINTR) {
            continue;
        }
        if (read_bytes < 0 && errno == EAGAIN) {
            return false;
        }
        throw std::runtime_error("failed to read eventfd value");
    }
}

[[nodiscard]] bool poll_epoll_ready(int epoll_fd, int expected_fd) {
    std::array<epoll_event, 1> events{};
    const int ready = ::epoll_wait(epoll_fd, events.data(), 1, 0);
    if (ready < 0) {
        throw std::runtime_error("epoll_wait failed");
    }
    if (ready == 0) {
        return false;
    }
    if (ready != 1) {
        throw std::runtime_error("epoll_wait returned unexpected count");
    }
    if (events[0].data.fd != expected_fd) {
        throw std::runtime_error("epoll returned unexpected fd");
    }
    if ((events[0].events & EPOLLIN) == 0) {
        throw std::runtime_error("epoll returned unexpected event mask");
    }
    return true;
}

}  // namespace

EventFdEpollReport run_eventfd_epoll_contract() {
    EventFdEpollReport report;

    UniqueFd queue_wake_fd = make_eventfd(EFD_CLOEXEC | EFD_NONBLOCK);
    UniqueFd semaphore_fd = make_eventfd(EFD_CLOEXEC | EFD_NONBLOCK | EFD_SEMAPHORE);
    UniqueFd epoll_fd = make_epollfd();

    epoll_event event{};
    event.events = EPOLLIN;
    event.data.fd = queue_wake_fd.get();
    if (::epoll_ctl(epoll_fd.get(), EPOLL_CTL_ADD, queue_wake_fd.get(), &event) != 0) {
        throw std::runtime_error("failed to register eventfd with epoll");
    }

    std::deque<std::int32_t> queue;
    std::vector<std::int32_t> drained_values;
    drained_values.reserve(COMPSYS_EVENTFD_EXPECTED_DRAIN_COUNT);

    auto enqueue_request = [&](std::int32_t value) {
        const bool was_empty = queue.empty();
        queue.push_back(value);
        if (was_empty) {
            write_eventfd_value(queue_wake_fd.get(), COMPSYS_EVENTFD_WAKE_VALUE);
            ++report.wake_write_count;
        }
    };

    auto drain_queue = [&]() {
        while (!queue.empty()) {
            drained_values.push_back(queue.front());
            queue.pop_front();
            ++report.queue_drain_count;
        }
    };

    enqueue_request(COMPSYS_EVENTFD_QUEUE_FIRST);
    enqueue_request(COMPSYS_EVENTFD_QUEUE_SECOND);
    if (!poll_epoll_ready(epoll_fd.get(), queue_wake_fd.get())) {
        throw std::runtime_error("eventfd should become readable after queue wake");
    }
    ++report.epoll_ready_observations;

    std::uint64_t value = 0;
    if (!read_eventfd_value(queue_wake_fd.get(), value)) {
        throw std::runtime_error("queue wake fd should be readable");
    }
    report.first_counter_read = value;

    drain_queue();
    if (poll_epoll_ready(epoll_fd.get(), queue_wake_fd.get())) {
        throw std::runtime_error("eventfd should be empty after queue drain");
    }
    ++report.epoll_empty_observations;

    enqueue_request(COMPSYS_EVENTFD_QUEUE_THIRD);
    if (!poll_epoll_ready(epoll_fd.get(), queue_wake_fd.get())) {
        throw std::runtime_error("eventfd should become readable after second wake");
    }
    ++report.epoll_ready_observations;

    value = 0;
    if (!read_eventfd_value(queue_wake_fd.get(), value)) {
        throw std::runtime_error("queue wake fd should be readable after second wake");
    }
    report.second_counter_read = value;

    drain_queue();
    if (poll_epoll_ready(epoll_fd.get(), queue_wake_fd.get())) {
        throw std::runtime_error("eventfd should be empty after second drain");
    }
    ++report.epoll_empty_observations;

    report.queue_fifo_ok =
        drained_values.size() == COMPSYS_EVENTFD_EXPECTED_DRAIN_COUNT &&
        drained_values[0] == COMPSYS_EVENTFD_QUEUE_FIRST &&
        drained_values[1] == COMPSYS_EVENTFD_QUEUE_SECOND &&
        drained_values[2] == COMPSYS_EVENTFD_QUEUE_THIRD;

    if (::write(semaphore_fd.get(), &COMPSYS_EVENTFD_SEMAPHORE_PULSES,
                sizeof(COMPSYS_EVENTFD_SEMAPHORE_PULSES)) !=
        static_cast<ssize_t>(sizeof(COMPSYS_EVENTFD_SEMAPHORE_PULSES))) {
        throw std::runtime_error("failed to prime eventfd semaphore mode");
    }

    for (std::int32_t i = 0; i < static_cast<std::int32_t>(COMPSYS_EVENTFD_SEMAPHORE_PULSES);
         ++i) {
        std::uint64_t one = 0;
        if (!read_eventfd_value(semaphore_fd.get(), one)) {
            throw std::runtime_error("semaphore eventfd should be readable");
        }
        if (one != 1U) {
            throw std::runtime_error("semaphore eventfd should return one wake per read");
        }
        ++report.semaphore_read_count;
    }

    std::uint64_t empty = 0;
    report.semaphore_empty_after_reads = !read_eventfd_value(semaphore_fd.get(), empty);
    return report;
}

}  // namespace compsys

#include <algorithm>
#include <array>
#include <cstdint>
#include <deque>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace osbook {

constexpr std::size_t OS_PAGE_SIZE = 4096;
constexpr std::size_t OS_NVME_QUEUE_SIZE = 8;
constexpr std::size_t OS_MAX_FILE_DESCRIPTORS = 64;
constexpr std::uint64_t OS_NICE_0_WEIGHT = 1024;
constexpr std::uint64_t OS_DEFAULT_TASK_WEIGHT = 1024;
constexpr std::uint64_t OS_HIGH_TASK_WEIGHT = 2048;
constexpr std::uint64_t OS_LOW_TASK_WEIGHT = 512;
constexpr std::size_t OS_BUDDY_MAX_ORDER = 5;
constexpr std::uint64_t OS_READ_DEADLINE_TICKS = 4;
constexpr std::uint64_t OS_WRITE_DEADLINE_TICKS = 12;
constexpr std::uint64_t OS_TCP_INITIAL_RTO_TICKS = 3;
constexpr int OS_INVALID_FD = -1;

[[noreturn]] void fail(std::string_view message) {
  throw std::runtime_error(std::string(message));
}

void require(bool condition, std::string_view message) {
  if (!condition) {
    fail(message);
  }
}

enum class TaskState {
  Runnable,
  Running,
  Sleeping,
  Zombie,
};

enum class AccessMode {
  Read,
  Write,
  Execute,
};

enum class DmaDirection {
  ToDevice,
  FromDevice,
  Bidirectional,
};

enum class CrashPoint {
  None,
  AfterJournalAppend,
};

enum class TcpState {
  Closed,
  SynSent,
  Established,
  FinWait1,
  TimeWait,
};

struct Task {
  int id = 0;
  std::string name;
  TaskState state = TaskState::Runnable;
  std::uint64_t weight = OS_DEFAULT_TASK_WEIGHT;
  std::uint64_t vruntime = 0;
  std::uint64_t executed = 0;
  std::string wait_reason;

  Task(int task_id, std::string task_name, std::uint64_t task_weight)
      : id(task_id), name(std::move(task_name)), weight(task_weight) {}
};

using TaskPtr = std::shared_ptr<Task>;

struct TaskByVirtualRuntime {
  bool operator()(const TaskPtr& left, const TaskPtr& right) const {
    if (left->vruntime != right->vruntime) {
      return left->vruntime < right->vruntime;
    }
    return left->id < right->id;
  }
};

class CfsRunQueue {
 public:
  bool empty() const {
    return this->tree_.empty();
  }

  std::size_t size() const {
    return this->tree_.size();
  }

  void enqueue(const TaskPtr& task) {
    require(task != nullptr, "cannot enqueue null task");
    require(task->state == TaskState::Runnable, "only runnable tasks can enter runqueue");
    this->tree_.insert(task);
  }

  void dequeue(const TaskPtr& task) {
    require(task != nullptr, "cannot dequeue null task");
    const auto erased = this->tree_.erase(task);
    require(erased == 1, "task must be present in runqueue exactly once");
  }

  TaskPtr pick_next() {
    if (this->tree_.empty()) {
      return nullptr;
    }
    const auto iterator = this->tree_.begin();
    TaskPtr task = *iterator;
    this->tree_.erase(iterator);
    task->state = TaskState::Running;
    return task;
  }

  void account_and_requeue(const TaskPtr& task, std::uint64_t delta_exec) {
    require(task != nullptr, "cannot account null task");
    require(task->state == TaskState::Running, "only running task can be accounted");
    const std::uint64_t weighted_delta = delta_exec * OS_NICE_0_WEIGHT / task->weight;
    task->executed += delta_exec;
    task->vruntime += weighted_delta;
    task->state = TaskState::Runnable;
    this->enqueue(task);
  }

  bool contains_task(int task_id) const {
    for (const TaskPtr& task : this->tree_) {
      if (task->id == task_id) {
        return true;
      }
    }
    return false;
  }

 private:
  std::set<TaskPtr, TaskByVirtualRuntime> tree_;
};

class WaitQueue {
 public:
  void sleep(const TaskPtr& task, std::string_view reason) {
    require(task != nullptr, "cannot sleep null task");
    require(task->state != TaskState::Zombie, "zombie task cannot sleep");
    task->state = TaskState::Sleeping;
    task->wait_reason = std::string(reason);
    this->waiters_.push_back(task);
  }

  std::optional<TaskPtr> wake_one() {
    if (this->waiters_.empty()) {
      return std::nullopt;
    }
    TaskPtr task = this->waiters_.front();
    this->waiters_.pop_front();
    task->state = TaskState::Runnable;
    task->wait_reason.clear();
    return task;
  }

  std::vector<TaskPtr> wake_all() {
    std::vector<TaskPtr> ready;
    while (!this->waiters_.empty()) {
      TaskPtr task = this->waiters_.front();
      this->waiters_.pop_front();
      task->state = TaskState::Runnable;
      task->wait_reason.clear();
      ready.push_back(task);
    }
    return ready;
  }

  std::size_t size() const {
    return this->waiters_.size();
  }

 private:
  std::deque<TaskPtr> waiters_;
};

class Scheduler {
 public:
  TaskPtr create_task(std::string name, std::uint64_t weight) {
    auto task = std::make_shared<Task>(this->next_task_id_++, std::move(name), weight);
    this->runqueue_.enqueue(task);
    return task;
  }

  TaskPtr schedule_next() {
    return this->runqueue_.pick_next();
  }

  void tick_and_requeue(const TaskPtr& task, std::uint64_t delta_exec) {
    this->runqueue_.account_and_requeue(task, delta_exec);
  }

  void block_current(const TaskPtr& task, WaitQueue& queue, std::string_view reason) {
    require(task != nullptr, "cannot block null task");
    require(task->state == TaskState::Running, "only running task blocks through this path");
    queue.sleep(task, reason);
  }

  void wake_to_runqueue(const TaskPtr& task) {
    require(task != nullptr, "cannot wake null task");
    require(task->state == TaskState::Runnable, "woken task must be runnable");
    this->runqueue_.enqueue(task);
  }

  std::size_t runnable_count() const {
    return this->runqueue_.size();
  }

 private:
  int next_task_id_ = 1;
  CfsRunQueue runqueue_;
};

struct PhysicalPage {
  int id = 0;
  std::array<std::uint8_t, OS_PAGE_SIZE> bytes{};

  explicit PhysicalPage(int page_id) : id(page_id) {}
};

using PagePtr = std::shared_ptr<PhysicalPage>;

class PageAllocator {
 public:
  PagePtr allocate_zeroed() {
    auto page = std::make_shared<PhysicalPage>(this->next_page_id_++);
    page->bytes.fill(0);
    return page;
  }

 private:
  int next_page_id_ = 1;
};

struct PageTableEntry {
  PagePtr page;
  bool present = false;
  bool writable = false;
  bool executable = false;
  bool cow = false;
};

struct TlbKey {
  int asid = 0;
  std::uint64_t vpn = 0;

  bool operator==(const TlbKey& other) const {
    return this->asid == other.asid && this->vpn == other.vpn;
  }
};

struct TlbKeyHash {
  std::size_t operator()(const TlbKey& key) const {
    const std::size_t asid_hash = std::hash<int>{}(key.asid);
    const std::size_t vpn_hash = std::hash<std::uint64_t>{}(key.vpn);
    return asid_hash ^ (vpn_hash << 1U);
  }
};

class Tlb {
 public:
  void insert(int asid, std::uint64_t vpn, const PageTableEntry& entry) {
    this->entries_[TlbKey{asid, vpn}] = entry;
  }

  std::optional<PageTableEntry> lookup(int asid, std::uint64_t vpn) const {
    const auto iterator = this->entries_.find(TlbKey{asid, vpn});
    if (iterator == this->entries_.end()) {
      return std::nullopt;
    }
    return iterator->second;
  }

  void invalidate_page(int asid, std::uint64_t vpn) {
    this->entries_.erase(TlbKey{asid, vpn});
  }

  void invalidate_asid(int asid) {
    for (auto iterator = this->entries_.begin(); iterator != this->entries_.end();) {
      if (iterator->first.asid == asid) {
        iterator = this->entries_.erase(iterator);
      } else {
        ++iterator;
      }
    }
  }

 private:
  std::unordered_map<TlbKey, PageTableEntry, TlbKeyHash> entries_;
};

class AddressSpace {
 public:
  AddressSpace(int asid, PageAllocator& allocator) : asid_(asid), allocator_(allocator) {}

  int asid() const {
    return this->asid_;
  }

  void map_anonymous(std::uint64_t vpn, bool writable, bool executable) {
    PageTableEntry entry;
    entry.page = this->allocator_.allocate_zeroed();
    entry.present = true;
    entry.writable = writable;
    entry.executable = executable;
    this->entries_[vpn] = entry;
  }

  std::uint8_t read_byte(std::uint64_t virtual_address, Tlb& tlb) {
    const auto [vpn, offset] = this->split_address(virtual_address);
    PageTableEntry entry = this->resolve(vpn, AccessMode::Read, tlb);
    return entry.page->bytes[offset];
  }

  void write_byte(std::uint64_t virtual_address, std::uint8_t value, Tlb& tlb) {
    const auto [vpn, offset] = this->split_address(virtual_address);
    PageTableEntry& entry = this->entry_for_write(vpn, tlb);
    entry.page->bytes[offset] = value;
    tlb.insert(this->asid_, vpn, entry);
  }

  AddressSpace fork_cow(int child_asid, Tlb& parent_tlb) {
    AddressSpace child(child_asid, this->allocator_);
    for (auto& [vpn, entry] : this->entries_) {
      PageTableEntry child_entry = entry;
      if (entry.present && entry.writable) {
        entry.writable = false;
        entry.cow = true;
        child_entry.writable = false;
        child_entry.cow = true;
        parent_tlb.invalidate_page(this->asid_, vpn);
      }
      child.entries_[vpn] = child_entry;
    }
    return child;
  }

  std::size_t mapped_pages() const {
    return this->entries_.size();
  }

 private:
  std::pair<std::uint64_t, std::size_t> split_address(std::uint64_t virtual_address) const {
    return {virtual_address / OS_PAGE_SIZE, static_cast<std::size_t>(virtual_address % OS_PAGE_SIZE)};
  }

  PageTableEntry resolve(std::uint64_t vpn, AccessMode mode, Tlb& tlb) {
    if (const std::optional<PageTableEntry> cached = tlb.lookup(this->asid_, vpn); cached.has_value()) {
      this->check_access(cached.value(), mode);
      return cached.value();
    }
    const auto iterator = this->entries_.find(vpn);
    require(iterator != this->entries_.end() && iterator->second.present, "page fault: no present PTE");
    this->check_access(iterator->second, mode);
    tlb.insert(this->asid_, vpn, iterator->second);
    return iterator->second;
  }

  PageTableEntry& entry_for_write(std::uint64_t vpn, Tlb& tlb) {
    const auto iterator = this->entries_.find(vpn);
    require(iterator != this->entries_.end() && iterator->second.present, "write fault: no present PTE");
    PageTableEntry& entry = iterator->second;
    if (entry.writable) {
      return entry;
    }
    require(entry.cow, "write fault: permission denied");
    if (!entry.page.unique()) {
      PagePtr copied = this->allocator_.allocate_zeroed();
      copied->bytes = entry.page->bytes;
      entry.page = copied;
    }
    entry.writable = true;
    entry.cow = false;
    tlb.invalidate_page(this->asid_, vpn);
    return entry;
  }

  void check_access(const PageTableEntry& entry, AccessMode mode) const {
    require(entry.present, "page fault: not present");
    if (mode == AccessMode::Write) {
      require(entry.writable, "page fault: write denied");
    }
    if (mode == AccessMode::Execute) {
      require(entry.executable, "page fault: execute denied");
    }
  }

  int asid_ = 0;
  PageAllocator& allocator_;
  std::map<std::uint64_t, PageTableEntry> entries_;
};

struct DmaMapping {
  PagePtr page;
  std::size_t length = 0;
  DmaDirection direction = DmaDirection::Bidirectional;
};

class IommuDomain {
 public:
  std::uint64_t map(PagePtr page, std::size_t length, DmaDirection direction) {
    require(page != nullptr, "cannot map null DMA page");
    require(length > 0 && length <= OS_PAGE_SIZE, "invalid DMA mapping length");
    const std::uint64_t iova = this->next_iova_;
    this->next_iova_ += OS_PAGE_SIZE;
    this->mappings_[iova] = DmaMapping{std::move(page), length, direction};
    this->iotlb_[iova] = this->mappings_.at(iova);
    return iova;
  }

  void unmap(std::uint64_t iova) {
    const auto erased = this->mappings_.erase(iova);
    require(erased == 1, "DMA unmap must remove an existing mapping");
    this->iotlb_.erase(iova);
  }

  std::uint8_t device_read(std::uint64_t iova, std::size_t offset) const {
    const DmaMapping& mapping = this->mapping_for(iova, DmaDirection::ToDevice);
    require(offset < mapping.length, "DMA read out of range");
    return mapping.page->bytes[offset];
  }

  void device_write(std::uint64_t iova, std::size_t offset, std::uint8_t value) {
    DmaMapping& mapping = this->mutable_mapping_for(iova, DmaDirection::FromDevice);
    require(offset < mapping.length, "DMA write out of range");
    mapping.page->bytes[offset] = value;
    this->iotlb_[iova] = mapping;
  }

  std::size_t mapping_count() const {
    return this->mappings_.size();
  }

 private:
  bool direction_allows(DmaDirection actual, DmaDirection required) const {
    return actual == DmaDirection::Bidirectional || actual == required;
  }

  const DmaMapping& mapping_for(std::uint64_t iova, DmaDirection required) const {
    const auto iterator = this->iotlb_.find(iova);
    require(iterator != this->iotlb_.end(), "IOMMU fault: no IOTLB entry");
    require(this->direction_allows(iterator->second.direction, required), "IOMMU fault: wrong DMA direction");
    return iterator->second;
  }

  DmaMapping& mutable_mapping_for(std::uint64_t iova, DmaDirection required) {
    const auto iterator = this->mappings_.find(iova);
    require(iterator != this->mappings_.end(), "IOMMU fault: no mapping");
    require(this->direction_allows(iterator->second.direction, required), "IOMMU fault: wrong DMA direction");
    return iterator->second;
  }

  std::uint64_t next_iova_ = OS_PAGE_SIZE;
  std::unordered_map<std::uint64_t, DmaMapping> mappings_;
  std::unordered_map<std::uint64_t, DmaMapping> iotlb_;
};

struct NvmeCompletion {
  std::uint16_t command_id = 0;
  std::uint16_t status = 0;
  bool phase = true;
};

class NvmeCompletionQueue {
 public:
  explicit NvmeCompletionQueue(std::size_t queue_size) : entries_(queue_size) {
    require(queue_size >= 2, "NVMe completion queue must have at least two slots");
    for (NvmeCompletion& completion : this->entries_) {
      completion.phase = !this->expected_phase_;
    }
  }

  void device_push(std::uint16_t command_id, std::uint16_t status) {
    NvmeCompletion& slot = this->entries_.at(this->device_tail_);
    slot.command_id = command_id;
    slot.status = status;
    slot.phase = this->device_phase_;
    this->device_tail_ = (this->device_tail_ + 1) % this->entries_.size();
    if (this->device_tail_ == 0) {
      this->device_phase_ = !this->device_phase_;
    }
  }

  std::optional<NvmeCompletion> host_poll() {
    const NvmeCompletion& slot = this->entries_.at(this->host_head_);
    if (slot.phase != this->expected_phase_) {
      return std::nullopt;
    }
    NvmeCompletion completion = slot;
    this->host_head_ = (this->host_head_ + 1) % this->entries_.size();
    if (this->host_head_ == 0) {
      this->expected_phase_ = !this->expected_phase_;
    }
    return completion;
  }

  std::size_t host_head() const {
    return this->host_head_;
  }

 private:
  std::vector<NvmeCompletion> entries_;
  std::size_t device_tail_ = 0;
  std::size_t host_head_ = 0;
  bool device_phase_ = true;
  bool expected_phase_ = true;
};

struct Folio {
  std::vector<std::uint8_t> bytes;
  bool uptodate = false;
  bool dirty = false;
  bool writeback = false;

  Folio() : bytes(OS_PAGE_SIZE, 0) {}
};

using FolioPtr = std::shared_ptr<Folio>;

class Inode {
 public:
  explicit Inode(std::string name) : name_(std::move(name)) {}

  const std::string& name() const {
    return this->name_;
  }

  std::size_t size() const {
    return this->size_;
  }

  std::vector<std::uint8_t> read_at(std::size_t offset, std::size_t length) {
    std::vector<std::uint8_t> result;
    result.reserve(length);
    std::size_t remaining = length;
    std::size_t cursor = offset;
    while (remaining > 0 && cursor < this->size_) {
      const std::size_t page_index = cursor / OS_PAGE_SIZE;
      const std::size_t page_offset = cursor % OS_PAGE_SIZE;
      const std::size_t chunk = std::min({remaining, OS_PAGE_SIZE - page_offset, this->size_ - cursor});
      FolioPtr folio = this->get_or_load_folio(page_index);
      result.insert(result.end(), folio->bytes.begin() + static_cast<std::ptrdiff_t>(page_offset),
                    folio->bytes.begin() + static_cast<std::ptrdiff_t>(page_offset + chunk));
      remaining -= chunk;
      cursor += chunk;
    }
    return result;
  }

  std::size_t write_at(std::size_t offset, std::span<const std::uint8_t> data) {
    std::size_t written = 0;
    while (written < data.size()) {
      const std::size_t cursor = offset + written;
      const std::size_t page_index = cursor / OS_PAGE_SIZE;
      const std::size_t page_offset = cursor % OS_PAGE_SIZE;
      const std::size_t chunk = std::min(data.size() - written, OS_PAGE_SIZE - page_offset);
      FolioPtr folio = this->get_or_load_folio(page_index);
      std::copy_n(data.begin() + static_cast<std::ptrdiff_t>(written), static_cast<std::ptrdiff_t>(chunk),
                  folio->bytes.begin() + static_cast<std::ptrdiff_t>(page_offset));
      folio->uptodate = true;
      folio->dirty = true;
      written += chunk;
    }
    this->size_ = std::max(this->size_, offset + data.size());
    return written;
  }

  void fsync() {
    this->storage_.resize(this->size_, 0);
    for (auto& [page_index, folio] : this->cache_) {
      if (!folio->dirty) {
        continue;
      }
      folio->writeback = true;
      const std::size_t file_offset = page_index * OS_PAGE_SIZE;
      if (file_offset < this->size_) {
        const std::size_t chunk = std::min(OS_PAGE_SIZE, this->size_ - file_offset);
        std::copy_n(folio->bytes.begin(), static_cast<std::ptrdiff_t>(chunk),
                    this->storage_.begin() + static_cast<std::ptrdiff_t>(file_offset));
      }
      folio->dirty = false;
      folio->writeback = false;
    }
  }

  std::vector<std::uint8_t> stable_storage() const {
    return this->storage_;
  }

 private:
  FolioPtr get_or_load_folio(std::size_t page_index) {
    const auto cached = this->cache_.find(page_index);
    if (cached != this->cache_.end()) {
      return cached->second;
    }
    auto folio = std::make_shared<Folio>();
    const std::size_t file_offset = page_index * OS_PAGE_SIZE;
    if (file_offset < this->storage_.size()) {
      const std::size_t chunk = std::min(OS_PAGE_SIZE, this->storage_.size() - file_offset);
      std::copy_n(this->storage_.begin() + static_cast<std::ptrdiff_t>(file_offset),
                  static_cast<std::ptrdiff_t>(chunk), folio->bytes.begin());
    }
    folio->uptodate = true;
    this->cache_[page_index] = folio;
    return folio;
  }

  std::string name_;
  std::size_t size_ = 0;
  std::vector<std::uint8_t> storage_;
  std::map<std::size_t, FolioPtr> cache_;
};

class OpenFile {
 public:
  explicit OpenFile(std::shared_ptr<Inode> inode) : inode_(std::move(inode)) {
    require(this->inode_ != nullptr, "open file requires inode");
  }

  std::vector<std::uint8_t> read(std::size_t length) {
    std::vector<std::uint8_t> data = this->inode_->read_at(this->offset_, length);
    this->offset_ += data.size();
    return data;
  }

  std::size_t write(std::span<const std::uint8_t> data) {
    const std::size_t written = this->inode_->write_at(this->offset_, data);
    this->offset_ += written;
    return written;
  }

  void seek(std::size_t offset) {
    this->offset_ = offset;
  }

  std::size_t offset() const {
    return this->offset_;
  }

  std::shared_ptr<Inode> inode() const {
    return this->inode_;
  }

 private:
  std::shared_ptr<Inode> inode_;
  std::size_t offset_ = 0;
};

class FileDescriptorTable {
 public:
  int install(std::shared_ptr<OpenFile> file) {
    require(file != nullptr, "cannot install null file");
    for (int fd = 0; fd < static_cast<int>(OS_MAX_FILE_DESCRIPTORS); ++fd) {
      if (!this->files_.contains(fd)) {
        this->files_[fd] = std::move(file);
        return fd;
      }
    }
    return OS_INVALID_FD;
  }

  int dup(int old_fd) {
    std::shared_ptr<OpenFile> file = this->get(old_fd);
    return this->install(std::move(file));
  }

  std::shared_ptr<OpenFile> get(int fd) const {
    const auto iterator = this->files_.find(fd);
    require(iterator != this->files_.end(), "bad file descriptor");
    return iterator->second;
  }

  void close(int fd) {
    const auto erased = this->files_.erase(fd);
    require(erased == 1, "close requires valid fd");
  }

  std::size_t size() const {
    return this->files_.size();
  }

 private:
  std::map<int, std::shared_ptr<OpenFile>> files_;
};

class FutexWord {
 public:
  explicit FutexWord(int initial_value) : value_(initial_value) {}

  int load() const {
    return this->value_;
  }

  void store(int value) {
    this->value_ = value;
  }

  bool wait_if_equal(const TaskPtr& task, int expected_value) {
    require(task != nullptr, "futex cannot wait a null task");
    if (this->value_ != expected_value) {
      return false;
    }
    this->waiters_.sleep(task, "futex");
    return true;
  }

  std::optional<TaskPtr> wake_one() {
    return this->waiters_.wake_one();
  }

  std::size_t waiter_count() const {
    return this->waiters_.size();
  }

 private:
  int value_ = 0;
  WaitQueue waiters_;
};

class DeadlockDetector {
 public:
  void add_wait_edge(int waiter_task_id, int owner_task_id) {
    require(waiter_task_id != owner_task_id, "self wait is an immediate deadlock");
    this->wait_for_[waiter_task_id].insert(owner_task_id);
    this->wait_for_.try_emplace(owner_task_id);
  }

  void clear_task(int task_id) {
    this->wait_for_.erase(task_id);
    for (auto& [node, successors] : this->wait_for_) {
      static_cast<void>(node);
      successors.erase(task_id);
    }
  }

  bool has_cycle() const {
    std::map<int, std::size_t> indegree;
    for (const auto& [node, successors] : this->wait_for_) {
      indegree.try_emplace(node, 0);
      for (const int successor : successors) {
        ++indegree[successor];
      }
    }

    std::deque<int> ready;
    for (const auto& [node, degree] : indegree) {
      if (degree == 0) {
        ready.push_back(node);
      }
    }

    std::size_t visited = 0;
    while (!ready.empty()) {
      const int node = ready.front();
      ready.pop_front();
      ++visited;
      const auto iterator = this->wait_for_.find(node);
      if (iterator == this->wait_for_.end()) {
        continue;
      }
      for (const int successor : iterator->second) {
        auto degree = indegree.find(successor);
        require(degree != indegree.end(), "wait-for graph indegree must contain every node");
        --degree->second;
        if (degree->second == 0) {
          ready.push_back(successor);
        }
      }
    }

    return visited != indegree.size();
  }

  std::size_t edge_count() const {
    std::size_t count = 0;
    for (const auto& [node, successors] : this->wait_for_) {
      static_cast<void>(node);
      count += successors.size();
    }
    return count;
  }

 private:
  std::map<int, std::set<int>> wait_for_;
};

class BuddyAllocator {
 public:
  explicit BuddyAllocator(std::size_t max_order) : max_order_(max_order), free_lists_(max_order + 1) {
    require(max_order <= OS_BUDDY_MAX_ORDER, "test buddy allocator max order too large");
    this->free_lists_.at(max_order).insert(0);
  }

  std::size_t allocate_pages(std::size_t page_count) {
    const std::size_t target_order = this->order_for_pages(page_count);
    std::size_t order = target_order;
    while (order <= this->max_order_ && this->free_lists_.at(order).empty()) {
      ++order;
    }
    require(order <= this->max_order_, "buddy allocator out of memory");

    auto block = this->free_lists_.at(order).begin();
    std::size_t start_page = *block;
    this->free_lists_.at(order).erase(block);

    while (order > target_order) {
      --order;
      const std::size_t buddy = start_page + (std::size_t{1} << order);
      this->free_lists_.at(order).insert(buddy);
    }

    this->allocated_orders_[start_page] = target_order;
    return start_page;
  }

  void free_pages(std::size_t start_page) {
    auto allocation = this->allocated_orders_.find(start_page);
    require(allocation != this->allocated_orders_.end(), "free must reference an allocated buddy block");
    std::size_t order = allocation->second;
    this->allocated_orders_.erase(allocation);

    while (order < this->max_order_) {
      const std::size_t buddy = start_page ^ (std::size_t{1} << order);
      auto& free_list = this->free_lists_.at(order);
      const auto buddy_iterator = free_list.find(buddy);
      if (buddy_iterator == free_list.end()) {
        break;
      }
      free_list.erase(buddy_iterator);
      start_page = std::min(start_page, buddy);
      ++order;
    }
    this->free_lists_.at(order).insert(start_page);
  }

  std::size_t free_blocks_at_order(std::size_t order) const {
    require(order <= this->max_order_, "free block order out of range");
    return this->free_lists_.at(order).size();
  }

  std::size_t allocated_block_count() const {
    return this->allocated_orders_.size();
  }

 private:
  std::size_t order_for_pages(std::size_t page_count) const {
    require(page_count > 0, "allocation must request at least one page");
    std::size_t order = 0;
    std::size_t capacity = 1;
    while (capacity < page_count) {
      capacity <<= 1U;
      ++order;
    }
    require(order <= this->max_order_, "allocation request exceeds allocator capacity");
    return order;
  }

  std::size_t max_order_ = 0;
  std::vector<std::set<std::size_t>> free_lists_;
  std::map<std::size_t, std::size_t> allocated_orders_;
};

struct CacheEntry {
  int page_id = 0;
  bool dirty = false;
};

class LruPageCache {
 public:
  explicit LruPageCache(std::size_t capacity) : capacity_(capacity) {
    require(capacity > 0, "LRU cache capacity must be positive");
  }

  void access(int page_id, bool write) {
    const auto cached = this->index_.find(page_id);
    if (cached != this->index_.end()) {
      cached->second->dirty = cached->second->dirty || write;
      this->entries_.splice(this->entries_.begin(), this->entries_, cached->second);
      cached->second = this->entries_.begin();
      return;
    }

    if (this->entries_.size() == this->capacity_) {
      this->evict_one();
    }
    this->entries_.push_front(CacheEntry{page_id, write});
    this->index_[page_id] = this->entries_.begin();
  }

  bool contains(int page_id) const {
    return this->index_.contains(page_id);
  }

  void flush_all() {
    for (CacheEntry& entry : this->entries_) {
      if (!entry.dirty) {
        continue;
      }
      ++this->writeback_count_;
      entry.dirty = false;
    }
  }

  std::size_t size() const {
    return this->entries_.size();
  }

  std::size_t writeback_count() const {
    return this->writeback_count_;
  }

 private:
  void evict_one() {
    require(!this->entries_.empty(), "cannot evict from empty page cache");
    auto victim = this->entries_.end();
    --victim;
    if (victim->dirty) {
      ++this->writeback_count_;
    }
    this->index_.erase(victim->page_id);
    this->entries_.erase(victim);
  }

  std::size_t capacity_ = 0;
  std::list<CacheEntry> entries_;
  std::unordered_map<int, std::list<CacheEntry>::iterator> index_;
  std::size_t writeback_count_ = 0;
};

struct Extent {
  std::uint64_t logical_start = 0;
  std::uint64_t physical_start = 0;
  std::uint64_t length = 0;
};

class ExtentMap {
 public:
  void add_extent(std::uint64_t logical_start, std::uint64_t physical_start, std::uint64_t length) {
    require(length > 0, "extent length must be positive");
    const std::uint64_t logical_end = logical_start + length;
    for (const auto& [existing_start, extent] : this->extents_) {
      static_cast<void>(existing_start);
      const std::uint64_t existing_end = extent.logical_start + extent.length;
      const bool disjoint = logical_end <= extent.logical_start || existing_end <= logical_start;
      require(disjoint, "extent must not overlap an existing extent");
    }
    this->extents_[logical_start] = Extent{logical_start, physical_start, length};
  }

  std::optional<std::uint64_t> lookup(std::uint64_t logical_block) const {
    for (const auto& [start, extent] : this->extents_) {
      static_cast<void>(start);
      if (logical_block < extent.logical_start) {
        break;
      }
      if (logical_block < extent.logical_start + extent.length) {
        return extent.physical_start + (logical_block - extent.logical_start);
      }
    }
    return std::nullopt;
  }

  std::size_t extent_count() const {
    return this->extents_.size();
  }

 private:
  std::map<std::uint64_t, Extent> extents_;
};

struct BlockRequest {
  int id = 0;
  std::uint64_t sector = 0;
  bool write = false;
  std::uint64_t deadline = 0;
};

class DeadlineBlockScheduler {
 public:
  int submit(std::uint64_t sector, bool write, std::uint64_t now) {
    const int request_id = this->next_request_id_++;
    const std::uint64_t delay = write ? OS_WRITE_DEADLINE_TICKS : OS_READ_DEADLINE_TICKS;
    this->pending_.push_back(BlockRequest{request_id, sector, write, now + delay});
    return request_id;
  }

  std::optional<BlockRequest> dispatch(std::uint64_t now) {
    if (this->pending_.empty()) {
      return std::nullopt;
    }
    const auto request = this->choose_request(now);
    BlockRequest selected = *request;
    this->pending_.erase(request);
    this->head_sector_ = selected.sector;
    return selected;
  }

  std::size_t pending_count() const {
    return this->pending_.size();
  }

 private:
  std::vector<BlockRequest>::iterator choose_request(std::uint64_t now) {
    auto best = this->pending_.begin();
    for (auto iterator = this->pending_.begin(); iterator != this->pending_.end(); ++iterator) {
      if (this->is_better(*iterator, *best, now)) {
        best = iterator;
      }
    }
    return best;
  }

  bool is_better(const BlockRequest& candidate, const BlockRequest& current, std::uint64_t now) const {
    const bool candidate_expired = candidate.deadline <= now;
    const bool current_expired = current.deadline <= now;
    if (candidate_expired != current_expired) {
      return candidate_expired;
    }
    if (candidate_expired && candidate.deadline != current.deadline) {
      return candidate.deadline < current.deadline;
    }
    const std::uint64_t candidate_seek = this->seek_distance(candidate.sector);
    const std::uint64_t current_seek = this->seek_distance(current.sector);
    if (candidate_seek != current_seek) {
      return candidate_seek < current_seek;
    }
    return candidate.id < current.id;
  }

  std::uint64_t seek_distance(std::uint64_t sector) const {
    if (sector >= this->head_sector_) {
      return sector - this->head_sector_;
    }
    return this->head_sector_ - sector;
  }

  int next_request_id_ = 1;
  std::uint64_t head_sector_ = 0;
  std::vector<BlockRequest> pending_;
};

struct JournalRecord {
  int transaction_id = 0;
  int block_id = 0;
  std::string payload;
};

class JournaledBlockDevice {
 public:
  void begin_transaction() {
    require(!this->transaction_active_, "nested transactions are not supported in this model");
    this->transaction_active_ = true;
    this->active_transaction_id_ = this->next_transaction_id_++;
    this->pending_.clear();
  }

  void write_block(int block_id, std::string payload) {
    require(this->transaction_active_, "write_block requires an active transaction");
    this->pending_.push_back(JournalRecord{this->active_transaction_id_, block_id, std::move(payload)});
  }

  void commit(CrashPoint crash_point) {
    require(this->transaction_active_, "commit requires an active transaction");
    for (const JournalRecord& record : this->pending_) {
      this->journal_.push_back(record);
    }
    this->transaction_active_ = false;
    this->pending_.clear();
    if (crash_point == CrashPoint::AfterJournalAppend) {
      return;
    }
    this->replay_journal();
  }

  void recover() {
    this->transaction_active_ = false;
    this->pending_.clear();
    this->replay_journal();
  }

  std::string read_block(int block_id) const {
    const auto iterator = this->home_.find(block_id);
    if (iterator == this->home_.end()) {
      return {};
    }
    return iterator->second;
  }

  std::size_t journal_record_count() const {
    return this->journal_.size();
  }

 private:
  void replay_journal() {
    for (const JournalRecord& record : this->journal_) {
      this->home_[record.block_id] = record.payload;
    }
    this->journal_.clear();
  }

  int next_transaction_id_ = 1;
  int active_transaction_id_ = 0;
  bool transaction_active_ = false;
  std::vector<JournalRecord> pending_;
  std::vector<JournalRecord> journal_;
  std::map<int, std::string> home_;
};

class TcpConnection {
 public:
  void active_open(std::uint64_t now) {
    require(this->state_ == TcpState::Closed, "active_open requires CLOSED");
    this->state_ = TcpState::SynSent;
    this->unacked_segments_ = 1;
    this->retransmission_deadline_ = now + OS_TCP_INITIAL_RTO_TICKS;
  }

  void receive_syn_ack() {
    require(this->state_ == TcpState::SynSent, "SYN-ACK is only valid in SYN-SENT");
    this->state_ = TcpState::Established;
    this->unacked_segments_ = 0;
    this->retransmission_deadline_.reset();
  }

  void send_data(std::size_t byte_count, std::uint64_t now) {
    require(this->state_ == TcpState::Established, "send_data requires ESTABLISHED");
    require(byte_count > 0, "send_data requires a non-empty payload");
    this->unacked_bytes_ += byte_count;
    if (!this->retransmission_deadline_.has_value()) {
      this->retransmission_deadline_ = now + OS_TCP_INITIAL_RTO_TICKS;
    }
  }

  bool tick(std::uint64_t now) {
    if (!this->retransmission_deadline_.has_value() || now < this->retransmission_deadline_.value()) {
      return false;
    }
    ++this->retransmission_count_;
    this->retransmission_deadline_ = now + OS_TCP_INITIAL_RTO_TICKS;
    return true;
  }

  void acknowledge_all() {
    this->unacked_bytes_ = 0;
    this->unacked_segments_ = 0;
    this->retransmission_deadline_.reset();
  }

  void close(std::uint64_t now) {
    require(this->state_ == TcpState::Established, "close requires ESTABLISHED");
    this->state_ = TcpState::FinWait1;
    this->unacked_segments_ = 1;
    this->retransmission_deadline_ = now + OS_TCP_INITIAL_RTO_TICKS;
  }

  void receive_fin_ack() {
    require(this->state_ == TcpState::FinWait1, "FIN ACK requires FIN-WAIT-1");
    this->state_ = TcpState::TimeWait;
    this->acknowledge_all();
  }

  TcpState state() const {
    return this->state_;
  }

  std::size_t retransmission_count() const {
    return this->retransmission_count_;
  }

  std::size_t unacked_bytes() const {
    return this->unacked_bytes_;
  }

 private:
  TcpState state_ = TcpState::Closed;
  std::size_t unacked_bytes_ = 0;
  std::size_t unacked_segments_ = 0;
  std::size_t retransmission_count_ = 0;
  std::optional<std::uint64_t> retransmission_deadline_;
};

std::vector<std::uint8_t> bytes(std::string_view text) {
  return {text.begin(), text.end()};
}

std::string text_of(const std::vector<std::uint8_t>& data) {
  return {data.begin(), data.end()};
}

template <typename Function>
void require_throws(Function action, std::string_view message) {
  bool thrown = false;
  try {
    action();
  } catch (const std::runtime_error&) {
    thrown = true;
  }
  require(thrown, message);
}

void test_cfs_scheduler_and_wait_queue() {
  Scheduler scheduler;
  TaskPtr low = scheduler.create_task("low", OS_LOW_TASK_WEIGHT);
  TaskPtr normal = scheduler.create_task("normal", OS_DEFAULT_TASK_WEIGHT);
  TaskPtr high = scheduler.create_task("high", OS_HIGH_TASK_WEIGHT);

  TaskPtr first = scheduler.schedule_next();
  require(first != nullptr, "scheduler must pick a task");
  scheduler.tick_and_requeue(first, 8);

  TaskPtr second = scheduler.schedule_next();
  require(second != nullptr, "scheduler must pick second task");
  scheduler.tick_and_requeue(second, 8);

  TaskPtr third = scheduler.schedule_next();
  require(third != nullptr, "scheduler must pick third task");
  WaitQueue io_wait;
  scheduler.block_current(third, io_wait, "disk");
  require(io_wait.size() == 1, "blocked task must enter wait queue");

  std::optional<TaskPtr> woken = io_wait.wake_one();
  require(woken.has_value(), "wake_one must wake task");
  scheduler.wake_to_runqueue(woken.value());
  require(scheduler.runnable_count() == 3, "all three tasks should be runnable again");

  require(low->executed + normal->executed + high->executed == 16, "two accounted time slices expected");
}

void test_cow_page_table_and_tlb() {
  PageAllocator allocator;
  Tlb tlb;
  AddressSpace parent(1, allocator);
  constexpr std::uint64_t TEST_VA = 0x1000;
  constexpr std::uint8_t PARENT_INITIAL_VALUE = 42;
  constexpr std::uint8_t CHILD_COW_VALUE = 7;
  constexpr std::uint8_t PARENT_COW_VALUE = 9;
  parent.map_anonymous(TEST_VA / OS_PAGE_SIZE, true, false);
  parent.write_byte(TEST_VA, PARENT_INITIAL_VALUE, tlb);
  require(parent.read_byte(TEST_VA, tlb) == PARENT_INITIAL_VALUE, "parent write must be readable");

  AddressSpace child = parent.fork_cow(2, tlb);
  require(parent.mapped_pages() == 1 && child.mapped_pages() == 1, "fork should preserve mappings");
  child.write_byte(TEST_VA, CHILD_COW_VALUE, tlb);
  require(child.read_byte(TEST_VA, tlb) == CHILD_COW_VALUE, "child COW write must change child");
  require(parent.read_byte(TEST_VA, tlb) == PARENT_INITIAL_VALUE, "child COW write must not change parent");

  parent.write_byte(TEST_VA, PARENT_COW_VALUE, tlb);
  require(parent.read_byte(TEST_VA, tlb) == PARENT_COW_VALUE, "parent COW write must succeed");
  require(child.read_byte(TEST_VA, tlb) == CHILD_COW_VALUE, "parent COW write must not change child");
}

void test_iommu_and_nvme_completion_queue() {
  PageAllocator allocator;
  PagePtr page = allocator.allocate_zeroed();
  constexpr std::uint8_t DMA_INITIAL_VALUE = 11;
  constexpr std::uint8_t DMA_DEVICE_VALUE = 22;
  constexpr std::uint16_t NVME_STATUS_SUCCESS = 0;
  page->bytes.at(0) = DMA_INITIAL_VALUE;
  IommuDomain domain;
  const std::uint64_t iova = domain.map(page, OS_PAGE_SIZE, DmaDirection::Bidirectional);
  require(domain.device_read(iova, 0) == DMA_INITIAL_VALUE, "device must read mapped page");
  domain.device_write(iova, 1, DMA_DEVICE_VALUE);
  require(page->bytes.at(1) == DMA_DEVICE_VALUE, "device write must update mapped page");
  domain.unmap(iova);
  require(domain.mapping_count() == 0, "unmap must remove DMA mapping");

  NvmeCompletionQueue queue(OS_NVME_QUEUE_SIZE);
  require(!queue.host_poll().has_value(), "empty completion queue must not report a completion");
  constexpr std::uint16_t FIRST_COMMAND_ID = 10;
  constexpr std::uint16_t SECOND_COMMAND_ID = 11;
  queue.device_push(FIRST_COMMAND_ID, NVME_STATUS_SUCCESS);
  queue.device_push(SECOND_COMMAND_ID, NVME_STATUS_SUCCESS);
  std::optional<NvmeCompletion> first = queue.host_poll();
  std::optional<NvmeCompletion> second = queue.host_poll();
  require(first.has_value() && first->command_id == FIRST_COMMAND_ID, "first completion id mismatch");
  require(second.has_value() && second->command_id == SECOND_COMMAND_ID, "second completion id mismatch");
}

void test_fd_table_page_cache_and_fsync() {
  auto inode = std::make_shared<Inode>("notes.txt");
  auto file = std::make_shared<OpenFile>(inode);
  FileDescriptorTable table;
  const int fd = table.install(file);
  require(fd != OS_INVALID_FD, "fd install must succeed");

  const std::vector<std::uint8_t> hello = bytes("hello");
  require(table.get(fd)->write(hello) == hello.size(), "write must copy all bytes");

  const int dup_fd = table.dup(fd);
  require(dup_fd != OS_INVALID_FD, "dup must return a valid fd");
  require(table.get(dup_fd)->offset() == table.get(fd)->offset(), "dup must share open-file offset");

  table.get(fd)->seek(0);
  const std::vector<std::uint8_t> read_back = table.get(dup_fd)->read(hello.size());
  require(text_of(read_back) == "hello", "read must observe page cache contents");

  require(inode->stable_storage().empty(), "buffered write should not be durable before fsync");
  inode->fsync();
  require(text_of(inode->stable_storage()) == "hello", "fsync must copy dirty cache to stable storage");

  table.close(fd);
  require(table.size() == 1, "close one fd should leave duplicated fd alive");
  table.close(dup_fd);
  require(table.size() == 0, "all fds closed");
}

void test_futex_deadlock_and_buddy_allocator() {
  auto waiter = std::make_shared<Task>(1, "waiter", OS_DEFAULT_TASK_WEIGHT);
  waiter->state = TaskState::Running;
  FutexWord futex(0);
  require(!futex.wait_if_equal(waiter, 1), "futex must not sleep when user value changed first");
  require(waiter->state == TaskState::Running, "failed futex compare must leave task runnable/running");
  require(futex.wait_if_equal(waiter, 0), "matching futex value must place task on wait queue");
  require(waiter->state == TaskState::Sleeping && futex.waiter_count() == 1, "futex wait must sleep atomically");
  futex.store(1);
  std::optional<TaskPtr> woken = futex.wake_one();
  require(woken.has_value() && woken.value()->state == TaskState::Runnable, "futex wake must return one task");

  DeadlockDetector detector;
  detector.add_wait_edge(1, 2);
  detector.add_wait_edge(2, 3);
  require(!detector.has_cycle(), "chain wait graph must not be a deadlock");
  detector.add_wait_edge(3, 1);
  require(detector.has_cycle(), "cycle in wait-for graph must be reported");
  detector.clear_task(2);
  require(!detector.has_cycle(), "removing a task from the cycle must break the deadlock");
  require(detector.edge_count() == 1, "clear_task must remove incoming and outgoing edges");

  BuddyAllocator allocator(OS_BUDDY_MAX_ORDER);
  const std::size_t first = allocator.allocate_pages(1);
  const std::size_t second = allocator.allocate_pages(1);
  require(first == 0 && second == 1, "buddy allocator should split the first pages deterministically");
  require(allocator.allocated_block_count() == 2, "two blocks should be allocated");
  allocator.free_pages(first);
  allocator.free_pages(second);
  require(allocator.allocated_block_count() == 0, "all buddy allocations should be released");
  require(allocator.free_blocks_at_order(OS_BUDDY_MAX_ORDER) == 1, "buddy free should coalesce to one block");
}

void test_lru_extent_block_scheduler_and_journal() {
  LruPageCache cache(2);
  cache.access(1, true);
  cache.access(2, false);
  cache.access(1, false);
  cache.access(3, false);
  require(cache.contains(1) && cache.contains(3), "LRU cache must retain recently touched pages");
  require(!cache.contains(2), "LRU cache must evict the cold page");
  require(cache.writeback_count() == 0, "clean eviction should not write back");
  cache.access(4, true);
  cache.access(5, false);
  require(cache.writeback_count() == 1, "dirty eviction must trigger writeback");
  cache.flush_all();
  require(cache.writeback_count() == 2, "flush_all should write remaining dirty entries and skip clean ones");

  ExtentMap extents;
  extents.add_extent(0, 100, 8);
  extents.add_extent(8, 200, 4);
  const std::optional<std::uint64_t> physical = extents.lookup(10);
  require(physical.has_value() && physical.value() == 202, "extent lookup must translate inside the extent");
  require(!extents.lookup(20).has_value(), "extent lookup outside all extents should miss");
  require_throws([&extents]() { extents.add_extent(4, 300, 2); }, "overlapping extents must be rejected");

  DeadlineBlockScheduler scheduler;
  const int far_read = scheduler.submit(90, false, 0);
  const int near_write = scheduler.submit(10, true, 0);
  const int expired_read = scheduler.submit(70, false, 0);
  static_cast<void>(far_read);
  static_cast<void>(near_write);
  std::optional<BlockRequest> first = scheduler.dispatch(OS_READ_DEADLINE_TICKS);
  require(first.has_value() && first->id == expired_read, "expired read should dispatch before seek optimization");
  std::optional<BlockRequest> second = scheduler.dispatch(OS_READ_DEADLINE_TICKS + 1);
  require(second.has_value() && second->sector == 90, "remaining closest request to current head should dispatch next");
  require(scheduler.pending_count() == 1, "one block request should remain queued");

  JournaledBlockDevice device;
  device.begin_transaction();
  device.write_block(7, "metadata-v2");
  device.commit(CrashPoint::AfterJournalAppend);
  require(device.read_block(7).empty(), "home block should not change before recovery after a crash");
  require(device.journal_record_count() == 1, "journal must retain committed intent after crash");
  device.recover();
  require(device.read_block(7) == "metadata-v2", "recovery must replay committed journal records");
  require(device.journal_record_count() == 0, "journal should be empty after replay");
}

void test_tcp_retransmission_state_machine() {
  TcpConnection connection;
  connection.active_open(100);
  require(connection.state() == TcpState::SynSent, "active open should send SYN and enter SYN-SENT");
  require(!connection.tick(102), "RTO should not fire before deadline");
  require(connection.tick(103), "RTO should fire at the retransmission deadline");
  require(connection.retransmission_count() == 1, "SYN retransmission should be counted");
  connection.receive_syn_ack();
  require(connection.state() == TcpState::Established, "SYN-ACK should establish the connection");
  connection.send_data(128, 200);
  require(connection.unacked_bytes() == 128, "send_data must track unacknowledged bytes");
  require(connection.tick(203), "data RTO should trigger retransmission");
  connection.acknowledge_all();
  require(!connection.tick(206), "ACK should cancel retransmission timer");
  connection.close(300);
  require(connection.state() == TcpState::FinWait1, "close should enter FIN-WAIT-1");
  connection.receive_fin_ack();
  require(connection.state() == TcpState::TimeWait, "FIN ACK should enter TIME-WAIT");
}

void run_all_tests() {
  test_cfs_scheduler_and_wait_queue();
  test_cow_page_table_and_tlb();
  test_iommu_and_nvme_completion_queue();
  test_fd_table_page_cache_and_fsync();
  test_futex_deadlock_and_buddy_allocator();
  test_lru_extent_block_scheduler_and_journal();
  test_tcp_retransmission_state_machine();
}

}  // namespace osbook

int main() {
  try {
    osbook::run_all_tests();
    std::cout << "os_reference_implementation: all tests passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "test failure: " << error.what() << '\n';
    return 1;
  }
}

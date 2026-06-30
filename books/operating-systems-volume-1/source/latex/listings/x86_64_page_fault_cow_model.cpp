#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

using Byte = std::uint8_t;
using Word = std::uint64_t;
using VirtualAddress = std::uint64_t;
using PageNumber = std::uint64_t;
using Pcid = std::uint16_t;
using FrameId = std::uint32_t;

constexpr std::uint64_t X86_VIRTUAL_ADDRESS_BITS = 48U;
constexpr std::uint64_t X86_CANONICAL_SIGN_BIT =
    1ULL << (X86_VIRTUAL_ADDRESS_BITS - 1U);
constexpr VirtualAddress X86_CANONICAL_LOW_MASK =
    (1ULL << X86_VIRTUAL_ADDRESS_BITS) - 1ULL;
constexpr VirtualAddress X86_CANONICAL_HIGH_MASK = ~X86_CANONICAL_LOW_MASK;
constexpr std::uint64_t X86_PAGE_SHIFT = 12U;
constexpr std::size_t X86_PAGE_SIZE = 1ULL << X86_PAGE_SHIFT;
constexpr VirtualAddress X86_PAGE_OFFSET_MASK =
    static_cast<VirtualAddress>(X86_PAGE_SIZE - 1U);
constexpr Word X86_PF_PROTECTION_BIT = 1ULL << 0U;
constexpr Word X86_PF_WRITE_BIT = 1ULL << 1U;
constexpr Word X86_PF_USER_BIT = 1ULL << 2U;
constexpr std::size_t X86_FAULT_RETRY_LIMIT = 3U;
constexpr std::size_t HASH_COMBINE_SHIFT = 1U;

constexpr Pcid TEST_PARENT_PCID = 41U;
constexpr Pcid TEST_CHILD_PCID = 42U;
constexpr Pcid TEST_FILE_PCID = 43U;
constexpr VirtualAddress TEST_ANON_PAGE = 0x0000'0000'0040'0000ULL;
constexpr VirtualAddress TEST_ANON_VA = TEST_ANON_PAGE + 0x30ULL;
constexpr VirtualAddress TEST_READ_ONLY_PAGE = 0x0000'0000'0050'0000ULL;
constexpr VirtualAddress TEST_READ_ONLY_VA = TEST_READ_ONLY_PAGE + 0x20ULL;
constexpr VirtualAddress TEST_COW_PAGE = 0x0000'0000'0060'0000ULL;
constexpr VirtualAddress TEST_COW_VA = TEST_COW_PAGE + 0x10ULL;
constexpr VirtualAddress TEST_FILE_PAGE = 0x0000'0000'0070'0000ULL;
constexpr VirtualAddress TEST_FILE_VA = TEST_FILE_PAGE + 0x80ULL;
constexpr VirtualAddress TEST_BAD_USER_VA = 0x0000'0000'0080'0000ULL;
constexpr VirtualAddress TEST_NON_CANONICAL_VA = 0x0000'8000'0000'0000ULL;
constexpr std::uint64_t TEST_FILE_PAGE_INDEX = 9U;
constexpr std::size_t TEST_FILE_BYTE_OFFSET =
    static_cast<std::size_t>(TEST_FILE_VA & X86_PAGE_OFFSET_MASK);
constexpr Byte TEST_ZERO_BYTE = 0x00U;
constexpr Byte TEST_ANON_BYTE = 0x5AU;
constexpr Byte TEST_ORIGINAL_BYTE = 0x11U;
constexpr Byte TEST_PARENT_BYTE = 0x22U;
constexpr Byte TEST_CHILD_BYTE = 0x33U;
constexpr Byte TEST_FILE_BYTE = 0x7EU;

enum class AccessKind {
  Read,
  Write,
  Execute,
};

enum class VmaKind {
  Anonymous,
  FileBacked,
};

enum class FaultContext {
  UserInstruction,
  UserCopy,
};

enum class FaultDisposition {
  Handled,
  Signal,
  BadUserPointer,
};

enum class AccessFailureKind {
  SegmentationViolation,
  BadUserPointer,
  RetryLimitExceeded,
};

struct PageTableEntry {
  FrameId frame = 0;
  bool present = false;
  bool writable = false;
  bool user = true;
  bool executable = false;
  bool cow = false;
};

struct Translation {
  FrameId frame = 0;
  std::size_t offset = 0;
};

struct Vma {
  VirtualAddress start = 0;
  VirtualAddress end = 0;
  bool readable = false;
  bool writable = false;
  bool executable = false;
  bool private_mapping = true;
  VmaKind kind = VmaKind::Anonymous;
  std::uint64_t file_page_index = 0;
};

struct FaultStats {
  std::size_t minor_faults = 0;
  std::size_t major_faults = 0;
  std::size_t cow_copies = 0;
  std::size_t cow_promotions = 0;
  std::size_t signals = 0;
  std::size_t bad_user_pointers = 0;
};

struct PageCacheResult {
  FrameId frame = 0;
  bool hit = false;
};

struct TlbKey {
  Pcid pcid = 0;
  PageNumber vpn = 0;

  bool operator==(const TlbKey& other) const {
    return this->pcid == other.pcid && this->vpn == other.vpn;
  }
};

struct TlbKeyHash {
  std::size_t operator()(const TlbKey& key) const {
    const std::size_t pcid_hash = std::hash<Pcid>{}(key.pcid);
    const std::size_t vpn_hash = std::hash<PageNumber>{}(key.vpn);
    return pcid_hash ^ (vpn_hash << HASH_COMBINE_SHIFT);
  }
};

bool is_canonical(VirtualAddress address) {
  const bool sign_bit_set = (address & X86_CANONICAL_SIGN_BIT) != 0;
  const VirtualAddress high_bits = address & X86_CANONICAL_HIGH_MASK;
  if (sign_bit_set) {
    return high_bits == X86_CANONICAL_HIGH_MASK;
  }
  return high_bits == 0;
}

VirtualAddress page_base(VirtualAddress address) {
  return address & ~X86_PAGE_OFFSET_MASK;
}

PageNumber virtual_page_number(VirtualAddress address) {
  return address >> X86_PAGE_SHIFT;
}

std::size_t page_offset(VirtualAddress address) {
  return static_cast<std::size_t>(address & X86_PAGE_OFFSET_MASK);
}

void require(bool condition, std::string_view message) {
  if (!condition) {
    throw std::runtime_error(std::string(message));
  }
}

class PageFault : public std::runtime_error {
 public:
  PageFault(VirtualAddress fault_address, AccessKind fault_access,
            Word fault_error_code, std::string_view message)
      : std::runtime_error(std::string(message)),
        address_(fault_address),
        access_(fault_access),
        error_code_(fault_error_code) {}

  VirtualAddress address() const {
    return this->address_;
  }

  AccessKind access() const {
    return this->access_;
  }

  Word error_code() const {
    return this->error_code_;
  }

 private:
  VirtualAddress address_ = 0;
  AccessKind access_ = AccessKind::Read;
  Word error_code_ = 0;
};

class AccessFailure : public std::runtime_error {
 public:
  AccessFailure(AccessFailureKind failure_kind, std::string_view message)
      : std::runtime_error(std::string(message)), kind_(failure_kind) {}

  AccessFailureKind kind() const {
    return this->kind_;
  }

 private:
  AccessFailureKind kind_ = AccessFailureKind::SegmentationViolation;
};

template <typename Callable>
void require_access_failure(AccessFailureKind expected, Callable action) {
  try {
    action();
  } catch (const AccessFailure& failure) {
    require(failure.kind() == expected, "unexpected access failure kind");
    return;
  }
  throw std::runtime_error("expected access failure was not raised");
}

class PhysicalMemory {
 public:
  FrameId allocate_zero_frame() {
    const FrameId frame = static_cast<FrameId>(this->frames_.size());
    this->frames_.push_back(Frame{});
    this->refcounts_.push_back(1U);
    return frame;
  }

  FrameId allocate_copy_frame(FrameId source) {
    const FrameId frame = this->allocate_zero_frame();
    this->frames_.at(frame) = this->frames_.at(source);
    return frame;
  }

  void increment_ref(FrameId frame) {
    ++this->refcounts_.at(frame);
  }

  void decrement_ref(FrameId frame) {
    std::size_t& refcount = this->refcounts_.at(frame);
    require(refcount > 0, "physical frame refcount underflow");
    --refcount;
  }

  std::size_t refcount(FrameId frame) const {
    return this->refcounts_.at(frame);
  }

  Byte load8(FrameId frame, std::size_t offset) const {
    require(offset < X86_PAGE_SIZE, "load offset must stay inside page");
    return this->frames_.at(frame).bytes.at(offset);
  }

  void store8(FrameId frame, std::size_t offset, Byte value) {
    require(offset < X86_PAGE_SIZE, "store offset must stay inside page");
    this->frames_.at(frame).bytes.at(offset) = value;
  }

  std::size_t frame_count() const {
    return this->frames_.size();
  }

 private:
  struct Frame {
    std::array<Byte, X86_PAGE_SIZE> bytes{};
  };

  std::vector<Frame> frames_;
  std::vector<std::size_t> refcounts_;
};

class PageCache {
 public:
  PageCacheResult get_or_read_page(PhysicalMemory& memory,
                                   std::uint64_t file_page_index) {
    const auto iterator = this->pages_.find(file_page_index);
    if (iterator != this->pages_.end()) {
      return PageCacheResult{.frame = iterator->second, .hit = true};
    }

    const FrameId frame = memory.allocate_zero_frame();
    memory.store8(frame, TEST_FILE_BYTE_OFFSET, TEST_FILE_BYTE);
    this->pages_[file_page_index] = frame;
    return PageCacheResult{.frame = frame, .hit = false};
  }

  std::size_t size() const {
    return this->pages_.size();
  }

 private:
  std::map<std::uint64_t, FrameId> pages_;
};

class Tlb {
 public:
  std::optional<PageTableEntry> lookup(Pcid pcid, PageNumber vpn) const {
    const auto iterator = this->entries_.find(TlbKey{.pcid = pcid, .vpn = vpn});
    if (iterator == this->entries_.end()) {
      return std::nullopt;
    }
    return iterator->second;
  }

  void insert(Pcid pcid, PageNumber vpn, const PageTableEntry& entry) {
    this->entries_[TlbKey{.pcid = pcid, .vpn = vpn}] = entry;
  }

  void invalidate(Pcid pcid, PageNumber vpn) {
    this->entries_.erase(TlbKey{.pcid = pcid, .vpn = vpn});
  }

  std::size_t size() const {
    return this->entries_.size();
  }

 private:
  std::unordered_map<TlbKey, PageTableEntry, TlbKeyHash> entries_;
};

class AddressSpace {
 public:
  explicit AddressSpace(Pcid pcid) : pcid_(pcid) {}

  Pcid pcid() const {
    return this->pcid_;
  }

  void add_vma(const Vma& vma) {
    require(is_canonical(vma.start), "VMA start must be canonical");
    require(is_canonical(vma.end - 1U), "VMA end must be canonical");
    require(vma.start < vma.end, "VMA must not be empty");
    this->vmas_.push_back(vma);
  }

  const Vma* find_vma(VirtualAddress address) const {
    for (const Vma& vma : this->vmas_) {
      if (address >= vma.start && address < vma.end) {
        return &vma;
      }
    }
    return nullptr;
  }

  void map_page(VirtualAddress virtual_page, const PageTableEntry& entry) {
    require((virtual_page & X86_PAGE_OFFSET_MASK) == 0,
            "mapped virtual address must be page aligned");
    this->entries_[virtual_page_number(virtual_page)] = entry;
  }

  const PageTableEntry* find_pte(PageNumber vpn) const {
    const auto iterator = this->entries_.find(vpn);
    if (iterator == this->entries_.end()) {
      return nullptr;
    }
    return &iterator->second;
  }

  PageTableEntry* find_pte_for_mutation(PageNumber vpn) {
    const auto iterator = this->entries_.find(vpn);
    if (iterator == this->entries_.end()) {
      return nullptr;
    }
    return &iterator->second;
  }

  const std::vector<Vma>& vmas() const {
    return this->vmas_;
  }

  const std::map<PageNumber, PageTableEntry>& entries() const {
    return this->entries_;
  }

  std::map<PageNumber, PageTableEntry>& entries_for_mutation() {
    return this->entries_;
  }

  std::size_t mapped_page_count() const {
    return this->entries_.size();
  }

 private:
  Pcid pcid_ = 0;
  std::vector<Vma> vmas_;
  std::map<PageNumber, PageTableEntry> entries_;
};

class Mmu {
 public:
  Translation translate(const AddressSpace& address_space, Tlb& tlb,
                        VirtualAddress virtual_address, AccessKind access) const {
    if (!is_canonical(virtual_address)) {
      throw PageFault(
          virtual_address, access, this->error_code(access, false),
          "x86-64 rejected non-canonical virtual address before VMA lookup");
    }

    const PageNumber vpn = virtual_page_number(virtual_address);
    const std::optional<PageTableEntry> cached = tlb.lookup(address_space.pcid(), vpn);
    if (cached.has_value()) {
      this->check_entry(virtual_address, access, cached.value());
      return Translation{.frame = cached->frame, .offset = page_offset(virtual_address)};
    }

    const PageTableEntry* walked = address_space.find_pte(vpn);
    if (walked == nullptr || !walked->present) {
      throw PageFault(virtual_address, access, this->error_code(access, false),
                      "page walk found no present PTE");
    }
    this->check_entry(virtual_address, access, *walked);
    tlb.insert(address_space.pcid(), vpn, *walked);
    return Translation{.frame = walked->frame, .offset = page_offset(virtual_address)};
  }

 private:
  void check_entry(VirtualAddress address, AccessKind access,
                   const PageTableEntry& entry) const {
    if (!entry.user) {
      throw PageFault(address, access, this->error_code(access, true),
                      "ring 3 access reached supervisor PTE");
    }
    if (access == AccessKind::Write && !entry.writable) {
      throw PageFault(address, access, this->error_code(access, true),
                      "write access rejected by PTE");
    }
    if (access == AccessKind::Execute && !entry.executable) {
      throw PageFault(address, access, this->error_code(access, true),
                      "execute access rejected by PTE");
    }
  }

  Word error_code(AccessKind access, bool protection) const {
    Word code = X86_PF_USER_BIT;
    if (protection) {
      code |= X86_PF_PROTECTION_BIT;
    }
    if (access == AccessKind::Write) {
      code |= X86_PF_WRITE_BIT;
    }
    return code;
  }
};

class KernelMemoryManager {
 public:
  KernelMemoryManager(PhysicalMemory& memory, PageCache& page_cache, Tlb& tlb)
      : memory_(memory), page_cache_(page_cache), tlb_(tlb) {}

  const FaultStats& stats() const {
    return this->stats_;
  }

  FaultDisposition handle_page_fault(AddressSpace& address_space,
                                     const PageFault& fault,
                                     FaultContext context) {
    const Vma* vma = address_space.find_vma(fault.address());
    if (vma == nullptr || !this->vma_allows(*vma, fault.access())) {
      return this->reject_fault(context);
    }

    const PageNumber vpn = virtual_page_number(fault.address());
    PageTableEntry* pte = address_space.find_pte_for_mutation(vpn);
    const bool protection_fault =
        (fault.error_code() & X86_PF_PROTECTION_BIT) != 0;
    if (!protection_fault) {
      return this->handle_missing_pte(address_space, *vma, vpn);
    }
    if (fault.access() == AccessKind::Write && pte != nullptr && pte->cow) {
      return this->handle_cow_fault(address_space, vpn, *pte);
    }
    return this->reject_fault(context);
  }

  void fork_private(AddressSpace& parent, AddressSpace& child,
                    bool invalidate_parent_tlb) {
    for (const Vma& vma : parent.vmas()) {
      child.add_vma(vma);
    }

    for (auto& [vpn, entry] : parent.entries_for_mutation()) {
      PageTableEntry child_entry = entry;
      const VirtualAddress virtual_page = vpn << X86_PAGE_SHIFT;
      const Vma* vma = parent.find_vma(virtual_page);
      if (entry.present && vma != nullptr) {
        this->memory_.increment_ref(entry.frame);
        if (vma->private_mapping && vma->writable) {
          entry.writable = false;
          entry.cow = true;
          child_entry.writable = false;
          child_entry.cow = true;
          if (invalidate_parent_tlb) {
            this->tlb_.invalidate(parent.pcid(), vpn);
          }
        }
      }
      child.map_page(virtual_page, child_entry);
    }
  }

 private:
  bool vma_allows(const Vma& vma, AccessKind access) const {
    if (access == AccessKind::Read) {
      return vma.readable;
    }
    if (access == AccessKind::Write) {
      return vma.writable;
    }
    return vma.executable;
  }

  FaultDisposition reject_fault(FaultContext context) {
    if (context == FaultContext::UserCopy) {
      ++this->stats_.bad_user_pointers;
      return FaultDisposition::BadUserPointer;
    }
    ++this->stats_.signals;
    return FaultDisposition::Signal;
  }

  FaultDisposition handle_missing_pte(AddressSpace& address_space, const Vma& vma,
                                      PageNumber vpn) {
    if (vma.kind == VmaKind::Anonymous) {
      const FrameId frame = this->memory_.allocate_zero_frame();
      address_space.map_page(
          vpn << X86_PAGE_SHIFT,
          PageTableEntry{
              .frame = frame,
              .present = true,
              .writable = vma.writable,
              .user = true,
              .executable = vma.executable,
              .cow = false,
          });
      ++this->stats_.minor_faults;
      return FaultDisposition::Handled;
    }

    PageCacheResult result =
        this->page_cache_.get_or_read_page(this->memory_, vma.file_page_index);
    this->memory_.increment_ref(result.frame);
    address_space.map_page(
        vpn << X86_PAGE_SHIFT,
        PageTableEntry{
            .frame = result.frame,
            .present = true,
            .writable = vma.writable && !vma.private_mapping,
            .user = true,
            .executable = vma.executable,
            .cow = false,
        });
    if (result.hit) {
      ++this->stats_.minor_faults;
    } else {
      ++this->stats_.major_faults;
    }
    return FaultDisposition::Handled;
  }

  FaultDisposition handle_cow_fault(AddressSpace& address_space, PageNumber vpn,
                                    PageTableEntry& pte) {
    if (this->memory_.refcount(pte.frame) == 1U) {
      pte.writable = true;
      pte.cow = false;
      this->tlb_.invalidate(address_space.pcid(), vpn);
      ++this->stats_.cow_promotions;
      return FaultDisposition::Handled;
    }

    const FrameId old_frame = pte.frame;
    const FrameId new_frame = this->memory_.allocate_copy_frame(old_frame);
    this->memory_.decrement_ref(old_frame);
    pte.frame = new_frame;
    pte.writable = true;
    pte.cow = false;
    this->tlb_.invalidate(address_space.pcid(), vpn);
    ++this->stats_.cow_copies;
    return FaultDisposition::Handled;
  }

  PhysicalMemory& memory_;
  PageCache& page_cache_;
  Tlb& tlb_;
  FaultStats stats_;
};

class UserCpu {
 public:
  UserCpu(const Mmu& mmu, PhysicalMemory& memory, Tlb& tlb,
          KernelMemoryManager& kernel)
      : mmu_(mmu), memory_(memory), tlb_(tlb), kernel_(kernel) {}

  Byte load8(AddressSpace& address_space, VirtualAddress virtual_address) {
    for (std::size_t attempt = 0; attempt < X86_FAULT_RETRY_LIMIT; ++attempt) {
      try {
        const Translation translation =
            this->mmu_.translate(address_space, this->tlb_, virtual_address,
                                 AccessKind::Read);
        return this->memory_.load8(translation.frame, translation.offset);
      } catch (const PageFault& fault) {
        this->handle_or_throw(address_space, fault, FaultContext::UserInstruction);
      }
    }
    throw AccessFailure(AccessFailureKind::RetryLimitExceeded,
                        "load exceeded page fault retry limit");
  }

  void store8(AddressSpace& address_space, VirtualAddress virtual_address,
              Byte value) {
    for (std::size_t attempt = 0; attempt < X86_FAULT_RETRY_LIMIT; ++attempt) {
      try {
        const Translation translation =
            this->mmu_.translate(address_space, this->tlb_, virtual_address,
                                 AccessKind::Write);
        this->memory_.store8(translation.frame, translation.offset, value);
        return;
      } catch (const PageFault& fault) {
        this->handle_or_throw(address_space, fault, FaultContext::UserInstruction);
      }
    }
    throw AccessFailure(AccessFailureKind::RetryLimitExceeded,
                        "store exceeded page fault retry limit");
  }

  bool copy_from_user(AddressSpace& address_space, VirtualAddress virtual_address,
                      Byte& output) {
    for (std::size_t attempt = 0; attempt < X86_FAULT_RETRY_LIMIT; ++attempt) {
      try {
        const Translation translation =
            this->mmu_.translate(address_space, this->tlb_, virtual_address,
                                 AccessKind::Read);
        output = this->memory_.load8(translation.frame, translation.offset);
        return true;
      } catch (const PageFault& fault) {
        const FaultDisposition disposition =
            this->kernel_.handle_page_fault(address_space, fault,
                                           FaultContext::UserCopy);
        if (disposition == FaultDisposition::Handled) {
          continue;
        }
        return false;
      }
    }
    return false;
  }

 private:
  void handle_or_throw(AddressSpace& address_space, const PageFault& fault,
                       FaultContext context) {
    const FaultDisposition disposition =
        this->kernel_.handle_page_fault(address_space, fault, context);
    if (disposition == FaultDisposition::Signal) {
      throw AccessFailure(AccessFailureKind::SegmentationViolation,
                          "page fault became a user signal");
    }
    if (disposition == FaultDisposition::BadUserPointer) {
      throw AccessFailure(AccessFailureKind::BadUserPointer,
                          "page fault became a bad user pointer");
    }
  }

  const Mmu& mmu_;
  PhysicalMemory& memory_;
  Tlb& tlb_;
  KernelMemoryManager& kernel_;
};

Vma anonymous_vma(VirtualAddress page, bool writable) {
  return Vma{
      .start = page,
      .end = page + X86_PAGE_SIZE,
      .readable = true,
      .writable = writable,
      .executable = false,
      .private_mapping = true,
      .kind = VmaKind::Anonymous,
      .file_page_index = 0,
  };
}

Vma file_vma(VirtualAddress page) {
  return Vma{
      .start = page,
      .end = page + X86_PAGE_SIZE,
      .readable = true,
      .writable = false,
      .executable = false,
      .private_mapping = true,
      .kind = VmaKind::FileBacked,
      .file_page_index = TEST_FILE_PAGE_INDEX,
  };
}

struct TestMachine {
  PhysicalMemory memory;
  PageCache page_cache;
  Tlb tlb;
  Mmu mmu;
  KernelMemoryManager kernel;
  UserCpu cpu;

  TestMachine()
      : memory(),
        page_cache(),
        tlb(),
        mmu(),
        kernel(memory, page_cache, tlb),
        cpu(mmu, memory, tlb, kernel) {}
};

void test_anonymous_fault_allocates_and_retries() {
  TestMachine machine;
  AddressSpace process(TEST_PARENT_PCID);
  process.add_vma(anonymous_vma(TEST_ANON_PAGE, true));

  require(process.mapped_page_count() == 0U,
          "anonymous VMA should not allocate a PTE before access");
  require(machine.cpu.load8(process, TEST_ANON_VA) == TEST_ZERO_BYTE,
          "anonymous demand fault should return a zero-filled byte");
  machine.cpu.store8(process, TEST_ANON_VA, TEST_ANON_BYTE);
  require(machine.cpu.load8(process, TEST_ANON_VA) == TEST_ANON_BYTE,
          "anonymous page did not preserve user store");
  require(machine.kernel.stats().minor_faults == 1U,
          "anonymous demand paging should be a minor fault in this model");
}

void test_vma_permission_rejects_write() {
  TestMachine machine;
  AddressSpace process(TEST_PARENT_PCID);
  process.add_vma(anonymous_vma(TEST_READ_ONLY_PAGE, false));

  require_access_failure(AccessFailureKind::SegmentationViolation, [&machine,
                                                                    &process] {
    machine.cpu.store8(process, TEST_READ_ONLY_VA, TEST_ANON_BYTE);
  });
  require(machine.kernel.stats().signals == 1U,
          "write to read-only VMA should become a user-visible failure");
  require(process.mapped_page_count() == 0U,
          "permission failure must not allocate a page");
}

void test_cow_split_preserves_parent_child_isolation() {
  TestMachine machine;
  AddressSpace parent(TEST_PARENT_PCID);
  parent.add_vma(anonymous_vma(TEST_COW_PAGE, true));

  machine.cpu.store8(parent, TEST_COW_VA, TEST_ORIGINAL_BYTE);
  const FrameId shared_frame =
      parent.find_pte(virtual_page_number(TEST_COW_VA))->frame;
  AddressSpace child(TEST_CHILD_PCID);
  machine.kernel.fork_private(parent, child, true);

  require(machine.memory.refcount(shared_frame) == 2U,
          "fork should share the original frame before either process writes");
  machine.cpu.store8(parent, TEST_COW_VA, TEST_PARENT_BYTE);
  require(machine.cpu.load8(parent, TEST_COW_VA) == TEST_PARENT_BYTE,
          "parent COW write did not install private data");
  require(machine.cpu.load8(child, TEST_COW_VA) == TEST_ORIGINAL_BYTE,
          "child should still see the pre-COW byte");
  require(machine.kernel.stats().cow_copies == 1U,
          "shared COW page should require one copy");

  machine.cpu.store8(child, TEST_COW_VA, TEST_CHILD_BYTE);
  require(machine.cpu.load8(child, TEST_COW_VA) == TEST_CHILD_BYTE,
          "child COW promotion did not make the child page writable");
  require(machine.kernel.stats().cow_promotions == 1U,
          "last private COW reference should be promoted without copying");
}

void test_missing_tlb_shootdown_breaks_cow() {
  TestMachine machine;
  AddressSpace parent(TEST_PARENT_PCID);
  parent.add_vma(anonymous_vma(TEST_COW_PAGE, true));

  machine.cpu.store8(parent, TEST_COW_VA, TEST_ORIGINAL_BYTE);
  AddressSpace child(TEST_CHILD_PCID);
  machine.kernel.fork_private(parent, child, false);

  machine.cpu.store8(parent, TEST_COW_VA, TEST_PARENT_BYTE);
  require(machine.cpu.load8(child, TEST_COW_VA) == TEST_PARENT_BYTE,
          "without shootdown, stale writable TLB should corrupt the shared page");
  require(machine.kernel.stats().cow_copies == 0U,
          "stale TLB write should bypass the COW handler entirely");
}

void test_file_fault_and_bad_user_pointer() {
  TestMachine machine;
  AddressSpace process(TEST_FILE_PCID);
  process.add_vma(file_vma(TEST_FILE_PAGE));

  require(machine.cpu.load8(process, TEST_FILE_VA) == TEST_FILE_BYTE,
          "file-backed fault did not load page-cache content");
  require(machine.kernel.stats().major_faults == 1U,
          "first file-backed page should model a major fault");
  require(machine.page_cache.size() == 1U,
          "file fault should populate exactly one page-cache entry");

  Byte copied = TEST_ZERO_BYTE;
  require(!machine.cpu.copy_from_user(process, TEST_BAD_USER_VA, copied),
          "bad user pointer should be reported to the syscall layer");
  require(machine.kernel.stats().bad_user_pointers == 1U,
          "bad user pointer statistic was not updated");
}

void test_non_canonical_address_is_not_a_vma_miss() {
  TestMachine machine;
  AddressSpace process(TEST_PARENT_PCID);
  process.add_vma(anonymous_vma(TEST_ANON_PAGE, true));

  require(!is_canonical(TEST_NON_CANONICAL_VA),
          "test address must be non-canonical");
  require_access_failure(AccessFailureKind::SegmentationViolation, [&machine,
                                                                    &process] {
    static_cast<void>(machine.cpu.load8(process, TEST_NON_CANONICAL_VA));
  });
}

int main() {
  test_anonymous_fault_allocates_and_retries();
  test_vma_permission_rejects_write();
  test_cow_split_preserves_parent_child_isolation();
  test_missing_tlb_shootdown_breaks_cow();
  test_file_fault_and_bad_user_pointer();
  test_non_canonical_address_is_not_a_vma_miss();
  std::cout << "x86-64 page fault and COW model checks passed\n";
}

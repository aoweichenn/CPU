#include <cstdint>
#include <cstddef>
#include <deque>
#include <functional>
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
using PhysicalAddress = std::uint64_t;
using PageNumber = std::uint64_t;
using Pcid = std::uint16_t;
using InterruptVector = std::uint8_t;
using Iova = std::uint64_t;

constexpr std::uint64_t X86_VIRTUAL_ADDRESS_BITS = 48U;
constexpr std::uint64_t X86_PAGE_SHIFT = 12U;
constexpr std::uint64_t X86_PAGE_TABLE_INDEX_BITS = 9U;
constexpr std::uint64_t X86_PML4_SHIFT = 39U;
constexpr std::uint64_t X86_PDPT_SHIFT = 30U;
constexpr std::uint64_t X86_PD_SHIFT = 21U;
constexpr std::uint64_t X86_PT_SHIFT = 12U;
constexpr std::uint64_t X86_PAGE_SIZE = 1ULL << X86_PAGE_SHIFT;
constexpr std::uint64_t X86_PAGE_OFFSET_MASK = X86_PAGE_SIZE - 1ULL;
constexpr std::uint64_t X86_PAGE_TABLE_INDEX_MASK =
    (1ULL << X86_PAGE_TABLE_INDEX_BITS) - 1ULL;
constexpr VirtualAddress X86_CANONICAL_LOW_MASK =
    (1ULL << X86_VIRTUAL_ADDRESS_BITS) - 1ULL;
constexpr VirtualAddress X86_CANONICAL_HIGH_MASK = ~X86_CANONICAL_LOW_MASK;
constexpr VirtualAddress X86_CANONICAL_SIGN_BIT =
    1ULL << (X86_VIRTUAL_ADDRESS_BITS - 1U);
constexpr Word X86_RFLAGS_INTERRUPT_ENABLE = 1ULL << 9U;
constexpr Word X86_PAGE_FAULT_WRITE_BIT = 1ULL << 1U;
constexpr Word X86_PAGE_FAULT_USER_BIT = 1ULL << 2U;
constexpr Word X86_PAGE_FAULT_INSTRUCTION_BIT = 1ULL << 4U;
constexpr std::size_t HASH_COMBINE_SHIFT = 1U;

constexpr InterruptVector X86_VECTOR_PAGE_FAULT = 14U;
constexpr InterruptVector X86_VECTOR_SYSCALL = 0x80U;
constexpr InterruptVector X86_VECTOR_TIMER = 0x20U;
constexpr InterruptVector X86_VECTOR_DEVICE = 0x31U;

constexpr Pcid TEST_USER_PCID = 7U;
constexpr VirtualAddress TEST_USER_RIP = 0x0000'0000'0040'1000ULL;
constexpr VirtualAddress TEST_USER_RSP = 0x0000'0000'0070'0000ULL;
constexpr VirtualAddress TEST_USER_VA = 0x0000'0000'0040'2008ULL;
constexpr VirtualAddress TEST_USER_PAGE = 0x0000'0000'0040'2000ULL;
constexpr VirtualAddress TEST_READ_ONLY_VA = 0x0000'0000'0040'3000ULL;
constexpr VirtualAddress TEST_NX_VA = 0x0000'0000'0040'4000ULL;
constexpr VirtualAddress TEST_KERNEL_VA = 0xFFFF'8000'0000'1000ULL;
constexpr VirtualAddress TEST_MMIO_VA = 0xFFFF'8000'000F'0000ULL;
constexpr VirtualAddress TEST_NON_CANONICAL_VA = 0x0000'8000'0000'0000ULL;
constexpr VirtualAddress X86_SYSCALL_HANDLER = 0xFFFF'8000'0000'8000ULL;
constexpr VirtualAddress X86_TIMER_HANDLER = 0xFFFF'8000'0000'9000ULL;
constexpr VirtualAddress X86_DEVICE_HANDLER = 0xFFFF'8000'0000'A000ULL;
constexpr PhysicalAddress TEST_USER_PA = 0x0000'0000'0010'0000ULL;
constexpr PhysicalAddress TEST_READ_ONLY_PA = 0x0000'0000'0011'0000ULL;
constexpr PhysicalAddress TEST_NX_PA = 0x0000'0000'0012'0000ULL;
constexpr PhysicalAddress TEST_KERNEL_PA = 0x0000'0000'0020'0000ULL;
constexpr PhysicalAddress X86_MMIO_BASE = 0x0000'0000'00F0'0000ULL;
constexpr PhysicalAddress X86_MMIO_DOORBELL = X86_MMIO_BASE;
constexpr PhysicalAddress X86_MMIO_END = X86_MMIO_BASE + X86_PAGE_SIZE;
constexpr Byte TEST_USER_BYTE = 0x5AU;
constexpr Byte TEST_DEVICE_BYTE = 0xC3U;
constexpr std::size_t TEST_DMA_LENGTH = 64U;
constexpr std::size_t TEST_DMA_OFFSET = 8U;

enum class PrivilegeLevel : std::uint8_t {
  Ring0 = 0U,
  Ring3 = 3U,
};

enum class AccessKind {
  Read,
  Write,
  Execute,
};

enum class FaultKind {
  NonCanonical,
  PageFault,
  Protection,
  MissingInterruptHandler,
  DmaFault,
};

enum class DmaDirection {
  ToDevice,
  FromDevice,
  Bidirectional,
};

struct Fault : public std::runtime_error {
  Fault(FaultKind fault_kind, std::string_view message)
      : std::runtime_error(std::string(message)), kind(fault_kind) {}

  FaultKind kind;
};

struct PageTableIndexes {
  std::uint16_t pml4 = 0;
  std::uint16_t pdpt = 0;
  std::uint16_t pd = 0;
  std::uint16_t pt = 0;
  std::uint16_t offset = 0;
};

struct PageAttributes {
  bool writable = false;
  bool user = false;
  bool executable = false;
};

struct PageTableEntry {
  PhysicalAddress page_base = 0;
  bool present = false;
  bool writable = false;
  bool user = false;
  bool executable = false;
};

struct Translation {
  PhysicalAddress physical = 0;
  PageTableEntry entry;
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

struct TrapFrame {
  VirtualAddress rip = 0;
  VirtualAddress rsp = 0;
  Word rflags = 0;
  PrivilegeLevel cpl = PrivilegeLevel::Ring3;
  InterruptVector vector = 0;
  Word error_code = 0;
  std::optional<VirtualAddress> fault_address;
};

struct DmaMapping {
  PhysicalAddress physical_base = 0;
  std::size_t length = 0;
  DmaDirection direction = DmaDirection::Bidirectional;
};

bool is_canonical(VirtualAddress address) {
  const bool sign_bit_set = (address & X86_CANONICAL_SIGN_BIT) != 0;
  const VirtualAddress high_bits = address & X86_CANONICAL_HIGH_MASK;
  if (sign_bit_set) {
    return high_bits == X86_CANONICAL_HIGH_MASK;
  }
  return high_bits == 0;
}

PageNumber virtual_page_number(VirtualAddress address) {
  return address >> X86_PAGE_SHIFT;
}

PhysicalAddress page_align(PhysicalAddress address) {
  return address & ~X86_PAGE_OFFSET_MASK;
}

PageTableIndexes split_virtual_address(VirtualAddress address) {
  if (!is_canonical(address)) {
    throw Fault(FaultKind::NonCanonical, "virtual address is not canonical");
  }
  return PageTableIndexes{
      .pml4 = static_cast<std::uint16_t>(
          (address >> X86_PML4_SHIFT) & X86_PAGE_TABLE_INDEX_MASK),
      .pdpt = static_cast<std::uint16_t>(
          (address >> X86_PDPT_SHIFT) & X86_PAGE_TABLE_INDEX_MASK),
      .pd = static_cast<std::uint16_t>(
          (address >> X86_PD_SHIFT) & X86_PAGE_TABLE_INDEX_MASK),
      .pt = static_cast<std::uint16_t>(
          (address >> X86_PT_SHIFT) & X86_PAGE_TABLE_INDEX_MASK),
      .offset = static_cast<std::uint16_t>(address & X86_PAGE_OFFSET_MASK),
  };
}

Word page_fault_error_code(AccessKind access, PrivilegeLevel cpl) {
  Word code = 0;
  if (access == AccessKind::Write) {
    code |= X86_PAGE_FAULT_WRITE_BIT;
  }
  if (access == AccessKind::Execute) {
    code |= X86_PAGE_FAULT_INSTRUCTION_BIT;
  }
  if (cpl == PrivilegeLevel::Ring3) {
    code |= X86_PAGE_FAULT_USER_BIT;
  }
  return code;
}

void require(bool condition, std::string_view message) {
  if (!condition) {
    throw std::runtime_error(std::string(message));
  }
}

template <typename Callable>
void require_fault(FaultKind expected, Callable action) {
  try {
    action();
  } catch (const Fault& fault) {
    require(fault.kind == expected, "unexpected fault kind");
    return;
  }
  throw std::runtime_error("expected fault was not raised");
}

class PhysicalMemory {
 public:
  void store8(PhysicalAddress address, Byte value) {
    this->bytes_[address] = value;
  }

  Byte load8(PhysicalAddress address) const {
    const auto iterator = this->bytes_.find(address);
    if (iterator == this->bytes_.end()) {
      return 0;
    }
    return iterator->second;
  }

 private:
  std::unordered_map<PhysicalAddress, Byte> bytes_;
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

  void invalidate_page(Pcid pcid, PageNumber vpn) {
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

  void map_page(VirtualAddress virtual_page, PhysicalAddress physical_page,
                PageAttributes attributes) {
    require(is_canonical(virtual_page), "mapped virtual address must be canonical");
    require((virtual_page & X86_PAGE_OFFSET_MASK) == 0,
            "virtual page must be page aligned");
    require((physical_page & X86_PAGE_OFFSET_MASK) == 0,
            "physical page must be page aligned");
    this->entries_[virtual_page_number(virtual_page)] = PageTableEntry{
        .page_base = physical_page,
        .present = true,
        .writable = attributes.writable,
        .user = attributes.user,
        .executable = attributes.executable,
    };
  }

  void set_writable(VirtualAddress virtual_page, bool writable) {
    PageTableEntry& entry = this->entry_for_mutation(virtual_page);
    entry.writable = writable;
  }

  const PageTableEntry& entry_for(PageNumber vpn) const {
    const auto iterator = this->entries_.find(vpn);
    if (iterator == this->entries_.end() || !iterator->second.present) {
      throw Fault(FaultKind::PageFault, "page walk found no present PTE");
    }
    return iterator->second;
  }

 private:
  PageTableEntry& entry_for_mutation(VirtualAddress virtual_page) {
    require(is_canonical(virtual_page), "updated virtual address must be canonical");
    const auto iterator = this->entries_.find(virtual_page_number(virtual_page));
    require(iterator != this->entries_.end(), "updated PTE must exist");
    return iterator->second;
  }

  Pcid pcid_ = 0;
  std::map<PageNumber, PageTableEntry> entries_;
};

class Mmu {
 public:
  Translation translate(const AddressSpace& address_space, Tlb& tlb,
                        VirtualAddress virtual_address, AccessKind access,
                        PrivilegeLevel cpl) const {
    const PageTableIndexes indexes = split_virtual_address(virtual_address);
    const PageNumber vpn = virtual_page_number(virtual_address);
    const std::optional<PageTableEntry> cached = tlb.lookup(address_space.pcid(), vpn);
    if (cached.has_value()) {
      this->check_access(cached.value(), access, cpl);
      return Translation{
          .physical = cached->page_base + indexes.offset,
          .entry = cached.value(),
      };
    }

    const PageTableEntry& walked = address_space.entry_for(vpn);
    this->check_access(walked, access, cpl);
    tlb.insert(address_space.pcid(), vpn, walked);
    return Translation{
        .physical = walked.page_base + indexes.offset,
        .entry = walked,
    };
  }

 private:
  void check_access(const PageTableEntry& entry, AccessKind access,
                    PrivilegeLevel cpl) const {
    if (!entry.present) {
      throw Fault(FaultKind::PageFault, "PTE is not present");
    }
    if (cpl == PrivilegeLevel::Ring3 && !entry.user) {
      throw Fault(FaultKind::PageFault, "ring 3 tried to access supervisor page");
    }
    if (access == AccessKind::Write && !entry.writable) {
      throw Fault(FaultKind::PageFault, "write access denied by PTE");
    }
    if (access == AccessKind::Execute && !entry.executable) {
      throw Fault(FaultKind::PageFault, "execute access denied by PTE");
    }
  }
};

class LocalApic {
 public:
  void raise(InterruptVector vector) {
    this->pending_.push_back(vector);
  }

  std::optional<InterruptVector> take_next() {
    if (this->pending_.empty()) {
      return std::nullopt;
    }
    const InterruptVector vector = this->pending_.front();
    this->pending_.pop_front();
    return vector;
  }

  std::size_t pending_count() const {
    return this->pending_.size();
  }

 private:
  std::deque<InterruptVector> pending_;
};

class InterruptDescriptorTable {
 public:
  void set_handler(InterruptVector vector, VirtualAddress handler) {
    require(is_canonical(handler), "interrupt handler must be canonical");
    this->handlers_[vector] = handler;
  }

  VirtualAddress handler_for(InterruptVector vector) const {
    const auto iterator = this->handlers_.find(vector);
    if (iterator == this->handlers_.end()) {
      throw Fault(FaultKind::MissingInterruptHandler, "missing IDT handler");
    }
    return iterator->second;
  }

 private:
  std::map<InterruptVector, VirtualAddress> handlers_;
};

class MmioDevice {
 public:
  explicit MmioDevice(LocalApic& apic) : apic_(apic) {}

  void store8(PhysicalAddress address, Byte value) {
    require(address == X86_MMIO_DOORBELL, "only doorbell MMIO is modeled");
    this->last_value_ = value;
    this->apic_.raise(X86_VECTOR_DEVICE);
  }

  Byte last_value() const {
    return this->last_value_;
  }

 private:
  LocalApic& apic_;
  Byte last_value_ = 0;
};

class Bus {
 public:
  Bus(PhysicalMemory& memory, MmioDevice& device)
      : memory_(memory), device_(device) {}

  void store8(PhysicalAddress physical, Byte value, PrivilegeLevel cpl) {
    if (this->is_mmio(physical)) {
      this->require_ring0(cpl);
      this->device_.store8(physical, value);
      return;
    }
    this->memory_.store8(physical, value);
  }

  Byte load8(PhysicalAddress physical, PrivilegeLevel cpl) const {
    if (this->is_mmio(physical)) {
      this->require_ring0(cpl);
      return 0;
    }
    return this->memory_.load8(physical);
  }

 private:
  bool is_mmio(PhysicalAddress physical) const {
    return physical >= X86_MMIO_BASE && physical < X86_MMIO_END;
  }

  void require_ring0(PrivilegeLevel cpl) const {
    if (cpl != PrivilegeLevel::Ring0) {
      throw Fault(FaultKind::Protection, "ring 3 cannot access MMIO");
    }
  }

  PhysicalMemory& memory_;
  MmioDevice& device_;
};

class IommuDomain {
 public:
  Iova map(PhysicalAddress physical_base, std::size_t length,
           DmaDirection direction) {
    require((physical_base & X86_PAGE_OFFSET_MASK) == 0,
            "DMA mapping must start on a page boundary");
    require(length > 0 && length <= X86_PAGE_SIZE,
            "DMA mapping length must fit one modeled page");
    const Iova iova = this->next_iova_;
    this->next_iova_ += X86_PAGE_SIZE;
    this->mappings_[iova] = DmaMapping{
        .physical_base = physical_base,
        .length = length,
        .direction = direction,
    };
    return iova;
  }

  void unmap(Iova iova) {
    const std::size_t erased = this->mappings_.erase(iova);
    require(erased == 1, "DMA unmap must remove an existing mapping");
  }

  PhysicalAddress translate_for_write(Iova iova, std::size_t offset) const {
    const DmaMapping& mapping = this->mapping_for(iova);
    require(this->allows_device_write(mapping.direction),
            "IOMMU rejected device write direction");
    require(offset < mapping.length, "DMA write offset outside mapping");
    return mapping.physical_base + offset;
  }

 private:
  const DmaMapping& mapping_for(Iova iova) const {
    const auto iterator = this->mappings_.find(iova);
    if (iterator == this->mappings_.end()) {
      throw Fault(FaultKind::DmaFault, "IOMMU has no mapping for IOVA");
    }
    return iterator->second;
  }

  bool allows_device_write(DmaDirection direction) const {
    return direction == DmaDirection::FromDevice ||
           direction == DmaDirection::Bidirectional;
  }

  Iova next_iova_ = X86_PAGE_SIZE;
  std::map<Iova, DmaMapping> mappings_;
};

class DmaEngine {
 public:
  DmaEngine(IommuDomain& domain, PhysicalMemory& memory)
      : domain_(domain), memory_(memory) {}

  void device_write(Iova iova, std::size_t offset, Byte value) {
    const PhysicalAddress physical = this->domain_.translate_for_write(iova, offset);
    this->memory_.store8(physical, value);
  }

 private:
  IommuDomain& domain_;
  PhysicalMemory& memory_;
};

class Cpu {
 public:
  Cpu(AddressSpace& address_space, Tlb& tlb, const Mmu& mmu, Bus& bus,
      LocalApic& apic, const InterruptDescriptorTable& idt)
      : address_space_(address_space),
        tlb_(tlb),
        mmu_(mmu),
        bus_(bus),
        apic_(apic),
        idt_(idt) {}

  void enter_user(VirtualAddress rip, VirtualAddress rsp) {
    require(is_canonical(rip), "user RIP must be canonical");
    require(is_canonical(rsp), "user RSP must be canonical");
    this->cpl_ = PrivilegeLevel::Ring3;
    this->rip_ = rip;
    this->rsp_ = rsp;
    this->rflags_ = X86_RFLAGS_INTERRUPT_ENABLE;
  }

  void enter_kernel(VirtualAddress rip) {
    require(is_canonical(rip), "kernel RIP must be canonical");
    this->cpl_ = PrivilegeLevel::Ring0;
    this->rip_ = rip;
  }

  Byte load8(VirtualAddress virtual_address) {
    const Translation translation =
        this->translate_or_trap(virtual_address, AccessKind::Read);
    return this->bus_.load8(translation.physical, this->cpl_);
  }

  void store8(VirtualAddress virtual_address, Byte value) {
    const Translation translation =
        this->translate_or_trap(virtual_address, AccessKind::Write);
    this->bus_.store8(translation.physical, value, this->cpl_);
  }

  void execute_at(VirtualAddress virtual_address) {
    const Translation translation =
        this->translate_or_trap(virtual_address, AccessKind::Execute);
    this->rip_ = virtual_address;
    this->last_fetch_physical_ = translation.physical;
  }

  void syscall() {
    require(this->cpl_ == PrivilegeLevel::Ring3,
            "syscall entry starts in ring 3 for this model");
    this->push_trap_frame(X86_VECTOR_SYSCALL, 0, std::nullopt);
    this->cpl_ = PrivilegeLevel::Ring0;
    this->rip_ = X86_SYSCALL_HANDLER;
    this->rflags_ &= ~X86_RFLAGS_INTERRUPT_ENABLE;
  }

  void handle_one_interrupt() {
    const std::optional<InterruptVector> vector = this->apic_.take_next();
    if (!vector.has_value()) {
      return;
    }
    this->push_trap_frame(*vector, 0, std::nullopt);
    this->cpl_ = PrivilegeLevel::Ring0;
    this->rip_ = this->idt_.handler_for(*vector);
    this->rflags_ &= ~X86_RFLAGS_INTERRUPT_ENABLE;
  }

  void iretq() {
    require(!this->trap_frames_.empty(), "iretq requires a saved frame");
    const TrapFrame frame = this->trap_frames_.back();
    this->trap_frames_.pop_back();
    this->rip_ = frame.rip;
    this->rsp_ = frame.rsp;
    this->rflags_ = frame.rflags;
    this->cpl_ = frame.cpl;
  }

  PrivilegeLevel cpl() const {
    return this->cpl_;
  }

  VirtualAddress rip() const {
    return this->rip_;
  }

  std::size_t trap_count() const {
    return this->trap_frames_.size();
  }

  const TrapFrame& last_trap() const {
    require(!this->trap_frames_.empty(), "trap frame stack must not be empty");
    return this->trap_frames_.back();
  }

  PhysicalAddress last_fetch_physical() const {
    return this->last_fetch_physical_;
  }

 private:
  Translation translate_or_trap(VirtualAddress virtual_address, AccessKind access) {
    try {
      return this->mmu_.translate(
          this->address_space_, this->tlb_, virtual_address, access, this->cpl_);
    } catch (const Fault& fault) {
      if (fault.kind == FaultKind::PageFault || fault.kind == FaultKind::NonCanonical) {
        this->push_trap_frame(
            X86_VECTOR_PAGE_FAULT,
            page_fault_error_code(access, this->cpl_),
            virtual_address);
      }
      throw;
    }
  }

  void push_trap_frame(InterruptVector vector, Word error_code,
                       std::optional<VirtualAddress> fault_address) {
    this->trap_frames_.push_back(TrapFrame{
        .rip = this->rip_,
        .rsp = this->rsp_,
        .rflags = this->rflags_,
        .cpl = this->cpl_,
        .vector = vector,
        .error_code = error_code,
        .fault_address = fault_address,
    });
  }

  AddressSpace& address_space_;
  Tlb& tlb_;
  const Mmu& mmu_;
  Bus& bus_;
  LocalApic& apic_;
  const InterruptDescriptorTable& idt_;
  PrivilegeLevel cpl_ = PrivilegeLevel::Ring3;
  VirtualAddress rip_ = TEST_USER_RIP;
  VirtualAddress rsp_ = TEST_USER_RSP;
  Word rflags_ = X86_RFLAGS_INTERRUPT_ENABLE;
  PhysicalAddress last_fetch_physical_ = 0;
  std::vector<TrapFrame> trap_frames_;
};

class Machine {
 public:
  Machine()
      : address_space_(TEST_USER_PCID),
        device_(apic_),
        bus_(memory_, device_),
        dma_(iommu_, memory_),
        cpu_(address_space_, tlb_, mmu_, bus_, apic_, idt_) {
    this->install_idt();
    this->install_mappings();
    this->cpu_.enter_user(TEST_USER_RIP, TEST_USER_RSP);
  }

  AddressSpace& address_space() {
    return this->address_space_;
  }

  Tlb& tlb() {
    return this->tlb_;
  }

  PhysicalMemory& memory() {
    return this->memory_;
  }

  IommuDomain& iommu() {
    return this->iommu_;
  }

  DmaEngine& dma() {
    return this->dma_;
  }

  Cpu& cpu() {
    return this->cpu_;
  }

  MmioDevice& device() {
    return this->device_;
  }

  LocalApic& apic() {
    return this->apic_;
  }

 private:
  void install_idt() {
    this->idt_.set_handler(X86_VECTOR_TIMER, X86_TIMER_HANDLER);
    this->idt_.set_handler(X86_VECTOR_DEVICE, X86_DEVICE_HANDLER);
  }

  void install_mappings() {
    this->address_space_.map_page(
        TEST_USER_PAGE,
        page_align(TEST_USER_PA),
        PageAttributes{.writable = true, .user = true, .executable = true});
    this->address_space_.map_page(
        TEST_READ_ONLY_VA,
        page_align(TEST_READ_ONLY_PA),
        PageAttributes{.writable = false, .user = true, .executable = true});
    this->address_space_.map_page(
        TEST_NX_VA,
        page_align(TEST_NX_PA),
        PageAttributes{.writable = true, .user = true, .executable = false});
    this->address_space_.map_page(
        TEST_KERNEL_VA,
        page_align(TEST_KERNEL_PA),
        PageAttributes{.writable = true, .user = false, .executable = true});
    this->address_space_.map_page(
        TEST_MMIO_VA,
        X86_MMIO_BASE,
        PageAttributes{.writable = true, .user = false, .executable = false});
  }

  PhysicalMemory memory_;
  AddressSpace address_space_;
  Tlb tlb_;
  Mmu mmu_;
  LocalApic apic_;
  InterruptDescriptorTable idt_;
  MmioDevice device_;
  Bus bus_;
  IommuDomain iommu_;
  DmaEngine dma_;
  Cpu cpu_;
};

void test_canonical_address_and_indexes() {
  require(is_canonical(TEST_USER_VA), "low canonical user address rejected");
  require(is_canonical(TEST_KERNEL_VA), "high canonical kernel address rejected");
  require(!is_canonical(TEST_NON_CANONICAL_VA),
          "non-canonical address was accepted");
  const PageTableIndexes indexes = split_virtual_address(TEST_USER_VA);
  require(indexes.pml4 == 0, "unexpected user PML4 index");
  require(indexes.offset == (TEST_USER_VA & X86_PAGE_OFFSET_MASK),
          "page offset was not preserved");
  require_fault(FaultKind::NonCanonical, [] {
    static_cast<void>(split_virtual_address(TEST_NON_CANONICAL_VA));
  });
}

void test_page_permissions_and_fault_frame() {
  Machine machine;
  machine.cpu().store8(TEST_USER_VA, TEST_USER_BYTE);
  require(machine.cpu().load8(TEST_USER_VA) == TEST_USER_BYTE,
          "user page read/write failed");

  require_fault(FaultKind::PageFault, [&machine] {
    machine.cpu().load8(TEST_KERNEL_VA);
  });
  require(machine.cpu().last_trap().vector == X86_VECTOR_PAGE_FAULT,
          "page fault did not save page-fault vector");
  require(machine.cpu().last_trap().fault_address == TEST_KERNEL_VA,
          "page fault did not save faulting virtual address");
  require((machine.cpu().last_trap().error_code & X86_PAGE_FAULT_USER_BIT) != 0,
          "ring 3 page fault did not set user bit");

  require_fault(FaultKind::PageFault, [&machine] {
    machine.cpu().store8(TEST_READ_ONLY_VA, TEST_USER_BYTE);
  });
  require((machine.cpu().last_trap().error_code & X86_PAGE_FAULT_WRITE_BIT) != 0,
          "write fault did not set write bit");

  require_fault(FaultKind::PageFault, [&machine] {
    machine.cpu().execute_at(TEST_NX_VA);
  });
  require((machine.cpu().last_trap().error_code &
           X86_PAGE_FAULT_INSTRUCTION_BIT) != 0,
          "execute fault did not set instruction bit");
}

void test_tlb_invalidation_requirement() {
  Machine machine;
  machine.cpu().store8(TEST_USER_VA, TEST_USER_BYTE);
  require(machine.tlb().size() == 1U, "first store should populate TLB");

  machine.address_space().set_writable(TEST_USER_PAGE, false);
  machine.cpu().store8(TEST_USER_VA, TEST_DEVICE_BYTE);
  require(machine.cpu().load8(TEST_USER_VA) == TEST_DEVICE_BYTE,
          "stale TLB did not preserve old writable permission");

  machine.tlb().invalidate_page(TEST_USER_PCID, virtual_page_number(TEST_USER_VA));
  require_fault(FaultKind::PageFault, [&machine] {
    machine.cpu().store8(TEST_USER_VA, TEST_USER_BYTE);
  });
}

void test_syscall_and_interrupt_entry() {
  Machine machine;
  machine.cpu().syscall();
  require(machine.cpu().cpl() == PrivilegeLevel::Ring0,
          "syscall did not enter ring 0");
  require(machine.cpu().rip() == X86_SYSCALL_HANDLER,
          "syscall did not load IA32_LSTAR-style handler");
  require(machine.cpu().last_trap().vector == X86_VECTOR_SYSCALL,
          "syscall did not save syscall frame");
  machine.cpu().iretq();
  require(machine.cpu().cpl() == PrivilegeLevel::Ring3,
          "iretq did not restore ring 3 after syscall");

  machine.cpu().enter_kernel(X86_SYSCALL_HANDLER);
  machine.cpu().store8(TEST_MMIO_VA, TEST_DEVICE_BYTE);
  require(machine.device().last_value() == TEST_DEVICE_BYTE,
          "MMIO doorbell did not reach device");
  require(machine.apic().pending_count() == 1U,
          "device did not raise MSI-X style interrupt");
  machine.cpu().handle_one_interrupt();
  require(machine.cpu().rip() == X86_DEVICE_HANDLER,
          "device interrupt did not dispatch through IDT");
}

void test_mmio_privilege_and_dma_lifetime() {
  Machine machine;
  require_fault(FaultKind::PageFault, [&machine] {
    machine.cpu().store8(TEST_MMIO_VA, TEST_DEVICE_BYTE);
  });

  const Iova iova = machine.iommu().map(
      page_align(TEST_USER_PA), TEST_DMA_LENGTH, DmaDirection::FromDevice);
  machine.dma().device_write(iova, TEST_DMA_OFFSET, TEST_DEVICE_BYTE);
  require(machine.memory().load8(page_align(TEST_USER_PA) + TEST_DMA_OFFSET) ==
              TEST_DEVICE_BYTE,
          "DMA write did not reach mapped physical page");
  machine.iommu().unmap(iova);
  require_fault(FaultKind::DmaFault, [&machine, iova] {
    machine.dma().device_write(iova, TEST_DMA_OFFSET, TEST_USER_BYTE);
  });
}

int main() {
  test_canonical_address_and_indexes();
  test_page_permissions_and_fault_frame();
  test_tlb_invalidation_requirement();
  test_syscall_and_interrupt_entry();
  test_mmio_privilege_and_dma_lifetime();
  std::cout << "x86-64 stage one contract model checks passed\n";
}

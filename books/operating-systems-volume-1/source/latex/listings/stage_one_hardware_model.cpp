#include <array>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using Address = std::uint16_t;
using Byte = std::uint8_t;
using Word = std::uint16_t;
using DmaHandle = std::uint32_t;

constexpr std::size_t HARDWARE_MEMORY_SIZE = 64U * 1024U;
constexpr std::size_t HARDWARE_PAGE_SIZE = 256U;
constexpr std::size_t HARDWARE_PAGE_COUNT =
    HARDWARE_MEMORY_SIZE / HARDWARE_PAGE_SIZE;
constexpr Address HARDWARE_KERNEL_START = 0x0000U;
constexpr Address HARDWARE_KERNEL_END = 0x0FFFU;
constexpr Address HARDWARE_USER_START = 0x1000U;
constexpr Address HARDWARE_USER_END = 0xDFFFU;
constexpr Address HARDWARE_MMIO_START = 0xF000U;
constexpr Address HARDWARE_MMIO_STATUS = 0xF000U;
constexpr Address HARDWARE_MMIO_DATA = 0xF002U;
constexpr Address HARDWARE_MMIO_DOORBELL = 0xF004U;
constexpr Address HARDWARE_TIMER_HANDLER = 0x0020U;
constexpr Address HARDWARE_DEVICE_HANDLER = 0x0040U;
constexpr Address TEST_USER_BUFFER = 0x2000U;
constexpr Address TEST_KERNEL_ADDRESS = 0x0100U;
constexpr Byte TEST_DEVICE_BYTE = 0x5AU;
constexpr Byte TEST_USER_BYTE = 0x33U;
constexpr std::size_t TEST_DMA_LENGTH = 1U;
constexpr DmaHandle DMA_INVALID_HANDLE = 0U;

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
  None,
  Protection,
  InvalidMmio,
  InvalidDma,
};

enum class InterruptVector {
  Timer,
  Device,
};

struct PagePermission {
  bool user = false;
  bool readable = false;
  bool writable = false;
  bool executable = false;
};

struct TrapFrame {
  Address rip = 0;
  Word rflags = 0;
  PrivilegeLevel previous_cpl = PrivilegeLevel::Ring3;
};

class Fault : public std::runtime_error {
 public:
  Fault(FaultKind kind, std::string_view message)
      : std::runtime_error(std::string(message)), kind_(kind) {}

  [[nodiscard]] FaultKind kind() const {
    return this->kind_;
  }

 private:
  FaultKind kind_ = FaultKind::None;
};

class PhysicalMemory {
 public:
  PhysicalMemory() {
    this->set_range_permission(
        HARDWARE_KERNEL_START,
        HARDWARE_KERNEL_END,
        PagePermission{
            .user = false,
            .readable = true,
            .writable = true,
            .executable = true,
        });
    this->set_range_permission(
        HARDWARE_USER_START,
        HARDWARE_USER_END,
        PagePermission{
            .user = true,
            .readable = true,
            .writable = true,
            .executable = true,
        });
  }

  [[nodiscard]] Byte load8(Address address, PrivilegeLevel cpl) const {
    this->check_access(address, cpl, AccessKind::Read);
    return this->bytes_.at(address);
  }

  void store8(Address address, Byte value, PrivilegeLevel cpl) {
    this->check_access(address, cpl, AccessKind::Write);
    this->bytes_.at(address) = value;
  }

  void dma_store8(Address address, Byte value) {
    this->check_address_in_ram(address);
    this->bytes_.at(address) = value;
  }

  [[nodiscard]] bool can_execute(Address address, PrivilegeLevel cpl) const {
    try {
      this->check_access(address, cpl, AccessKind::Execute);
      return true;
    } catch (const Fault&) {
      return false;
    }
  }

 private:
  void set_range_permission(Address start, Address end, PagePermission permission) {
    const std::size_t first_page = page_index(start);
    const std::size_t last_page = page_index(end);
    for (std::size_t page = first_page; page <= last_page; ++page) {
      this->permissions_.at(page) = permission;
    }
  }

  static std::size_t page_index(Address address) {
    return static_cast<std::size_t>(address) / HARDWARE_PAGE_SIZE;
  }

  void check_address_in_ram(Address address) const {
    if (address >= HARDWARE_MMIO_START) {
      throw Fault(FaultKind::Protection, "address is not normal RAM");
    }
  }

  void check_access(Address address, PrivilegeLevel cpl, AccessKind access) const {
    this->check_address_in_ram(address);
    const PagePermission& permission = this->permissions_.at(page_index(address));
    if (cpl == PrivilegeLevel::Ring3 && !permission.user) {
      throw Fault(FaultKind::Protection, "ring 3 cannot access kernel page");
    }
    const bool allowed =
        (access == AccessKind::Read && permission.readable) ||
        (access == AccessKind::Write && permission.writable) ||
        (access == AccessKind::Execute && permission.executable);
    if (!allowed) {
      throw Fault(FaultKind::Protection, "page permission rejected access");
    }
  }

  std::array<Byte, HARDWARE_MEMORY_SIZE> bytes_{};
  std::array<PagePermission, HARDWARE_PAGE_COUNT> permissions_{};
};

class InterruptController {
 public:
  void raise(InterruptVector vector) {
    this->pending_.push_back(vector);
  }

  [[nodiscard]] std::optional<InterruptVector> take_next() {
    if (this->pending_.empty()) {
      return std::nullopt;
    }
    const InterruptVector vector = this->pending_.front();
    this->pending_.pop_front();
    return vector;
  }

 private:
  std::deque<InterruptVector> pending_;
};

class SimpleDevice {
 public:
  explicit SimpleDevice(InterruptController& interrupts) : interrupts_(interrupts) {}

  [[nodiscard]] Byte read(Address address) const {
    if (address == HARDWARE_MMIO_STATUS) {
      return this->ready_ ? 1U : 0U;
    }
    if (address == HARDWARE_MMIO_DATA) {
      return this->data_;
    }
    throw Fault(FaultKind::InvalidMmio, "unknown MMIO read");
  }

  void write(Address address, Byte value) {
    if (address != HARDWARE_MMIO_DOORBELL) {
      throw Fault(FaultKind::InvalidMmio, "unknown MMIO write");
    }
    this->ready_ = true;
    this->data_ = value;
    this->interrupts_.raise(InterruptVector::Device);
  }

 private:
  InterruptController& interrupts_;
  bool ready_ = false;
  Byte data_ = 0;
};

class Bus {
 public:
  Bus(PhysicalMemory& memory, SimpleDevice& device)
      : memory_(memory), device_(device) {}

  [[nodiscard]] Byte load8(Address address, PrivilegeLevel cpl) const {
    if (address >= HARDWARE_MMIO_START) {
      this->require_ring0(cpl);
      return this->device_.read(address);
    }
    return this->memory_.load8(address, cpl);
  }

  void store8(Address address, Byte value, PrivilegeLevel cpl) {
    if (address >= HARDWARE_MMIO_START) {
      this->require_ring0(cpl);
      this->device_.write(address, value);
      return;
    }
    this->memory_.store8(address, value, cpl);
  }

 private:
  static void require_ring0(PrivilegeLevel cpl) {
    if (cpl != PrivilegeLevel::Ring0) {
      throw Fault(FaultKind::Protection, "ring 3 cannot access MMIO");
    }
  }

  PhysicalMemory& memory_;
  SimpleDevice& device_;
};

struct DmaMapping {
  DmaHandle handle = DMA_INVALID_HANDLE;
  Address start = 0;
  std::size_t length = 0;
  bool active = false;
};

class DmaEngine {
 public:
  explicit DmaEngine(PhysicalMemory& memory) : memory_(memory) {}

  [[nodiscard]] DmaHandle map(Address start, std::size_t length) {
    if (length == 0 || start >= HARDWARE_MMIO_START) {
      throw Fault(FaultKind::InvalidDma, "invalid DMA mapping");
    }
    const std::size_t end =
        static_cast<std::size_t>(start) + length;
    if (end > HARDWARE_MMIO_START) {
      throw Fault(FaultKind::InvalidDma, "invalid DMA mapping");
    }
    const DmaHandle handle = this->next_handle_++;
    this->mappings_.push_back(DmaMapping{
        .handle = handle,
        .start = start,
        .length = length,
        .active = true,
    });
    return handle;
  }

  void device_write(DmaHandle handle, std::size_t offset, Byte value) {
    DmaMapping& mapping = this->find_active_mapping(handle);
    if (offset >= mapping.length) {
      throw Fault(FaultKind::InvalidDma, "DMA write outside mapped buffer");
    }
    const Address target =
        static_cast<Address>(mapping.start + static_cast<Address>(offset));
    this->memory_.dma_store8(target, value);
  }

  void unmap(DmaHandle handle) {
    DmaMapping& mapping = this->find_active_mapping(handle);
    mapping.active = false;
  }

 private:
  DmaMapping& find_active_mapping(DmaHandle handle) {
    for (auto& mapping : this->mappings_) {
      if (mapping.handle == handle && mapping.active) {
        return mapping;
      }
    }
    throw Fault(FaultKind::InvalidDma, "DMA handle is not active");
  }

  PhysicalMemory& memory_;
  DmaHandle next_handle_ = 1U;
  std::vector<DmaMapping> mappings_;
};

class Cpu {
 public:
  Cpu(Bus& bus, InterruptController& interrupts)
      : bus_(bus), interrupts_(interrupts) {}

  void user_store(Address address, Byte value) {
    this->bus_.store8(address, value, PrivilegeLevel::Ring3);
  }

  [[nodiscard]] Byte user_load(Address address) const {
    return this->bus_.load8(address, PrivilegeLevel::Ring3);
  }

  void kernel_store(Address address, Byte value) {
    this->bus_.store8(address, value, PrivilegeLevel::Ring0);
  }

  void handle_one_interrupt() {
    const std::optional<InterruptVector> vector = this->interrupts_.take_next();
    if (!vector.has_value()) {
      return;
    }
    this->trap_frames_.push_back(TrapFrame{
        .rip = this->rip_,
        .rflags = this->rflags_,
        .previous_cpl = this->cpl_,
    });
    this->cpl_ = PrivilegeLevel::Ring0;
    this->rip_ = this->handler_address(*vector);
  }

  [[nodiscard]] PrivilegeLevel cpl() const {
    return this->cpl_;
  }

  [[nodiscard]] Address rip() const {
    return this->rip_;
  }

  [[nodiscard]] std::size_t trap_count() const {
    return this->trap_frames_.size();
  }

 private:
  static Address handler_address(InterruptVector vector) {
    switch (vector) {
      case InterruptVector::Timer:
        return HARDWARE_TIMER_HANDLER;
      case InterruptVector::Device:
        return HARDWARE_DEVICE_HANDLER;
    }
    throw std::logic_error("unknown interrupt vector");
  }

  Bus& bus_;
  InterruptController& interrupts_;
  PrivilegeLevel cpl_ = PrivilegeLevel::Ring3;
  Address rip_ = HARDWARE_USER_START;
  Word rflags_ = 0;
  std::vector<TrapFrame> trap_frames_;
};

void require(bool condition, std::string_view message) {
  if (!condition) {
    throw std::runtime_error(std::string(message));
  }
}

template <typename Fn>
void require_fault(FaultKind expected, Fn&& fn) {
  try {
    fn();
  } catch (const Fault& fault) {
    require(fault.kind() == expected, "unexpected fault kind");
    return;
  }
  throw std::runtime_error("expected a fault");
}

int main() {
  PhysicalMemory memory;
  InterruptController interrupts;
  SimpleDevice device(interrupts);
  Bus bus(memory, device);
  DmaEngine dma(memory);
  Cpu cpu(bus, interrupts);

  cpu.user_store(TEST_USER_BUFFER, TEST_USER_BYTE);
  require(cpu.user_load(TEST_USER_BUFFER) == TEST_USER_BYTE, "user RAM failed");
  require_fault(FaultKind::Protection, [&] {
    cpu.user_store(TEST_KERNEL_ADDRESS, TEST_USER_BYTE);
  });
  require_fault(FaultKind::Protection, [&] {
    cpu.user_store(HARDWARE_MMIO_DOORBELL, TEST_USER_BYTE);
  });

  cpu.kernel_store(HARDWARE_MMIO_DOORBELL, TEST_DEVICE_BYTE);
  cpu.handle_one_interrupt();
  require(cpu.cpl() == PrivilegeLevel::Ring0, "interrupt did not enter ring 0");
  require(cpu.rip() == HARDWARE_DEVICE_HANDLER, "device interrupt vector mismatch");
  require(cpu.trap_count() == 1U, "trap frame was not saved");

  const DmaHandle handle = dma.map(TEST_USER_BUFFER, TEST_DMA_LENGTH);
  dma.device_write(handle, 0U, TEST_DEVICE_BYTE);
  dma.unmap(handle);
  require(cpu.user_load(TEST_USER_BUFFER) == TEST_DEVICE_BYTE, "DMA write failed");
  require_fault(FaultKind::InvalidDma, [&] {
    dma.device_write(handle, 0U, TEST_USER_BYTE);
  });

  std::cout << "stage one hardware model checks passed\n";
  return EXIT_SUCCESS;
}

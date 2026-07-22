#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::size_t kBlockSize = 4096;
constexpr std::size_t kBlockCount = 64;
constexpr std::size_t kInodeCount = 32;
constexpr std::size_t kInodeSize = 64;
constexpr std::uint32_t kMagic = 0x3146534d;  // "MFS1" in little endian.
constexpr std::uint32_t kVersion = 1;

constexpr std::size_t kSuperBlock = 0;
constexpr std::size_t kInodeBitmapBlock = 1;
constexpr std::size_t kBlockBitmapBlock = 2;
constexpr std::size_t kInodeTableStart = 3;
constexpr std::size_t kInodeTableBlocks = 4;
constexpr std::size_t kRootDataBlock = 7;
constexpr std::size_t kDataStart = 8;
constexpr std::size_t kJournalBlock = 63;
constexpr std::uint32_t kRootInode = 1;

constexpr std::size_t kDirEntrySize = 64;
constexpr std::size_t kDirNameCapacity = 58;

enum class InodeKind : std::uint8_t { Free = 0, Regular = 1, Directory = 2 };

enum class CrashPoint {
  None,
  AfterInodeBitmap,
  AfterBlockBitmap,
  AfterInode,
  AfterData,
};

class SimulatedCrash : public std::runtime_error {
 public:
  SimulatedCrash() : std::runtime_error("simulated power loss") {}
};

void require(bool condition, std::string_view message) {
  if (!condition) {
    throw std::runtime_error(std::string(message));
  }
}

using Block = std::array<std::uint8_t, kBlockSize>;

class DiskImage {
 public:
  Block& block(std::size_t number) {
    require(number < kBlockCount, "block number out of range");
    return this->blocks_.at(number);
  }

  const Block& block(std::size_t number) const {
    require(number < kBlockCount, "block number out of range");
    return this->blocks_.at(number);
  }

 private:
  std::array<Block, kBlockCount> blocks_{};
};

void put_u16(Block& block, std::size_t offset, std::uint16_t value) {
  require(offset + 2 <= block.size(), "u16 write crosses block boundary");
  block.at(offset) = static_cast<std::uint8_t>(value & 0xffU);
  block.at(offset + 1) = static_cast<std::uint8_t>((value >> 8U) & 0xffU);
}

void put_u32(Block& block, std::size_t offset, std::uint32_t value) {
  require(offset + 4 <= block.size(), "u32 write crosses block boundary");
  for (std::size_t index = 0; index < 4; ++index) {
    block.at(offset + index) = static_cast<std::uint8_t>((value >> (index * 8U)) & 0xffU);
  }
}

std::uint16_t get_u16(const Block& block, std::size_t offset) {
  require(offset + 2 <= block.size(), "u16 read crosses block boundary");
  return static_cast<std::uint16_t>(block.at(offset)) |
         static_cast<std::uint16_t>(static_cast<std::uint16_t>(block.at(offset + 1)) << 8U);
}

std::uint32_t get_u32(const Block& block, std::size_t offset) {
  require(offset + 4 <= block.size(), "u32 read crosses block boundary");
  std::uint32_t value = 0;
  for (std::size_t index = 0; index < 4; ++index) {
    value |= static_cast<std::uint32_t>(block.at(offset + index)) << (index * 8U);
  }
  return value;
}

bool bitmap_test(const Block& bitmap, std::size_t bit) {
  require(bit < bitmap.size() * 8U, "bitmap bit out of range");
  return (bitmap.at(bit / 8U) & static_cast<std::uint8_t>(1U << (bit % 8U))) != 0;
}

void bitmap_set(Block& bitmap, std::size_t bit, bool allocated) {
  require(bit < bitmap.size() * 8U, "bitmap bit out of range");
  const auto mask = static_cast<std::uint8_t>(1U << (bit % 8U));
  if (allocated) {
    bitmap.at(bit / 8U) |= mask;
  } else {
    bitmap.at(bit / 8U) &= static_cast<std::uint8_t>(~mask);
  }
}

struct InodeRecord {
  InodeKind kind = InodeKind::Free;
  std::uint16_t links = 0;
  std::uint32_t size = 0;
  std::uint32_t direct_block = 0;
};

struct DirectoryEntry {
  std::uint32_t inode = 0;
  InodeKind kind = InodeKind::Free;
  std::string name;
};

class MiniFs {
 public:
  static void format(DiskImage& disk) {
    disk = DiskImage{};

    Block& super = disk.block(kSuperBlock);
    put_u32(super, 0, kMagic);
    put_u32(super, 4, kVersion);
    put_u32(super, 8, static_cast<std::uint32_t>(kBlockSize));
    put_u32(super, 12, static_cast<std::uint32_t>(kBlockCount));
    put_u32(super, 16, static_cast<std::uint32_t>(kInodeCount));
    put_u32(super, 20, static_cast<std::uint32_t>(kInodeBitmapBlock));
    put_u32(super, 24, static_cast<std::uint32_t>(kBlockBitmapBlock));
    put_u32(super, 28, static_cast<std::uint32_t>(kInodeTableStart));
    put_u32(super, 32, static_cast<std::uint32_t>(kInodeTableBlocks));
    put_u32(super, 36, static_cast<std::uint32_t>(kDataStart));
    put_u32(super, 40, kRootInode);

    Block& block_bitmap = disk.block(kBlockBitmapBlock);
    for (std::size_t block = 0; block <= kRootDataBlock; ++block) {
      bitmap_set(block_bitmap, block, true);
    }
    bitmap_set(block_bitmap, kJournalBlock, true);

    bitmap_set(disk.block(kInodeBitmapBlock), 0, true);  // Inode 0 is invalid.
    bitmap_set(disk.block(kInodeBitmapBlock), kRootInode, true);

    MiniFs fs(disk);
    fs.write_inode(kRootInode,
                   InodeRecord{InodeKind::Directory, 2, 2U * kDirEntrySize,
                               static_cast<std::uint32_t>(kRootDataBlock)});
    fs.write_directory_entry(0, DirectoryEntry{kRootInode, InodeKind::Directory, "."});
    fs.write_directory_entry(1, DirectoryEntry{kRootInode, InodeKind::Directory, ".."});
  }

  static MiniFs mount(DiskImage& disk) {
    const Block& super = disk.block(kSuperBlock);
    require(get_u32(super, 0) == kMagic, "bad MiniFS magic");
    require(get_u32(super, 4) == kVersion, "unsupported MiniFS version");
    require(get_u32(super, 8) == kBlockSize, "unexpected block size");
    require(get_u32(super, 12) == kBlockCount, "unexpected block count");
    require(get_u32(super, 16) == kInodeCount, "unexpected inode count");
    require(get_u32(super, 20) == kInodeBitmapBlock, "bad inode bitmap location");
    require(get_u32(super, 24) == kBlockBitmapBlock, "bad block bitmap location");
    require(get_u32(super, 28) == kInodeTableStart, "bad inode table location");
    require(get_u32(super, 32) == kInodeTableBlocks, "bad inode table size");
    require(get_u32(super, 36) == kDataStart, "bad data start");
    require(get_u32(super, 40) == kRootInode, "bad root inode number");

    MiniFs fs(disk);
    require(fs.inode_allocated(kRootInode), "root inode is not allocated");
    const InodeRecord root = fs.read_inode(kRootInode);
    require(root.kind == InodeKind::Directory, "root inode is not a directory");
    require(root.direct_block == kRootDataBlock, "root inode points to wrong block");
    require(fs.block_allocated(kRootDataBlock), "root directory block is not allocated");
    return fs;
  }

  std::uint32_t create(std::string_view name, std::string_view contents,
                       CrashPoint crash = CrashPoint::None) {
    require(valid_name(name), "invalid MiniFS filename");
    require(contents.size() <= kBlockSize, "MiniFS-Lab supports one data block per file");
    require(!lookup(name).has_value(), "file already exists");

    const std::uint32_t inode_number = allocate_inode();
    maybe_crash(crash, CrashPoint::AfterInodeBitmap);

    const std::uint32_t data_block = allocate_data_block();
    maybe_crash(crash, CrashPoint::AfterBlockBitmap);

    write_inode(inode_number,
                InodeRecord{InodeKind::Regular, 1,
                            static_cast<std::uint32_t>(contents.size()), data_block});
    maybe_crash(crash, CrashPoint::AfterInode);

    Block& payload = this->disk_.block(data_block);
    for (std::size_t index = 0; index < contents.size(); ++index) {
      payload.at(index) = static_cast<std::uint8_t>(contents.at(index));
    }
    maybe_crash(crash, CrashPoint::AfterData);

    const std::size_t slot = find_free_directory_slot();
    write_directory_entry(slot, DirectoryEntry{inode_number, InodeKind::Regular,
                                                std::string(name)});
    InodeRecord root = read_inode(kRootInode);
    const std::uint32_t required_size = static_cast<std::uint32_t>((slot + 1U) * kDirEntrySize);
    if (root.size < required_size) {
      root.size = required_size;
      write_inode(kRootInode, root);
    }
    return inode_number;
  }

  std::optional<std::uint32_t> lookup(std::string_view name) const {
    const InodeRecord root = read_inode(kRootInode);
    const std::size_t count = root.size / kDirEntrySize;
    for (std::size_t slot = 0; slot < count; ++slot) {
      const DirectoryEntry entry = read_directory_entry(slot);
      if (entry.inode != 0 && entry.name == name) {
        return entry.inode;
      }
    }
    return std::nullopt;
  }

  std::string read_file(std::string_view name) const {
    const std::optional<std::uint32_t> number = lookup(name);
    require(number.has_value(), "file not found");
    const InodeRecord inode = read_inode(*number);
    require(inode.kind == InodeKind::Regular, "directory cannot be read as regular file");
    require(inode.size <= kBlockSize, "corrupt inode size");
    require(inode.direct_block >= kDataStart && inode.direct_block < kJournalBlock,
            "corrupt data block number");
    const Block& data = this->disk_.block(inode.direct_block);
    return std::string(data.begin(), data.begin() + static_cast<std::ptrdiff_t>(inode.size));
  }

  void unlink(std::string_view name) {
    const InodeRecord root = read_inode(kRootInode);
    const std::size_t count = root.size / kDirEntrySize;
    for (std::size_t slot = 2; slot < count; ++slot) {
      const DirectoryEntry entry = read_directory_entry(slot);
      if (entry.inode == 0 || entry.name != name) {
        continue;
      }
      const InodeRecord inode = read_inode(entry.inode);
      write_directory_entry(slot, DirectoryEntry{});
      if (inode.direct_block >= kDataStart && inode.direct_block < kJournalBlock) {
        bitmap_set(this->disk_.block(kBlockBitmapBlock), inode.direct_block, false);
        this->disk_.block(inode.direct_block) = Block{};
      }
      bitmap_set(this->disk_.block(kInodeBitmapBlock), entry.inode, false);
      write_inode(entry.inode, InodeRecord{});
      return;
    }
    throw std::runtime_error("unlink target not found");
  }

  std::vector<std::string> audit() const {
    std::array<bool, kInodeCount> referenced_inodes{};
    std::array<bool, kBlockCount> referenced_blocks{};
    referenced_inodes.at(0) = true;
    referenced_inodes.at(kRootInode) = true;
    for (std::size_t block = 0; block <= kRootDataBlock; ++block) {
      referenced_blocks.at(block) = true;
    }
    referenced_blocks.at(kJournalBlock) = true;

    std::vector<std::string> errors;
    const InodeRecord root = read_inode(kRootInode);
    const std::size_t count = root.size / kDirEntrySize;
    for (std::size_t slot = 0; slot < count; ++slot) {
      const DirectoryEntry entry = read_directory_entry(slot);
      if (entry.inode == 0) {
        continue;
      }
      if (entry.inode >= kInodeCount) {
        errors.emplace_back("directory entry has out-of-range inode");
        continue;
      }
      referenced_inodes.at(entry.inode) = true;
      if (!inode_allocated(entry.inode)) {
        errors.emplace_back("directory entry points to free inode");
        continue;
      }
      const InodeRecord inode = read_inode(entry.inode);
      if (inode.kind == InodeKind::Regular && inode.direct_block < kBlockCount) {
        if (referenced_blocks.at(inode.direct_block)) {
          errors.emplace_back("two objects reference the same data block");
        }
        referenced_blocks.at(inode.direct_block) = true;
        if (!block_allocated(inode.direct_block)) {
          errors.emplace_back("inode points to a block marked free");
        }
      }
    }

    for (std::size_t inode = 2; inode < kInodeCount; ++inode) {
      if (inode_allocated(inode) && !referenced_inodes.at(inode)) {
        errors.emplace_back("allocated inode is unreachable from root directory");
      }
    }
    for (std::size_t block = kDataStart; block < kJournalBlock; ++block) {
      if (block_allocated(block) && !referenced_blocks.at(block)) {
        errors.emplace_back("allocated data block has no reachable owner");
      }
    }
    return errors;
  }

  void reclaim_unreachable() {
    std::array<bool, kInodeCount> reachable{};
    reachable.at(kRootInode) = true;
    const InodeRecord root = read_inode(kRootInode);
    const std::size_t count = root.size / kDirEntrySize;
    for (std::size_t slot = 0; slot < count; ++slot) {
      const DirectoryEntry entry = read_directory_entry(slot);
      if (entry.inode < kInodeCount) {
        reachable.at(entry.inode) = true;
      }
    }

    for (std::size_t inode_number = 2; inode_number < kInodeCount; ++inode_number) {
      if (!inode_allocated(inode_number) || reachable.at(inode_number)) {
        continue;
      }
      const InodeRecord inode = read_inode(static_cast<std::uint32_t>(inode_number));
      if (inode.direct_block >= kDataStart && inode.direct_block < kJournalBlock) {
        bitmap_set(this->disk_.block(kBlockBitmapBlock), inode.direct_block, false);
        this->disk_.block(inode.direct_block) = Block{};
      }
      bitmap_set(this->disk_.block(kInodeBitmapBlock), inode_number, false);
      write_inode(static_cast<std::uint32_t>(inode_number), InodeRecord{});
    }

    std::array<bool, kBlockCount> owned{};
    for (std::size_t block = 0; block <= kRootDataBlock; ++block) {
      owned.at(block) = true;
    }
    owned.at(kJournalBlock) = true;
    for (std::size_t inode_number = 1; inode_number < kInodeCount; ++inode_number) {
      if (!inode_allocated(inode_number)) {
        continue;
      }
      const InodeRecord inode = read_inode(static_cast<std::uint32_t>(inode_number));
      if (inode.kind == InodeKind::Regular && inode.direct_block < kBlockCount) {
        owned.at(inode.direct_block) = true;
      }
    }
    for (std::size_t block = kDataStart; block < kJournalBlock; ++block) {
      if (block_allocated(block) && !owned.at(block)) {
        bitmap_set(this->disk_.block(kBlockBitmapBlock), block, false);
        this->disk_.block(block) = Block{};
      }
    }
  }

 private:
  explicit MiniFs(DiskImage& disk) : disk_(disk) {}

  static bool valid_name(std::string_view name) {
    return !name.empty() && name.size() <= kDirNameCapacity && name != "." &&
           name != ".." && name.find('/') == std::string_view::npos;
  }

  static void maybe_crash(CrashPoint actual, CrashPoint expected) {
    if (actual == expected) {
      throw SimulatedCrash{};
    }
  }

  bool inode_allocated(std::size_t inode) const {
    return bitmap_test(this->disk_.block(kInodeBitmapBlock), inode);
  }

  bool block_allocated(std::size_t block) const {
    return bitmap_test(this->disk_.block(kBlockBitmapBlock), block);
  }

  std::uint32_t allocate_inode() {
    Block& bitmap = this->disk_.block(kInodeBitmapBlock);
    for (std::size_t inode = 2; inode < kInodeCount; ++inode) {
      if (!bitmap_test(bitmap, inode)) {
        bitmap_set(bitmap, inode, true);
        return static_cast<std::uint32_t>(inode);
      }
    }
    throw std::runtime_error("no free inode");
  }

  std::uint32_t allocate_data_block() {
    Block& bitmap = this->disk_.block(kBlockBitmapBlock);
    for (std::size_t block = kDataStart; block < kJournalBlock; ++block) {
      if (!bitmap_test(bitmap, block)) {
        bitmap_set(bitmap, block, true);
        return static_cast<std::uint32_t>(block);
      }
    }
    throw std::runtime_error("no free data block");
  }

  InodeRecord read_inode(std::uint32_t number) const {
    require(number < kInodeCount, "inode number out of range");
    const std::size_t byte_offset = static_cast<std::size_t>(number) * kInodeSize;
    const std::size_t block_number = kInodeTableStart + byte_offset / kBlockSize;
    const std::size_t offset = byte_offset % kBlockSize;
    const Block& block = this->disk_.block(block_number);
    return InodeRecord{static_cast<InodeKind>(block.at(offset)), get_u16(block, offset + 2),
                       get_u32(block, offset + 4), get_u32(block, offset + 8)};
  }

  void write_inode(std::uint32_t number, const InodeRecord& inode) {
    require(number < kInodeCount, "inode number out of range");
    const std::size_t byte_offset = static_cast<std::size_t>(number) * kInodeSize;
    const std::size_t block_number = kInodeTableStart + byte_offset / kBlockSize;
    const std::size_t offset = byte_offset % kBlockSize;
    Block& block = this->disk_.block(block_number);
    for (std::size_t index = 0; index < kInodeSize; ++index) {
      block.at(offset + index) = 0;
    }
    block.at(offset) = static_cast<std::uint8_t>(inode.kind);
    put_u16(block, offset + 2, inode.links);
    put_u32(block, offset + 4, inode.size);
    put_u32(block, offset + 8, inode.direct_block);
  }

  DirectoryEntry read_directory_entry(std::size_t slot) const {
    require((slot + 1U) * kDirEntrySize <= kBlockSize, "directory slot out of range");
    const Block& block = this->disk_.block(kRootDataBlock);
    const std::size_t offset = slot * kDirEntrySize;
    const std::uint32_t inode = get_u32(block, offset);
    const InodeKind kind = static_cast<InodeKind>(block.at(offset + 4));
    const std::size_t name_length = block.at(offset + 5);
    require(name_length <= kDirNameCapacity, "corrupt directory name length");
    std::string name;
    name.reserve(name_length);
    for (std::size_t index = 0; index < name_length; ++index) {
      name.push_back(static_cast<char>(block.at(offset + 6 + index)));
    }
    return DirectoryEntry{inode, kind, name};
  }

  void write_directory_entry(std::size_t slot, const DirectoryEntry& entry) {
    require((slot + 1U) * kDirEntrySize <= kBlockSize, "directory slot out of range");
    require(entry.name.size() <= kDirNameCapacity, "directory name too long");
    Block& block = this->disk_.block(kRootDataBlock);
    const std::size_t offset = slot * kDirEntrySize;
    for (std::size_t index = 0; index < kDirEntrySize; ++index) {
      block.at(offset + index) = 0;
    }
    put_u32(block, offset, entry.inode);
    block.at(offset + 4) = static_cast<std::uint8_t>(entry.kind);
    block.at(offset + 5) = static_cast<std::uint8_t>(entry.name.size());
    for (std::size_t index = 0; index < entry.name.size(); ++index) {
      block.at(offset + 6 + index) = static_cast<std::uint8_t>(entry.name.at(index));
    }
  }

  std::size_t find_free_directory_slot() const {
    const InodeRecord root = read_inode(kRootInode);
    const std::size_t count = root.size / kDirEntrySize;
    for (std::size_t slot = 2; slot < count; ++slot) {
      if (read_directory_entry(slot).inode == 0) {
        return slot;
      }
    }
    require((count + 1U) * kDirEntrySize <= kBlockSize, "root directory is full");
    return count;
  }

  DiskImage& disk_;
};

void test_format_create_remount_and_unlink() {
  DiskImage disk;
  MiniFs::format(disk);
  MiniFs fs = MiniFs::mount(disk);
  const std::uint32_t inode = fs.create("hello", "abc");
  require(inode == 2, "first user file should receive inode 2");
  require(fs.read_file("hello") == "abc", "created file content mismatch");
  require(fs.audit().empty(), "fresh filesystem should satisfy allocation invariants");

  MiniFs remounted = MiniFs::mount(disk);
  require(remounted.read_file("hello") == "abc", "remount must reconstruct file from disk bytes");
  remounted.unlink("hello");
  require(!remounted.lookup("hello").has_value(), "unlink must remove the directory name");
  require(remounted.audit().empty(), "unlink must release inode and data block consistently");
}

void test_crash_exposes_why_a_journal_is_needed() {
  DiskImage disk;
  MiniFs::format(disk);
  MiniFs fs = MiniFs::mount(disk);
  try {
    static_cast<void>(fs.create("lost", "payload", CrashPoint::AfterData));
    throw std::runtime_error("expected simulated crash");
  } catch (const SimulatedCrash&) {
  }

  MiniFs remounted = MiniFs::mount(disk);
  require(!remounted.lookup("lost").has_value(),
          "crash before dirent publication must leave name unreachable");
  const std::vector<std::string> errors = remounted.audit();
  require(errors.size() == 2, "audit should find one unreachable inode and one ownerless block");
  remounted.reclaim_unreachable();
  require(remounted.audit().empty(), "fsck-style repair should reclaim unreachable resources");
}

void test_mount_rejects_unknown_format() {
  DiskImage disk;
  MiniFs::format(disk);
  disk.block(kSuperBlock).at(0) ^= 0xffU;
  bool rejected = false;
  try {
    static_cast<void>(MiniFs::mount(disk));
  } catch (const std::runtime_error&) {
    rejected = true;
  }
  require(rejected, "mount must reject an image with the wrong magic");
}

}  // namespace

int main() {
  test_format_create_remount_and_unlink();
  test_crash_exposes_why_a_journal_is_needed();
  test_mount_rejects_unknown_format();
  std::cout << "MiniFS-Lab checks passed\n";
  return 0;
}

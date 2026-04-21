#include "market_data/MappedFile.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace cmf {

MappedFile::MappedFile(const std::filesystem::path &path) {
  fd_ = ::open(path.c_str(), O_RDONLY);
  if (fd_ < 0) {
    throw std::runtime_error("MappedFile: open '" + path.string() +
                             "': " + std::strerror(errno));
  }

  struct stat st {};
  if (::fstat(fd_, &st) != 0) {
    const int err = errno;
    ::close(fd_);
    fd_ = -1;
    throw std::runtime_error(std::string{"MappedFile: fstat: "} +
                             std::strerror(err));
  }
  size_ = static_cast<std::size_t>(st.st_size);

  if (size_ == 0) {
    // Empty file: nothing to map. data() stays null; consumer must handle
    // the empty range.
    return;
  }

  int flags = MAP_PRIVATE;
#ifdef MAP_POPULATE
  flags |= MAP_POPULATE; // Linux: pre-fault pages in the kernel.
#endif
  void *addr = ::mmap(nullptr, size_, PROT_READ, flags, fd_, 0);
  if (addr == MAP_FAILED) {
    const int err = errno;
    ::close(fd_);
    fd_ = -1;
    size_ = 0;
    throw std::runtime_error(std::string{"MappedFile: mmap: "} +
                             std::strerror(err));
  }
  data_ = static_cast<const char *>(addr);

  // Hint the kernel to aggressively prefetch for forward sequential scan.
  // Ignore madvise errors — they're purely advisory.
  ::madvise(const_cast<void *>(static_cast<const void *>(data_)), size_,
            MADV_SEQUENTIAL);
}

MappedFile::MappedFile(MappedFile &&other) noexcept
    : data_(other.data_), size_(other.size_), fd_(other.fd_) {
  other.data_ = nullptr;
  other.size_ = 0;
  other.fd_ = -1;
}

MappedFile &MappedFile::operator=(MappedFile &&other) noexcept {
  if (this != &other) {
    release();
    data_ = other.data_;
    size_ = other.size_;
    fd_ = other.fd_;
    other.data_ = nullptr;
    other.size_ = 0;
    other.fd_ = -1;
  }
  return *this;
}

MappedFile::~MappedFile() { release(); }

void MappedFile::release() noexcept {
  if (data_ != nullptr && size_ != 0) {
    ::munmap(const_cast<char *>(data_), size_);
  }
  if (fd_ >= 0) {
    ::close(fd_);
  }
  data_ = nullptr;
  size_ = 0;
  fd_ = -1;
}

} // namespace cmf

#include "io/MmapFile.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace md {

namespace {

[[noreturn]] void throwErrno(const std::filesystem::path& path, const char* op) {
    const int saved_errno = errno;
    throw std::runtime_error(
        std::string{op} + " failed for " + path.string() + ": " + std::strerror(saved_errno)
    );
}

} // namespace

MmapFile::MmapFile(const std::filesystem::path& path) {
    fd_ = ::open(path.c_str(), O_RDONLY);
    if (fd_ < 0) {
        throwErrno(path, "open");
    }

    struct stat st{};
    if (::fstat(fd_, &st) != 0) {
        const int saved_errno = errno;
        ::close(fd_);
        fd_ = -1;
        errno = saved_errno;
        throwErrno(path, "fstat");
    }

    size_ = static_cast<std::size_t>(st.st_size);

    // mmap with size 0 fails with EINVAL on macOS/Linux; treat empty files as a no-op.
    if (size_ == 0) {
        base_ = nullptr;
        return;
    }

    void* addr = ::mmap(nullptr, size_, PROT_READ, MAP_PRIVATE, fd_, 0);
    if (addr == MAP_FAILED) {
        const int saved_errno = errno;
        ::close(fd_);
        fd_ = -1;
        size_ = 0;
        errno = saved_errno;
        throwErrno(path, "mmap");
    }

    base_ = static_cast<const char*>(addr);
    ::madvise(const_cast<char*>(base_), size_, MADV_SEQUENTIAL);
}

MmapFile::~MmapFile() {
    close_();
}

MmapFile::MmapFile(MmapFile&& other) noexcept
    : fd_(other.fd_),
      base_(other.base_),
      size_(other.size_),
      pos_(other.pos_) {
    other.fd_ = -1;
    other.base_ = nullptr;
    other.size_ = 0;
    other.pos_ = 0;
}

MmapFile& MmapFile::operator=(MmapFile&& other) noexcept {
    if (this != &other) {
        close_();
        fd_ = other.fd_;
        base_ = other.base_;
        size_ = other.size_;
        pos_ = other.pos_;
        other.fd_ = -1;
        other.base_ = nullptr;
        other.size_ = 0;
        other.pos_ = 0;
    }
    return *this;
}

std::optional<std::string_view> MmapFile::nextLine() noexcept {
    if (pos_ >= size_) {
        return std::nullopt;
    }

    const char* start = base_ + pos_;
    const std::size_t remaining = size_ - pos_;
    const void* nl = std::memchr(start, '\n', remaining);

    if (nl == nullptr) {
        std::string_view line(start, remaining);
        pos_ = size_;
        return line;
    }

    const auto* nl_ptr = static_cast<const char*>(nl);
    const std::size_t length = static_cast<std::size_t>(nl_ptr - start);
    pos_ += length + 1;
    return std::string_view(start, length);
}

bool MmapFile::eof() const noexcept {
    return pos_ >= size_;
}

std::size_t MmapFile::size() const noexcept {
    return size_;
}

void MmapFile::close_() noexcept {
    if (base_ != nullptr) {
        ::munmap(const_cast<char*>(base_), size_);
        base_ = nullptr;
    }
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    size_ = 0;
    pos_ = 0;
}

} // namespace md

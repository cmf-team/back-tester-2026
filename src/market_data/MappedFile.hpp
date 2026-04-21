// MappedFile — RAII wrapper around a read-only POSIX mmap.
//
// Linux and macOS only (POSIX mmap / madvise). On empty files the mapping is
// skipped entirely: data() returns nullptr and size() returns 0.

#pragma once

#include <cstddef>
#include <filesystem>
#include <string_view>

namespace cmf {

class MappedFile {
public:
  // Opens the file read-only, mmaps the full length, and advises the kernel
  // that access will be sequential (triggers read-ahead). Throws
  // std::runtime_error with errno context on any failure.
  explicit MappedFile(const std::filesystem::path &path);

  ~MappedFile();

  MappedFile(const MappedFile &) = delete;
  MappedFile &operator=(const MappedFile &) = delete;

  MappedFile(MappedFile &&other) noexcept;
  MappedFile &operator=(MappedFile &&other) noexcept;

  const char *data() const noexcept { return data_; }
  std::size_t size() const noexcept { return size_; }
  std::string_view view() const noexcept { return {data_, size_}; }

private:
  void release() noexcept;

  const char *data_{nullptr};
  std::size_t size_{0};
  int fd_{-1};
};

} // namespace cmf

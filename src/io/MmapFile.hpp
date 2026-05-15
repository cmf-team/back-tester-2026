#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string_view>

namespace md {

class MmapFile {
public:
    explicit MmapFile(const std::filesystem::path& path);
    ~MmapFile();

    MmapFile(const MmapFile&) = delete;
    MmapFile& operator=(const MmapFile&) = delete;
    MmapFile(MmapFile&& other) noexcept;
    MmapFile& operator=(MmapFile&& other) noexcept;

    // Returns the next line (excluding trailing '\n'). Returns std::nullopt at EOF.
    // The returned string_view points into the mmap'd region; it is valid only
    // until the MmapFile is destroyed or moved-from.
    std::optional<std::string_view> nextLine() noexcept;

    [[nodiscard]] bool eof() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;

private:
    int fd_{-1};
    const char* base_{nullptr};
    std::size_t size_{0};
    std::size_t pos_{0};

    void close_() noexcept;
};

} // namespace md

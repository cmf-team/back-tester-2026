#include "ingestion/Producer.hpp"
#include "ingestion/JsonLineParser.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

namespace cmf {

Producer::Producer(std::filesystem::path file, EventQueue& out)
    : paths_{std::move(file)}, out_(out) {}

Producer::Producer(std::vector<std::filesystem::path> files, EventQueue& out)
    : paths_(std::move(files)), out_(out) {}

Producer::~Producer() {
    if (thread_.joinable()) thread_.join();
}

void Producer::start() {
    thread_ = std::thread(&Producer::run, this);
}

void Producer::join() {
    if (thread_.joinable()) thread_.join();
}

void Producer::run() {
    for (const auto& p : paths_) read_file(p);

    MarketDataEvent sentinel{};
    sentinel.ts_recv = MarketDataEvent::SENTINEL;
    out_.push(sentinel);
}

void Producer::read_file(const std::filesystem::path& path) {
    const int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) return;

    struct stat st{};
    fstat(fd, &st);
    const std::size_t sz = static_cast<std::size_t>(st.st_size);

    posix_fadvise(fd, 0, static_cast<off_t>(sz), POSIX_FADV_SEQUENTIAL);

    void* mapped = mmap(nullptr, sz, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (mapped == MAP_FAILED) return;

    madvise(mapped, sz, MADV_SEQUENTIAL);

    const char* data = static_cast<const char*>(mapped);
    const char* end  = data + sz;

    for (const char* p = data; p < end; ) {
        const char* nl = static_cast<const char*>(memchr(p, '\n', static_cast<std::size_t>(end - p)));
        const char* line_end = nl ? nl : end;
        if (auto ev = parse_mbo_line(std::string_view(p, static_cast<std::size_t>(line_end - p))))
            out_.push(*ev);
        p = nl ? nl + 1 : end;
    }

    munmap(mapped, sz);
}

} // namespace cmf

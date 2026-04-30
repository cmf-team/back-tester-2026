#include "Auxility.hpp"
#include "common/MarketDataParser.hpp"
#include <filesystem>
#include <sstream>
#include <unistd.h>

// Helper to generate a minimal valid NDJSON line
auto MakeNDJSON(uint64_t ts_event_ns, uint64_t ts_recv_ns, uint64_t order_id) -> std::string
{
    auto ts_event_iso = nanosToIso(ts_event_ns);
    auto ts_recv_iso = nanosToIso(ts_recv_ns);

    std::ostringstream oss;
    oss << R"({"ts_recv":")" << ts_recv_iso << R"(","hd":{"ts_event":")" << ts_event_iso
        << R"(","rtype":160,"publisher_id":1,"instrument_id":100},"action":"A","side":"B","price":"10000000000","size":10,"channel_id":0,"order_id":")"
        << order_id << R"(","flags":0,"ts_in_delta":100})";
    return oss.str();
}

// Helper to write NDJSON lines to a temp file
auto WriteTempNDJSON(const std::vector<std::string>& lines) -> std::string
{
    // Create template for mkstemp in system temp directory (XXXXXX must be at end)
    std::string temp_template = std::filesystem::temp_directory_path().string() + "/ndjson_XXXXXX";
    std::vector<char> temp_path(temp_template.begin(), temp_template.end());
    temp_path.push_back('\0');

    // mkstemp creates the file securely and returns a file descriptor
    int fd = mkstemp(temp_path.data());
    if (fd == -1)
    {
        throw std::runtime_error("Failed to create temporary file");
    }

    std::string temp_filename(temp_path.data());

    // Convert file descriptor to FILE* for writing
    FILE* cfile = fdopen(fd, "w");
    if (!cfile)
    {
        close(fd);
        throw std::runtime_error("Failed to open file descriptor");
    }

    for (const auto& line : lines)
    {
        fprintf(cfile, "%s\n", line.c_str());
    }

    fclose(cfile);

    // Rename to add .json extension
    std::string final_filename = temp_filename + ".json";
    std::filesystem::rename(temp_filename, final_filename);

    return final_filename;
}

// Helper to clean up temp file
void RemoveTempFile(const std::string& filename)
{
    std::filesystem::remove(filename);
}

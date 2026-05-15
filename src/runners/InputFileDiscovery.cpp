#include "runners/InputFileDiscovery.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace md {
namespace {

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool endsWith(const std::string& value, const std::string& suffix) {
    return value.size() >= suffix.size()
        && value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool isMetadataFile(const std::filesystem::path& path) {
    const std::string name = lower(path.filename().string());

    return name == "condition" ||
           name == "condition.json" ||
           name == "manifest" ||
           name == "manifest.json" ||
           name == "metadata" ||
           name == "metadata.json";
}

bool isSupportedDataFile(const std::filesystem::path& path) {
    const std::string filename = lower(path.filename().string());

    if (filename.empty() || filename.front() == '.') {
        return false;
    }

    if (isMetadataFile(path)) {
        return false;
    }

    // Real Databento/XEUR batch files used in this task.
    if (endsWith(filename, ".mbo.json") || endsWith(filename, ".mbo")) {
        return true;
    }

    // Small test/synthetic NDJSON files can still be used when they follow the XEUR row shape.
    if (endsWith(filename, ".ndjson") || endsWith(filename, ".jsonl")) {
        return true;
    }

    // Generic daily JSON files are allowed, but known metadata files are excluded above.
    if (endsWith(filename, ".json")) {
        return true;
    }

    return false;
}

} // namespace

std::vector<std::filesystem::path> discoverInputFiles(const std::filesystem::path& folder_path) {
    if (!std::filesystem::exists(folder_path)) {
        throw std::runtime_error("input folder does not exist: " + folder_path.string());
    }

    if (!std::filesystem::is_directory(folder_path)) {
        throw std::runtime_error("input path must be a folder: " + folder_path.string());
    }

    std::vector<std::filesystem::path> files;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(folder_path)) {
        if (entry.is_regular_file() && isSupportedDataFile(entry.path())) {
            files.push_back(entry.path());
        }
    }

    std::sort(files.begin(), files.end());

    if (files.empty()) {
        throw std::runtime_error("input folder contains no supported market-data files: " + folder_path.string());
    }

    return files;
}

} // namespace md

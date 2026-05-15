#pragma once

#include <filesystem>
#include <vector>

namespace md {

std::vector<std::filesystem::path> discoverInputFiles(const std::filesystem::path& folder_path);

} // namespace md

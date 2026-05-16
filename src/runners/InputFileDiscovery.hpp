#pragma once

#include "runners/InputFormat.hpp"

#include <filesystem>
#include <vector>

namespace md {

std::vector<std::filesystem::path> discoverInputFiles(
    const std::filesystem::path& folder_path,
    InputFormat input_format = InputFormat::Json
);

} // namespace md

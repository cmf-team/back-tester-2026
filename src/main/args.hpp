#pragma once

#include <filesystem>
#include <span>
#include <vector>

namespace cmf
{

struct Config
{
    std::vector<std::filesystem::path> data_files;
};

Config parse_args(std::span<const char*> args, std::string_view filename_ext);

} // namespace cmf
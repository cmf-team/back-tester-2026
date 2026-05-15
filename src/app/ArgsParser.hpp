#pragma once

#include "app/AppConfig.hpp"

#include <stdexcept>
#include <string>

namespace md {

class ArgsError final : public std::runtime_error {
public:
    explicit ArgsError(const std::string& message) : std::runtime_error(message) {}
};

class ArgsParser {
public:
    static AppConfig parse(int argc, char* argv[]);
    static std::string usage(const std::string& executable_name);
};

} // namespace md

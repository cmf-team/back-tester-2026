#pragma once

#include <string_view>

namespace md {

enum class InputFormat {
    Json,
    Feather
};

constexpr std::string_view inputFormatName(InputFormat input_format) noexcept {
    switch (input_format) {
        case InputFormat::Json:
            return "json";
        case InputFormat::Feather:
            return "feather";
    }

    return "unknown";
}

} // namespace md

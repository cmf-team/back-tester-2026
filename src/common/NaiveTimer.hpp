#pragma once

#include <chrono>

namespace cmf {

class NaiveTimer {
public:
  NaiveTimer() : start_(std::chrono::high_resolution_clock::now()) {}

  [[nodiscard]] double elapsed_seconds() const {
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double>(end - start_).count();
  }

private:
  std::chrono::high_resolution_clock::time_point start_;
};

} // namespace cmf

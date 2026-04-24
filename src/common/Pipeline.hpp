#pragma once

#include <exception>

namespace common::pipeline {
  
namespace detail {

template <typename Stage>
void tryStopStage(Stage &stage, std::exception_ptr &first_error) {
  try {
    stage.stop();
  } catch (...) {
    if (!first_error) {
      first_error = std::current_exception();
    }
  }
}

} 

template <typename... Stages> void runPipeline(Stages &...stages) {
  (..., stages.run());
}

template <typename... Stages> void stopPipeline(Stages &...stages) {
  std::exception_ptr first_error;
  (detail::tryStopStage(stages, first_error), ...);
  if (first_error) {
    std::rethrow_exception(first_error);
  }
}

} 

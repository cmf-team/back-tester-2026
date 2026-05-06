#pragma once

#include "common/BasicTypes.hpp"
#include <string>

namespace cmf
{

class PipelineRunner
{
  public:
    void run(const std::string &inputFilePath) const;
};

} // namespace cmf

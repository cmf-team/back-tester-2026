#pragma once

#include <fstream>
#include <string>



namespace cmf{



class LineReader {
public:
    explicit LineReader(const std::string& path);
    bool nextLine(std::string& out);

private:
    std::ifstream stream_;
};

}
 // namespace cmf

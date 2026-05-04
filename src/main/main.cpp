// main function for the back-tester app
// please, keep it minimalistic

#include "common/BasicTypes.hpp"

#include <fstream>
#include <iostream>
#include <cstddef>
#include <string>

using namespace cmf;

int main(int argc, const char *argv[])
{
    try
    {
        if (argc != 2)
        {
            std::cerr << "Usage: back-tester <input_file_path>" << std::endl;
            return 1;
        }

        const std::string inputFilePath{argv[1]};
        std::ifstream inputFile(inputFilePath);
        if (!inputFile.is_open())
        {
            std::cerr << "Failed to open file: " << inputFilePath << std::endl;
            return 1;
        }

        std::string line;
        std::size_t totalLines = 0;
        while (std::getline(inputFile, line))
        {
            ++totalLines;
        }

        if (!inputFile.eof())
        {
            std::cerr << "Error while reading file: " << inputFilePath << std::endl;
            return 1;
        }

        std::cout << "Total lines: " << totalLines << std::endl;
    }
    catch (std::exception &ex)
    {
        std::cerr << "Back-tester threw an exception: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}

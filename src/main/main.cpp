// main function for the back-tester app
// please, keep it minimalistic

#include "common/BasicTypes.hpp"
#include "main/PipelineRunner.hpp"

#include <iostream>
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
        PipelineRunner runner;
        runner.run(inputFilePath);
    }
    catch (std::exception &ex)
    {
        std::cerr << "Back-tester threw an exception: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}

#include "common/BasicTypes.hpp"
#include "DataIngestionLayer.hpp"

#include <exception>
#include <iostream>
#include <string>

using namespace cmf;
 
using namespace cmf;

int main(int argc, const char* argv[])
{
    (void)argc; // Suppress unused warning
    (void)argv; // Suppress unused warning

    try
    {
        // Hardcoded path for testing
        std::string file_path = R"(s:\YandexDisk\CMF\Cpp\XEUR-20260409-HJTR7RCAKT\xeur-eobi-20260407.mbo.json)";

        std::cout << "Starting ingestion for: " << file_path << std::endl;
        bool is_debug_tst=true; // хочу прогнать один файл в студии а не в cmd

        int result = 0;
        if( is_debug_tst)
            result =RunDataIngestionFile(file_path);//(argv[1]);
        else 
            result= RunDataIngestionFile(argv[1]);

        std::cout << "Ingestion finished with code: " << result << std::endl;
        return result;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Back-tester threw an exception: " << ex.what() << std::endl;
        return 1;
    }
}


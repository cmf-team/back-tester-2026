// main function for the back-tester app
// please, keep it minimalistic

#include "common/BasicTypes.hpp"

#include <print>

using namespace cmf;

int main([[maybe_unused]] int argc, [[maybe_unused]] const char* argv[])
{
    try
    {
        std::print("Hell! Oh, world!");
    }
    catch (std::exception& ex)
    {
        std::print(stderr, "Back-tester threw an exception: {}", ex.what());
        return 1;
    }

    return 0;
}

#pragma once

#include "ingestion/EventQueue.hpp"
#include <filesystem>
#include <thread>

namespace cmf {

class Producer {
public:
    Producer(std::filesystem::path file, EventQueue& out);
    ~Producer();

    void start();
    void join();

private:
    void run();

    std::filesystem::path path_;
    EventQueue&           out_;
    std::thread           thread_;
};

} // namespace cmf

#pragma once

#include "ingestion/EventQueue.hpp"
#include <filesystem>
#include <thread>
#include <vector>

namespace cmf {

class Producer {
public:
    Producer(std::filesystem::path file, EventQueue& out);
    Producer(std::vector<std::filesystem::path> files, EventQueue& out);
    ~Producer();

    void start();
    void join();

private:
    void run();
    void read_file(const std::filesystem::path& file);

    std::vector<std::filesystem::path> paths_;
    EventQueue&                        out_;
    std::thread                        thread_;
};

} // namespace cmf

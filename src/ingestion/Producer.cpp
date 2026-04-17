#include "ingestion/Producer.hpp"
#include "ingestion/JsonLineParser.hpp"

#include <fstream>
#include <string>

namespace cmf {

Producer::Producer(std::filesystem::path file, EventQueue& out)
    : path_(std::move(file)), out_(out) {}

Producer::~Producer() {
    if (thread_.joinable()) thread_.join();
}

void Producer::start() {
    thread_ = std::thread(&Producer::run, this);
}

void Producer::join() {
    if (thread_.joinable()) thread_.join();
}

void Producer::run() {
    std::ifstream file(path_);
    std::string   line;
    while (std::getline(file, line)) {
        if (auto ev = parse_mbo_line(line))
            out_.push(std::move(ev));
    }
    out_.push(std::nullopt);
}

} // namespace cmf

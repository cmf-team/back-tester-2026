#pragma once
#include "data_layer/JsonParser.hpp"
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

namespace cmf {

    class Producer {
    public:
        Producer(std::filesystem::path file, SpscQueue<MarketDataEvent>& out)
            : paths_{std::move(file)}, out_{out} {}

        Producer(std::vector<std::filesystem::path> files, SpscQueue<MarketDataEvent>& out)
            : paths_{std::move(files)}, out_{out} {}

        ~Producer() {
            if (thread_.joinable()) thread_.join();
        }

        void start() {
            thread_ = std::thread(&Producer::run, this);
        }

        void join() {
            if (thread_.joinable()) thread_.join();
        }

    private:
        void run() noexcept {
            for (const auto& path : paths_) {
                readFile(path);
            }

            MarketDataEvent sentinel{};
            sentinel.ts_recv = MarketDataEvent::SENTINEL;
            out_.push(sentinel);
        }

        void readFile(const std::filesystem::path& path) noexcept {
            std::ifstream file(path, std::ios::in);
            if (!file) return;

            std::string line;
            line.reserve(512);

            while (std::getline(file, line)) {
                if (line.empty()) continue;

                auto ev = parse_mbo_line(line);
                if (ev) {
                    out_.push(*ev);
                }
            }
        }

        std::vector<std::filesystem::path> paths_;
        SpscQueue<MarketDataEvent>& out_;
        std::thread thread_;
    };

} // namespace cmf
#pragma once

#include <cstdint>
#include <string>
#include <vector>

auto MakeNDJSON(uint64_t ts_event_ns, uint64_t ts_recv_ns, uint64_t order_id) -> std::string;

auto WriteTempNDJSON(const std::vector<std::string>& lines) -> std::string;

auto RemoveTempFile(const std::string& filename) -> void;
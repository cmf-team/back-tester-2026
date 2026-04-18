// Reporting: pretty-prints a one-line file banner and the final HW-1
// summary (first/last N events + ingestion stats). Free functions, no
// state, no I/O ownership - the caller passes the streams.

#pragma once

#include "main/EventCollector.hpp"
#include "main/IngestionRunner.hpp"

#include <iosfwd>

namespace cmf {

void printBanner(std::ostream &os, const std::filesystem::path &path);

void printReport(std::ostream &os, const IngestionStats &stats,
                 const EventCollector &collector);

} // namespace cmf

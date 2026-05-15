#include "TestSupport.hpp"

#include "runners/StandardRunner.hpp"

#include <sstream>

namespace md::test {

void testStandardRunner() {
    const auto dir = makeTempDir("standard");
    const auto file = dir / "sample.ndjson";
    writeFile(file, line(100, 1) + "\n" + line(200, 2) + "\n");

    CapturingProcessor processor;
    std::ostringstream err;
    const auto result = StandardRunner{}.run(file, processor, false, err);

    require(processor.events.size() == 2, "standard processed both events");
    require(result.strategy_name == "standard", "standard strategy name");
    require(result.summary.total_messages_processed == 2, "standard summary count");
    require(result.summary.chronological_violations == 0, "standard chronological violations");
    require(result.summary.first_timestamp == xeur_base_timestamp + 100, "standard first timestamp");
    require(result.summary.last_timestamp == xeur_base_timestamp + 200, "standard last timestamp");
    require(result.diagnostics.total_lines_read == 2, "standard total lines read");
    require(err.str().empty(), "standard non-verbose stderr is quiet");

    CapturingProcessor verbose_processor;
    std::ostringstream verbose_err;
    const auto verbose_result = StandardRunner{}.run(file, verbose_processor, true, verbose_err);
    require(verbose_result.summary.total_messages_processed == 2, "standard verbose processed both events");
    requireContains(verbose_err.str(), "selected_mode=standard", "standard verbose mode logged");
    requireContains(verbose_err.str(), "messages_processed=2", "standard verbose count logged");

    std::filesystem::remove_all(dir);
}

} // namespace md::test

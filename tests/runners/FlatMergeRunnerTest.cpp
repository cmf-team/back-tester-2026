#include "TestSupport.hpp"

#include "runners/FlatMergeRunner.hpp"

#include <sstream>

namespace md::test {

void testFlatMergeRunner() {
    const auto dir = makeTempDir("flat");
    writeMultiFileDataset(dir);

    CapturingProcessor processor;
    std::ostringstream err;
    const auto result = FlatMergeRunner{}.run(dir, processor, false, err);

    require(processor.events.size() == 9, "flat processed all events");
    require(result.strategy_name == "flat", "flat strategy name");
    require(result.summary.total_messages_processed == 9, "flat summary count");
    require(result.summary.chronological_violations == 0, "flat chronological violations");
    requireChronological(processor.events, "flat");
    requireTimestampOffsets(processor.events, {100, 200, 300, 400, 500, 600, 700, 800, 900}, "flat");
    require(result.diagnostics.total_lines_read == 9, "flat total lines read");
    require(err.str().empty(), "flat non-verbose stderr is quiet");

    CapturingProcessor verbose_processor;
    std::ostringstream verbose_err;
    const auto verbose_result = FlatMergeRunner{}.run(dir, verbose_processor, true, verbose_err);
    require(verbose_result.summary.total_messages_processed == 9, "flat verbose processed all events");
    requireContains(verbose_err.str(), "selected_mode=flat", "flat verbose mode logged");
    requireContains(verbose_err.str(), "discovered_files_count=3", "flat verbose file count logged");
    requireContains(verbose_err.str(), "merge_strategy=single_level_k_way_heap", "flat verbose strategy logged");
    requireContains(verbose_err.str(), "chronological_violations=0", "flat verbose violations logged");

    std::filesystem::remove_all(dir);
}

} // namespace md::test

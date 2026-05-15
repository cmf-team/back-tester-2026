#include "TestSupport.hpp"

#include "runners/HierarchicalMergeRunner.hpp"

#include <sstream>

namespace md::test {

void testHierarchicalMergeRunner() {
    const auto dir = makeTempDir("hierarchy");
    writeMultiFileDataset(dir);

    CapturingProcessor processor;
    std::ostringstream err;
    const auto result = HierarchicalMergeRunner{}.run(dir, processor, false, err);

    require(processor.events.size() == 9, "hierarchy processed all events");
    require(result.strategy_name == "hierarchy", "hierarchy strategy name");
    require(result.summary.total_messages_processed == 9, "hierarchy summary count");
    require(result.summary.chronological_violations == 0, "hierarchy chronological violations");
    requireChronological(processor.events, "hierarchy");
    requireTimestampOffsets(processor.events, {100, 200, 300, 400, 500, 600, 700, 800, 900}, "hierarchy");
    require(result.diagnostics.total_lines_read == 9, "hierarchy total lines read");
    require(err.str().empty(), "hierarchy non-verbose stderr is quiet");

    CapturingProcessor verbose_processor;
    std::ostringstream verbose_err;
    const auto verbose_result = HierarchicalMergeRunner{}.run(dir, verbose_processor, true, verbose_err);
    require(verbose_result.summary.total_messages_processed == 9, "hierarchy verbose processed all events");
    requireContains(verbose_err.str(), "selected_mode=hierarchy", "hierarchy verbose mode logged");
    requireContains(verbose_err.str(), "discovered_files_count=3", "hierarchy verbose file count logged");
    requireContains(verbose_err.str(), "merge_strategy=4_way_tree", "hierarchy verbose strategy logged");
    requireContains(verbose_err.str(), "chronological_violations=0", "hierarchy verbose violations logged");

    std::filesystem::remove_all(dir);
}

} // namespace md::test

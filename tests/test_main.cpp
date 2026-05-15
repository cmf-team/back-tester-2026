#include <exception>
#include <iostream>

namespace md::test {

void testArgsParserAllSupportedForms();
void testNonBlockingQueue();
void testMarketDataEventFormattingAndOrdering();
void testProcessingSummaryChronologicalViolations();
void testParserValidEvent();
void testParserNullAndDecimalPrices();
void testParserTimestampFallbackAndNumericFields();
void testParserSideAndActionCodes();
void testStandardRunner();
void testFlatMergeRunner();
void testHierarchicalMergeRunner();

} // namespace md::test

int main() {
    try {
        md::test::testNonBlockingQueue();
        md::test::testMarketDataEventFormattingAndOrdering();
        md::test::testProcessingSummaryChronologicalViolations();
        md::test::testParserValidEvent();
        md::test::testParserNullAndDecimalPrices();
        md::test::testParserTimestampFallbackAndNumericFields();
        md::test::testParserSideAndActionCodes();
        md::test::testArgsParserAllSupportedForms();
        md::test::testStandardRunner();
        md::test::testFlatMergeRunner();
        md::test::testHierarchicalMergeRunner();
    } catch (const std::exception& e) {
        std::cerr << "TEST FAILURE: " << e.what() << '\n';
        return 1;
    }

    std::cout << "All tests passed\n";
    return 0;
}

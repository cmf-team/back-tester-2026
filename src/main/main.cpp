// main function for the back-tester app
// please, keep it minimalistic

#include "common/BasicTypes.hpp"
#include "../utils/MarketDataReader.h"
#include "../utils/MarketDataProcessor.h"

using namespace cmf;

int main([[maybe_unused]] int argc, [[maybe_unused]] const char *argv[]) {
  try {
    if (argc < 2)
    {
      std::cerr << "Usage: " << argv[0] << " <market_data_file>\n";
      return 1;
    }
    loadMarketData(
        argv[1],
        processMarketDataEvent
    );
  } catch (std::exception &ex) {
    std::cerr << "Back-tester threw an exception: " << ex.what() << std::endl;
    return 1;
  }

  return 0;
}

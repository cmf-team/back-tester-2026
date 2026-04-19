#include "EventParser.hpp"
#include "json.hpp"
#include <ctime>
#include <limits>
using json = nlohmann::json;

using namespace cmf;

namespace {
    
NanoTime parseTimestamp(const std::string& iso) {
    
    std::tm tm{};
    strptime(iso.c_str(), "%Y-%m-%dT%H:%M:%S", &tm);
    

    
    NanoTime nanoseconds = static_cast<NanoTime>(timegm(&tm)) * 1'000'000'000LL;

    // NANOSECONDS FROM ISO

    auto dotPos = iso.find('.');

    if (dotPos != std::string::npos){
        std::string nanoSec =  iso.substr(dotPos + 1, 9);

        nanoseconds += std::stol(nanoSec);

    }

    return nanoseconds;
}
} //anonymus namespace
namespace cmf {
MarketDataEvent parseEvent(const std::string& jsonLine){

    json j = json::parse(jsonLine);

    MarketDataEvent event;

    // filling in the event
    
    // PRICE
    if (j["price"].is_null()) {

        event.price = std::numeric_limits<double>::quiet_NaN();

    }
    else {
        event.price = std::stod(j["price"].get<std::string>());
    }
    // RTYPE & INSTRUMENT_ID & PUBLISHER_ID
    auto& hd = j["hd"];
    event.rtype = static_cast<uint8_t>(hd["rtype"].get<int>()); 
    event.instrument_id = hd["instrument_id"].get<uint32_t>();
    event.publisher_id = hd["publisher_id"].get<uint16_t>(); 

    // ORDER_ID
    event.order_id = std::stoull(j["order_id"].get<std::string>());
    
    // SIDE
    char sideChar = j["side"].get<std::string>()[0];

    switch (sideChar){
        case 'A' : event.side = Side::Sell; break;
        case 'B' : event.side = Side::Buy; break;
        default : event.side = Side::None; break; 
    }

    //ACTION
    char actionChar = j["action"].get<std::string>()[0];
    event.action = static_cast<Action>(actionChar);

    //TIMES

    event.ts_recv = parseTimestamp(j["ts_recv"].get<std::string>());
    event.ts_event = parseTimestamp(hd["ts_event"].get<std::string>());
    

    event.ts_in_delta = j["ts_in_delta"].get<int32_t>();

    //FLAGS 
    event.flags = j["flags"].get<uint8_t>();

    //SEQUENCE
    event.sequence = j["sequence"].get<uint32_t>();

    return event;
}
} //namespace cmf
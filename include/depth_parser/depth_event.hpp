#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace depth_parser {

// Price and quantity as fixed-point integers scaled by 1e8.
struct PriceLevel {
    int64_t price_e8;
    int64_t qty_e8;
    bool operator==(const PriceLevel &) const = default;
};

// Binance futures `depthUpdate` event.
struct DepthEvent {
    uint64_t exchange_time;        // T
    uint64_t update_id;            // u
    uint64_t prev_update_id;       // pu
    std::string symbol;            // s
    std::vector<PriceLevel> bids;  // b
    std::vector<PriceLevel> asks;  // a
    bool operator==(const DepthEvent &) const = default;
};

}  // namespace depth_parser

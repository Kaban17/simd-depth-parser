#pragma once

#include "depth_parser/depth_event.hpp"

#include <charconv>
#include <cstdint>
#include <string_view>
#include <vector>

namespace depth_parser {

// Straightforward scalar parser used as a correctness oracle and a speed
// baseline. Searches for each key with std::string_view::find.
class ReferenceParser {
  public:
    void ParseTo(std::string_view data, DepthEvent &event) {
        event.exchange_time = ReadU64(After(data, "\"T\":"));
        event.update_id = ReadU64(After(data, "\"u\":"));
        event.prev_update_id = ReadU64(After(data, "\"pu\":"));

        std::string_view sym = After(data, "\"s\":\"");
        event.symbol.assign(sym.substr(0, sym.find('"')));

        ParseLevels(After(data, "\"b\":["), event.bids);
        ParseLevels(After(data, "\"a\":["), event.asks);
    }

  private:
    static std::string_view After(std::string_view s, std::string_view key) {
        size_t pos = s.find(key);
        return pos == std::string_view::npos ? std::string_view{}
                                             : s.substr(pos + key.size());
    }

    static uint64_t ReadU64(std::string_view v) {
        uint64_t r = 0;
        std::from_chars(v.data(), v.data() + v.size(), r);
        return r;
    }

    // Parses "<digits>[.<digits>]" into an integer scaled by 1e8.
    static int64_t ReadFixedE8(std::string_view v) {
        int64_t whole = 0;
        int64_t frac = 0;
        int fdig = 0;
        size_t i = 0;
        for (; i < v.size() && v[i] != '.'; ++i) {
            whole = whole * 10 + (v[i] - '0');
        }
        if (i < v.size()) {
            for (++i; i < v.size(); ++i) {
                if (fdig < 8) {
                    frac = frac * 10 + (v[i] - '0');
                    ++fdig;
                }
            }
        }
        for (; fdig < 8; ++fdig) {
            frac *= 10;
        }
        return whole * 100000000LL + frac;
    }

    // `body` starts right after the opening '[' of the levels array.
    static void ParseLevels(std::string_view body, std::vector<PriceLevel> &out) {
        size_t i = 0;
        while (i < body.size() && body[i] == '[') {
            size_t p_start = i + 2;
            size_t p_end = body.find('"', p_start);
            size_t q_start = p_end + 3;
            size_t q_end = body.find('"', q_start);
            out.push_back({ReadFixedE8(body.substr(p_start, p_end - p_start)),
                           ReadFixedE8(body.substr(q_start, q_end - q_start))});
            i = q_end + 2;  // skip "]
            if (i < body.size() && body[i] == ',') {
                ++i;
            }
        }
    }
};

}  // namespace depth_parser

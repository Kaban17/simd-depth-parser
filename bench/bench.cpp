// Validates SimdDepthParser against ReferenceParser on a JSONL file and
// measures per-event parse latency of both with rdtsc.
//
// usage: depth_bench <input.jsonl> [iters] [cpu]

#include "depth_parser/reference_parser.hpp"
#include "depth_parser/simd_parser.hpp"

#include <sched.h>
#include <x86intrin.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace depth_parser;

// SimdDepthParser reads up to 64 bytes past the end of the input.
constexpr size_t kSimdPad = 64;
constexpr size_t kMaxLevels = 4096;

std::vector<std::string> ReadLines(const char *path) {
    std::ifstream f(path);
    if (!f) {
        std::cerr << std::format("cannot open {}\n", path);
        std::exit(2);
    }
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty()) {
            continue;
        }
        line.reserve(line.size() + kSimdPad);
        lines.push_back(std::move(line));
    }
    return lines;
}

void Reset(DepthEvent &e) {
    e.symbol.clear();
    e.bids.clear();
    e.asks.clear();
}

DepthEvent MakeEvent() {
    DepthEvent e{};
    e.symbol.reserve(64);
    e.bids.reserve(kMaxLevels);
    e.asks.reserve(kMaxLevels);
    return e;
}

bool Validate(const std::vector<std::string_view> &lines) {
    ReferenceParser ref;
    SimdDepthParser simd;
    DepthEvent want = MakeEvent();
    DepthEvent got = MakeEvent();
    for (size_t i = 0; i < lines.size(); ++i) {
        Reset(want);
        Reset(got);
        ref.ParseTo(lines[i], want);
        simd.ParseTo(lines[i], got);
        if (!(want == got)) {
            std::cerr << std::format("mismatch at line {}: {}\n", i + 1, lines[i]);
            return false;
        }
    }
    std::cerr << std::format("validation OK: {} events\n", lines.size());
    return true;
}

uint64_t StartTicks() {
    _mm_lfence();
    return __rdtsc();
}

uint64_t EndTicks() {
    unsigned aux;
    uint64_t t = __rdtscp(&aux);
    _mm_lfence();
    return t;
}

double TicksPerNs() {
    using clk = std::chrono::steady_clock;
    auto w0 = clk::now();
    uint64_t t0 = StartTicks();
    while (clk::now() - w0 < std::chrono::milliseconds(200)) {
    }
    uint64_t t1 = EndTicks();
    double ns = std::chrono::duration<double, std::nano>(clk::now() - w0).count();
    return double(t1 - t0) / ns;
}

struct Percentiles {
    double p50, p95, p99;
};

template <typename Parser>
Percentiles Measure(const std::vector<std::string_view> &lines, int iters,
                    double ticks_per_ns) {
    std::vector<uint64_t> lat;
    lat.reserve(size_t(iters) * lines.size());
    for (int it = 0; it < iters; ++it) {
        Parser parser{};
        DepthEvent out = MakeEvent();
        for (std::string_view line : lines) {
            Reset(out);
            uint64_t t0 = StartTicks();
            parser.ParseTo(line, out);
            asm volatile("" : : "r,m"(out) : "memory");
            lat.push_back(EndTicks() - t0);
        }
    }
    std::ranges::sort(lat);
    auto pct = [&](double p) {
        return double(lat[size_t(p * double(lat.size() - 1))]) / ticks_per_ns;
    };
    return {pct(0.50), pct(0.95), pct(0.99)};
}

}  // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "usage: depth_bench <input.jsonl> [iters] [cpu]\n";
        return 2;
    }
    int iters = argc >= 3 ? std::max(1, std::atoi(argv[2])) : 3;
    if (argc >= 4) {
        cpu_set_t set;
        CPU_ZERO(&set);
        CPU_SET(std::atoi(argv[3]), &set);
        sched_setaffinity(0, sizeof(set), &set);
    }

    std::vector<std::string> storage = ReadLines(argv[1]);
    std::vector<std::string_view> lines(storage.begin(), storage.end());
    if (!Validate(lines)) {
        return 1;
    }

    double tpn = TicksPerNs();
    Percentiles ref = Measure<ReferenceParser>(lines, iters, tpn);
    Percentiles simd = Measure<SimdDepthParser>(lines, iters, tpn);

    std::cout << std::format("{:<10} {:>10} {:>10} {:>10}\n", "parser", "p50, ns",
                             "p95, ns", "p99, ns");
    std::cout << std::format("{:<10} {:>10.1f} {:>10.1f} {:>10.1f}\n", "reference",
                             ref.p50, ref.p95, ref.p99);
    std::cout << std::format("{:<10} {:>10.1f} {:>10.1f} {:>10.1f}\n", "simd",
                             simd.p50, simd.p95, simd.p99);
    std::cout << std::format("{:<10} {:>9.2f}x {:>9.2f}x {:>9.2f}x\n", "speedup",
                             ref.p50 / simd.p50, ref.p95 / simd.p95,
                             ref.p99 / simd.p99);
    return 0;
}

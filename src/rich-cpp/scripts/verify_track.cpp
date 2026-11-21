// Throwaway verification for rich::progress::track (not part of the oracle
// suite — timing-dependent, so checked structurally instead).
#include <Track.h>
#include <rich_core.h>

#include <cassert>
#include <iostream>
#include <vector>

using namespace rich;
using namespace rich::progress;

int main() {
    using namespace std::chrono;
    const auto sec = [](double s) { return nanoseconds{static_cast<long long>(s * 1e9)}; };

    ProgressTracker tracker;
    std::vector<int> items{1, 2, 3, 4, 5};

    int sum = 0;
    for (auto& x : track(tracker, "Processing")(items)) {
        sum += x;
    }
    assert(sum == 15);

    // Render at t=1: all 5 steps done -> finished row, 100%, ETA 0:00:00.
    auto segs = tracker.render(Console(std::cout), Options{.width = 60}, sec(1));
    std::string out;
    for (const auto& s : segs) out += s.text;
    std::cout << out;

    // Structural checks on the rendered line.
    assert(out.find("Processing") != std::string::npos);
    assert(out.find("100%") != std::string::npos);
    assert(out.find("0:00:00") != std::string::npos);
    // Bar should be fully filled (no trailing spaces before the percent col).
    assert(out.find("\xe2\x94\x81\xe2\x94\x81\xe2\x94\x81") != std::string::npos);

    // Exception safety: task left in final state when loop throws mid-way.
    {
        ProgressTracker t2;
        std::vector<int> v{10, 20, 30, 40, 50};
        bool threw = false;
        try {
            for (auto& x : track(t2, "Boom")(v)) {
                if (x == 30) throw 42;
            }
        } catch (...) {
            threw = true;
        }
        assert(threw);
        auto segs2 = t2.render(Console(std::cout), Options{.width = 60}, sec(1));
        std::string out2;
        for (const auto& s : segs2) out2 += s.text;
        std::cout << out2;
        // 3 of 5 steps completed before the throw.
        assert(out2.find("60%") != std::string::npos);
    }

    std::cout << "\ntrack adapter: OK\n";
    return 0;
}

#pragma once
// rich::progress::track — range-tracking adapter (Tier 3 item 3.2).
//
// Python Rich exposes `rich.progress.track(iterable, description=..., total=...)`
// as a generator that yields each item while advancing a Progress task. The C++
// equivalent is an RAII view used with range-for:
//
//   ProgressTracker tracker;
//   for (auto& x : track(tracker, "Processing")(items)) { ... }
//
// Semantics (mirroring Python's track):
//   - On construction the adapter adds a task to the tracker (total = number of
//     elements when known, else unknown) and starts it at the current clock.
//   - Each iteration advances the task by one step (tick(1)).
//   - On scope exit (normal or exceptional) the task is left in its final state;
//     if the loop completed fully the task is finished (completed == total).
//
// Determinism: like ProgressTracker, the clock is injected via nanoseconds
// (default steady_clock); oracle tests use fixed values.

#include <ProgressTracker.h>

#include <chrono>
#include <cstddef>
#include <iterator>
#include <utility>

namespace rich::progress {

template<class It>
class TrackRange {
public:
    using value_type = typename std::iterator_traits<It>::value_type;
    using reference = value_type&;
    using difference_type = std::ptrdiff_t;
    using pointer = value_type*;
    using iterator_category = std::input_iterator_tag;

    // total: expected element count; pass -1 when unknown (pulsing bar).
    TrackRange(ProgressTracker& tracker, TaskId id, It begin, It end, double total)
        : tracker_(tracker), id_(id), it_(begin), end_(end), total_(total) {}

    // Range interface (for range-for): begin/end return copies of this view.
    TrackRange begin() const { return *this; }
    TrackRange end() const { return TrackRange(tracker_, id_, end_, end_, total_); }

    // Advance the tracked task by one step, then yield the current element.
    reference operator*() {
        tracker_.tick(id_, 1.0);
        return *it_;
    }

    TrackRange& operator++() {
        ++it_;
        return *this;
    }
    // Post-increment is required by range-for; the returned value is unused.
    TrackRange operator++(int) {
        TrackRange tmp = *this;
        ++it_;
        return tmp;
    }

    bool operator!=(const TrackRange& o) const { return it_ != o.it_; }
    bool operator==(const TrackRange& o) const { return it_ == o.it_; }

private:
    ProgressTracker& tracker_;
    TaskId id_;
    It it_;
    It end_;
    double total_;
};

// track(tracker, description) -> callable adapting any range:
//   for (auto& x : track(tracker, "Processing")(items)) { ... }
struct TrackAdapter {
    ProgressTracker& tracker;
    std::string description;

    template<class Range>
    auto operator()(Range&& range) const {
        using R = std::remove_reference_t<Range>;
        using It = decltype(std::begin(std::declval<R&>()));
        auto begin = std::begin(range);
        auto end = std::end(range);
        // Detect a usable size() member (covers std containers and any
        // range exposing it); fall back to unknown total otherwise.
        double total = -1.0;
        if constexpr (requires(R& r) { std::size(r); }) {
            total = static_cast<double>(std::size(range));
        }
        ProgressTracker::RowOptions opts{.description = description, .total = total};
        TaskId id = tracker.add_task(opts);
        return TrackRange<It>(tracker, id, begin, end, total);
    }
};

inline TrackAdapter track(ProgressTracker& tracker, std::string_view description) {
    return TrackAdapter{tracker, std::string(description)};
}

} // namespace rich::progress

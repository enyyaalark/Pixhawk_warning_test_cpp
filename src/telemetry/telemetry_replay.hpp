#pragma once

/// Replay normalized telemetry recordings without Pixhawk hardware.

#include <chrono>
#include <filesystem>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

#include "telemetry_data.hpp"

namespace telemetry {

class TelemetryReplayError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/// One recorded normalized snapshot and its wall-clock recording time.
struct ReplaySample {
    std::chrono::system_clock::time_point recorded_at;
    std::optional<double> recorded_monotonic_s;
    TelemetryData telemetry;
};

/// Read V1 JSONL recordings and optionally reproduce their timing.
class TelemetryReplay {
public:
    explicit TelemetryReplay(
        std::filesystem::path path,
        std::function<void(double)> sleep_fn =
            [](double s) { /* no-op (caller provides std::this_thread::sleep_for) */ });

    /// Yield validated samples without adding delays.
    std::vector<ReplaySample> samples();

    /// Yield samples using recorded intervals divided by *speed*.
    ///
    /// A speed of 1.0 reproduces original timing, 2.0 is twice as fast,
    /// and ``std::numeric_limits<double>::infinity()`` performs immediate replay.
    std::vector<ReplaySample> replay(double speed = 1.0);

    const std::filesystem::path& path() const noexcept { return _path; }

private:
    std::filesystem::path _path;
    std::function<void(double)> _sleep;
};

}  // namespace telemetry

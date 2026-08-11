#pragma once

/// Record normalized telemetry snapshots as replay-friendly JSON Lines.

#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <stdexcept>
#include <string>

#include "telemetry_data.hpp"

namespace telemetry {

constexpr int SCHEMA_VERSION = 1;

class TelemetryRecordingError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/// Write latest-state telemetry snapshots at a bounded sample rate.
class TelemetryRecorder {
public:
    /// Construct a recorder.
    ///
    /// *sample_rate_hz* controls the minimum interval between snapshots.
    TelemetryRecorder(std::filesystem::path path,
                      double sample_rate_hz = 5.0,
                      std::function<double()> clock = nullptr,
                      std::function<std::chrono::system_clock::time_point()> utc_now = nullptr);

    ~TelemetryRecorder();

    // Non-copyable, movable
    TelemetryRecorder(const TelemetryRecorder&) = delete;
    TelemetryRecorder& operator=(const TelemetryRecorder&) = delete;

    /// Open the output file in append mode.
    void open();

    /// Record a snapshot if the sampling deadline has arrived.
    /// Returns true if a snapshot was written.
    bool record_if_due(const TelemetryData& data);

    /// Immediately append one normalized snapshot.
    void record(const TelemetryData& data);

    /// Close the recording; calling this repeatedly is safe.
    void close() noexcept;

    const std::filesystem::path& path() const noexcept { return _path; }

private:
    std::filesystem::path _path;
    double _sample_period_s;
    std::function<double()> _clock;
    std::function<std::chrono::system_clock::time_point()> _utc_now;
    std::optional<double> _next_record_time;
    std::ofstream _file;
};

}  // namespace telemetry

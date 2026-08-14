#pragma once

/// Evaluate normalized telemetry and emit warning state transitions.

#include <chrono>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "warning_event.hpp"
#include "telemetry/telemetry_data.hpp"

namespace warning {

/// First-version rules for connection loss and unhealthy sensors.
class WarningEngine {
public:
    explicit WarningEngine(
        std::function<std::chrono::system_clock::time_point()> utc_now = nullptr,
        std::function<double()> clock = nullptr,
        double sensor_activation_delay_s = 1.0,
        double sensor_clear_delay_s = 1.0,
        double sensor_data_timeout_s = 3.0);

    /// Return only newly activated or newly cleared warning events.
    ///
    /// *timestamp* overrides the current time (used during replay).
    /// *evaluation_time_s* overrides the monotonic clock (used during replay).
    std::vector<WarningEvent> evaluate(
        const telemetry::TelemetryData& data,
        std::optional<std::chrono::system_clock::time_point> timestamp = std::nullopt,
        std::optional<double> evaluation_time_s = std::nullopt);

private:
    std::vector<WarningEvent> evaluate_connection(
        const telemetry::TelemetryData& data,
        std::chrono::system_clock::time_point timestamp);

    std::vector<WarningEvent> evaluate_sensors(
        const telemetry::TelemetryData& data,
        std::chrono::system_clock::time_point timestamp,
        double evaluation_time_s);

    std::function<std::chrono::system_clock::time_point()> _utc_now;
    std::function<double()> _clock;
    double _sensor_activation_delay_s;
    double _sensor_clear_delay_s;
    double _sensor_data_timeout_s;

    bool _connection_lost_active{false};
    std::unordered_set<std::string> _unhealthy_sensors;
    std::unordered_map<std::string, double> _unhealthy_since;
    std::unordered_map<std::string, double> _healthy_since;
};

}  // namespace warning

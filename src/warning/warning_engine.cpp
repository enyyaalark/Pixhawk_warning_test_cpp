/// Warning engine implementation.

#include "warning_engine.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <stdexcept>

#include "utils/chrono_utils.hpp"

namespace warning {

using telemetry::TelemetryData;

namespace {
    std::string to_upper(const std::string& s) {
        std::string result = s;
        for (auto& c : result) {
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
        return result;
    }
}  // anonymous namespace

WarningEngine::WarningEngine(
    std::function<std::chrono::system_clock::time_point()> utc_now,
    std::function<double()> clock,
    double sensor_activation_delay_s,
    double sensor_clear_delay_s)
    : _utc_now(utc_now ? utc_now : []() {
          return std::chrono::system_clock::now();
      }),
      _clock(clock ? clock : utils::steady_seconds),
      _sensor_activation_delay_s(sensor_activation_delay_s),
      _sensor_clear_delay_s(sensor_clear_delay_s) {
    if (sensor_activation_delay_s < 0) {
        throw std::invalid_argument("sensor_activation_delay_s must not be negative");
    }
    if (sensor_clear_delay_s < 0) {
        throw std::invalid_argument("sensor_clear_delay_s must not be negative");
    }
}

std::vector<WarningEvent> WarningEngine::evaluate(
    const TelemetryData& data,
    std::optional<std::chrono::system_clock::time_point> timestamp,
    std::optional<double> evaluation_time_s) {

    auto event_time = timestamp.value_or(_utc_now());
    double rule_time_s = evaluation_time_s.value_or(_clock());

    if (!std::isfinite(rule_time_s)) {
        throw std::invalid_argument("evaluation_time_s must be finite");
    }

    auto events = evaluate_connection(data, event_time);
    auto sensor_events = evaluate_sensors(data, event_time, rule_time_s);
    events.insert(events.end(),
                  std::make_move_iterator(sensor_events.begin()),
                  std::make_move_iterator(sensor_events.end()));
    return events;
}

std::vector<WarningEvent> WarningEngine::evaluate_connection(
    const TelemetryData& data,
    std::chrono::system_clock::time_point timestamp) {

    const auto& conn = data.connection;

    // No warning during startup before the first observed heartbeat.
    if (!conn.last_heartbeat_monotonic_s.has_value()) {
        return {};
    }

    bool lost = !conn.connected;
    if (lost == _connection_lost_active) {
        return {};
    }

    _connection_lost_active = lost;
    return {WarningEvent{
        "CONNECTION_LOST",
        Severity::CRITICAL,
        lost ? "Pixhawk telemetry heartbeat lost"
             : "Pixhawk telemetry heartbeat restored",
        timestamp,
        lost,
    }};
}

std::vector<WarningEvent> WarningEngine::evaluate_sensors(
    const TelemetryData& data,
    std::chrono::system_clock::time_point timestamp,
    double evaluation_time_s) {

    // Build the set of currently unhealthy sensors
    std::unordered_set<std::string> currently_unhealthy;
    for (const auto& [name, state] : data.system.sensors) {
        if (state.present && state.enabled && !state.healthy) {
            currently_unhealthy.insert(name);
        }
    }

    std::vector<WarningEvent> events;

    // Activation: newly unhealthy sensors that persist beyond the activation delay
    for (const auto& name : currently_unhealthy) {
        _healthy_since.erase(name);

        if (_unhealthy_sensors.count(name)) {
            continue;  // already active
        }

        auto [it, inserted] = _unhealthy_since.try_emplace(name, evaluation_time_s);
        double elapsed = evaluation_time_s - it->second;
        if (elapsed >= _sensor_activation_delay_s) {
            _unhealthy_sensors.insert(name);
            _unhealthy_since.erase(name);
            events.push_back(WarningEvent{
                "SENSOR_UNHEALTHY_" + to_upper(name),
                Severity::WARNING,
                "Enabled sensor is unhealthy: " + name,
                timestamp,
                true,
            });
        }
    }

    // Clean up tracking for sensors that are no longer unhealthy
    for (auto it = _unhealthy_since.begin(); it != _unhealthy_since.end(); ) {
        if (!currently_unhealthy.count(it->first)) {
            it = _unhealthy_since.erase(it);
        } else {
            ++it;
        }
    }

    // Clear: sensors that recovered and stay healthy beyond the clear delay
    for (auto it = _unhealthy_sensors.begin(); it != _unhealthy_sensors.end(); ) {
        const auto& name = *it;
        if (!currently_unhealthy.count(name)) {
            auto [ins, _] = _healthy_since.try_emplace(name, evaluation_time_s);
            double elapsed = evaluation_time_s - ins->second;
            if (elapsed >= _sensor_clear_delay_s) {
                // Copy name before erasing — erase invalidates the reference
                std::string sensor_name = name;
                it = _unhealthy_sensors.erase(it);
                _healthy_since.erase(sensor_name);
                events.push_back(WarningEvent{
                    "SENSOR_UNHEALTHY_" + to_upper(sensor_name),
                    Severity::WARNING,
                    "Sensor health restored: " + sensor_name,
                    timestamp,
                    false,
                });
                continue;
            }
        } else {
            _healthy_since.erase(name);
        }
        ++it;
    }

    return events;
}

}  // namespace warning

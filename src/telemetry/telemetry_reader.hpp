#pragma once

/// Receive MAVLink messages and update normalized telemetry state.

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

extern "C" {
#include <mavlink_types.h>
}

#include "pixhawk_connection.hpp"
#include "telemetry_data.hpp"

namespace telemetry {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// Return a finite double, or std::nullopt for missing / NaN / infinite values.
std::optional<double> finite(double value);

// ---------------------------------------------------------------------------
// Sensor bitmask map
// ---------------------------------------------------------------------------

/// MAV_SYS_STATUS_SENSOR bit names from the MAVLink common message set.
extern const std::unordered_map<uint32_t, std::string> SENSOR_BITS;

// ---------------------------------------------------------------------------
// TelemetryReader
// ---------------------------------------------------------------------------

class TelemetryReader {
public:
    /// Construct a reader that writes into *data*.
    ///
    /// If *data* is not provided the reader owns an internal
    /// ``TelemetryData`` instance accessible via ``data()``.
    explicit TelemetryReader(PixhawkConnection& connection,
                             TelemetryData* data = nullptr,
                             std::function<double()> clock = nullptr);

    /// The latest telemetry snapshot.
    TelemetryData& data() { return *_data; }
    const TelemetryData& data() const { return *_data; }

    /// Receive and process one message; return false on timeout / queue empty.
    bool read_once(int timeout_ms = 1000);

    /// Update ``connected`` based on the last heartbeat time.
    bool refresh_connection_status(double heartbeat_timeout_s);

    /// Process a single MAVLink message (useful for testing).
    bool process_message(const mavlink_message_t& message);

private:
    // Internal helpers
    void handle_heartbeat(const mavlink_message_t& msg, double now);
    void handle_battery_status(const mavlink_message_t& msg, double now);
    void handle_sys_status(const mavlink_message_t& msg, double now);
    void handle_attitude(const mavlink_message_t& msg, double now);
    void handle_altitude(const mavlink_message_t& msg, double now);
    void handle_local_position_ned(const mavlink_message_t& msg, double now);
    void handle_estimator_status(const mavlink_message_t& msg, double now);
    void handle_vibration(const mavlink_message_t& msg, double now);
    void handle_extended_sys_state(const mavlink_message_t& msg, double now);

    using HandlerFunc = void (TelemetryReader::*)(const mavlink_message_t&, double);

    PixhawkConnection& _connection;
    TelemetryData* _data;
    TelemetryData _owned_data;  // used when no external data is provided
    std::function<double()> _clock;
    std::unordered_map<uint32_t, HandlerFunc> _handlers;
};

}  // namespace telemetry

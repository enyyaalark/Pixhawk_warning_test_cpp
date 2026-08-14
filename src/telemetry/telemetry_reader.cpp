/// MAVLink message reader — converts raw messages to normalized telemetry.

#include "telemetry_reader.hpp"

#include <cmath>
#include <ctime>
#include <stdexcept>

#include "utils/chrono_utils.hpp"

// MAVLink common message definitions
#ifndef MAVLINK_HELPER
#define MAVLINK_HELPER static inline
#endif
extern "C" {
#include <common/mavlink.h>
#include <mavlink_helpers.h>
}

namespace telemetry {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

std::optional<double> finite(double value) {
    if (!std::isfinite(value)) {
        return std::nullopt;
    }
    return value;
}

// ---------------------------------------------------------------------------
// Sensor bitmask
// ---------------------------------------------------------------------------

const std::unordered_map<uint32_t, std::string> SENSOR_BITS = {
    {1U << 0,  "gyro_3d"},
    {1U << 1,  "accelerometer_3d"},
    {1U << 2,  "magnetometer_3d"},
    {1U << 3,  "absolute_pressure"},
    {1U << 4,  "differential_pressure"},
    {1U << 5,  "gps"},
    {1U << 6,  "optical_flow"},
    {1U << 7,  "computer_vision_position"},
    {1U << 8,  "laser_position"},
    {1U << 9,  "external_ground_truth"},
    {1U << 10, "angular_rate_control"},
    {1U << 11, "attitude_stabilization"},
    {1U << 12, "yaw_position"},
    {1U << 13, "z_altitude_control"},
    {1U << 14, "xy_position_control"},
    {1U << 15, "motor_outputs"},
    {1U << 16, "rc_receiver"},
    {1U << 17, "gyro_2_3d"},
    {1U << 18, "accelerometer_2_3d"},
    {1U << 19, "magnetometer_2_3d"},
    {1U << 25, "battery"},
    {1U << 26, "proximity"},
    {1U << 27, "satcom"},
    {1U << 30, "propulsion"},
};

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

TelemetryReader::TelemetryReader(PixhawkConnection& connection,
                                 TelemetryData* data,
                                 std::function<double()> clock)
    : _connection(connection),
      _data(data ? data : &_owned_data),
      _clock(clock ? clock : utils::steady_seconds) {
    // Register handlers for the 9 V1 message types
    _handlers[MAVLINK_MSG_ID_HEARTBEAT]          = &TelemetryReader::handle_heartbeat;
    _handlers[MAVLINK_MSG_ID_BATTERY_STATUS]     = &TelemetryReader::handle_battery_status;
    _handlers[MAVLINK_MSG_ID_SYS_STATUS]         = &TelemetryReader::handle_sys_status;
    _handlers[MAVLINK_MSG_ID_ATTITUDE]           = &TelemetryReader::handle_attitude;
    _handlers[MAVLINK_MSG_ID_ALTITUDE]           = &TelemetryReader::handle_altitude;
    _handlers[MAVLINK_MSG_ID_LOCAL_POSITION_NED] = &TelemetryReader::handle_local_position_ned;
    _handlers[MAVLINK_MSG_ID_ESTIMATOR_STATUS]   = &TelemetryReader::handle_estimator_status;
    _handlers[MAVLINK_MSG_ID_VIBRATION]          = &TelemetryReader::handle_vibration;
    _handlers[MAVLINK_MSG_ID_EXTENDED_SYS_STATE] = &TelemetryReader::handle_extended_sys_state;
}

// ---------------------------------------------------------------------------
// Public interface
// ---------------------------------------------------------------------------

bool TelemetryReader::read_once(int timeout_ms) {
    auto maybe_msg = _connection.receive_message(timeout_ms);
    if (!maybe_msg.has_value()) {
        return false;
    }
    return process_message(maybe_msg.value());
}

bool TelemetryReader::refresh_connection_status(double heartbeat_timeout_s) {
    if (heartbeat_timeout_s <= 0) {
        throw std::invalid_argument("heartbeat_timeout_s must be positive");
    }

    auto& conn = _data->connection;
    double now = _clock();
    if (conn.last_heartbeat_monotonic_s.has_value()) {
        conn.connected = (now - conn.last_heartbeat_monotonic_s.value()) <= heartbeat_timeout_s;
    } else {
        conn.connected = false;
    }
    return conn.connected;
}

bool TelemetryReader::process_message(const mavlink_message_t& message) {
    auto it = _handlers.find(message.msgid);
    if (it == _handlers.end()) {
        return false;
    }
    double now = _clock();
    (this->*(it->second))(message, now);
    _data->last_message_monotonic_s = now;
    return true;
}

// ---------------------------------------------------------------------------
// Message handlers
// ---------------------------------------------------------------------------

void TelemetryReader::handle_heartbeat(const mavlink_message_t& msg, double now) {
    mavlink_heartbeat_t heartbeat;
    mavlink_msg_heartbeat_decode(&msg, &heartbeat);

    auto& conn = _data->connection;
    conn.connected = true;
    conn.last_heartbeat_monotonic_s = now;
    conn.autopilot_type = static_cast<int>(heartbeat.autopilot);
    conn.vehicle_type = static_cast<int>(heartbeat.type);
    conn.system_status = static_cast<int>(heartbeat.system_status);
}

void TelemetryReader::handle_battery_status(const mavlink_message_t& msg, double now) {
    mavlink_battery_status_t battery;
    mavlink_msg_battery_status_decode(&msg, &battery);

    auto& b = _data->battery;
    b.last_update_monotonic_s = now;
    b.battery_id = static_cast<int>(battery.id);

    // Cell voltages: sum non-sentinel values, convert mV → V
    double total_v = 0.0;
    int valid_cells = 0;
    for (int i = 0; i < 10; ++i) {
        uint16_t v = battery.voltages[i];
        if (v > 0 && v < 65535) {
            total_v += v;
            valid_cells++;
        }
    }
    b.voltage_v = (valid_cells > 0) ? std::optional(total_v / 1000.0) : std::nullopt;

    // Current: sentinel is -1 (stored as int16_t, -1 = 0xFFFF)
    b.current_a = (battery.current_battery != -1)
                      ? std::optional(static_cast<double>(battery.current_battery) / 100.0)
                      : std::nullopt;

    // Remaining percent: sentinel is -1 or > 100
    int remaining = static_cast<int>(battery.battery_remaining);
    b.remaining_percent = (remaining >= 0 && remaining <= 100)
                              ? std::optional(remaining)
                              : std::nullopt;

    // Temperature: sentinel is INT16_MAX (32767)
    b.temperature_c = (battery.temperature != 32767)
                          ? std::optional(static_cast<double>(battery.temperature) / 100.0)
                          : std::nullopt;
}

void TelemetryReader::handle_sys_status(const mavlink_message_t& msg, double now) {
    mavlink_sys_status_t sys;
    mavlink_msg_sys_status_decode(&msg, &sys);

    auto& s = _data->system;
    s.last_update_monotonic_s = now;

    // Load: raw value / 10 = percent; valid range 0..1000
    int load = static_cast<int>(sys.load);
    s.load_percent = (load >= 0 && load <= 1000)
                         ? std::optional(static_cast<double>(load) / 10.0)
                         : std::nullopt;

    // Comm drop rate: / 100 = percent; valid range 0..10000
    int drop = static_cast<int>(sys.drop_rate_comm);
    s.communication_drop_percent = (drop >= 0 && drop <= 10000)
                                       ? std::optional(static_cast<double>(drop) / 100.0)
                                       : std::nullopt;

    s.communication_errors = static_cast<int>(sys.errors_comm);

    // Sensor bitmasks
    uint32_t present = static_cast<uint32_t>(sys.onboard_control_sensors_present);
    uint32_t enabled = static_cast<uint32_t>(sys.onboard_control_sensors_enabled);
    uint32_t healthy = static_cast<uint32_t>(sys.onboard_control_sensors_health);

    s.sensors.clear();
    for (const auto& [bit, name] : SENSOR_BITS) {
        if (present & bit) {
            s.sensors[name] = SensorState{
                /*present=*/true,
                /*enabled=*/(enabled & bit) != 0,
                /*healthy=*/(healthy & bit) != 0,
            };
        }
    }
}

void TelemetryReader::handle_attitude(const mavlink_message_t& msg, double now) {
    mavlink_attitude_t att;
    mavlink_msg_attitude_decode(&msg, &att);

    auto& a = _data->attitude;
    a.last_update_monotonic_s = now;
    a.roll_rad = finite(static_cast<double>(att.roll));
    a.pitch_rad = finite(static_cast<double>(att.pitch));
    a.yaw_rad = finite(static_cast<double>(att.yaw));
    a.roll_rate_rad_s = finite(static_cast<double>(att.rollspeed));
    a.pitch_rate_rad_s = finite(static_cast<double>(att.pitchspeed));
    a.yaw_rate_rad_s = finite(static_cast<double>(att.yawspeed));
}

void TelemetryReader::handle_altitude(const mavlink_message_t& msg, double now) {
    mavlink_altitude_t alt;
    mavlink_msg_altitude_decode(&msg, &alt);

    auto& m = _data->motion;
    m.altitude_last_update_monotonic_s = now;
    m.relative_altitude_m = finite(static_cast<double>(alt.altitude_relative));
    m.local_altitude_m = finite(static_cast<double>(alt.altitude_local));
}

void TelemetryReader::handle_local_position_ned(const mavlink_message_t& msg, double now) {
    mavlink_local_position_ned_t pos;
    mavlink_msg_local_position_ned_decode(&msg, &pos);

    auto& m = _data->motion;
    m.local_position_last_update_monotonic_s = now;
    m.velocity_north_m_s = finite(static_cast<double>(pos.vx));
    m.velocity_east_m_s = finite(static_cast<double>(pos.vy));
    m.velocity_down_m_s = finite(static_cast<double>(pos.vz));
}

void TelemetryReader::handle_estimator_status(const mavlink_message_t& msg, double now) {
    mavlink_estimator_status_t est;
    mavlink_msg_estimator_status_decode(&msg, &est);

    auto& e = _data->estimator;
    e.last_update_monotonic_s = now;
    e.flags = static_cast<int>(est.flags);
    e.velocity_ratio = finite(static_cast<double>(est.vel_ratio));
    e.horizontal_position_ratio = finite(static_cast<double>(est.pos_horiz_ratio));
    e.vertical_position_ratio = finite(static_cast<double>(est.pos_vert_ratio));
    e.magnetometer_ratio = finite(static_cast<double>(est.mag_ratio));
    e.horizontal_accuracy_m = finite(static_cast<double>(est.pos_horiz_accuracy));
    e.vertical_accuracy_m = finite(static_cast<double>(est.pos_vert_accuracy));
}

void TelemetryReader::handle_vibration(const mavlink_message_t& msg, double now) {
    mavlink_vibration_t vib;
    mavlink_msg_vibration_decode(&msg, &vib);

    auto& v = _data->vibration;
    v.last_update_monotonic_s = now;
    v.x_m_s2 = finite(static_cast<double>(vib.vibration_x));
    v.y_m_s2 = finite(static_cast<double>(vib.vibration_y));
    v.z_m_s2 = finite(static_cast<double>(vib.vibration_z));
    v.clipping_0 = static_cast<int>(vib.clipping_0);
    v.clipping_1 = static_cast<int>(vib.clipping_1);
    v.clipping_2 = static_cast<int>(vib.clipping_2);
}

void TelemetryReader::handle_extended_sys_state(const mavlink_message_t& msg, double now) {
    mavlink_extended_sys_state_t state;
    mavlink_msg_extended_sys_state_decode(&msg, &state);

    _data->flight_state.last_update_monotonic_s = now;
    _data->flight_state.landed_state = static_cast<int>(state.landed_state);
}

}  // namespace telemetry

#pragma once

/// Normalized telemetry types used by the application.
///
/// These structs deliberately contain no MAVLink objects or warning
/// decisions.  ``std::nullopt`` means that a value is unavailable or has
/// not been received yet (equivalent to ``None`` in the Python version).

#include <cstdint>
#include <map>
#include <optional>
#include <string>

namespace telemetry {

struct ConnectionTelemetry {
    bool connected{false};
    std::optional<double> last_heartbeat_monotonic_s;
    std::optional<int> autopilot_type;
    std::optional<int> vehicle_type;
    std::optional<int> system_status;
};

struct BatteryTelemetry {
    std::optional<int> battery_id;
    std::optional<double> voltage_v;
    std::optional<double> current_a;
    std::optional<int> remaining_percent;
    std::optional<double> temperature_c;
};

struct SensorState {
    bool present{false};
    bool enabled{false};
    bool healthy{false};
};

struct SystemTelemetry {
    std::optional<double> load_percent;
    std::optional<double> communication_drop_percent;
    std::optional<int> communication_errors;
    std::map<std::string, SensorState> sensors;
};

struct AttitudeTelemetry {
    std::optional<double> roll_rad;
    std::optional<double> pitch_rad;
    std::optional<double> yaw_rad;
    std::optional<double> roll_rate_rad_s;
    std::optional<double> pitch_rate_rad_s;
    std::optional<double> yaw_rate_rad_s;
};

struct MotionTelemetry {
    std::optional<double> relative_altitude_m;
    std::optional<double> local_altitude_m;
    std::optional<double> velocity_north_m_s;
    std::optional<double> velocity_east_m_s;
    std::optional<double> velocity_down_m_s;
};

struct EstimatorTelemetry {
    std::optional<int> flags;
    std::optional<double> velocity_ratio;
    std::optional<double> horizontal_position_ratio;
    std::optional<double> vertical_position_ratio;
    std::optional<double> magnetometer_ratio;
    std::optional<double> horizontal_accuracy_m;
    std::optional<double> vertical_accuracy_m;
};

struct VibrationTelemetry {
    std::optional<double> x_m_s2;
    std::optional<double> y_m_s2;
    std::optional<double> z_m_s2;
    std::optional<int> clipping_0;
    std::optional<int> clipping_1;
    std::optional<int> clipping_2;
};

struct FlightStateTelemetry {
    std::optional<int> landed_state;
};

/// Latest normalized snapshot of the selected V1 telemetry.
struct TelemetryData {
    std::optional<double> last_message_monotonic_s;
    ConnectionTelemetry connection;
    BatteryTelemetry battery;
    SystemTelemetry system;
    AttitudeTelemetry attitude;
    MotionTelemetry motion;
    EstimatorTelemetry estimator;
    VibrationTelemetry vibration;
    FlightStateTelemetry flight_state;
};

}  // namespace telemetry

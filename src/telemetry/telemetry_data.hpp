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

/// Whether a telemetry source has updated within *max_age_s*.
///
/// A missing timestamp, a negative age (incompatible clock), or an invalid
/// maximum age is never considered fresh.
inline bool is_fresh(const std::optional<double>& last_update_monotonic_s,
                     double now_monotonic_s,
                     double max_age_s) {
    if (!last_update_monotonic_s.has_value() || max_age_s < 0.0 ||
        now_monotonic_s < last_update_monotonic_s.value()) {
        return false;
    }
    return now_monotonic_s - last_update_monotonic_s.value() <= max_age_s;
}

struct ConnectionTelemetry {
    bool connected{false};
    std::optional<double> last_heartbeat_monotonic_s;
    std::optional<int> autopilot_type;
    std::optional<int> vehicle_type;
    std::optional<int> system_status;

    bool is_fresh(double now_monotonic_s, double max_age_s) const {
        return telemetry::is_fresh(last_heartbeat_monotonic_s,
                                   now_monotonic_s, max_age_s);
    }
};

struct BatteryTelemetry {
    std::optional<double> last_update_monotonic_s;
    std::optional<int> battery_id;
    std::optional<double> voltage_v;
    std::optional<double> current_a;
    std::optional<int> remaining_percent;
    std::optional<double> temperature_c;

    bool is_fresh(double now_monotonic_s, double max_age_s) const {
        return telemetry::is_fresh(last_update_monotonic_s,
                                   now_monotonic_s, max_age_s);
    }
};

struct SensorState {
    bool present{false};
    bool enabled{false};
    bool healthy{false};
};

struct SystemTelemetry {
    std::optional<double> last_update_monotonic_s;
    std::optional<double> load_percent;
    std::optional<double> communication_drop_percent;
    std::optional<int> communication_errors;
    std::map<std::string, SensorState> sensors;

    bool is_fresh(double now_monotonic_s, double max_age_s) const {
        return telemetry::is_fresh(last_update_monotonic_s,
                                   now_monotonic_s, max_age_s);
    }
};

struct AttitudeTelemetry {
    std::optional<double> last_update_monotonic_s;
    std::optional<double> roll_rad;
    std::optional<double> pitch_rad;
    std::optional<double> yaw_rad;
    std::optional<double> roll_rate_rad_s;
    std::optional<double> pitch_rate_rad_s;
    std::optional<double> yaw_rate_rad_s;

    bool is_fresh(double now_monotonic_s, double max_age_s) const {
        return telemetry::is_fresh(last_update_monotonic_s,
                                   now_monotonic_s, max_age_s);
    }
};

struct MotionTelemetry {
    std::optional<double> altitude_last_update_monotonic_s;
    std::optional<double> local_position_last_update_monotonic_s;
    std::optional<double> relative_altitude_m;
    std::optional<double> local_altitude_m;
    std::optional<double> velocity_north_m_s;
    std::optional<double> velocity_east_m_s;
    std::optional<double> velocity_down_m_s;

    bool altitude_is_fresh(double now_monotonic_s, double max_age_s) const {
        return telemetry::is_fresh(altitude_last_update_monotonic_s,
                                   now_monotonic_s, max_age_s);
    }

    bool local_position_is_fresh(double now_monotonic_s,
                                 double max_age_s) const {
        return telemetry::is_fresh(local_position_last_update_monotonic_s,
                                   now_monotonic_s, max_age_s);
    }
};

struct EstimatorTelemetry {
    std::optional<double> last_update_monotonic_s;
    std::optional<int> flags;
    std::optional<double> velocity_ratio;
    std::optional<double> horizontal_position_ratio;
    std::optional<double> vertical_position_ratio;
    std::optional<double> magnetometer_ratio;
    std::optional<double> horizontal_accuracy_m;
    std::optional<double> vertical_accuracy_m;

    bool is_fresh(double now_monotonic_s, double max_age_s) const {
        return telemetry::is_fresh(last_update_monotonic_s,
                                   now_monotonic_s, max_age_s);
    }
};

struct VibrationTelemetry {
    std::optional<double> last_update_monotonic_s;
    std::optional<double> x_m_s2;
    std::optional<double> y_m_s2;
    std::optional<double> z_m_s2;
    std::optional<int> clipping_0;
    std::optional<int> clipping_1;
    std::optional<int> clipping_2;

    bool is_fresh(double now_monotonic_s, double max_age_s) const {
        return telemetry::is_fresh(last_update_monotonic_s,
                                   now_monotonic_s, max_age_s);
    }
};

struct FlightStateTelemetry {
    std::optional<double> last_update_monotonic_s;
    std::optional<int> landed_state;

    bool is_fresh(double now_monotonic_s, double max_age_s) const {
        return telemetry::is_fresh(last_update_monotonic_s,
                                   now_monotonic_s, max_age_s);
    }
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

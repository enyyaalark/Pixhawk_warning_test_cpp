/// Replay normalized telemetry recordings (implementation).

#include "telemetry_replay.hpp"
#include "telemetry_recorder.hpp"  // for SCHEMA_VERSION

#include <cmath>
#include <fstream>
#include <limits>
#include <nlohmann/json.hpp>
#include <thread>

#include "utils/chrono_utils.hpp"

namespace telemetry {

using json = nlohmann::json;

// ---------------------------------------------------------------------------
// JSON deserialization helpers
// ---------------------------------------------------------------------------

template <typename T>
static void get_optional(const json& j, const char* key, std::optional<T>& out) {
    auto it = j.find(key);
    if (it == j.end() || it->is_null()) {
        out = std::nullopt;
        return;
    }
    out = it->get<T>();
}

static SensorState sensor_state_from_json(const json& j) {
    return SensorState{
        j.at("present").get<bool>(),
        j.at("enabled").get<bool>(),
        j.at("healthy").get<bool>(),
    };
}

static ConnectionTelemetry connection_from_json(const json& j) {
    ConnectionTelemetry c;
    c.connected = j.at("connected").get<bool>();
    get_optional(j, "last_heartbeat_monotonic_s", c.last_heartbeat_monotonic_s);
    get_optional(j, "autopilot_type", c.autopilot_type);
    get_optional(j, "vehicle_type", c.vehicle_type);
    get_optional(j, "system_status", c.system_status);
    return c;
}

static BatteryTelemetry battery_from_json(const json& j) {
    BatteryTelemetry b;
    get_optional(j, "last_update_monotonic_s", b.last_update_monotonic_s);
    get_optional(j, "battery_id", b.battery_id);
    get_optional(j, "voltage_v", b.voltage_v);
    get_optional(j, "current_a", b.current_a);
    get_optional(j, "remaining_percent", b.remaining_percent);
    get_optional(j, "temperature_c", b.temperature_c);
    return b;
}

static SystemTelemetry system_from_json(const json& j) {
    SystemTelemetry s;
    get_optional(j, "last_update_monotonic_s", s.last_update_monotonic_s);
    get_optional(j, "load_percent", s.load_percent);
    get_optional(j, "communication_drop_percent", s.communication_drop_percent);
    get_optional(j, "communication_errors", s.communication_errors);
    if (j.contains("sensors") && j["sensors"].is_object()) {
        for (const auto& [name, state_json] : j["sensors"].items()) {
            s.sensors[name] = sensor_state_from_json(state_json);
        }
    }
    return s;
}

static AttitudeTelemetry attitude_from_json(const json& j) {
    AttitudeTelemetry a;
    get_optional(j, "last_update_monotonic_s", a.last_update_monotonic_s);
    get_optional(j, "roll_rad", a.roll_rad);
    get_optional(j, "pitch_rad", a.pitch_rad);
    get_optional(j, "yaw_rad", a.yaw_rad);
    get_optional(j, "roll_rate_rad_s", a.roll_rate_rad_s);
    get_optional(j, "pitch_rate_rad_s", a.pitch_rate_rad_s);
    get_optional(j, "yaw_rate_rad_s", a.yaw_rate_rad_s);
    return a;
}

static MotionTelemetry motion_from_json(const json& j) {
    MotionTelemetry m;
    get_optional(j, "altitude_last_update_monotonic_s",
                 m.altitude_last_update_monotonic_s);
    get_optional(j, "local_position_last_update_monotonic_s",
                 m.local_position_last_update_monotonic_s);
    get_optional(j, "relative_altitude_m", m.relative_altitude_m);
    get_optional(j, "local_altitude_m", m.local_altitude_m);
    get_optional(j, "velocity_north_m_s", m.velocity_north_m_s);
    get_optional(j, "velocity_east_m_s", m.velocity_east_m_s);
    get_optional(j, "velocity_down_m_s", m.velocity_down_m_s);
    return m;
}

static EstimatorTelemetry estimator_from_json(const json& j) {
    EstimatorTelemetry e;
    get_optional(j, "last_update_monotonic_s", e.last_update_monotonic_s);
    get_optional(j, "flags", e.flags);
    get_optional(j, "velocity_ratio", e.velocity_ratio);
    get_optional(j, "horizontal_position_ratio", e.horizontal_position_ratio);
    get_optional(j, "vertical_position_ratio", e.vertical_position_ratio);
    get_optional(j, "magnetometer_ratio", e.magnetometer_ratio);
    get_optional(j, "horizontal_accuracy_m", e.horizontal_accuracy_m);
    get_optional(j, "vertical_accuracy_m", e.vertical_accuracy_m);
    return e;
}

static VibrationTelemetry vibration_from_json(const json& j) {
    VibrationTelemetry v;
    get_optional(j, "last_update_monotonic_s", v.last_update_monotonic_s);
    get_optional(j, "x_m_s2", v.x_m_s2);
    get_optional(j, "y_m_s2", v.y_m_s2);
    get_optional(j, "z_m_s2", v.z_m_s2);
    get_optional(j, "clipping_0", v.clipping_0);
    get_optional(j, "clipping_1", v.clipping_1);
    get_optional(j, "clipping_2", v.clipping_2);
    return v;
}

static FlightStateTelemetry flight_state_from_json(const json& j) {
    FlightStateTelemetry f;
    get_optional(j, "last_update_monotonic_s", f.last_update_monotonic_s);
    get_optional(j, "landed_state", f.landed_state);
    return f;
}

static TelemetryData telemetry_from_json(const json& j) {
    TelemetryData d;
    get_optional(j, "last_message_monotonic_s", d.last_message_monotonic_s);
    d.connection = connection_from_json(j.at("connection"));
    d.battery = battery_from_json(j.at("battery"));
    d.system = system_from_json(j.at("system"));
    d.attitude = attitude_from_json(j.at("attitude"));
    d.motion = motion_from_json(j.at("motion"));
    d.estimator = estimator_from_json(j.at("estimator"));
    d.vibration = vibration_from_json(j.at("vibration"));
    d.flight_state = flight_state_from_json(j.at("flight_state"));
    return d;
}

// ---------------------------------------------------------------------------
// TelemetryReplay implementation
// ---------------------------------------------------------------------------

TelemetryReplay::TelemetryReplay(std::filesystem::path path,
                                 std::function<void(double)> sleep_fn)
    : _path(std::move(path)), _sleep(std::move(sleep_fn)) {}

std::vector<ReplaySample> TelemetryReplay::samples() {
    std::vector<ReplaySample> result;
    std::ifstream file(_path);
    if (!file) {
        throw TelemetryReplayError("Could not open telemetry recording " +
                                   _path.string());
    }

    std::string line;
    int line_number = 0;
    while (std::getline(file, line)) {
        line_number++;
        if (line.empty()) {
            continue;
        }

        json record;
        try {
            record = json::parse(line);
        } catch (const json::parse_error& e) {
            throw TelemetryReplayError(
                "Invalid telemetry recording " + _path.string() +
                ", line " + std::to_string(line_number) + ": " + e.what());
        }

        if (!record.is_object()) {
            throw TelemetryReplayError(
                "Invalid telemetry recording " + _path.string() +
                ", line " + std::to_string(line_number) + ": record must be an object");
        }

        if (record.value("schema_version", 0) != SCHEMA_VERSION) {
            throw TelemetryReplayError(
                "Invalid telemetry recording " + _path.string() +
                ", line " + std::to_string(line_number) +
                ": unsupported schema_version " +
                std::to_string(record.value("schema_version", 0)));
        }

        auto recorded_at = utils::parse_iso8601(record.at("recorded_at").get<std::string>());

        ReplaySample sample;
        sample.recorded_at = recorded_at;
        get_optional(record, "recorded_monotonic_s", sample.recorded_monotonic_s);
        sample.telemetry = telemetry_from_json(record.at("telemetry"));
        result.push_back(std::move(sample));
    }

    return result;
}

std::vector<ReplaySample> TelemetryReplay::replay(double speed) {
    if (speed <= 0 || std::isnan(speed)) {
        throw std::invalid_argument("speed must be positive");
    }

    auto all_samples = samples();
    std::chrono::system_clock::time_point previous_time;

    for (size_t i = 0; i < all_samples.size(); ++i) {
        if (i > 0) {
            auto recorded_delay = all_samples[i].recorded_at - all_samples[i - 1].recorded_at;
            double delay_s = std::chrono::duration<double>(recorded_delay).count();
            if (delay_s < 0) {
                throw TelemetryReplayError(
                    "Recording timestamps must be in chronological order");
            }
            if (!std::isinf(speed) && delay_s > 0) {
                double scaled_delay = delay_s / speed;
                _sleep(scaled_delay);
            }
        }
    }

    return all_samples;
}

}  // namespace telemetry

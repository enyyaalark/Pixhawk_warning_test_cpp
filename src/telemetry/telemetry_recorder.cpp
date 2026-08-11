/// Record normalized telemetry snapshots as JSON Lines.

#include "telemetry_recorder.hpp"

#include <cmath>
#include <nlohmann/json.hpp>

#include "utils/chrono_utils.hpp"

namespace telemetry {

using json = nlohmann::json;

// ---------------------------------------------------------------------------
// JSON serialization helpers (ADL-based to_json)
// ---------------------------------------------------------------------------

// Forward declarations
static json to_json(const SensorState& s);
static json to_json(const ConnectionTelemetry& c);
static json to_json(const BatteryTelemetry& b);
static json to_json(const SystemTelemetry& s);
static json to_json(const AttitudeTelemetry& a);
static json to_json(const MotionTelemetry& m);
static json to_json(const EstimatorTelemetry& e);
static json to_json(const VibrationTelemetry& v);
static json to_json(const FlightStateTelemetry& f);
static json to_json(const TelemetryData& d);

// Helper: write optional<T> as null if empty
template <typename T>
static void set_optional(json& j, const char* key, const std::optional<T>& opt) {
    if (opt.has_value()) {
        j[key] = opt.value();
    } else {
        j[key] = nullptr;
    }
}

static json to_json(const SensorState& s) {
    return json{{"present", s.present},
                {"enabled", s.enabled},
                {"healthy", s.healthy}};
}

static json to_json(const ConnectionTelemetry& c) {
    json j;
    j["connected"] = c.connected;
    set_optional(j, "last_heartbeat_monotonic_s", c.last_heartbeat_monotonic_s);
    set_optional(j, "autopilot_type", c.autopilot_type);
    set_optional(j, "vehicle_type", c.vehicle_type);
    set_optional(j, "system_status", c.system_status);
    return j;
}

static json to_json(const BatteryTelemetry& b) {
    json j;
    set_optional(j, "battery_id", b.battery_id);
    set_optional(j, "voltage_v", b.voltage_v);
    set_optional(j, "current_a", b.current_a);
    set_optional(j, "remaining_percent", b.remaining_percent);
    set_optional(j, "temperature_c", b.temperature_c);
    return j;
}

static json to_json(const SystemTelemetry& s) {
    json j;
    set_optional(j, "load_percent", s.load_percent);
    set_optional(j, "communication_drop_percent", s.communication_drop_percent);
    set_optional(j, "communication_errors", s.communication_errors);
    json sensors_obj = json::object();
    for (const auto& [name, state] : s.sensors) {
        sensors_obj[name] = to_json(state);
    }
    j["sensors"] = sensors_obj;
    return j;
}

static json to_json(const AttitudeTelemetry& a) {
    json j;
    set_optional(j, "roll_rad", a.roll_rad);
    set_optional(j, "pitch_rad", a.pitch_rad);
    set_optional(j, "yaw_rad", a.yaw_rad);
    set_optional(j, "roll_rate_rad_s", a.roll_rate_rad_s);
    set_optional(j, "pitch_rate_rad_s", a.pitch_rate_rad_s);
    set_optional(j, "yaw_rate_rad_s", a.yaw_rate_rad_s);
    return j;
}

static json to_json(const MotionTelemetry& m) {
    json j;
    set_optional(j, "relative_altitude_m", m.relative_altitude_m);
    set_optional(j, "local_altitude_m", m.local_altitude_m);
    set_optional(j, "velocity_north_m_s", m.velocity_north_m_s);
    set_optional(j, "velocity_east_m_s", m.velocity_east_m_s);
    set_optional(j, "velocity_down_m_s", m.velocity_down_m_s);
    return j;
}

static json to_json(const EstimatorTelemetry& e) {
    json j;
    set_optional(j, "flags", e.flags);
    set_optional(j, "velocity_ratio", e.velocity_ratio);
    set_optional(j, "horizontal_position_ratio", e.horizontal_position_ratio);
    set_optional(j, "vertical_position_ratio", e.vertical_position_ratio);
    set_optional(j, "magnetometer_ratio", e.magnetometer_ratio);
    set_optional(j, "horizontal_accuracy_m", e.horizontal_accuracy_m);
    set_optional(j, "vertical_accuracy_m", e.vertical_accuracy_m);
    return j;
}

static json to_json(const VibrationTelemetry& v) {
    json j;
    set_optional(j, "x_m_s2", v.x_m_s2);
    set_optional(j, "y_m_s2", v.y_m_s2);
    set_optional(j, "z_m_s2", v.z_m_s2);
    set_optional(j, "clipping_0", v.clipping_0);
    set_optional(j, "clipping_1", v.clipping_1);
    set_optional(j, "clipping_2", v.clipping_2);
    return j;
}

static json to_json(const FlightStateTelemetry& f) {
    json j;
    set_optional(j, "landed_state", f.landed_state);
    return j;
}

static json to_json(const TelemetryData& d) {
    json j;
    set_optional(j, "last_message_monotonic_s", d.last_message_monotonic_s);
    j["connection"] = to_json(d.connection);
    j["battery"] = to_json(d.battery);
    j["system"] = to_json(d.system);
    j["attitude"] = to_json(d.attitude);
    j["motion"] = to_json(d.motion);
    j["estimator"] = to_json(d.estimator);
    j["vibration"] = to_json(d.vibration);
    j["flight_state"] = to_json(d.flight_state);
    return j;
}

// ---------------------------------------------------------------------------
// TelemetryRecorder implementation
// ---------------------------------------------------------------------------

TelemetryRecorder::TelemetryRecorder(
    std::filesystem::path path,
    double sample_rate_hz,
    std::function<double()> clock,
    std::function<std::chrono::system_clock::time_point()> utc_now)
    : _path(std::move(path)),
      _sample_period_s(1.0 / sample_rate_hz),
      _clock(clock ? clock : utils::steady_seconds),
      _utc_now(utc_now ? utc_now : []() {
          return std::chrono::system_clock::now();
      }) {
    if (sample_rate_hz <= 0) {
        throw std::invalid_argument("sample_rate_hz must be positive");
    }
}

TelemetryRecorder::~TelemetryRecorder() {
    close();
}

void TelemetryRecorder::open() {
    if (_file.is_open()) {
        return;
    }
    if (!_path.parent_path().empty()) {
        std::filesystem::create_directories(_path.parent_path());
    }
    _file.open(_path, std::ios::app);
    if (!_file) {
        throw TelemetryRecordingError("Could not open telemetry recording " +
                                      _path.string());
    }
}

bool TelemetryRecorder::record_if_due(const TelemetryData& data) {
    double now = _clock();
    if (_next_record_time.has_value() && now < _next_record_time.value()) {
        return false;
    }
    record(data);
    _next_record_time = now + _sample_period_s;
    return true;
}

void TelemetryRecorder::record(const TelemetryData& data) {
    if (!_file.is_open()) {
        throw TelemetryRecordingError("Telemetry recorder is not open");
    }

    json record;
    record["schema_version"] = SCHEMA_VERSION;
    record["recorded_at"] = utils::format_iso8601(_utc_now());
    record["telemetry"] = to_json(data);

    std::function<void(const json&)> reject_non_finite = [&](const json& value) {
        if (value.is_number_float() && !std::isfinite(value.get<double>())) {
            throw TelemetryRecordingError("Non-finite float value in telemetry data");
        }
        if (value.is_array() || value.is_object()) {
            for (const auto& child : value) reject_non_finite(child);
        }
    };
    reject_non_finite(record);

    std::string line = record.dump();
    _file << line << '\n';
    _file.flush();

    if (!_file) {
        throw TelemetryRecordingError("Could not write telemetry recording " +
                                      _path.string());
    }
}

void TelemetryRecorder::close() noexcept {
    if (_file.is_open()) {
        _file.close();
    }
}

}  // namespace telemetry

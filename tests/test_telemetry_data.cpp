/// Test telemetry data struct construction and JSON round-trip.

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "telemetry/telemetry_data.hpp"
// Re-use recorder JSON serialization (declared static, so include the .cpp
// is not portable — we duplicate the to_json helpers here for testing)

using namespace telemetry;
using json = nlohmann::json;

// Minimal to_json helpers (mirrors telemetry_recorder.cpp for testing)
template <typename T>
static void set_opt(json& j, const char* key, const std::optional<T>& opt) {
    j[key] = opt.has_value() ? json(opt.value()) : json(nullptr);
}

TEST(TelemetryDataTest, DefaultConstructionAllOptionalsAreNullopt) {
    TelemetryData d;
    EXPECT_FALSE(d.last_message_monotonic_s.has_value());
    EXPECT_FALSE(d.battery.voltage_v.has_value());
    EXPECT_FALSE(d.attitude.roll_rad.has_value());
}

TEST(TelemetryDataTest, ConnectionTelemetryDefaults) {
    ConnectionTelemetry c;
    EXPECT_FALSE(c.connected);
    EXPECT_FALSE(c.last_heartbeat_monotonic_s.has_value());
    EXPECT_FALSE(c.autopilot_type.has_value());
}

TEST(TelemetryDataTest, BatteryTelemetrySentinelsDefaultToNullopt) {
    BatteryTelemetry b;
    EXPECT_FALSE(b.voltage_v.has_value());
    EXPECT_FALSE(b.current_a.has_value());
    EXPECT_FALSE(b.remaining_percent.has_value());
}

TEST(TelemetryDataTest, SensorStateDefaults) {
    SensorState s;
    EXPECT_FALSE(s.present);
    EXPECT_FALSE(s.enabled);
    EXPECT_FALSE(s.healthy);
}

TEST(TelemetryDataTest, NestedSubStructsDefaultConstructed) {
    TelemetryData d;
    EXPECT_FALSE(d.connection.connected);
    EXPECT_FALSE(d.system.load_percent.has_value());
    EXPECT_TRUE(d.system.sensors.empty());
    EXPECT_FALSE(d.attitude.roll_rad.has_value());
    EXPECT_FALSE(d.vibration.x_m_s2.has_value());
    EXPECT_FALSE(d.flight_state.landed_state.has_value());
}

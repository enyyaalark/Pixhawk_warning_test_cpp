/// Port of Python test_warning_engine.py tests.

#include <gtest/gtest.h>
#include <chrono>
#include <ctime>

#include "warning/warning_engine.hpp"
#include "warning/warning_event.hpp"
#include "telemetry/telemetry_data.hpp"

using namespace warning;
using namespace telemetry;

class WarningEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        _timestamp = std::chrono::system_clock::from_time_t(
            std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
        _engine = std::make_unique<WarningEngine>(
            [this]() { return _timestamp; },
            nullptr,
            0.0,  // zero delays for fast testing
            0.0);
        _data = TelemetryData{};
    }

    std::chrono::system_clock::time_point _timestamp;
    std::unique_ptr<WarningEngine> _engine;
    TelemetryData _data;
};

TEST_F(WarningEngineTest, NoConnectionWarningBeforeFirstHeartbeat) {
    auto events = _engine->evaluate(_data);
    EXPECT_TRUE(events.empty());
}

TEST_F(WarningEngineTest, ConnectionLossAndRestoreEmitOneTransitionEach) {
    _data.connection.last_heartbeat_monotonic_s = 10.0;
    _data.connection.connected = true;
    EXPECT_TRUE(_engine->evaluate(_data).empty());

    _data.connection.connected = false;
    auto lost = _engine->evaluate(_data);
    ASSERT_EQ(lost.size(), 1);
    EXPECT_EQ(lost[0].code, "CONNECTION_LOST");
    EXPECT_EQ(lost[0].severity, Severity::CRITICAL);
    EXPECT_TRUE(lost[0].active);
    EXPECT_TRUE(_engine->evaluate(_data).empty());  // no repeat

    _data.connection.connected = true;
    auto restored = _engine->evaluate(_data);
    ASSERT_EQ(restored.size(), 1);
    EXPECT_EQ(restored[0].code, "CONNECTION_LOST");
    EXPECT_FALSE(restored[0].active);
}

TEST_F(WarningEngineTest, OnlyPresentEnabledUnhealthySensorActivates) {
    _data.system.sensors = {
        {"gyro_3d", SensorState{true, true, false}},
        {"gps", SensorState{true, false, false}},
        {"battery", SensorState{true, true, true}},
    };

    auto events = _engine->evaluate(_data);
    ASSERT_EQ(events.size(), 1);
    EXPECT_EQ(events[0].code, "SENSOR_UNHEALTHY_GYRO_3D");
    EXPECT_TRUE(events[0].active);
    EXPECT_TRUE(_engine->evaluate(_data).empty());  // no repeat
}

TEST_F(WarningEngineTest, SensorRecoveryEmitsClearEvent) {
    _data.system.sensors["gyro_3d"] = SensorState{true, true, false};
    _engine->evaluate(_data);

    _data.system.sensors["gyro_3d"] = SensorState{true, true, true};
    auto events = _engine->evaluate(_data);
    ASSERT_EQ(events.size(), 1);
    EXPECT_EQ(events[0].code, "SENSOR_UNHEALTHY_GYRO_3D");
    EXPECT_FALSE(events[0].active);
}

TEST_F(WarningEngineTest, ReplayCanSupplyOriginalEventTimestamp) {
    _data.system.sensors["gyro_3d"] = SensorState{true, true, false};

    auto event = _engine->evaluate(_data, _timestamp)[0];
    EXPECT_EQ(event.timestamp, _timestamp);
}

TEST_F(WarningEngineTest, ShortSensorFaultDoesNotActivate) {
    double now = 10.0;
    WarningEngine engine(nullptr,
                         [&now]() { return now; },
                         1.0,  // activation delay
                         1.0); // clear delay

    TelemetryData d;
    d.system.sensors["gyro_3d"] = SensorState{true, true, false};

    EXPECT_TRUE(engine.evaluate(d).empty());
    now = 10.9;
    EXPECT_TRUE(engine.evaluate(d).empty());
    // Fault clears before activation delay expires
    d.system.sensors["gyro_3d"] = SensorState{true, true, true};
    now = 11.0;
    EXPECT_TRUE(engine.evaluate(d).empty());
}

TEST_F(WarningEngineTest, SensorFaultAndRecoveryRequireStableDuration) {
    double now = 20.0;
    WarningEngine engine(nullptr,
                         [&now]() { return now; },
                         1.0,   // activation delay
                         0.5);  // clear delay

    TelemetryData d;
    d.system.sensors["gyro_3d"] = SensorState{true, true, false};
    EXPECT_TRUE(engine.evaluate(d).empty());

    now = 21.0;
    auto activated = engine.evaluate(d);
    ASSERT_EQ(activated.size(), 1);
    EXPECT_TRUE(activated[0].active);

    // Sensor recovers
    d.system.sensors["gyro_3d"] = SensorState{true, true, true};
    now = 21.4;
    EXPECT_TRUE(engine.evaluate(d).empty());  // not yet stable
    now = 21.9;
    auto cleared = engine.evaluate(d);
    ASSERT_EQ(cleared.size(), 1);
    EXPECT_FALSE(cleared[0].active);
}

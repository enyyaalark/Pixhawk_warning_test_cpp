/// Port of Python test_telemetry_reader.py tests.

#include <cmath>
#include <gtest/gtest.h>

#ifndef MAVLINK_HELPER
#define MAVLINK_HELPER static inline
#endif
extern "C" {
#include <common/mavlink.h>
#include <mavlink_helpers.h>
}

#include "telemetry/pixhawk_connection.hpp"
#include "telemetry/telemetry_reader.hpp"

using namespace telemetry;

// Fixture that creates a reader backed by a fake connection
class TelemetryReaderTest : public ::testing::Test {
protected:
    void SetUp() override {
        _data = TelemetryData{};
        _clock_value = 10.0;
        _reader = std::make_unique<TelemetryReader>(
            _connection, &_data,
            [this]() { return _clock_value; });
    }

    // Helper: push a message into the connection's queue and process it
    void push_and_process(const mavlink_message_t& msg) {
        // Hack: directly call process_message on the reader
        _reader->process_message(msg);
    }

    PixhawkConnection _connection{"/dev/fake", 115200};
    TelemetryData _data;
    double _clock_value;
    std::unique_ptr<TelemetryReader> _reader;
};

TEST_F(TelemetryReaderTest, InvalidBatterySentinelsBecomeNullopt) {
    mavlink_message_t msg;
    mavlink_battery_status_t battery{};
    battery.id = 0;
    for (int i = 0; i < 10; ++i) battery.voltages[i] = 65535;
    battery.current_battery = -1;
    battery.battery_remaining = -1;
    battery.temperature = 32767;
    mavlink_msg_battery_status_encode(1, 1, &msg, &battery);

    _reader->process_message(msg);

    EXPECT_FALSE(_data.battery.voltage_v.has_value());
    EXPECT_FALSE(_data.battery.current_a.has_value());
    EXPECT_FALSE(_data.battery.remaining_percent.has_value());
    EXPECT_FALSE(_data.battery.temperature_c.has_value());
}

TEST_F(TelemetryReaderTest, ValidBatteryValuesAreConvertedToSiUnits) {
    mavlink_message_t msg;
    mavlink_battery_status_t battery{};
    battery.id = 1;
    battery.voltages[0] = 4200;
    battery.voltages[1] = 4190;
    battery.voltages[2] = 4180;
    // rest are 0 (or 65535 = sentinel)
    battery.voltages[3] = 0;
    battery.voltages[4] = 65535;
    battery.current_battery = 253;    // 2.53 A
    battery.battery_remaining = 75;    // 75%
    battery.temperature = 2450;        // 24.5 C
    mavlink_msg_battery_status_encode(1, 1, &msg, &battery);

    _reader->process_message(msg);

    ASSERT_TRUE(_data.battery.voltage_v.has_value());
    EXPECT_NEAR(_data.battery.voltage_v.value(), 12.57, 0.01);
    ASSERT_TRUE(_data.battery.current_a.has_value());
    EXPECT_DOUBLE_EQ(_data.battery.current_a.value(), 2.53);
    ASSERT_TRUE(_data.battery.remaining_percent.has_value());
    EXPECT_EQ(_data.battery.remaining_percent.value(), 75);
    ASSERT_TRUE(_data.battery.temperature_c.has_value());
    EXPECT_DOUBLE_EQ(_data.battery.temperature_c.value(), 24.5);
}

TEST_F(TelemetryReaderTest, NanAttitudeIsUnavailable) {
    mavlink_message_t msg;
    mavlink_attitude_t att{};
    att.roll = NAN;
    att.pitch = 0.2f;
    att.yaw = 0.3f;
    att.rollspeed = 0.0f;
    att.pitchspeed = INFINITY;
    att.yawspeed = -0.1f;
    mavlink_msg_attitude_encode(1, 1, &msg, &att);

    _reader->process_message(msg);

    EXPECT_FALSE(_data.attitude.roll_rad.has_value());
    ASSERT_TRUE(_data.attitude.pitch_rad.has_value());
    EXPECT_NEAR(_data.attitude.pitch_rad.value(), 0.2, 1e-6);
    EXPECT_FALSE(_data.attitude.pitch_rate_rad_s.has_value());
}

TEST_F(TelemetryReaderTest, HeartbeatMarksConnectionAndRecordsTime) {
    _clock_value = 42.5;
    mavlink_message_t msg;
    mavlink_heartbeat_t hb{};
    hb.autopilot = 12;
    hb.type = 1;
    hb.system_status = 3;
    hb.base_mode = 81;
    hb.custom_mode = 0;
    hb.mavlink_version = 3;
    mavlink_msg_heartbeat_encode(1, 1, &msg, &hb);

    _reader->process_message(msg);

    EXPECT_TRUE(_data.connection.connected);
    ASSERT_TRUE(_data.connection.last_heartbeat_monotonic_s.has_value());
    EXPECT_DOUBLE_EQ(_data.connection.last_heartbeat_monotonic_s.value(), 42.5);
    ASSERT_TRUE(_data.connection.autopilot_type.has_value());
    EXPECT_EQ(_data.connection.autopilot_type.value(), 12);
    ASSERT_TRUE(_data.last_message_monotonic_s.has_value());
    EXPECT_DOUBLE_EQ(_data.last_message_monotonic_s.value(), 42.5);
}

TEST_F(TelemetryReaderTest, ConnectionBecomesFalseAfterHeartbeatTimeout) {
    // Send heartbeat at t=10.0
    _clock_value = 10.0;
    mavlink_message_t msg;
    mavlink_heartbeat_t hb{};
    hb.autopilot = 12;
    hb.type = 1;
    hb.system_status = 3;
    hb.base_mode = 81;
    hb.custom_mode = 0;
    hb.mavlink_version = 3;
    mavlink_msg_heartbeat_encode(1, 1, &msg, &hb);
    _reader->process_message(msg);

    // At 12.9 (within 3s timeout), still connected
    _clock_value = 12.9;
    EXPECT_TRUE(_reader->refresh_connection_status(3.0));

    // At 13.1 (past 3s timeout), disconnected
    _clock_value = 13.1;
    EXPECT_FALSE(_reader->refresh_connection_status(3.0));
}

TEST_F(TelemetryReaderTest, SensorMasksAreExposedAsNamedStates) {
    mavlink_message_t msg;
    mavlink_sys_status_t sys{};
    sys.load = 123;
    sys.drop_rate_comm = 25;
    sys.errors_comm = 2;
    sys.onboard_control_sensors_present = (1 << 0) | (1 << 5);
    sys.onboard_control_sensors_enabled = (1 << 0) | (1 << 5);
    sys.onboard_control_sensors_health = (1 << 0);  // only gyro healthy, gps unhealthy
    mavlink_msg_sys_status_encode(1, 1, &msg, &sys);

    _reader->process_message(msg);

    ASSERT_TRUE(_data.system.sensors.count("gyro_3d"));
    EXPECT_TRUE(_data.system.sensors["gyro_3d"].healthy);
    ASSERT_TRUE(_data.system.sensors.count("gps"));
    EXPECT_FALSE(_data.system.sensors["gps"].healthy);
    ASSERT_TRUE(_data.system.load_percent.has_value());
    EXPECT_DOUBLE_EQ(_data.system.load_percent.value(), 12.3);
}

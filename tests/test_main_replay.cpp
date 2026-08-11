/// Port of Python test_main_replay.py — end-to-end replay → warning test.

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "telemetry/telemetry_recorder.hpp"
#include "telemetry/telemetry_replay.hpp"
#include "warning/warning_engine.hpp"
#include "warning/warning_event.hpp"

using namespace telemetry;
using namespace warning;

class MainReplayTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto tmp = std::filesystem::temp_directory_path();
        std::string dirname = "pixhawk_main_" + std::to_string(std::rand());
        _tmpdir = tmp / dirname;
        std::filesystem::create_directories(_tmpdir);
    }

    void TearDown() override {
        std::filesystem::remove_all(_tmpdir);
    }

    std::filesystem::path _tmpdir;
};

TEST_F(MainReplayTest, ReplayFeedsSnapshotsToWarningOutput) {
    auto path = _tmpdir / "telemetry.jsonl";

    // Build two snapshots: connected then disconnected
    nlohmann::json connected_telemetry = {
        {"last_message_monotonic_s", nullptr},
        {"connection", {
            {"connected", true},
            {"last_heartbeat_monotonic_s", 10.0},
            {"autopilot_type", nullptr},
            {"vehicle_type", nullptr},
            {"system_status", nullptr}
        }},
        {"battery", {
            {"battery_id", nullptr}, {"voltage_v", nullptr},
            {"current_a", nullptr}, {"remaining_percent", nullptr},
            {"temperature_c", nullptr}
        }},
        {"system", {{"load_percent", nullptr},
                    {"communication_drop_percent", nullptr},
                    {"communication_errors", nullptr},
                    {"sensors", nlohmann::json::object()}}},
        {"attitude", {{"roll_rad", nullptr}, {"pitch_rad", nullptr},
                      {"yaw_rad", nullptr}, {"roll_rate_rad_s", nullptr},
                      {"pitch_rate_rad_s", nullptr}, {"yaw_rate_rad_s", nullptr}}},
        {"motion", {{"relative_altitude_m", nullptr}, {"local_altitude_m", nullptr},
                    {"velocity_north_m_s", nullptr}, {"velocity_east_m_s", nullptr},
                    {"velocity_down_m_s", nullptr}}},
        {"estimator", {{"flags", nullptr}, {"velocity_ratio", nullptr},
                       {"horizontal_position_ratio", nullptr},
                       {"vertical_position_ratio", nullptr},
                       {"magnetometer_ratio", nullptr},
                       {"horizontal_accuracy_m", nullptr},
                       {"vertical_accuracy_m", nullptr}}},
        {"vibration", {{"x_m_s2", nullptr}, {"y_m_s2", nullptr},
                       {"z_m_s2", nullptr}, {"clipping_0", nullptr},
                       {"clipping_1", nullptr}, {"clipping_2", nullptr}}},
        {"flight_state", {{"landed_state", nullptr}}}
    };

    nlohmann::json disconnected_telemetry = connected_telemetry;
    disconnected_telemetry["connection"]["connected"] = false;

    auto make_record = [](const std::string& ts, const nlohmann::json& tel) {
        nlohmann::json r;
        r["schema_version"] = SCHEMA_VERSION;
        r["recorded_at"] = ts;
        r["telemetry"] = tel;
        return r.dump();
    };

    std::ofstream file(path);
    file << make_record("2026-08-11T12:00:00Z", connected_telemetry) << '\n';
    file << make_record("2026-08-11T12:00:01Z", disconnected_telemetry) << '\n';
    file.close();

    // Replay with immediate speed
    WarningEngine engine(nullptr, nullptr, 0.0, 0.0);
    TelemetryReplay replay(path, [](double) {});  // no-op sleep
    auto samples = replay.replay(std::numeric_limits<double>::infinity());

    std::vector<WarningEvent> all_events;
    TelemetryData latest;
    for (const auto& sample : samples) {
        latest = sample.telemetry;
        auto events = engine.evaluate(
            latest,
            sample.recorded_at,
            static_cast<double>(
                std::chrono::system_clock::to_time_t(sample.recorded_at)));
        all_events.insert(all_events.end(), events.begin(), events.end());
    }

    // Should have one CONNECTION_LOST warning (not during startup — heartbeat was observed)
    ASSERT_FALSE(all_events.empty());
    EXPECT_EQ(all_events[0].code, "CONNECTION_LOST");
    EXPECT_TRUE(all_events[0].active);
}

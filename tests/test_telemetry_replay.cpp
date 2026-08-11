/// Port of Python test_telemetry_replay.py tests.

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "telemetry/telemetry_recorder.hpp"
#include "telemetry/telemetry_replay.hpp"

using namespace telemetry;

// Helper: construct a JSONL record line
static std::string make_record_line(const std::string& recorded_at,
                                    const TelemetryData& data) {
    // We need to reuse the to_json logic from the recorder.
    // For testing, build the JSON manually.

    nlohmann::json record;
    record["schema_version"] = SCHEMA_VERSION;
    record["recorded_at"] = recorded_at;

    nlohmann::json telemetry;
    telemetry["connection"] = {{"connected", false}};
    telemetry["battery"] = nlohmann::json::object();
    telemetry["system"] = {{"sensors", nlohmann::json::object()}};
    telemetry["attitude"] = nlohmann::json::object();
    telemetry["motion"] = nlohmann::json::object();
    telemetry["estimator"] = nlohmann::json::object();
    telemetry["vibration"] = nlohmann::json::object();
    telemetry["flight_state"] = nlohmann::json::object();

    // Add test-specific fields
    if (data.attitude.yaw_rad.has_value()) {
        telemetry["attitude"]["yaw_rad"] = data.attitude.yaw_rad.value();
    }

    record["telemetry"] = telemetry;
    return record.dump();
}

class TelemetryReplayTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto tmp = std::filesystem::temp_directory_path();
        std::string dirname = "pixhawk_replay_" + std::to_string(std::rand());
        _tmpdir = tmp / dirname;
        std::filesystem::create_directories(_tmpdir);
    }

    void TearDown() override {
        std::filesystem::remove_all(_tmpdir);
    }

    void write_lines(const std::filesystem::path& path,
                     const std::vector<std::string>& lines) {
        std::ofstream file(path);
        for (const auto& line : lines) {
            file << line << '\n';
        }
    }

    std::filesystem::path _tmpdir;
};

TEST_F(TelemetryReplayTest, SamplesReconstructNestedTelemetry) {
    auto path = _tmpdir / "replay.jsonl";

    TelemetryData expected;
    expected.attitude.yaw_rad = 1.25;

    write_lines(path, {make_record_line("2026-08-11T12:00:00Z", expected)});

    auto samples = TelemetryReplay(path).samples();
    ASSERT_EQ(samples.size(), 1);
    ASSERT_TRUE(samples[0].telemetry.attitude.yaw_rad.has_value());
    EXPECT_DOUBLE_EQ(samples[0].telemetry.attitude.yaw_rad.value(), 1.25);
}

TEST_F(TelemetryReplayTest, ReplayReproducesTimingAtSelectedSpeed) {
    auto path = _tmpdir / "replay.jsonl";

    TelemetryData d;
    write_lines(path, {
        make_record_line("2026-08-11T12:00:00Z", d),
        make_record_line("2026-08-11T12:00:02Z", d),
    });

    // Use a sleep counter instead of actual sleep
    double total_slept = 0.0;
    TelemetryReplay replay(path, [&total_slept](double s) { total_slept += s; });

    auto samples = replay.replay(2.0);
    EXPECT_EQ(samples.size(), 2);
    EXPECT_NEAR(total_slept, 1.0, 0.01);  // 2s / speed 2.0 = 1.0s
}

TEST_F(TelemetryReplayTest, ParsesPythonTimezoneAndFractionalSeconds) {
    auto path = _tmpdir / "python_time.jsonl";
    TelemetryData d;
    write_lines(path, {
        make_record_line("2026-08-11T12:00:00.250000+00:00", d),
        make_record_line("2026-08-11T12:00:00.750000+00:00", d),
    });

    double total_slept = 0.0;
    TelemetryReplay replay(path, [&total_slept](double s) { total_slept += s; });
    auto samples = replay.replay(1.0);

    ASSERT_EQ(samples.size(), 2);
    EXPECT_NEAR(total_slept, 0.5, 1e-6);
}

TEST_F(TelemetryReplayTest, InvalidSchemaReportsLineNumber) {
    auto path = _tmpdir / "replay.jsonl";

    nlohmann::json record;
    record["schema_version"] = 999;
    record["recorded_at"] = "2026-08-11T12:00:00Z";
    record["telemetry"] = nlohmann::json::object();

    write_lines(path, {record.dump()});

    EXPECT_THROW(TelemetryReplay(path).samples(), TelemetryReplayError);
}

TEST_F(TelemetryReplayTest, EmptyFileReturnsNoSamples) {
    auto path = _tmpdir / "empty.jsonl";
    write_lines(path, {});

    auto samples = TelemetryReplay(path).samples();
    EXPECT_TRUE(samples.empty());
}

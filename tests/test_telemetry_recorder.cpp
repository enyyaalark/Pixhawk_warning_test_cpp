/// Port of Python test_telemetry_recorder.py tests.

#include <cmath>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <string>

#include "telemetry/telemetry_recorder.hpp"

using namespace telemetry;
using json = nlohmann::json;

class TelemetryRecorderTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto tmp = std::filesystem::temp_directory_path();
        std::string dirname = "pixhawk_test_" + std::to_string(std::rand());
        _tmpdir = tmp / dirname;
        std::filesystem::create_directories(_tmpdir);
    }

    void TearDown() override {
        std::filesystem::remove_all(_tmpdir);
    }

    std::filesystem::path _tmpdir;
};

TEST_F(TelemetryRecorderTest, RecordWritesNormalizedJsonLine) {
    auto path = _tmpdir / "telemetry.jsonl";
    auto t0 = std::chrono::system_clock::from_time_t(
        std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

    TelemetryData data;
    data.attitude.pitch_rad = 0.25;

    {
        TelemetryRecorder recorder(path, 5.0, nullptr, [t0]() { return t0; });
        recorder.open();
        recorder.record(data);
    }

    std::ifstream file(path);
    std::string line;
    ASSERT_TRUE(std::getline(file, line));
    auto record = json::parse(line);

    EXPECT_EQ(record["schema_version"].get<int>(), SCHEMA_VERSION);
    EXPECT_EQ(record["telemetry"]["attitude"]["pitch_rad"].get<double>(), 0.25);
    EXPECT_TRUE(record["telemetry"]["battery"]["voltage_v"].is_null());
}

TEST_F(TelemetryRecorderTest, RecordIfDueLimitsSampleRate) {
    auto path = _tmpdir / "telemetry.jsonl";
    double now = 10.0;
    auto clock = [&now]() { return now; };

    TelemetryData data;

    {
        TelemetryRecorder recorder(path, 2.0, clock);
        recorder.open();

        EXPECT_TRUE(recorder.record_if_due(data));
        now = 10.49;
        EXPECT_FALSE(recorder.record_if_due(data));
        now = 10.5;
        EXPECT_TRUE(recorder.record_if_due(data));
    }

    std::ifstream file(path);
    int lines = 0;
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty()) lines++;
    }
    EXPECT_EQ(lines, 2);
}

TEST_F(TelemetryRecorderTest, NonFiniteValueIsRejected) {
    auto path = _tmpdir / "telemetry.jsonl";
    TelemetryData data;
    data.attitude.roll_rad = NAN;

    TelemetryRecorder recorder(path);
    recorder.open();
    EXPECT_THROW(recorder.record(data), TelemetryRecordingError);
}

TEST_F(TelemetryRecorderTest, BareFilenameCanBeOpened) {
    auto previous_directory = std::filesystem::current_path();
    std::filesystem::current_path(_tmpdir);
    try {
        TelemetryRecorder recorder("telemetry.jsonl");
        recorder.open();
        recorder.record(TelemetryData{});
        recorder.close();
        EXPECT_TRUE(std::filesystem::exists(_tmpdir / "telemetry.jsonl"));
    } catch (...) {
        std::filesystem::current_path(previous_directory);
        throw;
    }
    std::filesystem::current_path(previous_directory);
}

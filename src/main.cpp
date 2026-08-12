/// Application entry point for continuous read-only telemetry processing.

#include <algorithm>
#include <atomic>
#include <csignal>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <thread>

#include <CLI/CLI.hpp>

#include "telemetry/pixhawk_connection.hpp"
#include "telemetry/telemetry_data.hpp"
#include "telemetry/telemetry_reader.hpp"
#include "telemetry/telemetry_recorder.hpp"
#include "telemetry/telemetry_replay.hpp"
#include "warning/warning_engine.hpp"
#include "warning/warning_event.hpp"

using namespace telemetry;
using namespace warning;

// ---------------------------------------------------------------------------
// Signal handling (Ctrl+C)
// ---------------------------------------------------------------------------

static std::atomic<bool> shutdown_requested{false};

static void signal_handler(int /*signum*/) {
    shutdown_requested.store(true);
}

// ---------------------------------------------------------------------------
// Warning event logging
// ---------------------------------------------------------------------------

static void log_warning_events(const std::vector<WarningEvent>& events) {
    for (const auto& event : events) {
        const char* severity_str = "INFO";
        switch (event.severity) {
            case Severity::INFO:     severity_str = "INFO"; break;
            case Severity::CAUTION:  severity_str = "CAUTION"; break;
            case Severity::WARNING:  severity_str = "WARNING"; break;
            case Severity::CRITICAL: severity_str = "CRITICAL"; break;
        }
        if (!event.active) {
            severity_str = "INFO";  // cleared warnings are informational
        }
        std::cout << severity_str << " " << event.code << ": "
                  << event.message << std::endl;
    }
}

// ---------------------------------------------------------------------------
// Live run loop
// ---------------------------------------------------------------------------

static TelemetryData run(const std::string& device,
                         int baud,
                         const std::string& udp_bind_address,
                         int udp_port,
                         double duration_s,
                         double heartbeat_timeout_s,
                         double reconnect_delay_s,
                         const std::string& record_path,
                         double record_rate_hz,
                         double sensor_warning_delay_s,
                         double sensor_clear_delay_s) {

    TelemetryData data;
    WarningEngine warning_engine(
        nullptr,  // default utc_now
        nullptr,  // default clock
        sensor_warning_delay_s,
        sensor_clear_delay_s);

    std::unique_ptr<PixhawkConnection> manager = udp_port > 0
        ? std::make_unique<PixhawkConnection>(
              static_cast<uint16_t>(udp_port), udp_bind_address)
        : std::make_unique<PixhawkConnection>(device, baud);
    const std::string endpoint = udp_port > 0
        ? "UDP " + udp_bind_address + ":" + std::to_string(udp_port)
        : device;

    std::unique_ptr<TelemetryRecorder> recorder;
    if (!record_path.empty()) {
        recorder = std::make_unique<TelemetryRecorder>(record_path, record_rate_hz);
    }

    auto start_time = std::chrono::steady_clock::now();
    auto deadline = (duration_s > 0)
        ? std::optional(start_time + std::chrono::duration<double>(duration_s))
        : std::nullopt;

    try {
        if (recorder) {
            recorder->open();
            std::cerr << "Recording normalized telemetry to " << recorder->path()
                      << std::endl;
        }

        while (!shutdown_requested.load()) {
            // Check duration expiry
            if (deadline.has_value() && std::chrono::steady_clock::now() >= *deadline) {
                break;
            }

            try {
                manager->connect();
                std::cerr << "Receiving telemetry from " << endpoint << std::endl;
                TelemetryReader reader(*manager, &data);

                while (!shutdown_requested.load()) {
                    if (deadline.has_value() &&
                        std::chrono::steady_clock::now() >= *deadline) {
                        break;
                    }

                    reader.read_once(1000);
                    reader.refresh_connection_status(heartbeat_timeout_s);
                    log_warning_events(warning_engine.evaluate(data));

                    if (recorder) {
                        recorder->record_if_due(data);
                    }
                }

            } catch (const std::exception& exc) {
                data.connection.connected = false;
                log_warning_events(warning_engine.evaluate(data));
                std::cerr << "Pixhawk connection unavailable: " << exc.what()
                          << std::endl;
                manager->close();

                if (deadline.has_value() &&
                    std::chrono::steady_clock::now() >= *deadline) {
                    break;
                }
                if (!shutdown_requested.load()) {
                    double sleep_s = reconnect_delay_s;
                    if (deadline.has_value()) {
                        sleep_s = std::min(
                            sleep_s,
                            std::max(0.0, std::chrono::duration<double>(
                                *deadline - std::chrono::steady_clock::now()).count()));
                    }
                    std::this_thread::sleep_for(std::chrono::duration<double>(sleep_s));
                }
            }
        }
    } catch (...) {
        data.connection.connected = false;
        manager->close();
        if (recorder) recorder->close();
        throw;
    }

    data.connection.connected = false;
    manager->close();
    if (recorder) recorder->close();
    return data;
}

// ---------------------------------------------------------------------------
// Replay run loop
// ---------------------------------------------------------------------------

static TelemetryData run_replay(const std::string& replay_path,
                                double replay_speed,
                                double sensor_warning_delay_s,
                                double sensor_clear_delay_s) {

    WarningEngine warning_engine(
        nullptr,
        nullptr,
        sensor_warning_delay_s,
        sensor_clear_delay_s);

    TelemetryReplay replay(replay_path, [](double s) {
        std::this_thread::sleep_for(std::chrono::duration<double>(s));
    });

    // For immediate replay we use infinity
    double speed = (replay_speed <= 0)
        ? std::numeric_limits<double>::infinity()
        : replay_speed;

    auto samples = replay.replay(speed);
    TelemetryData latest;
    for (const auto& sample : samples) {
        latest = sample.telemetry;
        log_warning_events(warning_engine.evaluate(
            latest,
            sample.recorded_at,
            std::chrono::duration<double>(
                sample.recorded_at.time_since_epoch()).count()));
    }

    std::cerr << "Replayed " << samples.size() << " telemetry snapshots from "
              << replay_path << std::endl;
    return latest;
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main(int argc, char** argv) {
    // Install signal handler
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    CLI::App app{"Read-only Pixhawk telemetry monitoring and warning system"};

    std::string device = DEFAULT_DEVICE;
    int baud = DEFAULT_BAUD;
    std::string udp_bind_address = "127.0.0.1";
    int udp_port = 0;
    double duration = 0;  // 0 means run until Ctrl+C
    double heartbeat_timeout = 3.0;
    double reconnect_delay = 2.0;
    std::string record_path;
    double record_rate = 5.0;
    std::string replay_path;
    double replay_speed = 1.0;
    double sensor_warning_delay = 1.0;
    double sensor_clear_delay = 1.0;

    app.add_option("--device", device,
                   "Serial device path (default: " + std::string(DEFAULT_DEVICE) + ")");
    app.add_option("--baud", baud,
                   "Baud rate (default: " + std::to_string(DEFAULT_BAUD) + ")");
    app.add_option("--udp-port", udp_port,
                   "Receive MAVLink over UDP instead of serial (1-65535)");
    app.add_option("--udp-bind", udp_bind_address,
                   "UDP bind address (default: 127.0.0.1)");
    app.add_option("--duration", duration,
                   "Stop after this many seconds (0 = run until Ctrl+C)");
    app.add_option("--heartbeat-timeout", heartbeat_timeout,
                   "Heartbeat timeout in seconds");
    app.add_option("--reconnect-delay", reconnect_delay,
                   "Reconnect delay in seconds");
    app.add_option("--record", record_path,
                   "Write normalized telemetry snapshots as JSON Lines");
    app.add_option("--record-rate", record_rate,
                   "Recording sample rate in Hz");
    app.add_option("--replay", replay_path,
                   "Replay normalized JSON Lines instead of connecting to Pixhawk");
    app.add_option("--replay-speed", replay_speed,
                   "Recording playback speed; use 0 for immediate replay");
    app.add_option("--sensor-warning-delay", sensor_warning_delay,
                   "Sensor warning activation delay in seconds");
    app.add_option("--sensor-clear-delay", sensor_clear_delay,
                   "Sensor warning clear delay in seconds");

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        return app.exit(e);
    }

    // Validate: --record and --replay are mutually exclusive
    if (!replay_path.empty() && !record_path.empty()) {
        std::cerr << "Error: --record cannot be combined with --replay" << std::endl;
        return 2;
    }
    if (baud <= 0 || udp_port < 0 || udp_port > 65535 ||
        udp_bind_address.empty() || duration < 0 || heartbeat_timeout <= 0 ||
        reconnect_delay < 0 || record_rate <= 0 || replay_speed < 0 ||
        sensor_warning_delay < 0 || sensor_clear_delay < 0) {
        std::cerr << "Error: numeric options must be within their documented "
                     "positive ranges" << std::endl;
        return 2;
    }

    try {
        if (!replay_path.empty()) {
            run_replay(replay_path, replay_speed,
                       sensor_warning_delay, sensor_clear_delay);
        } else {
            run(device, baud, udp_bind_address, udp_port,
                duration, heartbeat_timeout, reconnect_delay,
                record_path, record_rate,
                sensor_warning_delay, sensor_clear_delay);
        }
    } catch (const std::exception& exc) {
        std::cerr << "Fatal: " << exc.what() << std::endl;
        return 1;
    }

    std::cerr << "Shutdown complete" << std::endl;
    return 0;
}

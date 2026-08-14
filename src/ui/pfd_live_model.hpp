#pragma once

#include <QObject>
#include <QTimer>

#include <cstdint>
#include <memory>
#include <string>

#include "telemetry/pixhawk_connection.hpp"
#include "telemetry/telemetry_data.hpp"
#include "telemetry/telemetry_reader.hpp"
#include "warning/warning_engine.hpp"
#include "warning/warning_scheduler.hpp"

/// Non-blocking adapter from live MAVLink UDP telemetry to the PFD QML API.
class PfdLiveModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(double rollDegrees READ rollDegrees NOTIFY snapshotChanged)
    Q_PROPERTY(double pitchDegrees READ pitchDegrees NOTIFY snapshotChanged)
    Q_PROPERTY(double yawDegrees READ yawDegrees NOTIFY snapshotChanged)
    Q_PROPERTY(double relativeAltitudeMetres READ relativeAltitudeMetres NOTIFY snapshotChanged)
    Q_PROPERTY(double groundSpeedMetresPerSecond READ groundSpeedMetresPerSecond NOTIFY snapshotChanged)
    Q_PROPERTY(double verticalSpeedMetresPerSecond READ verticalSpeedMetresPerSecond NOTIFY snapshotChanged)
    Q_PROPERTY(bool attitudeValid READ attitudeValid NOTIFY snapshotChanged)
    Q_PROPERTY(bool altitudeValid READ altitudeValid NOTIFY snapshotChanged)
    Q_PROPERTY(bool groundSpeedValid READ groundSpeedValid NOTIFY snapshotChanged)
    Q_PROPERTY(bool verticalSpeedValid READ verticalSpeedValid NOTIFY snapshotChanged)
    Q_PROPERTY(QString attitudeDataState READ attitudeDataState NOTIFY snapshotChanged)
    Q_PROPERTY(QString altitudeDataState READ altitudeDataState NOTIFY snapshotChanged)
    Q_PROPERTY(QString velocityDataState READ velocityDataState NOTIFY snapshotChanged)
    Q_PROPERTY(QString alertText READ alertText NOTIFY snapshotChanged)
    Q_PROPERTY(QString alertSeverity READ alertSeverity NOTIFY snapshotChanged)
    Q_PROPERTY(int activeAlertCount READ activeAlertCount NOTIFY snapshotChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY snapshotChanged)
    Q_PROPERTY(QString sourceLabel READ sourceLabel CONSTANT)
    Q_PROPERTY(QString positionLabel READ positionLabel NOTIFY snapshotChanged)

public:
    explicit PfdLiveModel(std::uint16_t udp_port,
                          std::string bind_address = "127.0.0.1",
                          QObject* parent = nullptr);

    double rollDegrees() const noexcept { return _roll_degrees; }
    double pitchDegrees() const noexcept { return _pitch_degrees; }
    double yawDegrees() const noexcept { return _yaw_degrees; }
    double relativeAltitudeMetres() const noexcept { return _relative_altitude_metres; }
    double groundSpeedMetresPerSecond() const noexcept { return _ground_speed_metres_per_second; }
    double verticalSpeedMetresPerSecond() const noexcept { return _vertical_speed_metres_per_second; }
    bool attitudeValid() const noexcept { return _attitude_valid; }
    bool altitudeValid() const noexcept { return _altitude_valid; }
    bool groundSpeedValid() const noexcept { return _ground_speed_valid; }
    bool verticalSpeedValid() const noexcept { return _vertical_speed_valid; }
    QString attitudeDataState() const { return _attitude_data_state; }
    QString altitudeDataState() const { return _altitude_data_state; }
    QString velocityDataState() const { return _velocity_data_state; }
    QString alertText() const { return _alert_text; }
    QString alertSeverity() const { return _alert_severity; }
    int activeAlertCount() const noexcept { return static_cast<int>(_scheduler.active_count()); }
    bool connected() const noexcept { return _connected; }
    QString sourceLabel() const { return QStringLiteral("LIVE MAVLINK"); }
    QString positionLabel() const;

    void start();

signals:
    void snapshotChanged();

private slots:
    void pollTelemetry();

private:
    void updatePresentation();

    std::uint16_t _udp_port;
    std::string _bind_address;
    telemetry::TelemetryData _data;
    telemetry::PixhawkConnection _connection;
    telemetry::TelemetryReader _reader;
    warning::WarningEngine _warning_engine;
    warning::WarningScheduler _scheduler;
    QTimer _poll_timer;
    std::uint64_t _message_count{0};
    double _roll_degrees{0.0};
    double _pitch_degrees{0.0};
    double _yaw_degrees{0.0};
    double _relative_altitude_metres{0.0};
    double _ground_speed_metres_per_second{0.0};
    double _vertical_speed_metres_per_second{0.0};
    bool _attitude_valid{false};
    bool _altitude_valid{false};
    bool _ground_speed_valid{false};
    bool _vertical_speed_valid{false};
    bool _connected{false};
    QString _attitude_data_state{QStringLiteral("NO DATA")};
    QString _altitude_data_state{QStringLiteral("NO DATA")};
    QString _velocity_data_state{QStringLiteral("NO DATA")};
    QString _alert_text{QStringLiteral("STANDBY")};
    QString _alert_severity{QStringLiteral("standby")};
};

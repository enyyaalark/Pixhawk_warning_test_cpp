#pragma once

#include <QObject>
#include <QTimer>

#include <cstddef>
#include <string>
#include <vector>

#include "telemetry/telemetry_replay.hpp"

/// Presentation-only model for the first Qt PFD prototype.
///
/// It consumes normalized JSONL replay samples. It does not parse MAVLink and
/// does not contain warning thresholds. A later live adapter can update the
/// same QML-facing properties from TelemetryData snapshots.
class PfdReplayModel final : public QObject {
    Q_OBJECT

    Q_PROPERTY(double rollDegrees READ rollDegrees NOTIFY snapshotChanged)
    Q_PROPERTY(double pitchDegrees READ pitchDegrees NOTIFY snapshotChanged)
    Q_PROPERTY(double yawDegrees READ yawDegrees NOTIFY snapshotChanged)
    Q_PROPERTY(double relativeAltitudeMetres READ relativeAltitudeMetres
               NOTIFY snapshotChanged)
    Q_PROPERTY(double groundSpeedMetresPerSecond READ groundSpeedMetresPerSecond NOTIFY snapshotChanged)
    Q_PROPERTY(double verticalSpeedMetresPerSecond READ verticalSpeedMetresPerSecond NOTIFY snapshotChanged)
    Q_PROPERTY(bool attitudeValid READ attitudeValid NOTIFY snapshotChanged)
    Q_PROPERTY(bool altitudeValid READ altitudeValid NOTIFY snapshotChanged)
    Q_PROPERTY(bool groundSpeedValid READ groundSpeedValid NOTIFY snapshotChanged)
    Q_PROPERTY(bool verticalSpeedValid READ verticalSpeedValid NOTIFY snapshotChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY snapshotChanged)
    Q_PROPERTY(QString attitudeDataState READ attitudeDataState NOTIFY snapshotChanged)
    Q_PROPERTY(QString altitudeDataState READ altitudeDataState NOTIFY snapshotChanged)
    Q_PROPERTY(QString velocityDataState READ velocityDataState NOTIFY snapshotChanged)
    Q_PROPERTY(QString alertText READ alertText CONSTANT)
    Q_PROPERTY(QString alertSeverity READ alertSeverity CONSTANT)
    Q_PROPERTY(int activeAlertCount READ activeAlertCount CONSTANT)
    Q_PROPERTY(QString sourceLabel READ sourceLabel CONSTANT)
    Q_PROPERTY(QString positionLabel READ positionLabel NOTIFY snapshotChanged)

public:
    explicit PfdReplayModel(std::vector<telemetry::ReplaySample> samples,
                            double replay_speed,
                            QObject* parent = nullptr);

    double rollDegrees() const noexcept { return _roll_degrees; }
    double pitchDegrees() const noexcept { return _pitch_degrees; }
    double yawDegrees() const noexcept { return _yaw_degrees; }
    double relativeAltitudeMetres() const noexcept {
        return _relative_altitude_metres;
    }
    double groundSpeedMetresPerSecond() const noexcept { return _ground_speed_metres_per_second; }
    double verticalSpeedMetresPerSecond() const noexcept { return _vertical_speed_metres_per_second; }
    bool attitudeValid() const noexcept { return _attitude_valid; }
    bool altitudeValid() const noexcept { return _altitude_valid; }
    bool groundSpeedValid() const noexcept { return _ground_speed_valid; }
    bool verticalSpeedValid() const noexcept { return _vertical_speed_valid; }
    bool connected() const noexcept { return _connected; }
    QString attitudeDataState() const { return _attitude_valid ? QStringLiteral("VALID") : QStringLiteral("NO DATA"); }
    QString altitudeDataState() const { return _altitude_valid ? QStringLiteral("VALID") : QStringLiteral("NO DATA"); }
    QString velocityDataState() const { return (_ground_speed_valid || _vertical_speed_valid) ? QStringLiteral("VALID") : QStringLiteral("NO DATA"); }
    QString alertText() const { return QStringLiteral("STANDBY"); }
    QString alertSeverity() const { return QStringLiteral("standby"); }
    int activeAlertCount() const noexcept { return 0; }
    QString sourceLabel() const { return QStringLiteral("RECORDED TELEMETRY"); }
    QString positionLabel() const;

    void start();

signals:
    void snapshotChanged();

private slots:
    void advance();

private:
    void applySample(std::size_t index);
    int delayToNextSampleMs() const;

    std::vector<telemetry::ReplaySample> _samples;
    QTimer _timer;
    std::size_t _index{0};
    double _replay_speed{1.0};

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
};

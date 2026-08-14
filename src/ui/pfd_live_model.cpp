#include "pfd_live_model.hpp"

#include <algorithm>
#include <cmath>
#include "utils/chrono_utils.hpp"

namespace {
constexpr double radians_to_degrees = 180.0 / 3.14159265358979323846;
constexpr int poll_interval_ms = 16;   // align updates with a ~60 Hz display
constexpr int max_messages_per_tick = 256;
constexpr double heartbeat_timeout_s = 3.0;
constexpr double attitude_timeout_s = 1.0;
constexpr double altitude_timeout_s = 2.0;
constexpr double velocity_timeout_s = 2.0;

QString stateFor(bool available, bool fresh) {
    return !available ? QStringLiteral("NO DATA")
                      : fresh ? QStringLiteral("VALID") : QStringLiteral("STALE");
}
}

PfdLiveModel::PfdLiveModel(std::uint16_t udp_port,
                           std::string bind_address,
                           QObject* parent)
    : QObject(parent),
      _udp_port(udp_port),
      _bind_address(std::move(bind_address)),
      _connection(_udp_port, _bind_address),
      _reader(_connection, &_data) {
    _poll_timer.setInterval(poll_interval_ms);
    _poll_timer.setTimerType(Qt::PreciseTimer);
    connect(&_poll_timer, &QTimer::timeout, this, &PfdLiveModel::pollTelemetry);
}

QString PfdLiveModel::positionLabel() const {
    return QStringLiteral("UDP %1 · %2 MSG")
        .arg(_udp_port)
        .arg(static_cast<qulonglong>(_message_count));
}

void PfdLiveModel::start() {
    _connection.connect();
    _poll_timer.start();
}

void PfdLiveModel::pollTelemetry() {
    bool received = false;
    for (int i = 0; i < max_messages_per_tick; ++i) {
        const auto message = _connection.receive_message(0);
        if (!message.has_value()) {
            break;
        }
        ++_message_count;
        received = _reader.process_message(message.value()) || received;
    }

    const bool was_connected = _data.connection.connected;
    _reader.refresh_connection_status(heartbeat_timeout_s);
    const double evaluation_time = utils::steady_seconds();
    for (const auto& event : _warning_engine.evaluate(_data, std::nullopt, evaluation_time))
        _scheduler.submit(event, evaluation_time);
    if (const auto alert = _scheduler.highest_active()) {
        _alert_text = QString::fromStdString(alert->message).toUpper();
        switch (alert->severity) {
            case warning::Severity::CRITICAL: _alert_severity = QStringLiteral("critical"); break;
            case warning::Severity::WARNING: _alert_severity = QStringLiteral("warning"); break;
            case warning::Severity::CAUTION: _alert_severity = QStringLiteral("caution"); break;
            default: _alert_severity = QStringLiteral("info"); break;
        }
    } else { _alert_text = QStringLiteral("STANDBY"); _alert_severity = QStringLiteral("standby"); }
    if (received || was_connected != _data.connection.connected) {
        updatePresentation();
        emit snapshotChanged();
    }
}

void PfdLiveModel::updatePresentation() {
    const double now = utils::steady_seconds();
    const auto& attitude = _data.attitude;
    const bool attitude_available = attitude.roll_rad.has_value() &&
                      attitude.pitch_rad.has_value() &&
                      attitude.yaw_rad.has_value();
    _attitude_valid = attitude_available && attitude.is_fresh(now, attitude_timeout_s);
    _attitude_data_state = stateFor(attitude_available, _attitude_valid);
    if (_attitude_valid) {
        _roll_degrees = *attitude.roll_rad * radians_to_degrees;
        _pitch_degrees = *attitude.pitch_rad * radians_to_degrees;
        _yaw_degrees = *attitude.yaw_rad * radians_to_degrees;
        if (_yaw_degrees < 0.0) {
            _yaw_degrees += 360.0;
        }
    }

    const bool altitude_available = _data.motion.relative_altitude_m.has_value();
    _altitude_valid = altitude_available &&
                      _data.motion.altitude_is_fresh(now, altitude_timeout_s);
    _altitude_data_state = stateFor(altitude_available, _altitude_valid);
    if (_altitude_valid) {
        _relative_altitude_metres = *_data.motion.relative_altitude_m;
    }
    const bool velocity_fresh = _data.motion.local_position_is_fresh(now, velocity_timeout_s);
    const bool ground_speed_available = _data.motion.velocity_north_m_s.has_value() &&
                                        _data.motion.velocity_east_m_s.has_value();
    _ground_speed_valid = ground_speed_available && velocity_fresh;
    if (_ground_speed_valid) {
        _ground_speed_metres_per_second = std::hypot(
            *_data.motion.velocity_north_m_s, *_data.motion.velocity_east_m_s);
    }
    const bool vertical_speed_available = _data.motion.velocity_down_m_s.has_value();
    _vertical_speed_valid = vertical_speed_available && velocity_fresh;
    _velocity_data_state = stateFor(
        ground_speed_available || vertical_speed_available,
        velocity_fresh && (ground_speed_available || vertical_speed_available));
    if (_vertical_speed_valid) {
        _vertical_speed_metres_per_second = -*_data.motion.velocity_down_m_s;
    }
    _connected = _data.connection.connected;
}

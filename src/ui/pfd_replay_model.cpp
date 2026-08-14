#include "pfd_replay_model.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdexcept>

namespace {
constexpr double radians_to_degrees = 180.0 / 3.14159265358979323846;
}

PfdReplayModel::PfdReplayModel(
    std::vector<telemetry::ReplaySample> samples,
    double replay_speed,
    QObject* parent)
    : QObject(parent),
      _samples(std::move(samples)),
      _replay_speed(replay_speed) {
    if (_samples.empty()) {
        throw std::invalid_argument("PFD replay requires at least one sample");
    }
    if (!std::isfinite(_replay_speed) || _replay_speed <= 0.0) {
        throw std::invalid_argument("PFD replay speed must be finite and positive");
    }

    _timer.setSingleShot(true);
    connect(&_timer, &QTimer::timeout, this, &PfdReplayModel::advance);
    applySample(0);
}

QString PfdReplayModel::positionLabel() const {
    return QStringLiteral("SAMPLE %1 / %2")
        .arg(static_cast<qulonglong>(_index + 1))
        .arg(static_cast<qulonglong>(_samples.size()));
}

void PfdReplayModel::start() {
    _timer.start(delayToNextSampleMs());
}

void PfdReplayModel::advance() {
    _index = (_index + 1) % _samples.size();
    applySample(_index);
    emit snapshotChanged();
    _timer.start(delayToNextSampleMs());
}

void PfdReplayModel::applySample(std::size_t index) {
    const auto& data = _samples.at(index).telemetry;
    const auto& attitude = data.attitude;

    _attitude_valid = attitude.roll_rad.has_value() &&
                      attitude.pitch_rad.has_value() &&
                      attitude.yaw_rad.has_value();
    if (_attitude_valid) {
        _roll_degrees = attitude.roll_rad.value() * radians_to_degrees;
        _pitch_degrees = attitude.pitch_rad.value() * radians_to_degrees;
        _yaw_degrees = attitude.yaw_rad.value() * radians_to_degrees;
        if (_yaw_degrees < 0.0) {
            _yaw_degrees += 360.0;
        }
    }

    _altitude_valid = data.motion.relative_altitude_m.has_value();
    if (_altitude_valid) {
        _relative_altitude_metres = data.motion.relative_altitude_m.value();
    }
    _ground_speed_valid = data.motion.velocity_north_m_s.has_value() &&
                          data.motion.velocity_east_m_s.has_value();
    if (_ground_speed_valid) {
        _ground_speed_metres_per_second = std::hypot(
            data.motion.velocity_north_m_s.value(), data.motion.velocity_east_m_s.value());
    }
    _vertical_speed_valid = data.motion.velocity_down_m_s.has_value();
    if (_vertical_speed_valid) {
        _vertical_speed_metres_per_second = -data.motion.velocity_down_m_s.value();
    }
    _connected = data.connection.connected;
}

int PfdReplayModel::delayToNextSampleMs() const {
    if (_samples.size() < 2) {
        return 1000;
    }

    const std::size_t next = (_index + 1) % _samples.size();
    if (next == 0) {
        return 500;
    }

    const double recorded_seconds = std::chrono::duration<double>(
        _samples[next].recorded_at - _samples[_index].recorded_at).count();
    const double scaled_ms = recorded_seconds * 1000.0 / _replay_speed;
    return std::clamp(static_cast<int>(std::lround(scaled_ms)), 1, 2000);
}

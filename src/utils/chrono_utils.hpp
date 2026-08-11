#pragma once

/// ISO 8601 formatting / parsing helpers for C++17.

#include <chrono>
#include <cctype>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <stdexcept>

namespace utils {

inline double steady_seconds() {
    return std::chrono::duration<double>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

/// Format a time point as UTC ISO 8601 while preserving microseconds.
inline std::string format_iso8601(std::chrono::system_clock::time_point tp) {
    auto since_epoch = tp.time_since_epoch();
    auto whole_seconds = std::chrono::duration_cast<std::chrono::seconds>(since_epoch);
    auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(
        since_epoch - whole_seconds).count();
    if (microseconds < 0) {
        whole_seconds -= std::chrono::seconds(1);
        microseconds += 1000000;
    }
    std::time_t t = static_cast<std::time_t>(whole_seconds.count());
    std::tm utc_tm{};
    if (gmtime_r(&t, &utc_tm) == nullptr) {
        throw std::runtime_error("gmtime_r failed");
    }
    std::ostringstream oss;
    oss << std::put_time(&utc_tm, "%Y-%m-%dT%H:%M:%S")
        << '.' << std::setw(6) << std::setfill('0') << microseconds << 'Z';
    return oss.str();
}

/// Parse ISO 8601 timestamps emitted by both the Python and C++ recorders.
inline std::chrono::system_clock::time_point parse_iso8601(const std::string& s) {
    if (s.size() < 19) {
        throw std::runtime_error("Invalid ISO 8601 timestamp: " + s);
    }
    std::tm tm{};
    std::istringstream iss(s.substr(0, 19));
    iss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    if (iss.fail()) {
        throw std::runtime_error("Invalid ISO 8601 timestamp: " + s);
    }

    size_t position = 19;
    long microseconds = 0;
    if (position < s.size() && s[position] == '.') {
        size_t start = ++position;
        while (position < s.size() && std::isdigit(static_cast<unsigned char>(s[position]))) {
            ++position;
        }
        if (position == start) {
            throw std::runtime_error("Invalid ISO 8601 timestamp: " + s);
        }
        std::string fraction = s.substr(start, position - start);
        if (fraction.size() > 6) fraction.resize(6);
        while (fraction.size() < 6) fraction.push_back('0');
        microseconds = std::stol(fraction);
    }

    int offset_seconds = 0;
    if (position < s.size() && s[position] == 'Z') {
        ++position;
    } else if (position < s.size() && (s[position] == '+' || s[position] == '-')) {
        int sign = s[position] == '+' ? 1 : -1;
        if (position + 6 != s.size() || s[position + 3] != ':') {
            throw std::runtime_error("Invalid ISO 8601 timestamp: " + s);
        }
        int hours = std::stoi(s.substr(position + 1, 2));
        int minutes = std::stoi(s.substr(position + 4, 2));
        if (hours > 23 || minutes > 59) {
            throw std::runtime_error("Invalid ISO 8601 timestamp: " + s);
        }
        offset_seconds = sign * (hours * 3600 + minutes * 60);
        position += 6;
    } else {
        throw std::runtime_error("ISO 8601 timestamp must include a timezone: " + s);
    }
    if (position != s.size()) {
        throw std::runtime_error("Invalid ISO 8601 timestamp: " + s);
    }

    std::time_t t = timegm(&tm) - offset_seconds;
    return std::chrono::system_clock::from_time_t(t) +
           std::chrono::microseconds(microseconds);
}

}  // namespace utils

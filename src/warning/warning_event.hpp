#pragma once

/// Structured warning events shared by console, logs, and future TTS.

#include <chrono>
#include <string>

namespace warning {

enum class Severity {
    INFO,
    CAUTION,
    WARNING,
    CRITICAL,
};

/// A warning state transition, rather than a continuously repeated alert.
struct WarningEvent {
    std::string code;
    Severity severity;
    std::string message;
    std::chrono::system_clock::time_point timestamp;
    bool active;
};

}  // namespace warning

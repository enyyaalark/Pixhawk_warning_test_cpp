#pragma once

/// Read-only MAVLink connection lifecycle management.
///
/// Manages a serial port (via termios) and feeds raw bytes to the
/// mavlink C parser.  Exposes a ``receive_message()`` method that
/// mirrors pymavlink's ``recv_match()``.

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

extern "C" {
#include <mavlink_types.h>
}

namespace telemetry {

// ---------------------------------------------------------------------------
// Constants (matching the Python defaults)
// ---------------------------------------------------------------------------

constexpr const char* DEFAULT_DEVICE = "/dev/ttyACM0";
constexpr int DEFAULT_BAUD = 115200;

// ---------------------------------------------------------------------------
// Exceptions
// ---------------------------------------------------------------------------

class PixhawkConnectionError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// ---------------------------------------------------------------------------
// Connection manager
// ---------------------------------------------------------------------------

class PixhawkConnection {
public:
    /// Construct a connection manager.
    ///
    /// The connection is *not* opened automatically — call connect().
    explicit PixhawkConnection(std::string device = DEFAULT_DEVICE,
                               int baud = DEFAULT_BAUD);

    ~PixhawkConnection();

    // Non-copyable, movable
    PixhawkConnection(const PixhawkConnection&) = delete;
    PixhawkConnection& operator=(const PixhawkConnection&) = delete;
    PixhawkConnection(PixhawkConnection&& other) noexcept;
    PixhawkConnection& operator=(PixhawkConnection&& other) noexcept;

    /// Whether the serial port is currently open.
    bool is_open() const noexcept { return _fd >= 0; }

    /// Open the configured serial port, or return immediately if already open.
    void connect();

    /// Close the current connection; calling this repeatedly is safe.
    void close() noexcept;

    /// Close the old connection and perform one immediate reconnect attempt.
    void reconnect();

    /// Block until a message is received or *timeout_ms* expires.
    ///
    /// Returns std::nullopt on timeout.  The returned message must be
    /// decoded with the mavlink_msg_*_decode() family.
    std::optional<mavlink_message_t> receive_message(int timeout_ms = 1000);

    /// Feed raw bytes directly (useful for testing without a serial port).
    /// Returns true when a complete message was assembled.
    bool push_bytes(const uint8_t* data, size_t len);

    /// Pop the next buffered message assembled by push_bytes().
    std::optional<mavlink_message_t> pop_message();

    /// Accessors
    const std::string& device() const noexcept { return _device; }
    int baud() const noexcept { return _baud; }

private:
    void configure_termios();

    std::string _device;
    int _baud;
    int _fd{-1};

    // MAVLink parser state
    mavlink_status_t _status{};
    std::vector<mavlink_message_t> _message_queue;
};

}  // namespace telemetry

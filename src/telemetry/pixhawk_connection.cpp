/// Read-only MAVLink connection lifecycle management (implementation).

#include "pixhawk_connection.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <memory>
#include <sstream>
#include <stdexcept>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

// MAVLink C library (MAVLINK_HELPER must be defined before inclusion
// because mavlink_conversions.h uses it before mavlink_helpers.h defines it)
#ifndef MAVLINK_HELPER
#define MAVLINK_HELPER static inline
#endif
extern "C" {
#include <common/mavlink.h>
#include <mavlink_helpers.h>
}

namespace telemetry {

namespace {

speed_t baud_to_termios(int baud) {
    switch (baud) {
        case 1200: return B1200;
        case 2400: return B2400;
        case 4800: return B4800;
        case 9600: return B9600;
        case 19200: return B19200;
        case 38400: return B38400;
        case 57600: return B57600;
        case 115200: return B115200;
#ifdef B230400
        case 230400: return B230400;
#endif
#ifdef B460800
        case 460800: return B460800;
#endif
#ifdef B921600
        case 921600: return B921600;
#endif
        default:
            throw PixhawkConnectionError(
                "Unsupported serial baud rate: " + std::to_string(baud));
    }
}

}  // namespace

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

PixhawkConnection::PixhawkConnection(std::string device, int baud)
    : _device(std::move(device)), _baud(baud) {
    if (_device.empty()) {
        throw std::invalid_argument("device must not be empty");
    }
    if (_baud <= 0) {
        throw std::invalid_argument("baud must be positive");
    }
}

PixhawkConnection::PixhawkConnection(uint16_t udp_port, std::string bind_address)
    : _device(std::move(bind_address)), _baud(0), _udp_port(udp_port) {
    if (_device.empty()) {
        throw std::invalid_argument("UDP bind address must not be empty");
    }
    if (udp_port == 0) {
        throw std::invalid_argument("UDP port must be between 1 and 65535");
    }
}

PixhawkConnection::~PixhawkConnection() {
    close();
}

PixhawkConnection::PixhawkConnection(PixhawkConnection&& other) noexcept
    : _device(std::move(other._device)),
      _baud(other._baud),
      _udp_port(other._udp_port),
      _fd(other._fd),
      _status(other._status),
      _message_queue(std::move(other._message_queue)) {
    other._fd = -1;
}

PixhawkConnection& PixhawkConnection::operator=(PixhawkConnection&& other) noexcept {
    if (this != &other) {
        close();
        _device = std::move(other._device);
        _baud = other._baud;
        _udp_port = other._udp_port;
        _fd = other._fd;
        _status = other._status;
        _message_queue = std::move(other._message_queue);
        other._fd = -1;
    }
    return *this;
}

// ---------------------------------------------------------------------------
// Serial port management
// ---------------------------------------------------------------------------

void PixhawkConnection::configure_termios() {
    struct termios tty {};
    if (tcgetattr(_fd, &tty) != 0) {
        throw PixhawkConnectionError("tcgetattr failed: " +
                                     std::string(std::strerror(errno)));
    }

    // Raw mode: no canonical processing, no echo, no signals
    cfmakeraw(&tty);

    // 8N1
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;

    // No hardware flow control
    tty.c_cflag &= ~CRTSCTS;

    // No software flow control
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);

    // Enable receiver, ignore modem control lines
    tty.c_cflag |= CREAD | CLOCAL;

    // Set baud rate
    speed_t speed = baud_to_termios(_baud);
    if (cfsetispeed(&tty, speed) != 0 || cfsetospeed(&tty, speed) != 0) {
        throw PixhawkConnectionError("could not set baud rate " +
                                     std::to_string(_baud) + ": " +
                                     std::string(std::strerror(errno)));
    }

    // Read timeout: 100ms deciseconds
    tty.c_cc[VTIME] = 1;
    tty.c_cc[VMIN] = 0;

    if (tcsetattr(_fd, TCSANOW, &tty) != 0) {
        throw PixhawkConnectionError("tcsetattr failed: " +
                                     std::string(std::strerror(errno)));
    }
}

void PixhawkConnection::connect() {
    if (_fd >= 0) {
        return;  // already open
    }

    if (_udp_port.has_value()) {
        _fd = ::socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);
        if (_fd < 0) {
            throw PixhawkConnectionError("Could not create UDP socket: " +
                                         std::string(std::strerror(errno)));
        }

        struct sockaddr_in address {};
        address.sin_family = AF_INET;
        address.sin_port = htons(*_udp_port);
        if (::inet_pton(AF_INET, _device.c_str(), &address.sin_addr) != 1) {
            ::close(_fd);
            _fd = -1;
            throw PixhawkConnectionError("Invalid UDP bind address: " + _device);
        }
        if (::bind(_fd, reinterpret_cast<struct sockaddr*>(&address),
                   sizeof(address)) != 0) {
            const std::string message = std::strerror(errno);
            ::close(_fd);
            _fd = -1;
            throw PixhawkConnectionError("Could not bind UDP " + _device + ":" +
                                         std::to_string(*_udp_port) + ": " + message);
        }
        std::memset(&_status, 0, sizeof(_status));
        _message_queue.clear();
        return;
    }

    _fd = ::open(_device.c_str(), O_RDONLY | O_NOCTTY | O_NONBLOCK);
    if (_fd < 0) {
        throw PixhawkConnectionError("Could not open " + _device + ": " +
                                     std::string(std::strerror(errno)));
    }

    // Verify it's a TTY device
    if (!isatty(_fd)) {
        ::close(_fd);
        _fd = -1;
        throw PixhawkConnectionError(_device + " is not a TTY device");
    }

    try {
        configure_termios();
    } catch (...) {
        ::close(_fd);
        _fd = -1;
        throw;
    }

    // Drain any stale data
    tcflush(_fd, TCIOFLUSH);

    // Reset mavlink parser state
    std::memset(&_status, 0, sizeof(_status));
    _message_queue.clear();
}

void PixhawkConnection::close() noexcept {
    if (_fd >= 0) {
        ::close(_fd);
        _fd = -1;
    }
    _message_queue.clear();
}

void PixhawkConnection::reconnect() {
    close();
    connect();
}

// ---------------------------------------------------------------------------
// Message reception
// ---------------------------------------------------------------------------

std::optional<mavlink_message_t> PixhawkConnection::receive_message(int timeout_ms) {
    // First check if we already have a queued message
    if (!_message_queue.empty()) {
        auto msg = std::move(_message_queue.front());
        _message_queue.erase(_message_queue.begin());
        return msg;
    }

    if (_fd < 0) {
        throw PixhawkConnectionError("Connection is not open");
    }

    // Use poll() for timeout support
    struct pollfd pfd {};
    pfd.fd = _fd;
    pfd.events = POLLIN;

    int ret = ::poll(&pfd, 1, timeout_ms);
    if (ret < 0) {
        if (errno == EINTR) {
            return std::nullopt;
        }
        throw PixhawkConnectionError("poll error: " +
                                     std::string(std::strerror(errno)));
    }
    if (ret == 0) {
        return std::nullopt;  // timeout
    }
    if (pfd.revents & (POLLERR | POLLNVAL) ||
        (!_udp_port.has_value() && (pfd.revents & POLLHUP))) {
        throw PixhawkConnectionError(_udp_port.has_value()
                                         ? "UDP socket error"
                                         : "serial device disconnected");
    }

    // Read available bytes and feed to mavlink parser
    // UDP preserves datagram boundaries. Use the maximum datagram size so a
    // forwarded batch of MAVLink frames is never silently truncated.
    std::vector<uint8_t> buf(_udp_port.has_value() ? 65535 : 256);
    ssize_t n = _udp_port.has_value()
                    ? ::recv(_fd, buf.data(), buf.size(), 0)
                    : ::read(_fd, buf.data(), buf.size());
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return std::nullopt;
        }
        throw PixhawkConnectionError("read error: " +
                                     std::string(std::strerror(errno)));
    }
    if (n == 0) {
        return std::nullopt;
    }

    push_bytes(buf.data(), static_cast<size_t>(n));
    return pop_message();
}

bool PixhawkConnection::push_bytes(const uint8_t* data, size_t len) {
    bool got_message = false;
    for (size_t i = 0; i < len; ++i) {
        mavlink_message_t msg;
        if (mavlink_parse_char(MAVLINK_COMM_0, data[i], &msg, &_status)) {
            _message_queue.push_back(msg);
            got_message = true;
        }
    }
    return got_message;
}

std::optional<mavlink_message_t> PixhawkConnection::pop_message() {
    if (_message_queue.empty()) {
        return std::nullopt;
    }
    auto msg = std::move(_message_queue.front());
    _message_queue.erase(_message_queue.begin());
    return msg;
}

}  // namespace telemetry

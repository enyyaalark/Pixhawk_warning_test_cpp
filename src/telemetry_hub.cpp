/// Read-only MAVLink telemetry fan-out.
///
/// Opens the Pixhawk serial device O_RDONLY through PixhawkConnection and
/// republishes received MAVLink frames to one or more UDP consumers. UDP is
/// strictly output-only: no command path exists back to the flight controller.

#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <chrono>
#include <vector>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <CLI/CLI.hpp>

#include "telemetry/pixhawk_connection.hpp"

#ifndef MAVLINK_HELPER
#define MAVLINK_HELPER static inline
#endif
extern "C" {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
#include <common/mavlink.h>
#include <mavlink_helpers.h>
#pragma GCC diagnostic pop
}

namespace {
std::atomic<bool> stop_requested{false};

void signal_handler(int) { stop_requested.store(true); }

struct UdpDestination {
    std::string specification;
    sockaddr_in address{};
};

UdpDestination parse_destination(const std::string& value) {
    const auto separator = value.rfind(':');
    if (separator == std::string::npos || separator == 0 ||
        separator + 1 >= value.size()) {
        throw std::invalid_argument("UDP endpoint must be ADDRESS:PORT: " + value);
    }
    const std::string host = value.substr(0, separator);
    const int port = std::stoi(value.substr(separator + 1));
    if (port < 1 || port > 65535) {
        throw std::invalid_argument("UDP endpoint port must be 1..65535: " + value);
    }

    UdpDestination result;
    result.specification = value;
    result.address.sin_family = AF_INET;
    result.address.sin_port = htons(static_cast<std::uint16_t>(port));
    if (::inet_pton(AF_INET, host.c_str(), &result.address.sin_addr) != 1) {
        throw std::invalid_argument("UDP endpoint address is invalid: " + value);
    }
    return result;
}
}

int main(int argc, char** argv) {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    CLI::App app{"Read-only Pixhawk MAVLink telemetry distributor"};
    std::string device = telemetry::DEFAULT_DEVICE;
    int baud = telemetry::DEFAULT_BAUD;
    double reconnect_delay_s = 2.0;
    std::vector<std::string> endpoint_values{
        "127.0.0.1:14445", "127.0.0.1:14446", "127.0.0.1:14550"};
    app.add_option("--device", device, "Pixhawk serial device");
    app.add_option("--baud", baud, "Serial baud rate");
    app.add_option("--reconnect-delay", reconnect_delay_s,
                   "Seconds between serial reconnect attempts");
    app.add_option("-e,--endpoint", endpoint_values,
                   "UDP output ADDRESS:PORT; repeat for multiple consumers");

    try {
        app.parse(argc, argv);
        if (baud <= 0 || endpoint_values.empty() || reconnect_delay_s < 0.1) {
            throw std::invalid_argument("baud must be positive and at least one endpoint is required");
        }

        std::vector<UdpDestination> destinations;
        destinations.reserve(endpoint_values.size());
        for (const auto& value : endpoint_values) {
            destinations.push_back(parse_destination(value));
        }

        const int udp_socket = ::socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
        if (udp_socket < 0) {
            throw std::runtime_error("Could not create UDP output socket: " +
                                     std::string(std::strerror(errno)));
        }

        telemetry::PixhawkConnection connection(device, baud);
        std::cerr << "Read-only telemetry hub: " << device << " @ " << baud << " baud\n";
        for (const auto& destination : destinations) {
            std::cerr << "  -> UDP " << destination.specification << '\n';
        }

        std::uint64_t forwarded = 0;
        std::uint64_t reconnects = 0;
        while (!stop_requested.load()) {
            try {
                if (!connection.is_open()) {
                    connection.connect();
                    ++reconnects;
                    std::cerr << "Pixhawk serial connected (attempt " << reconnects << ")\n";
                }
                const auto message = connection.receive_message(500);
                if (!message.has_value()) continue;
                std::uint8_t frame[MAVLINK_MAX_PACKET_LEN];
                const std::uint16_t length = mavlink_msg_to_send_buffer(frame, &*message);
                for (const auto& destination : destinations) {
                    const auto sent = ::sendto(udp_socket, frame, length, 0,
                        reinterpret_cast<const sockaddr*>(&destination.address),
                        sizeof(destination.address));
                    if (sent != length) {
                        std::cerr << "UDP send failed for " << destination.specification
                                  << ": " << std::strerror(errno) << '\n';
                    }
                }
                ++forwarded;
            } catch (const telemetry::PixhawkConnectionError& error) {
                connection.close();
                std::cerr << "Pixhawk serial unavailable: " << error.what()
                          << "; retrying in " << reconnect_delay_s << " s\n";
                std::this_thread::sleep_for(std::chrono::duration<double>(reconnect_delay_s));
            }
        }

        connection.close();
        ::close(udp_socket);
        std::cerr << "Telemetry hub stopped after " << forwarded << " messages\n";
        return 0;
    } catch (const CLI::ParseError& error) {
        return app.exit(error);
    } catch (const std::exception& error) {
        std::cerr << "Telemetry hub failed: " << error.what() << '\n';
        return 1;
    }
}

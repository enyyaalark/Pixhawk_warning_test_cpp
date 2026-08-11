/// Test PixhawkConnection lifecycle (termios independent tests).

#include <gtest/gtest.h>
#include <cstring>
#include <cstdlib>

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#ifndef MAVLINK_HELPER
#define MAVLINK_HELPER static inline
#endif
extern "C" {
#include <common/mavlink.h>
#include <mavlink_helpers.h>
}

#include "telemetry/pixhawk_connection.hpp"

using namespace telemetry;

TEST(PixhawkConnectionTest, ConstructWithValidArguments) {
    PixhawkConnection manager("/dev/fake", 115200);
    EXPECT_FALSE(manager.is_open());
    EXPECT_EQ(manager.device(), "/dev/fake");
    EXPECT_EQ(manager.baud(), 115200);
}

TEST(PixhawkConnectionTest, EmptyDeviceRejected) {
    EXPECT_THROW(PixhawkConnection("", 115200), std::invalid_argument);
}

TEST(PixhawkConnectionTest, NonPositiveBaudRejected) {
    EXPECT_THROW(PixhawkConnection("/dev/ttyACM0", 0), std::invalid_argument);
    EXPECT_THROW(PixhawkConnection("/dev/ttyACM0", -1), std::invalid_argument);
}

TEST(PixhawkConnectionTest, PushBytesAndPopMessageWithValidMavlink2Frame) {
    PixhawkConnection manager("/dev/fake", 115200);

    // Build a valid HEARTBEAT message using the mavlink encode functions
    mavlink_message_t msg;
    mavlink_heartbeat_t hb{};
    hb.type = 1;
    hb.autopilot = 12;
    hb.base_mode = 81;
    hb.custom_mode = 0;
    hb.system_status = 3;
    hb.mavlink_version = 3;
    mavlink_msg_heartbeat_encode(1, 1, &msg, &hb);

    // Serialize to wire format
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];
    uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);

    bool got_msg = manager.push_bytes(buf, len);
    EXPECT_TRUE(got_msg);

    auto maybe_msg = manager.pop_message();
    ASSERT_TRUE(maybe_msg.has_value());
    EXPECT_EQ(maybe_msg->msgid, MAVLINK_MSG_ID_HEARTBEAT);
    EXPECT_EQ(maybe_msg->sysid, 1);
    EXPECT_EQ(maybe_msg->compid, 1);

    // Queue should be empty after pop
    EXPECT_FALSE(manager.pop_message().has_value());
}

TEST(PixhawkConnectionTest, PopEmptyQueueReturnsNullopt) {
    PixhawkConnection manager("/dev/fake", 115200);
    EXPECT_FALSE(manager.pop_message().has_value());
}

TEST(PixhawkConnectionTest, CloseWithoutConnectIsSafe) {
    PixhawkConnection manager("/dev/fake", 115200);
    manager.close();  // should not throw
    EXPECT_FALSE(manager.is_open());
}

TEST(PixhawkConnectionTest, ConnectConfiguresRequestedBaudRate) {
    int master_fd = posix_openpt(O_RDWR | O_NOCTTY);
    ASSERT_GE(master_fd, 0);
    ASSERT_EQ(grantpt(master_fd), 0);
    ASSERT_EQ(unlockpt(master_fd), 0);
    char* slave_name = ptsname(master_fd);
    ASSERT_NE(slave_name, nullptr);

    PixhawkConnection manager(slave_name, 115200);
    ASSERT_NO_THROW(manager.connect());

    int inspect_fd = ::open(slave_name, O_RDONLY | O_NOCTTY);
    ASSERT_GE(inspect_fd, 0);
    struct termios tty {};
    ASSERT_EQ(tcgetattr(inspect_fd, &tty), 0);
    EXPECT_EQ(cfgetispeed(&tty), B115200);
    EXPECT_EQ(cfgetospeed(&tty), B115200);

    ::close(inspect_fd);
    manager.close();
    ::close(master_fd);
}

TEST(PixhawkConnectionTest, MoveConstructorTransfersState) {
    PixhawkConnection a("/dev/a", 9600);
    PixhawkConnection b(std::move(a));
    EXPECT_EQ(b.device(), "/dev/a");
    EXPECT_EQ(b.baud(), 9600);
    EXPECT_FALSE(a.is_open());  // moved-from has fd = -1
}

TEST(PixhawkConnectionTest, MoveAssignmentTransfersState) {
    PixhawkConnection a("/dev/a", 9600);
    PixhawkConnection b("/dev/b", 115200);
    b = std::move(a);
    EXPECT_EQ(b.device(), "/dev/a");
    EXPECT_EQ(b.baud(), 9600);
}

// CRC helper functions (from mavlink_helpers.h)
// These are needed for test frame construction
// Note: the mavlink_helpers.h already provides crc_accumulate and crc_calculate,
// so we use those directly from the mavlink library.
// We redeclare them here for clarity (they're static inline in the header).

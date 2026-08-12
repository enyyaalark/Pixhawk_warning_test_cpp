# Pixhawk_warning_test_cpp

C++17 implementation of the read-only Pixhawk telemetry monitoring and warning
system. It receives MAVLink telemetry, normalizes values, records and replays
JSON Lines snapshots, and emits local warning events.

## Safety boundary

This application is a telemetry consumer. It does not arm or disarm an aircraft,
change flight modes or parameters, command actuators, upload missions, or send
control feedback to the flight controller. The serial device is opened read-only,
and the UDP transport only binds and receives—it never transmits packets.

## Requirements

- Ubuntu 22.04 or a compatible Linux environment
- CMake 3.16 or newer
- A C++17 compiler
- Git and network access during the first configuration, for pinned CMake dependencies

## Build and test

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Run

Run continuously using the default `/dev/ttyACM0` device:

```bash
./build/src/pixhawk_monitor
```

Run for 30 seconds and record normalized telemetry:

```bash
./build/src/pixhawk_monitor \
  --duration 30 \
  --record logs/telemetry.jsonl
```

Receive MAVLink forwarded locally by QGroundControl over UDP:

```bash
./build/src/pixhawk_monitor \
  --udp-port 14445 \
  --duration 15 \
  --record logs/qgc_udp_attitude_15s.jsonl \
  --record-rate 5
```

The default UDP bind address is `127.0.0.1`, so only programs on the same
computer can send data to the listener. Use QGroundControl's MAVLink forwarding
setting to forward to `127.0.0.1:14445` (or another unused port, provided both
sides use the same value). Restart QGroundControl after changing its forwarding
host. QGroundControl keeps the USB link active;
this application remains a receive-only telemetry consumer.

The verified bench-test flow is:

```text
Pixhawk USB -> QGroundControl -> one-way UDP forwarding -> pixhawk_monitor
```

QGroundControl must remain open while using this flow. Its MAVLink heartbeat
keeps the tested PX4 USB telemetry profile active, while the C++ application only
receives the forwarded packets.

Replay a recording without Pixhawk hardware:

```bash
./build/src/pixhawk_monitor \
  --replay logs/telemetry.jsonl \
  --replay-speed 0
```

`--replay-speed 0` performs immediate replay. Use `1` for original timing or
`2` for twice the recorded speed.

## Verified hardware state

The UDP path was bench-tested with an Auterion PX4 FMUv6C controller running a
custom PX4 1.12.3 build. A 15-second recording contained valid roll, pitch, yaw,
and all three angular-rate fields. The test controller was powered only by USB
and was not connected to a complete aircraft.

That firmware's board defaults force `SYS_HITL=1`, which prevents the real IMU
topics from publishing. For this specific test controller, the following SD-card
startup override was required:

```text
/fs/microsd/etc/config.txt
```

```sh
param set SYS_HITL 0
```

This is a hardware/firmware-specific bench setup, not a general application
requirement. Confirm the installed firmware and startup configuration before
copying it to another controller. The override can be removed from the PX4 shell
with:

```sh
rm /fs/microsd/etc/config.txt
```

The current USB-only bench setup reports `magnetometer_3d` as present and enabled
but unhealthy. This is retained as a real warning rather than suppressed. It does
not block telemetry pipeline development, but yaw must not be treated as a
validated heading and the magnetometer fault must be resolved before flight.

## Current warning scope

- MAVLink heartbeat loss and restoration
- Present and enabled sensors reported unhealthy by `SYS_STATUS`

Battery and aircraft-attitude warning thresholds are intentionally not included
until the relevant hardware and final aircraft type have been validated.

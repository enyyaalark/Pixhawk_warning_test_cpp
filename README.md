# Pixhawk_warning_test_cpp

C++17 implementation of the read-only Pixhawk telemetry monitoring and warning
system. It receives MAVLink telemetry, normalizes values, records and replays
JSON Lines snapshots, and emits local warning events.

## Safety boundary

This application is a telemetry consumer. It does not arm or disarm an aircraft,
change flight modes or parameters, command actuators, upload missions, or send
control feedback to the flight controller. The serial device is opened read-only.

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

Replay a recording without Pixhawk hardware:

```bash
./build/src/pixhawk_monitor \
  --replay logs/telemetry.jsonl \
  --replay-speed 0
```

`--replay-speed 0` performs immediate replay. Use `1` for original timing or
`2` for twice the recorded speed.

## Current warning scope

- MAVLink heartbeat loss and restoration
- Present and enabled sensors reported unhealthy by `SYS_STATUS`

Battery and aircraft-attitude warning thresholds are intentionally not included
until the relevant hardware and final aircraft type have been validated.

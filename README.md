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

The optional Qt 5 PFD prototype also requires the QML runtime modules on
Ubuntu 22.04:

```bash
sudo apt-get install qml-module-qtquick2 qml-module-qtquick-window2
```

Set `-DBUILD_PFD=OFF` when configuring if a headless build must not depend on
Qt. When Qt 5 Core, Gui, Qml, and Quick are available, the default build also
produces `build/src/ui/pixhawk_pfd`.

## Run

For independent PFD and warning consumers without QGroundControl, use the
read-only telemetry distributor documented in
[`docs/TELEMETRY_DISTRIBUTION.md`](docs/TELEMETRY_DISTRIBUTION.md).

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

## Qt PFD replay prototype

The first PFD version deliberately uses normalized JSON Lines replay instead
of opening a second Pixhawk connection. It displays the verified roll, pitch,
and attitude-yaw values in a Qt Quick attitude indicator and loops the selected
recording at its original timing:

```bash
./build/src/ui/pixhawk_pfd \
  --replay logs/qgc_udp_attitude_fixed_15s.jsonl \
  --speed 1
```

Use `--speed 2` for twice-speed demonstration. The default recording path is
the one shown above, so `./build/src/ui/pixhawk_pfd` is sufficient when that
local bench recording exists.

This prototype contains no flight-envelope warning thresholds and does not
invoke TTS. Missing airspeed is explicitly shown as `NO AIR DATA`; attitude
yaw is not presented as a validated magnetic heading. A later live adapter
will feed the same display properties from the existing normalized telemetry
pipeline, while the warning engine remains the single source for future visual
and voice alerts.

## Telemetry freshness

Each normalized telemetry group stores the monotonic time of its most recent
source message and exposes an `is_fresh(now, max_age)` check. Missing data is not
fresh, and a retained value becomes stale when its source stops updating.

Motion uses two independent timestamps because its fields have different MAVLink
sources:

- altitude fields are updated by `ALTITUDE`
- velocity fields are updated by `LOCAL_POSITION_NED`

This prevents fresh altitude messages from making old or never-received velocity
values appear current. Freshness metadata is written to JSON Lines and restored
during replay. Older recordings without this metadata remain readable and are
treated as having unknown freshness.

Sensor-health warnings only evaluate fresh `SYS_STATUS` data. The default maximum
age is three seconds and can be changed with `--sensor-data-timeout`. If status
data becomes stale, an active sensor warning is cleared as unavailable; stale
state is not treated as proof of either a healthy or unhealthy sensor. When data
returns, an unhealthy condition must satisfy the activation delay again.

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

The generic scheduling, PFD alert presentation and audio-output interfaces are
documented in [`docs/ALERT_INFRASTRUCTURE.md`](docs/ALERT_INFRASTRUCTURE.md).
These components do not define aircraft flight-envelope limits.

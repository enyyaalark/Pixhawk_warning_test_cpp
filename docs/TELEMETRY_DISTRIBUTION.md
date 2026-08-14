# Read-only telemetry distribution

`pixhawk_telemetry_hub` is the single owner of the Pixhawk serial device. It
opens the device read-only and fans received MAVLink frames out over UDP:

- `127.0.0.1:14445` — Qt PFD
- `127.0.0.1:14446` — warning and voice service
- `127.0.0.1:14550` — optional QGroundControl UDP link

There is deliberately no UDP-to-serial path. This preserves the project's
read-only safety boundary: commands, parameters and actuator control cannot be
forwarded to the aircraft through this hub.

Build and start the hub:

```bash
cmake --build build --parallel
./scripts/start_telemetry_hub.sh
```

The script uses the stable bench-controller device name. A different device
can be supplied as its first argument:

```bash
./scripts/start_telemetry_hub.sh /dev/serial/by-id/usb-OTHER_DEVICE
```

Start the PFD in a second terminal:

```bash
./build/src/ui/pixhawk_pfd --udp-port 14445 --udp-bind 127.0.0.1
```

Start the warning consumer in a third terminal:

```bash
./build/src/pixhawk_monitor --udp-port 14446 --udp-bind 127.0.0.1
```

QGroundControl is optional. When it is wanted, disable its automatic serial
connection to this controller and add a UDP link listening on port `14550`.
Only one process may own the Pixhawk serial device; do not let QGroundControl
and the hub open it simultaneously.

On the tested QGroundControl 4.4.5 installation, merely adding/listening on UDP
14550 did not disable USB auto-connect: QGroundControl still opened
`/dev/ttyACM0`. Disable the Pixhawk USB auto-connect option under Application
Settings before starting QGroundControl with the hub. Confirm exclusivity with:

```bash
fuser -v /dev/serial/by-id/usb-Auterion_PX4_FMU_v6C.x_0-if00
```

The only serial owner should be `pixhawk_telemetry_hub`. The startup script now
refuses to launch if another process already has the device open.

# Alert infrastructure (rules intentionally incomplete)

The project now separates rule evaluation, scheduling and output:

```text
TelemetryData -> WarningEngine -> WarningScheduler -> PFD / AlertAudioOutput
```

Only the previously approved heartbeat-loss and enabled/unhealthy-sensor rules
exist. Aircraft envelope rules (bank, pitch, stall, overspeed, sink rate,
terrain and flight-phase limits) remain intentionally undefined.

`WarningScheduler` supplies severity ordering, event-code de-duplication,
cooldown, repeat intervals, recovery transitions and the current highest active
event. `AlertAudioOutput` has a deterministic console backend and a non-blocking
WAV backend. WAV names follow `<CODE>_ACTIVE.wav` and `<CODE>_CLEAR.wav`.

The Kokoro model is used offline to generate reviewed WAV assets; neural
inference is not placed in the real-time telemetry loop.

Live PFD freshness limits are currently presentation-safety defaults:

- attitude: 1 second
- altitude: 2 seconds
- local velocity: 2 seconds
- heartbeat: 3 seconds

Expired values become `STALE`; values never observed are `NO DATA`.

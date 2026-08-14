Pixhawk warning voice samples
=============================

Voice configuration
-------------------

Engine: sherpa-onnx 1.13.5, official precompiled CPU package
Model: Kokoro English v0.19
Voice: af_sarah
Speaker ID: 3
Output: mono, 24 kHz, 16-bit PCM WAV

The model weights and local sherpa-onnx checkout are intentionally excluded
from Git. The reviewed WAV files in this directory are runtime-ready alert
assets and may be stored in the repository.

Alert phrase catalogue
----------------------

telemetry_lost.wav                 Telemetry lost.
telemetry_restored.wav             Telemetry restored.
attitude_data_lost.wav             Attitude data lost.
attitude_estimate_invalid.wav      Attitude estimate invalid.
inertial_sensor_failure.wav        Inertial sensor failure.
bank_angle.wav                     Bank angle.
excessive_bank.wav                 Excessive bank.
pitch_high.wav                     Pitch high.
pitch_low.wav                      Pitch low.
attitude_rate.wav                  Attitude rate.
heading_unreliable.wav             Heading unreliable.
navigation_estimate_invalid.wav    Navigation estimate invalid.
airspeed_data_invalid.wav          Airspeed data invalid.
airspeed_low.wav                   Airspeed low.
overspeed.wav                      Overspeed.
stall.wav                          Stall. Stall.
sink_rate.wav                      Sink rate.
altitude_low.wav                   Altitude low.

stall_sarah.wav is the initial approved reference sample for the selected
voice. It contains the same phrase as stall.wav and is retained for comparison.

These phrases are voice-output candidates. Their presence does not mean that
the corresponding warning rule is enabled or validated for flight.

This is the pre-MicroFi MQTT publisher: an Arduino sketch (`xiao-telemetry.ino`) and an
equivalent PlatformIO project (`pio-project/`) that read the ESP32-S3's internal temperature
sensor and publish it as JSON (`{"device_id", "temperature", "humidity", "timestamp"}`) to
`test/sensor/data` over MQTT every 5 seconds — no EFM, no C2, no agent, just a WiFi+MQTT loop.
It's kept here as the starting point; MicroFi (the C2 agent covered by the rest of this
directory) superseded it. Copy `secrets.h.example` to `secrets.h` (sketch folder) or
`pio-project/src/secrets.h.example` to `secrets.h` and fill in real WiFi/broker values before
building either version.

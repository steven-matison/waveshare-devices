# waveshare-devices

Tuna Street's platform work on small ESP32 devices under Cloudera Edge Flow Manager. One
directory per board: the Waveshare 1.8″ AMOLED (a full launcher platform with an EFM agent
running inside it) and the Seeed XIAO ESP32-S3 Sense (three headless agents). Both run
[MicroFi](https://github.com/) — a clean-room microcontroller MiNiFi C2 agent, published
separately.

## amoled-1.8-v2 — Waveshare ESP32-S3-Touch-AMOLED-1.8 V2

CO5300 368×448 QSPI AMOLED, CST820 touch, QMI8658 IMU, AXP2101 PMIC, ES8311 audio, 16 MB
flash / 8 MB octal PSRAM. Runs one platform image —
[ESP-Brookesia](https://github.com/espressif/esp-brookesia) v0.8 (launcher, status bar, App
Store, JS runtime) on a V2 HAL board upstream doesn't have — flashed once. Apps are runtime
packages (`manifest.json` + JS + JSON-UI) dropped onto LittleFS, and the MicroFi agent runs
inside the image as a native component: it adopts Brookesia's WiFi, heartbeats to EFM, and
takes flows over C2 while the launcher keeps running. The board's senses are EFM processors —
`GetIMU` publishes the accelerometer, `DisplayMessage` puts a flow-sent string on the glass.

Layout, bring-up order, the UI kit and panel simulator, and the hard-won facts are in
[`amoled-1.8-v2/README.md`](amoled-1.8-v2/README.md).

**Apps** — five runtime packages, each its own repo:
[`amoled-agent`](https://github.com/TunaStreetTest/amoled-agent) (EFM agent monitor) ·
[`amoled-racing`](https://github.com/TunaStreetTest/amoled-racing) (Cloudera Racing leaderboard + mini game) ·
[`amoled-tminus`](https://github.com/TunaStreetTest/amoled-tminus) (next-launch countdown) ·
[`amoled-xviewer`](https://github.com/TunaStreetTest/amoled-xviewer) (X feed, swipe and like) ·
[`amoled-hello`](https://github.com/TunaStreetTest/amoled-hello) (the template).

## xiao-esp32s3-sense — Seeed XIAO ESP32-S3 Sense

Three identical units — ESP32-S3, 8 MB flash, octal PSRAM, camera + mic + SD on board — each a
headless MicroFi agent under its own EFM agent class, all on one desk over USB. Where the AMOLED
is a platform with an agent inside it, a XIAO is the agent and nothing else: boot → WiFi →
heartbeat → flow. Not a Waveshare board; small-device work lives together here.

| Unit | Class | Track | Flow |
|---|---|---|---|
| MicroFi-1 | `MicroFi-1` | JSON telemetry | `GenerateFlowFile → PublishMQTT` to `test/sensor/data`, payload `{"device_id":"MicroFi-1"}` so central NiFi can key Kafka by class |
| MicroFi-2 | `MicroFi-2` | camera | `CaptureImage → PublishMQTT` — a VGA JPEG every 10 ticks to `microfi2/camera/jpg`, metadata JSON to `microfi2/camera/meta`; a NiFi process group bridges both topics into Kafka |
| MicroFi-3 | `MicroFi-3` | Sparkplug B | `GenerateFlowFile → PublishSparkplug` — real NBIRTH/NDATA on `spBv1.0/MicroFi/…/MicroFi-3` |

### What the agent is

- One firmware image, one partition table (two 2 MB OTA slots + ~3.9 MB LittleFS at
  `0x420000`), interchangeable across all three units. PlatformIO envs `microfi1` / `microfi2` /
  `microfi3` extend a shared `esp32s3-8mb` base and differ only by `CONFIG_MICROFI_AGENT_CLASS`.
- No deployer command. Class and id are compile-time; EFM creates the class on the first
  heartbeat, and the agent acknowledges every C2 apply explicitly (`FULLY_APPLIED` /
  `NOT_APPLIED`).
- A compile-time processor registry — `GenerateFlowFile`, `PublishMQTT`, `ListenHTTP`,
  `SetGPIO`, `CaptureImage`, `PublishSparkplug`. A new capability is a rebuild + reflash, not
  a flow change, and there is no Expression Language.
- Liveness on GPIO21: the orange user LED strobes at 1 s only once every fatal-init gate in
  boot is cleared. A dark LED means boot never finished — not a flow or C2 symptom. The red LED
  is the BQ25101 charge indicator, wired to the charger, never drivable from firmware.

### Limits that shape every flow

- **4 processors per flow** (`kMaxFlowNodes`) — the engine silently drops anything past it.
- **256-byte FlowFile content ceiling** — bytes (camera JPEGs) go broker-direct over MQTT and a
  small metadata FlowFile rides the chain instead.
- **Distinct MQTT Client IDs** for every MQTT-owning processor on a device, or the broker kicks
  the older session on every reconnect.
- A down MQTT sink fills LittleFS (80 % watermark under `DropOldest`) and the agent stalls in
  replay before C2 even starts. `esptool erase-region 0x420000 0x3E0000` clears the filesystem —
  and the saved flow — without touching firmware or NVS; republish the class flow after.

### EFM mechanics learned here

- Pin the Designer palette to a new class's manifest with `DELETE` then
  `POST /efm/api/agent-class-manifest-config` — `POST` alone won't overwrite an existing mapping,
  `PUT` 500s.
- EFM's auto-`UPDATE` on re-registration is one-shot; push a flow again with
  `POST /efm/api/designer/flows/{flowId}/publish`.
- The C2 ack body must omit `agentInfo` / `deviceInfo` / `flowInfo`, or EFM also processes the
  ack as a heartbeat.
- `agent.last_seen` in EFM is not a heartbeat clock — it moves only when a descriptive field on
  the agent entity changes.

### Desk mechanics

- Three identical units are usually plugged in together and Windows renumbers COM ports on
  replug: verify the port by MAC (`python -m serial.tools.list_ports -v`, the `SER=` value)
  before every flash — that listing doesn't reset the board.
- A plain `serial.Serial('COMx')` asserts DTR/RTS and reboots the board, and a DTR reset can
  drop the S3 into ROM download mode (`esptool --chip esp32s3 --port <COM> --after hard-reset
  flash-id` recovers). The no-reset open sequence for watching serial is in the directory README.
- Build + flash one unit: `pio run -e microfi1 -t upload --upload-port <COM>` — native
  Windows, no WSL2 USB passthrough needed.

Where things are: [`xiao-esp32s3-sense/README.md`](xiao-esp32s3-sense/README.md) (hardware,
flash/serial, class mechanics, recovery), [`flows/`](xiao-esp32s3-sense/flows) (the three
agent flows and the NiFi camera bridge — credential-free exports),
[`prototype/`](xiao-esp32s3-sense/prototype) (the pre-agent Arduino/PlatformIO MQTT publisher
this track started from).

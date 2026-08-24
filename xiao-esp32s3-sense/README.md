# xiao-esp32s3-sense — Seeed XIAO ESP32-S3 Sense

Three physical units (MicroFi-1/2/3), each running [MicroFi](https://github.com/) — a
clean-room microcontroller MiNiFi C2 agent (repo to be published separately) — as an EFM-managed
edge agent. Not a Waveshare board, but small-device work belongs alongside the rest of this
repo's platform work.

- **MicroFi-1 — JSON telemetry.** `GenerateFlowFile → PublishMQTT` to `test/sensor/data`, payload
  `{"device_id":"MicroFi-1"}` so central NiFi can key Kafka by class identity.
- **MicroFi-2 — camera.** `CaptureImage → PublishMQTT`: VGA JPEG every 10 ticks to
  `microfi2/camera/jpg`, metadata JSON to `microfi2/camera/meta`. A central-NiFi process group
  bridges both topics into Kafka.
- **MicroFi-3 — Sparkplug B emitter.** `GenerateFlowFile → PublishSparkplug`, real NBIRTH/NDATA
  on `spBv1.0/MicroFi/…/MicroFi-3`.

## Hardware — all three units identical

- Seeed XIAO ESP32-S3 **Sense** (adds camera + mic + SD to the base XIAO ESP32-S3): ESP32-S3
  QFN56 rev v0.2, octal PSRAM.
- **8 MB flash** (GigaDevice GD25Q64, quad, 3.3V, JEDEC-verified on every unit). Earlier notes
  calling this a 2 MB part are wrong — that was a stock PlatformIO board file assuming the
  non-Sense XIAO's smaller chip, not the real hardware.
- Partition table `partitions_8mb.csv`: nvs/otadata/phy_init + two 2 MB OTA app slots + LittleFS
  at offset `0x420000`, size `0x3E0000` (~3.9 MB). Firmware images and this partition table are
  interchangeable across all three units — only the per-device sdkconfig overlay (agent class)
  differs.
- Camera hardware is present on all three units; #1 and #3 have it in reserve beyond their
  current tracks.

## Flash / serial mechanics

- **Verify the COM port by MAC before flashing** — never assume port-to-device mapping, Windows
  renumbers on replug and three identical units are usually plugged in at once:
  ```bash
  python -m serial.tools.list_ports -v
  ```
  This lists each port's MAC as its `SER=` value without resetting the device (unlike opening the
  port to probe it, which asserts DTR/RTS and reboots the board).
- Build + flash a specific unit with PlatformIO (native, no WSL2/usbipd passthrough needed):
  ```bash
  pio run -e microfi1 -t upload --upload-port <COM>
  ```
  Envs: `microfi1` / `microfi2` / `microfi3`, each extending a shared `esp32s3-8mb` base env.
  Only the device envs differ, by `CONFIG_MICROFI_AGENT_CLASS`.
- **To watch serial without rebooting the device**, construct the port unopened, clear DTR/RTS,
  then open it — a plain `serial.Serial('COMx', ...)` constructor call asserts both lines and
  trips the auto-reset circuit:
  ```python
  s = serial.Serial()
  s.port = 'COMx'
  s.dtr = False
  s.rts = False
  s.open()
  ```
- **A DTR-asserting reset drops the S3 into ROM download mode.** Recover with:
  ```bash
  esptool --chip esp32s3 --port <COM> --after hard-reset flash-id
  ```
  then reopen the serial connection normally.
- **LittleFS recovery** (e.g. after the flash filesystem fills, see below) erases only the
  filesystem region, leaving firmware and NVS untouched:
  ```bash
  esptool --chip esp32s3 --port <COM> erase-region 0x420000 0x3E0000
  ```
  This also erases the agent's saved flow definition — republish the class flow after recovery.

## EFM agent class mechanics

- No deployer command — the agent class and id are compile-time sdkconfig values; EFM creates
  the class automatically on the agent's first heartbeat.
- A new class needs the Designer palette pinned to its manifest: **`DELETE` then `POST
  /efm/api/agent-class-manifest-config`** — `POST` alone won't overwrite an existing mapping, and
  `PUT` 500s.
- EFM's auto-`UPDATE` on re-registration is one-shot. To push a flow again, re-publish it
  explicitly: `POST /efm/api/designer/flows/{flowId}/publish`.
- The agent POSTs an explicit acknowledgment after every configuration apply: `POST
  /efm/api/c2-protocol/acknowledge` with `{"operationId": …, "operationState": {"state":
  "FULLY_APPLIED"|"NOT_APPLIED", ...}}`. The body must omit `agentInfo`/`deviceInfo`/`flowInfo` —
  including any of those makes EFM also process the ack as a heartbeat, which is not what you
  want.
- Flows are capped at **4 processors per graph** (`kMaxFlowNodes=4`) — the engine silently drops
  anything past the limit rather than erroring.
- No Expression Language support on the agent — the processor registry is compile-time and
  static; adding a capability is a firmware rebuild + reflash, not a flow change.
- FlowFile content has a **256-byte inline ceiling** — binary payloads (like camera JPEGs) can't
  ride the FlowFile chain. The pattern: publish bytes broker-direct over MQTT, and emit a small
  metadata FlowFile (size/dims/topic) into the normal flow instead.
- Distinct MQTT Client IDs are mandatory for every MQTT-owning processor on one device — reusing
  one causes the broker to kick the older session on every reconnect.
- `agent.last_seen` in EFM is not a heartbeat clock — it only updates when a *descriptive* field
  on the agent entity changes, not on every heartbeat.

## Boot / recovery facts

- A down MQTT sink fills the LittleFS repo (fills to an 80% watermark under `DropOldest`), after
  which the agent stalls in replay before C2 startup even begins. Recover with the
  `erase-region` command above, then republish the class's flow.
- On a filesystem fill or firmware issue, verify the agent actually finished booting before
  assuming the flow or C2 side is broken — see the LED facts below.

## LED facts

- **Agent-liveness strobe, GPIO21** — the onboard orange user LED, active-low, 1-second period —
  starts only after the agent has cleared every fatal-init gate in boot. A dark (non-strobing)
  LED means boot never completed; it is not a flow or C2 symptom.
- A flow-level `SetGPIO` targeting pin 21 fights the liveness strobe — there's no arbitration,
  last writer wins on every flow apply.
- The **red LED is the BQ25101 charge-indicator**, wired directly to the charger chip with no MCU
  connection. It is never firmware-drivable; its glow is pure charging hardware behavior, not a
  bug.

## Layout of this directory

| Path | What |
|---|---|
| `flows/` | EFM Designer flow exports for MicroFi-1 (telemetry) and MicroFi-3 (Sparkplug B emit, and the earlier LED-trigger track), plus the central-NiFi process group that bridges MicroFi-2's camera MQTT topics into Kafka. `flows/README.md` has the file-by-file breakdown. |
| `prototype/` | The pre-MicroFi Arduino/PlatformIO MQTT publisher this track started from — kept as a reference starting point, superseded by the agent. |

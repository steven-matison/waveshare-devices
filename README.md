# waveshare-devices

Tuna Street's platform work on small ESP32 devices under Cloudera Edge Flow Manager. One
directory per board: the Waveshare 1.8″ AMOLED (a full launcher platform with an EFM agent
running inside it) and the Seeed XIAO ESP32-S3 Sense (three headless agents). Both run
[MicroFi](https://github.com/) — a clean-room microcontroller MiNiFi C2 agent, published
separately.

## amoled-1.8-v2 — Waveshare ESP32-S3-Touch-AMOLED-1.8 V2

CO5300 368×448 QSPI AMOLED, CST820 touch, TCA9554 IO expander, AXP2101 PMIC,
QMI8658 IMU, ES8311 audio, 16 MB flash / 8 MB octal PSRAM.

The device runs one platform image — [ESP-Brookesia](https://github.com/espressif/esp-brookesia)
v0.8 (super system: launcher, status bar, App Store, JS runtime) — flashed once.
Everything else arrives without reflashing:

- **Apps are files.** Runtime app packages (`manifest.json` + JS entry +
  JSON-UI resources) dropped into `apps/` on LittleFS or SD install at boot —
  a new tile appears on the launcher. `apps/tunastreet.hello/` here is, as far
  as we know, the first runtime package built for Brookesia v0.8 outside
  Espressif — upstream ships the runtime but no example package.
- **The device is a fleet citizen.** A [MicroFi](https://github.com/) EFM/MiNiFi
  C2 agent runs inside the platform image as a native background component:
  it adopts Brookesia's WiFi, heartbeats to Cloudera Edge Flow Manager, and
  receives flow definitions over C2 — while the launcher keeps running. The
  board's senses are its processors: `GetIMU` (QMI8658, with a motion
  threshold so a bump is an event) and `DisplayMessage` (a flow-sent string
  onto the glass).

### Layout

| Path | What |
|---|---|
| `platform/overlay/` | Our delta over the pinned esp-brookesia master: the **V2 HAL board** (`esp32_s3_touch_amoled_1_8_v2` — upstream only has the V1 SH8601/FT3168 board), the `microfi_agent` guest component, super-example wiring (WiFi pre-provision + agent task + status tile), the LVGL gesture-routing fix, and the Tuna Street boot screen resources |
| `platform/setup.sh` | Clone upstream at `PINNED_UPSTREAM`, apply overlay, select board, stage the `tunastreet.*` app packages, build |
| `platform/sdkconfig.microfi` | Agent + platform config (WiFi creds live in gitignored `sdkconfig.local`) |
| `bringup/amoled-colorbar/` | Minimal display bring-up: `esp_lcd_co5300` + `draw_bitmap` only. **Flash this first on a new board** — solid color on the glass before any LVGL/launcher work |
| `apps/tunastreet.*/` | The runtime app packages — developed here, published as their own repos (below) |
| `uikit/` | **panelkit** — generates every app screen from `tokens.json` so an illegal size, layout or tap contract fails at generation time, not on the glass; `lint.py` is the pre-flash gate |
| `tools/simulator/` | Runs an app's real `app.js` + screen off-device at true 368×448 — headless for scoring, in a browser for watching; `lint.js --check` before every flash |
| `tools/` | Windows-side flash + serial: `bootlog.py` (reset + capture), `readlog.py` (attach without reset), `stage_apps.py`, `lint_shell.py` (the kit's rules over the shell resources) |
| `boot-screen/compose.py` | Generates the 368×448 boot splash (pixel tuna on black) |

### Hard-won facts

- The CO5300 is **QSPI** — the RGB/vsync LVGL path gives a permanently black
  panel. Prove the display with `bringup/amoled-colorbar` before anything else.
- Panel init order: AXP2101 rails → TCA9554 reset pulse → panel. QSPI pins
  CS=12, PCLK=11, D0–3=4/5/6/7, X-offset 16. Touch is CST820 via the
  `esp_lcd_touch_cst816s` driver at 0x15, INT=13, RST=39.
- Brookesia master needs **ESP-IDF 6.0–6.2** on the S3 (IDF 6 no longer
  bundles cmake/ninja — `pip install cmake ninja` into the IDF venv works).
- A guest component's static BSS competes with the display's internal-DMA
  buffers — `microfi_agent`'s 79.6 KB BSS goes to PSRAM via a linker fragment
  (`extram_bss`), leaving 262 B internal.
- The example's `littlefs/` tree is a **staged output**: boot-screen edits go
  in `system/brookesia_system_super/resource/startup/` or the build silently
  reverts them. A full-size 368×448 splash needs `CONFIG_LV_CACHE_DEF_SIZE`
  ≥ ~660 KB or it silently doesn't render.
- Programmatic WiFi provisioning: `SetConnectAp` alone stores the target;
  `GeneralAction::Connect` is what joins (`Start` is ignored once the service
  is already Started).
- LVGL sets `GESTURE_BUBBLE` on every parented object, so a `gesture` declared
  in a screen.json never fires until the overlay's `event.cpp` clears the flag.
  Swipe is the only reliable navigation; a tap zone on a swipe surface fires on
  press *and* release and eats the drag.
- Fonts are compiled-in Montserrat faces, ASCII only, and an off-ladder
  `fontSize` rounds **down** with no warning (11sp drew at 8px).
- The launcher grid comes from `constants/portrait.json` (3 columns, 108dp
  tile, 16dp gap): nine tiles fit without scrolling. Launcher order is install
  order — natives first — and no manifest field changes it.

Built on the desk at Tuna Street. The V2 HAL board is a candidate for an
upstream esp-brookesia PR.

### Apps

Five ESP-Brookesia v0.8 runtime app packages, each published as its own repo
with its own release history:

- [`amoled-agent`](https://github.com/TunaStreetTest/amoled-agent) — MicroFi EFM agent monitor: live heartbeat, processor count, and the metrics the board ships to Cloudera Edge Flow Manager.
- [`amoled-racing`](https://github.com/TunaStreetTest/amoled-racing) — live Cloudera Racing leaderboard plus a three-screen mini racing game, sprites rasterised from the upstream game's own SVGs.
- [`amoled-tminus`](https://github.com/TunaStreetTest/amoled-tminus) — a true-black launch clock counting down to the next real rocket launch (Launch Library 2).
- [`amoled-xviewer`](https://github.com/TunaStreetTest/amoled-xviewer) — swipe an X feed one card at a time, tap to like.
- [`amoled-hello`](https://github.com/TunaStreetTest/amoled-hello) — the minimal runtime app template: one label on a black screen, the starting point for a new app.

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

# Flow exports

| File | Agent/class | What it does |
|---|---|---|
| `microfi-1-telemetry.json` | `MicroFi` (pre-migration class, superseded by per-device `MicroFi-1/2/3`) | `GenerateFlowFile-XiaoTelemetry` (64 B tick) → `PublishMQTT-XiaoTelemetry` to `test/sensor/data`; `ListenHTTP-Trigger` on `:8095/test` parked alongside it. EFM agent-class flow export (has the full `agentManifest`). |
| `microfi-3-sparkplug.json` | `MicroFi-3` | `GenerateFlowFile-SpbTick` (1s) → `PublishSparkplug-Telemetry`, broker `mqtt://192.168.1.121:1883`, client `microfi3-spb`, group `MicroFi`, metric `Sensors/Temperature`. Real NBIRTH/NDATA on `spBv1.0/MicroFi/…/MicroFi-3`. EFM Designer flow export. |
| `microfi-3-led.json` | `MicroFi-3` (prior track, backed up before the device was reassigned to Sparkplug B) | `ListenHTTP-LED` (`:8095/led`) → `SetGPIO-UserLED` (pin 21, active-low, `Invert=true`) — toggles the onboard LED on POST. EFM Designer flow export. |
| `nifi-microfi2-camera-bridge.json` | Central NiFi (not an EFM agent flow) | `ConsumeMQTT-MicroFi2Camera` (`microfi2/camera/#`, client `nifi-microfi2-camera`) → `PublishKafka-MicroFi2Camera` (topic `${mqtt.topic:replaceAll('/', '.')}`, key `${mqtt.topic}`) → `LogKafkaFailure` on the failure relationship. Bridges MicroFi-2's camera MQTT output into Kafka. NiFi process-group export. |

All four are as exported — no credential values were present (every `Password`/`sasl.password` property is `null`; the property descriptors just document which fields are sensitive). IPs left in place are LAN addresses (`192.168.1.x`) and in-cluster Kubernetes DNS (`*.svc.cluster.local`), not secrets.

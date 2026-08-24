// XIAO ESP32-S3 telemetry publisher — v1 (efm-xiao.md) + Sparkplug B leg (#126,
// efm-sparkplug-b-hardware-lab-plan.md). Additive: the original plain-JSON publish
// to test/sensor/data is unchanged; NBIRTH/NDATA on spBv1.0/<group>/... is a second,
// independent leg over the same MQTT connection.

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <BasicTag.h>
#include <SparkplugNode.h>
#include "secrets.h"

static const char* DEVICE_ID = "XiaoESP32-01";
static const char* MQTT_TOPIC = "test/sensor/data";
static const unsigned long PUBLISH_INTERVAL_MS = 5000;

static const char* SPARKPLUG_GROUP_ID = "XiaoTelemetry";
static const char* SPARKPLUG_NODE_ID = DEVICE_ID;
static const size_t SPARKPLUG_PAYLOAD_BUFFER_SIZE = 512;

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
unsigned long lastPublish = 0;

float g_temperature = 0.0f;
SparkplugNodeConfig* g_spNode = NULL;
bool g_wasMqttConnected = false;

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  Serial.printf("WiFi: connecting to %s...\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(250);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\nWiFi: connected, IP %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\nWiFi: connect timed out, will retry");
  }
}

void connectMQTT() {
  if (mqttClient.connected()) return;
  Serial.printf("MQTT: connecting to %s:%d...\n", MQTT_BROKER, MQTT_PORT);
  String clientId = String(DEVICE_ID) + "-" + String((uint32_t)ESP.getEfuseMac(), HEX);
  if (mqttClient.connect(clientId.c_str())) {
    Serial.println("MQTT: connected");
  } else {
    Serial.printf("MQTT: connect failed, rc=%d\n", mqttClient.state());
  }
}

float readInternalTempC() {
  // Core-provided cross-variant internal temp sensor (works on S3, unlike the classic-ESP32-only ROM call).
  return temperatureRead();
}

uint64_t sparkplugTimestampMs() {
  // NTP-synced real epoch (setup() blocks on sync before this is ever called for a birth/data payload).
  return (uint64_t)time(nullptr) * 1000ULL;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("xiao-telemetry starting");
  connectWiFi();
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  // PubSubClient's default MQTT_MAX_PACKET_SIZE (256 bytes) is smaller than an
  // NBIRTH payload (node's own bdSeq/rebirth/scan-rate tags + ours) -- publish()
  // silently returns false over that limit. Match SPARKPLUG_PAYLOAD_BUFFER_SIZE.
  mqttClient.setBufferSize(SPARKPLUG_PAYLOAD_BUFFER_SIZE);

  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  Serial.print("NTP: syncing");
  time_t nowSecs = time(nullptr);
  unsigned long ntpStart = millis();
  while (nowSecs < 8 * 3600 * 2 && millis() - ntpStart < 15000) {
    delay(250);
    Serial.print(".");
    nowSecs = time(nullptr);
  }
  Serial.printf("\nNTP: epoch now %ld\n", (long)nowSecs);

  g_temperature = readInternalTempC();

  g_spNode = createSparkplugNode(SPARKPLUG_GROUP_ID, SPARKPLUG_NODE_ID,
                                  SPARKPLUG_PAYLOAD_BUFFER_SIZE, sparkplugTimestampMs);
  if (g_spNode == NULL) {
    Serial.println("Sparkplug: FAILED to create node");
  } else {
    *(g_spNode->vars.scan_rate_tag_value) = PUBLISH_INTERVAL_MS;
    FunctionalBasicTag* tempTag = createFloatTag("Sensors/Temperature", &g_temperature,
                                                  getNextAlias(), false, false);
    if (tempTag == NULL) {
      Serial.println("Sparkplug: FAILED to create temperature tag");
    }
    Serial.printf("Sparkplug: node ready, NBIRTH=%s NDATA=%s\n",
                  g_spNode->topics.NBIRTH, g_spNode->topics.NDATA);
  }
}

void publishSparkplug() {
  if (g_spNode == NULL) return;

  bool nowConnected = mqttClient.connected();
  if (nowConnected && !g_wasMqttConnected) {
    spnOnMQTTConnected(g_spNode);
  } else if (!nowConnected && g_wasMqttConnected) {
    spnOnMQTTDisconnected(g_spNode);
  }
  g_wasMqttConnected = nowConnected;

  if (!nowConnected) return;

  SparkplugNodeState state = tickSparkplugNode(g_spNode);
  switch (state) {
    case spn_NBIRTH_PL_READY:
      if (mqttClient.publish(g_spNode->mqtt_message.topic,
                              g_spNode->mqtt_message.payload->buffer,
                              g_spNode->mqtt_message.payload->written_length)) {
        Serial.printf("Sparkplug: published NBIRTH (%u bytes) -> %s\n",
                      (unsigned)g_spNode->mqtt_message.payload->written_length,
                      g_spNode->mqtt_message.topic);
        spnOnPublishNBIRTH(g_spNode);
      } else {
        Serial.println("Sparkplug: NBIRTH publish FAILED");
      }
      break;
    case spn_NDATA_PL_READY:
      if (mqttClient.publish(g_spNode->mqtt_message.topic,
                              g_spNode->mqtt_message.payload->buffer,
                              g_spNode->mqtt_message.payload->written_length)) {
        Serial.printf("Sparkplug: published NDATA (%u bytes) -> %s\n",
                      (unsigned)g_spNode->mqtt_message.payload->written_length,
                      g_spNode->mqtt_message.topic);
        spnOnPublishNDATA(g_spNode);
      } else {
        Serial.println("Sparkplug: NDATA publish FAILED");
      }
      break;
    case spn_MAKE_NBIRTH_FAILED:
      Serial.println("Sparkplug: ERROR making NBIRTH payload");
      break;
    case spn_MAKE_NDATA_FAILED:
      Serial.println("Sparkplug: ERROR making NDATA payload");
      break;
    default:
      break;
  }
}

void loop() {
  connectWiFi();
  connectMQTT();
  mqttClient.loop();

  unsigned long now = millis();
  if (now - lastPublish >= PUBLISH_INTERVAL_MS) {
    lastPublish = now;
    g_temperature = readInternalTempC();

    if (mqttClient.connected()) {
      JsonDocument doc;
      doc["device_id"] = DEVICE_ID;
      doc["temperature"] = g_temperature;
      doc["humidity"] = nullptr;
      doc["timestamp"] = (uint32_t)time(nullptr);

      char payload[192];
      size_t len = serializeJson(doc, payload, sizeof(payload));

      bool ok = mqttClient.publish(MQTT_TOPIC, (uint8_t*)payload, len, false);
      Serial.printf("publish %s -> %s: %s\n", MQTT_TOPIC, payload, ok ? "ok" : "FAILED");
    } else {
      Serial.println("skip publish: MQTT not connected");
    }
  }

  publishSparkplug();
}

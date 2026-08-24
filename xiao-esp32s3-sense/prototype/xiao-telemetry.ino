// XIAO ESP32-S3 telemetry publisher — v1 per efm-xiao.md
// Publishes JSON to Mosquitto on test/sensor/data every ~5s, matching the shape
// ConsumeMQTT in the SparkPlug PG already filters on:
//   {"device_id": "...", "temperature": <float>, "humidity": null, "timestamp": <epoch>}
// Starting metric is the ESP32-S3's internal temperature sensor — no external hardware.

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include "secrets.h"

static const char* DEVICE_ID = "XiaoESP32-01";
static const char* MQTT_TOPIC = "test/sensor/data";
static const unsigned long PUBLISH_INTERVAL_MS = 5000;

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
unsigned long lastPublish = 0;

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

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("xiao-telemetry starting");
  connectWiFi();
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);

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
}

void loop() {
  connectWiFi();
  connectMQTT();
  mqttClient.loop();

  unsigned long now = millis();
  if (now - lastPublish >= PUBLISH_INTERVAL_MS) {
    lastPublish = now;

    if (mqttClient.connected()) {
      JsonDocument doc;
      doc["device_id"] = DEVICE_ID;
      doc["temperature"] = readInternalTempC();
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
}

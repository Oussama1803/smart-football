// Goalkeeper MPU6050 acquisition and telemetry publisher (Étape 11).
#include <Arduino.h>
#include <Wire.h>
#include <esp_task_wdt.h>
#include "imu_logic.h"

#ifndef SF_KEEPER_NETWORK_ENABLED
#define SF_KEEPER_NETWORK_ENABLED 0
#endif

#if SF_KEEPER_NETWORK_ENABLED
#include <WiFi.h>
#include <mqtt_client.h>
#endif

constexpr uint8_t MPU_ADDRESS = 0x68;
constexpr uint8_t SDA_PIN = 21, SCL_PIN = 22;
constexpr const char *DEVICE_ID = "keeper-esp32-01";
constexpr const char *SESSION_ID = "session-001";
constexpr const char *FIRMWARE_VERSION = "0.1.0";

static bool initialized = false;
static uint32_t next_sample = 0;
static uint32_t event_counter = 0;
static char json_buffer[512];

#if SF_KEEPER_NETWORK_ENABLED
static esp_mqtt_client_handle_t mqtt_client = nullptr;
static bool mqtt_connected = false;

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
  if (event_id == MQTT_EVENT_CONNECTED) {
    mqtt_connected = true;
    Serial.println("[MQTT] Keeper connected to broker");
  } else if (event_id == MQTT_EVENT_DISCONNECTED) {
    mqtt_connected = false;
    Serial.println("[MQTT] Keeper disconnected");
  }
}

static void init_network() {
  WiFi.begin("Wokwi-GUEST", "");
  esp_mqtt_client_config_t mqtt_cfg = {};
  mqtt_cfg.broker.address.uri = "mqtt://10.0.2.2:1883";
  mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
  esp_mqtt_client_register_event(mqtt_client, MQTT_EVENT_ANY, mqtt_event_handler, nullptr);
  esp_mqtt_client_start(mqtt_client);
}
#endif

static bool write_register(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(MPU_ADDRESS);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

static bool read_registers(uint8_t reg, uint8_t *data, size_t size) {
  Wire.beginTransmission(MPU_ADDRESS);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(MPU_ADDRESS, size, true) != size) return false;
  for (size_t i = 0; i < size; ++i) data[i] = Wire.read();
  return true;
}

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN, 400000);
  Wire.setTimeOut(10);
  esp_task_wdt_init(3, true);
  esp_task_wdt_add(nullptr);
  Serial.println("[BOOT] smart-football keeper acquisition; MQTT contract established");
#if SF_KEEPER_NETWORK_ENABLED
  init_network();
#endif
}

void loop() {
  esp_task_wdt_reset();
  const uint32_t now = millis();
  if ((int32_t)(now - next_sample) >= 0) {
    next_sample = now + (initialized ? 100 : 1000);
    if (!initialized) {
      uint8_t identity = 0;
      initialized = read_registers(0x75, &identity, 1) && identity == 0x68 &&
        write_register(0x6b, 0x01) && write_register(0x1b, 0) && write_register(0x1c, 0);
      Serial.println(initialized ? "[IMU] ready" : "[IMU] unavailable; retrying");
    } else {
      uint8_t bytes[14];
      if (read_registers(0x3b, bytes, sizeof(bytes))) {
        const auto motion = keeper::decode(bytes);
        // Retain strict format expected by Wokwi motion.test.yaml
        Serial.printf("[IMU] accel_g=%.3f,%.3f,%.3f rotation_dps=%.3f,%.3f,%.3f\n",
          motion.accel_g[0], motion.accel_g[1], motion.accel_g[2],
          motion.rotation_dps[0], motion.rotation_dps[1], motion.rotation_dps[2]);

        const char *dive = keeper::classify_dive(motion);
        if (strcmp(dive, "none") != 0 || (event_counter % 20 == 0)) {
          char event_id[48];
          snprintf(event_id, sizeof(event_id), "keeper-evt-%lu", (unsigned long)++event_counter);
          char timestamp[32];
          snprintf(timestamp, sizeof(timestamp), "1970-01-01T00:00:%02luZ", (unsigned long)(now / 1000));
          keeper::format_json(
            json_buffer, sizeof(json_buffer),
            event_id, DEVICE_ID, SESSION_ID, timestamp,
            motion, FIRMWARE_VERSION
          );
#if SF_KEEPER_NETWORK_ENABLED
          if (mqtt_connected && mqtt_client) {
            esp_mqtt_client_publish(mqtt_client, "smartfootball/keeper-esp32-01/events", json_buffer, 0, 1, 0);
          }
#endif
        }
      } else { initialized = false; Serial.println("[IMU] read failed"); }
    }
  }
  delay(1);
}

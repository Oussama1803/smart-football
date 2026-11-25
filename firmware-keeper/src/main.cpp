// Local acquisition only; the keeper MQTT contract needs backend coordination.
#include <Arduino.h>
#include <Wire.h>
#include <esp_task_wdt.h>
#include "imu_logic.h"

constexpr uint8_t MPU_ADDRESS = 0x68;
constexpr uint8_t SDA_PIN = 21, SCL_PIN = 22;
static bool initialized = false;
static uint32_t next_sample = 0;

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
  Serial.println("[BOOT] smart-football keeper acquisition; MQTT contract pending");
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
        Serial.printf("[IMU] accel_g=%.3f,%.3f,%.3f rotation_dps=%.3f,%.3f,%.3f\n",
          motion.accel_g[0], motion.accel_g[1], motion.accel_g[2],
          motion.rotation_dps[0], motion.rotation_dps[1], motion.rotation_dps[2]);
      } else { initialized = false; Serial.println("[IMU] read failed"); }
    }
  }
  delay(1);
}

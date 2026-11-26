#pragma once
#include <stdint.h>
#include <stdio.h>

namespace keeper {
struct Motion { float accel_g[3]; float rotation_dps[3]; };
inline int16_t signed_word(const uint8_t *bytes) {
  const uint16_t word = ((uint16_t)bytes[0] << 8) | bytes[1];
  return word < 0x8000 ? (int16_t)word : (int16_t)((int32_t)word - 65536);
}
// MPU6050 register burst from ACCEL_XOUT_H, +/-2g and +/-250 deg/s.
inline Motion decode(const uint8_t bytes[14]) {
  Motion sample = {};
  for (int i = 0; i < 3; ++i) {
    sample.accel_g[i] = signed_word(bytes + 2 * i) / 16384.0f;
    sample.rotation_dps[i] = signed_word(bytes + 8 + 2 * i) / 131.0f;
  }
  return sample;
}

inline const char* classify_dive(const Motion &m) {
  if (m.accel_g[0] > 0.8f || m.rotation_dps[2] > 45.0f) return "right";
  if (m.accel_g[0] < -0.8f || m.rotation_dps[2] < -45.0f) return "left";
  if (m.accel_g[1] > 1.2f) return "jump";
  if (m.accel_g[1] < -0.8f) return "dive";
  return "none";
}

inline int format_json(
    char *out,
    size_t size,
    const char *event_id,
    const char *device_id,
    const char *session_id,
    const char *timestamp_iso,
    const Motion &m,
    const char *firmware_version
) {
  return snprintf(
      out,
      size,
      "{\"event_id\":\"%s\",\"device_id\":\"%s\",\"session_id\":\"%s\","
      "\"event_type\":\"keeper_motion\",\"timestamp\":\"%s\","
      "\"accel_x\":%.3f,\"accel_y\":%.3f,\"accel_z\":%.3f,"
      "\"gyro_x\":%.2f,\"gyro_y\":%.2f,\"gyro_z\":%.2f,"
      "\"dive_direction\":\"%s\",\"firmware_version\":\"%s\"}",
      event_id,
      device_id,
      session_id,
      timestamp_iso,
      m.accel_g[0],
      m.accel_g[1],
      m.accel_g[2],
      m.rotation_dps[0],
      m.rotation_dps[1],
      m.rotation_dps[2],
      classify_dive(m),
      firmware_version
  );
}
}  // namespace keeper

#pragma once
#include <stdint.h>

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
}  // namespace keeper

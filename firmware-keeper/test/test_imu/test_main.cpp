#include <unity.h>
#include "imu_logic.h"
void setUp() {}
void tearDown() {}
void test_stationary_gravity_and_zero_rotation() {
  uint8_t data[14] = {};
  data[4] = 0x40;
  const auto motion = keeper::decode(data);
  TEST_ASSERT_EQUAL_FLOAT(1, motion.accel_g[2]);
  TEST_ASSERT_EQUAL_FLOAT(0, motion.accel_g[0]);
  TEST_ASSERT_EQUAL_FLOAT(0, motion.rotation_dps[2]);
}
void test_signed_big_endian_extremes_and_axis_order() {
  uint8_t data[14] = {0xc0,0, 0x20,0, 0x80,0, 0,0, 0x00,0x83, 0xff,0x7d, 0x2e,0x0e};
  const auto motion = keeper::decode(data);
  TEST_ASSERT_EQUAL_FLOAT(-1, motion.accel_g[0]);
  TEST_ASSERT_EQUAL_FLOAT(0.5, motion.accel_g[1]);
  TEST_ASSERT_EQUAL_FLOAT(-2, motion.accel_g[2]);
  TEST_ASSERT_EQUAL_FLOAT(1, motion.rotation_dps[0]);
  TEST_ASSERT_EQUAL_FLOAT(-1, motion.rotation_dps[1]);
  TEST_ASSERT_EQUAL_FLOAT(90, motion.rotation_dps[2]);
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(test_stationary_gravity_and_zero_rotation);
  RUN_TEST(test_signed_big_endian_extremes_and_axis_order);
  return UNITY_END();
}

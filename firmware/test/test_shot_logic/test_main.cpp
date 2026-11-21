// Native unit tests for the pure shot logic. Run with: pio test -e native
#include <string.h>
#include <unity.h>

#include "shot_logic.h"

using namespace sf;

void setUp(void) {}
void tearDown(void) {}

// ---- EF1: speed from the delay between the two beams --------------------------------

void test_ef1_speed_at_9_10_11_ms(void) {
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 80.0f, speed_kmh_from_delta_us(9000));
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 72.0f, speed_kmh_from_delta_us(10000));
  // 65.45 km/h exactly; the brief rounds it to 65.5.
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 65.5f, speed_kmh_from_delta_us(11000));
}

void test_ef1_reference_value_is_72_kmh(void) {
  // D = 0.20 m, dt = 0.01 s -> 20 m/s -> 72 km/h (reference of all tests).
  TEST_ASSERT_EQUAL_FLOAT(72.0f, speed_kmh_from_delta_us(10000));
}

void test_ef1_within_5_percent_on_whole_range(void) {
  // Check against an independent double computation for a spread of delays.
  const uint32_t delays[] = {4800, 5000, 8000, 10000, 25000, 60000, 100000, 144000};
  for (size_t i = 0; i < sizeof(delays) / sizeof(delays[0]); i++) {
    const double expected = 0.20 / (delays[i] / 1e6) * 3.6;
    const double got = speed_kmh_from_delta_us(delays[i]);
    TEST_ASSERT_TRUE(got >= expected * 0.95 && got <= expected * 1.05);
  }
}

void test_zero_delay_is_zero_speed(void) {
  TEST_ASSERT_EQUAL_FLOAT(0.0f, speed_kmh_from_delta_us(0));
}

// ---- SEC2: speed range 5 to 150 km/h ---------------------------------------------------

void test_sec2_limits_in_delay(void) {
  // 150 km/h <-> 4800 us ; 5 km/h <-> 144000 us. Just below, at, just above.
  TEST_ASSERT_EQUAL(SPEED_TOO_FAST, check_delta_us(4799));
  TEST_ASSERT_EQUAL(SPEED_OK, check_delta_us(4800));
  TEST_ASSERT_EQUAL(SPEED_OK, check_delta_us(4801));
  TEST_ASSERT_EQUAL(SPEED_OK, check_delta_us(143999));
  TEST_ASSERT_EQUAL(SPEED_OK, check_delta_us(144000));
  TEST_ASSERT_EQUAL(SPEED_TOO_SLOW, check_delta_us(144001));
}

void test_sec2_limits_in_speed(void) {
  TEST_ASSERT_EQUAL(SPEED_TOO_SLOW, check_speed(4.99f));
  TEST_ASSERT_EQUAL(SPEED_OK, check_speed(5.0f));
  TEST_ASSERT_EQUAL(SPEED_OK, check_speed(5.01f));
  TEST_ASSERT_EQUAL(SPEED_OK, check_speed(149.99f));
  TEST_ASSERT_EQUAL(SPEED_OK, check_speed(150.0f));
  TEST_ASSERT_EQUAL(SPEED_TOO_FAST, check_speed(150.01f));
}

void test_sec2_rejects_1_ms_and_200_ms(void) {
  TEST_ASSERT_EQUAL(SPEED_TOO_FAST, check_delta_us(1000));    // 720 km/h
  TEST_ASSERT_EQUAL(SPEED_TOO_SLOW, check_delta_us(200000));  // 3.6 km/h
}

void test_sec2_rejects_null_delay(void) {
  TEST_ASSERT_EQUAL(SPEED_TOO_FAST, check_delta_us(0));
}

void test_nominal_shot_is_accepted(void) {
  TEST_ASSERT_EQUAL(SPEED_OK, check_delta_us(10000));
}

// ---- Zone selection (EF2, D2) -----------------------------------------------------------

void test_zone_3_strongest_gives_lower_left(void) {
  const uint16_t peaks[ZONE_COUNT] = {120, 400, 2500, 600};
  uint16_t intensity = 0;
  const Zone zone = pick_zone(peaks, 300, &intensity);
  TEST_ASSERT_EQUAL(ZONE_LOWER_LEFT, zone);  // piezo of zone 3 is index 2
  TEST_ASSERT_EQUAL_UINT16(2500, intensity);
  TEST_ASSERT_EQUAL_STRING("lower_left", zone_name(zone));
}

void test_zone_below_threshold_is_none(void) {
  const uint16_t peaks[ZONE_COUNT] = {10, 299, 120, 50};
  uint16_t intensity = 77;
  TEST_ASSERT_EQUAL(ZONE_NONE, pick_zone(peaks, 300, &intensity));
  TEST_ASSERT_EQUAL_UINT16(0, intensity);
  TEST_ASSERT_EQUAL_STRING("none", zone_name(ZONE_NONE));
}

void test_zone_threshold_is_inclusive(void) {
  const uint16_t peaks[ZONE_COUNT] = {0, 300, 0, 0};
  TEST_ASSERT_EQUAL(ZONE_UPPER_RIGHT, pick_zone(peaks, 300, nullptr));
}

void test_zone_tie_picks_lowest_index(void) {
  const uint16_t peaks[ZONE_COUNT] = {0, 800, 800, 0};
  TEST_ASSERT_EQUAL(ZONE_UPPER_RIGHT, pick_zone(peaks, 300, nullptr));
}

void test_zone_names_for_all_four_zones(void) {
  TEST_ASSERT_EQUAL_STRING("upper_left", zone_name(ZONE_UPPER_LEFT));
  TEST_ASSERT_EQUAL_STRING("upper_right", zone_name(ZONE_UPPER_RIGHT));
  TEST_ASSERT_EQUAL_STRING("lower_left", zone_name(ZONE_LOWER_LEFT));
  TEST_ASSERT_EQUAL_STRING("lower_right", zone_name(ZONE_LOWER_RIGHT));
}

// ---- Debounce -----------------------------------------------------------------------------

void test_debounce(void) {
  TEST_ASSERT_TRUE(debounce_accept(false, 0, 5, DEBOUNCE_US));  // first edge
  TEST_ASSERT_FALSE(debounce_accept(true, 1000, 1500, DEBOUNCE_US));
  TEST_ASSERT_TRUE(debounce_accept(true, 1000, 2000, DEBOUNCE_US));
  // Wrap-around of micros().
  TEST_ASSERT_FALSE(debounce_accept(true, 0xFFFFFF00u, 0x00000010u, DEBOUNCE_US));
  TEST_ASSERT_TRUE(debounce_accept(true, 0xFFFFFF00u, 0x00000800u, DEBOUNCE_US));
}

// ---- Passage tracker ----------------------------------------------------------------------

void test_tracker_normal_shot(void) {
  Tracker t;
  tracker_init(&t);
  uint32_t dt = 0;
  TEST_ASSERT_EQUAL(TRACKER_NO_EVENT, tracker_on_beam(&t, 1, 1000, &dt));
  TEST_ASSERT_EQUAL(TRACKER_SHOT, tracker_on_beam(&t, 2, 11000, &dt));
  TEST_ASSERT_EQUAL_UINT32(10000, dt);
  TEST_ASSERT_EQUAL_FLOAT(72.0f, speed_kmh_from_delta_us(dt));
}

void test_tracker_wraparound(void) {
  Tracker t;
  tracker_init(&t);
  uint32_t dt = 0;
  tracker_on_beam(&t, 1, 0xFFFFFF00u, &dt);
  TEST_ASSERT_EQUAL(TRACKER_SHOT, tracker_on_beam(&t, 2, 0x00000200u, &dt));
  TEST_ASSERT_EQUAL_UINT32(0x300u, dt);
}

void test_tracker_abort_after_300_ms(void) {
  Tracker t;
  tracker_init(&t);
  uint32_t dt = 0;
  tracker_on_beam(&t, 1, 5000, &dt);
  TEST_ASSERT_EQUAL(TRACKER_NO_EVENT, tracker_poll(&t, 5000 + 299999));
  TEST_ASSERT_EQUAL(TRACKER_NO_EVENT, tracker_poll(&t, 5000 + 300000));
  TEST_ASSERT_EQUAL(TRACKER_TIMEOUT, tracker_poll(&t, 5000 + 300001));
  // Re-armed by itself: a new shot works.
  TEST_ASSERT_EQUAL(TRACKER_NO_EVENT, tracker_on_beam(&t, 1, 400000, &dt));
  TEST_ASSERT_EQUAL(TRACKER_SHOT, tracker_on_beam(&t, 2, 410000, &dt));
}

void test_tracker_late_beam2_after_abort_does_not_make_a_shot(void) {
  Tracker t;
  tracker_init(&t);
  uint32_t dt = 0;
  tracker_on_beam(&t, 1, 0, &dt);
  TEST_ASSERT_EQUAL(TRACKER_TIMEOUT, tracker_poll(&t, 400000));
  // Beam 2 alone now looks like a ball coming from the wrong side.
  TEST_ASSERT_EQUAL(TRACKER_WRONG_DIRECTION, tracker_on_beam(&t, 2, 500000, &dt));
}

void test_tracker_wrong_direction(void) {
  Tracker t;
  tracker_init(&t);
  uint32_t dt = 0;
  TEST_ASSERT_EQUAL(TRACKER_WRONG_DIRECTION, tracker_on_beam(&t, 2, 1000, &dt));
  // The beam 1 edge that follows must not produce a shot.
  TEST_ASSERT_EQUAL(TRACKER_NO_EVENT, tracker_on_beam(&t, 1, 11000, &dt));
  TEST_ASSERT_EQUAL(TRACKER_NO_EVENT, tracker_poll(&t, 1000 + 300001));
  // After the timeout the tracker is idle again.
  TEST_ASSERT_EQUAL(TRACKER_NO_EVENT, tracker_on_beam(&t, 1, 400000, &dt));
  TEST_ASSERT_EQUAL(TRACKER_SHOT, tracker_on_beam(&t, 2, 410000, &dt));
}

void test_tracker_duplicate_beam1_is_ignored(void) {
  Tracker t;
  tracker_init(&t);
  uint32_t dt = 0;
  tracker_on_beam(&t, 1, 1000, &dt);
  TEST_ASSERT_EQUAL(TRACKER_NO_EVENT, tracker_on_beam(&t, 1, 3000, &dt));
  TEST_ASSERT_EQUAL(TRACKER_SHOT, tracker_on_beam(&t, 2, 11000, &dt));
  TEST_ASSERT_EQUAL_UINT32(10000, dt);  // measured from the first edge
}

void test_tracker_ignores_edges_until_rearmed(void) {
  Tracker t;
  tracker_init(&t);
  uint32_t dt = 0;
  tracker_on_beam(&t, 1, 0, &dt);
  tracker_on_beam(&t, 2, 10000, &dt);
  TEST_ASSERT_EQUAL(TRACKER_NO_EVENT, tracker_on_beam(&t, 1, 20000, &dt));
  TEST_ASSERT_EQUAL(TRACKER_NO_EVENT, tracker_on_beam(&t, 2, 30000, &dt));
  tracker_rearm(&t);
  TEST_ASSERT_EQUAL(TRACKER_NO_EVENT, tracker_on_beam(&t, 1, 100000, &dt));
  TEST_ASSERT_EQUAL(TRACKER_SHOT, tracker_on_beam(&t, 2, 110000, &dt));
}

void test_tracker_poll_while_idle_does_nothing(void) {
  Tracker t;
  tracker_init(&t);
  TEST_ASSERT_EQUAL(TRACKER_NO_EVENT, tracker_poll(&t, 999999999u));
}

// ---- Message formatting ----------------------------------------------------------------------

void test_shot_json_matches_the_brief(void) {
  ShotMessage m;
  m.event_id = "b3f1c2a0-7d4e-4f6a-9c1e-2a5d8e7f0b11";
  m.device_id = "goal-esp32-01";
  m.session_id = "session-001";
  m.timestamp = "2026-11-10T14:30:00Z";
  m.ball_speed_kmh = 72.0f;
  m.pressure_reading = 640;
  m.target_zone = "upper_right";
  m.impact_intensity = 845;
  m.firmware_version = "0.1.0";
  char buf[320];
  const size_t n = build_shot_json(buf, sizeof(buf), m);
  const char *expected =
      "{\"event_id\":\"b3f1c2a0-7d4e-4f6a-9c1e-2a5d8e7f0b11\",\"device_id\":\"goal-esp32-01\","
      "\"session_id\":\"session-001\",\"event_type\":\"shot\","
      "\"timestamp\":\"2026-11-10T14:30:00Z\",\"ball_speed_kmh\":72.0,"
      "\"pressure_reading\":640,\"target_zone\":\"upper_right\",\"impact_intensity\":845,"
      "\"firmware_version\":\"0.1.0\"}";
  TEST_ASSERT_EQUAL_UINT32(strlen(expected), n);
  TEST_ASSERT_EQUAL_STRING(expected, buf);
}

void test_shot_json_rejects_small_buffer(void) {
  ShotMessage m = {"a", "b", "c", "d", 1.0f, 1, "none", 0, "0.1.0"};
  char buf[20];
  TEST_ASSERT_EQUAL_UINT32(0, build_shot_json(buf, sizeof(buf), m));
}

void test_status_json_for_rejected_speed(void) {
  char buf[200];
  build_status_json(buf, sizeof(buf), "goal-esp32-01", "1970-01-01T00:00:05Z", "error",
                    "vitesse rejetée");
  TEST_ASSERT_EQUAL_STRING(
      "{\"device_id\":\"goal-esp32-01\",\"timestamp\":\"1970-01-01T00:00:05Z\","
      "\"level\":\"error\",\"message\":\"vitesse rejetée\"}",
      buf);
}

void test_uuid_v4_format(void) {
  const uint32_t words[4] = {0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu};
  char uuid[37];
  format_uuid_v4(uuid, words);
  TEST_ASSERT_EQUAL_STRING("ffffffff-ffff-4fff-bfff-ffffffffffff", uuid);
  const uint32_t zeros[4] = {0, 0, 0, 0};
  format_uuid_v4(uuid, zeros);
  TEST_ASSERT_EQUAL_STRING("00000000-0000-4000-8000-000000000000", uuid);
}

void test_iso8601_known_dates(void) {
  char s[21];
  format_iso8601_utc(s, 0);
  TEST_ASSERT_EQUAL_STRING("1970-01-01T00:00:00Z", s);
  format_iso8601_utc(s, 951782400u);  // leap day 2000
  TEST_ASSERT_EQUAL_STRING("2000-02-29T00:00:00Z", s);
  format_iso8601_utc(s, 1700000000u);
  TEST_ASSERT_EQUAL_STRING("2023-11-14T22:13:20Z", s);
  format_iso8601_utc(s, 4294967295u);  // end of the uint32 range
  TEST_ASSERT_EQUAL_STRING("2106-02-07T06:28:15Z", s);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_ef1_speed_at_9_10_11_ms);
  RUN_TEST(test_ef1_reference_value_is_72_kmh);
  RUN_TEST(test_ef1_within_5_percent_on_whole_range);
  RUN_TEST(test_zero_delay_is_zero_speed);
  RUN_TEST(test_sec2_limits_in_delay);
  RUN_TEST(test_sec2_limits_in_speed);
  RUN_TEST(test_sec2_rejects_1_ms_and_200_ms);
  RUN_TEST(test_sec2_rejects_null_delay);
  RUN_TEST(test_nominal_shot_is_accepted);
  RUN_TEST(test_zone_3_strongest_gives_lower_left);
  RUN_TEST(test_zone_below_threshold_is_none);
  RUN_TEST(test_zone_threshold_is_inclusive);
  RUN_TEST(test_zone_tie_picks_lowest_index);
  RUN_TEST(test_zone_names_for_all_four_zones);
  RUN_TEST(test_debounce);
  RUN_TEST(test_tracker_normal_shot);
  RUN_TEST(test_tracker_wraparound);
  RUN_TEST(test_tracker_abort_after_300_ms);
  RUN_TEST(test_tracker_late_beam2_after_abort_does_not_make_a_shot);
  RUN_TEST(test_tracker_wrong_direction);
  RUN_TEST(test_tracker_duplicate_beam1_is_ignored);
  RUN_TEST(test_tracker_ignores_edges_until_rearmed);
  RUN_TEST(test_tracker_poll_while_idle_does_nothing);
  RUN_TEST(test_shot_json_matches_the_brief);
  RUN_TEST(test_shot_json_rejects_small_buffer);
  RUN_TEST(test_status_json_for_rejected_speed);
  RUN_TEST(test_uuid_v4_format);
  RUN_TEST(test_iso8601_known_dates);
  return UNITY_END();
}

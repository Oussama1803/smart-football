#include "shot_logic.h"

#include <stdio.h>

namespace sf {

// ---- Speed --------------------------------------------------------------------------

float speed_kmh_from_delta_us(uint32_t delta_us) {
  if (delta_us == 0) {
    return 0.0f;
  }
  const float delta_s = (float)delta_us / 1000000.0f;
  const float speed_m_s = BEAM_SPACING_M / delta_s;
  return speed_m_s * 3.0f;
}

SpeedStatus check_speed(float speed_kmh) {
  if (speed_kmh < MIN_SPEED_KMH) {
    return SPEED_TOO_SLOW;
  }
  if (speed_kmh > MAX_SPEED_KMH) {
    return SPEED_TOO_FAST;
  }
  return SPEED_OK;
}

SpeedStatus check_delta_us(uint32_t delta_us) {
  if (delta_us == 0) {
    return SPEED_TOO_FAST;
  }
  return check_speed(speed_kmh_from_delta_us(delta_us));
}

// ---- Zones --------------------------------------------------------------------------

Zone pick_zone(const uint16_t peaks[ZONE_COUNT], uint16_t threshold, uint16_t *intensity_out) {
  Zone best = ZONE_NONE;
  uint16_t best_peak = 0;
  for (int i = 0; i < ZONE_COUNT; i++) {
    if (peaks[i] >= threshold && peaks[i] > best_peak) {
      best = (Zone)i;
      best_peak = peaks[i];
    }
  }
  if (intensity_out != nullptr) {
    *intensity_out = best_peak;
  }
  return best;
}

const char *zone_name(Zone zone) {
  switch (zone) {
    case ZONE_UPPER_LEFT:
      return "upper_left";
    case ZONE_UPPER_RIGHT:
      return "upper_right";
    case ZONE_LOWER_LEFT:
      return "lower_left";
    case ZONE_LOWER_RIGHT:
      return "lower_right";
    default:
      return "none";
  }
}

// ---- Passage tracker ------------------------------------------------------------------

void tracker_init(Tracker *tracker) {
  tracker->state = TRACKER_IDLE;
  tracker->first_edge_us = 0;
}

TrackerEvent tracker_on_beam(Tracker *tracker, int beam, uint32_t t_us, uint32_t *delta_us) {
  switch (tracker->state) {
    case TRACKER_IDLE:
      tracker->first_edge_us = t_us;
      if (beam == 1) {
        tracker->state = TRACKER_WAIT_BEAM2;
        return TRACKER_NO_EVENT;
      }
      // Beam 2 first: the ball goes the wrong way. Ignore everything until the timeout.
      tracker->state = TRACKER_REVERSED;
      return TRACKER_WRONG_DIRECTION;
    case TRACKER_WAIT_BEAM2:
      if (beam == 2) {
        if (delta_us != nullptr) {
          *delta_us = (uint32_t)(t_us - tracker->first_edge_us);
        }
        tracker->state = TRACKER_DONE;
        return TRACKER_SHOT;
      }
      return TRACKER_NO_EVENT;  // second edge on beam 1: ignored
    case TRACKER_DONE:
    case TRACKER_REVERSED:
    default:
      return TRACKER_NO_EVENT;
  }
}

TrackerEvent tracker_poll(Tracker *tracker, uint32_t now_us) {
  const uint32_t elapsed = (uint32_t)(now_us - tracker->first_edge_us);
  if (tracker->state == TRACKER_WAIT_BEAM2 && elapsed > ABORT_TIMEOUT_US) {
    tracker->state = TRACKER_IDLE;
    return TRACKER_TIMEOUT;
  }
  if (tracker->state == TRACKER_REVERSED && elapsed > ABORT_TIMEOUT_US) {
    tracker->state = TRACKER_IDLE;
  }
  return TRACKER_NO_EVENT;
}

void tracker_rearm(Tracker *tracker) {
  tracker->state = TRACKER_IDLE;
}

// ---- Message formatting ---------------------------------------------------------------

size_t build_shot_json(char *buf, size_t capacity, const ShotMessage &m) {
  const int written = snprintf(
      buf, capacity,
      "{\"event_id\":\"%s\",\"device_id\":\"%s\",\"session_id\":\"%s\","
      "\"event_type\":\"shot\",\"timestamp\":\"%s\",\"ball_speed_kmh\":%.1f,"
      "\"pressure_reading\":%u,\"target_zone\":\"%s\",\"impact_intensity\":%u,"
      "\"firmware_version\":\"%s\"}",
      m.event_id, m.device_id, m.session_id, m.timestamp, (double)m.ball_speed_kmh,
      (unsigned)m.pressure_reading, m.target_zone, (unsigned)m.impact_intensity,
      m.firmware_version);
  if (written < 0 || (size_t)written >= capacity) {
    return 0;
  }
  return (size_t)written;
}

size_t build_status_json(char *buf, size_t capacity, const char *device_id,
                         const char *timestamp, const char *level, const char *message) {
  const int written = snprintf(
      buf, capacity,
      "{\"device_id\":\"%s\",\"timestamp\":\"%s\",\"level\":\"%s\",\"message\":\"%s\"}",
      device_id, timestamp, level, message);
  if (written < 0 || (size_t)written >= capacity) {
    return 0;
  }
  return (size_t)written;
}

void format_uuid_v4(char out[37], const uint32_t random_words[4]) {
  uint8_t b[16];
  for (int i = 0; i < 4; i++) {
    b[i * 4 + 0] = (uint8_t)(random_words[i] >> 24);
    b[i * 4 + 1] = (uint8_t)(random_words[i] >> 16);
    b[i * 4 + 2] = (uint8_t)(random_words[i] >> 8);
    b[i * 4 + 3] = (uint8_t)(random_words[i]);
  }
  b[6] = (uint8_t)((b[6] & 0x0F) | 0x40);  // version 4
  b[8] = (uint8_t)((b[8] & 0x3F) | 0x80);  // variant 10xx
  snprintf(out, 37, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
           b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7], b[8], b[9], b[10], b[11], b[12],
           b[13], b[14], b[15]);
}

void format_iso8601_utc(char out[21], uint32_t unix_seconds) {
  // Civil-from-days algorithm (Howard Hinnant), valid for the whole uint32 range.
  const uint32_t days = unix_seconds / 86400u;
  const uint32_t secs = unix_seconds % 86400u;
  const int64_t z = (int64_t)days + 719468;
  const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  const int64_t doe = z - era * 146097;
  const int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const int64_t mp = (5 * doy + 2) / 153;
  const int day = (int)(doy - (153 * mp + 2) / 5 + 1);
  const int month = (int)(mp < 10 ? mp + 3 : mp - 9);
  const int year = (int)(yoe + era * 400 + (month <= 2 ? 1 : 0));
  snprintf(out, 21, "%04d-%02d-%02dT%02u:%02u:%02uZ", year, month, day,
           (unsigned)(secs / 3600), (unsigned)((secs % 3600) / 60), (unsigned)(secs % 60));
}

}  // namespace sf

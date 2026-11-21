// Pure shot logic: no Arduino, no hardware, no global state. Everything here runs in the
// native unit tests (`pio test -e native`).
//
// What each sensor gives us (cahier des charges, section 13):
//   - the IR beams give the ball speed between two points, nothing else;
//   - the piezos give a probable zone and a relative intensity, not a force in Newton;
//   - the FSR gives a relative pressure, not a speed or a shot power.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace sf {

// ---- Physics and limits (EF1, SEC2) -------------------------------------------------
// Distance between the two IR beams. MUST match BEAM_SPACING_M in chips/ir-break-beam/main.c.
constexpr float BEAM_SPACING_M = 0.20f;
constexpr float MIN_SPEED_KMH = 5.0f;    // below: rejected (SEC2)
constexpr float MAX_SPEED_KMH = 150.0f;  // above: rejected (SEC2)
// If beam 2 is not cut within this delay after beam 1, the measure is dropped.
constexpr uint32_t ABORT_TIMEOUT_US = 300000;
// Two edges on the same beam closer than this are treated as bounce.
constexpr uint32_t DEBOUNCE_US = 1000;

constexpr int ZONE_COUNT = 4;

// ---- Speed --------------------------------------------------------------------------
enum SpeedStatus { SPEED_OK, SPEED_TOO_SLOW, SPEED_TOO_FAST };

// v = D / dt ; v_kmh = v * 3.6. Returns 0 when dt is 0.
float speed_kmh_from_delta_us(uint32_t delta_us);
SpeedStatus check_speed(float speed_kmh);
// Same check from the raw delay; a null delay is an impossible (too fast) speed.
SpeedStatus check_delta_us(uint32_t delta_us);

// ---- Debounce (inline: called from an interrupt handler) -----------------------------
inline bool debounce_accept(bool has_last, uint32_t last_us, uint32_t now_us,
                            uint32_t min_gap_us) {
  return !has_last || (uint32_t)(now_us - last_us) >= min_gap_us;
}

// ---- Zones --------------------------------------------------------------------------
enum Zone {
  ZONE_NONE = -1,
  ZONE_UPPER_LEFT = 0,
  ZONE_UPPER_RIGHT = 1,
  ZONE_LOWER_LEFT = 2,
  ZONE_LOWER_RIGHT = 3
};

// Strongest peak at or above the threshold wins (D2: vibration reaches several piezos,
// the strongest one is the one closest to the impact). On a tie the lowest index wins.
// `intensity_out` receives that peak (raw ADC value), or 0 when there is no impact.
Zone pick_zone(const uint16_t peaks[ZONE_COUNT], uint16_t threshold, uint16_t *intensity_out);
// "upper_left", "upper_right", "lower_left", "lower_right", or "none".
const char *zone_name(Zone zone);

// ---- Passage tracker (direction, incomplete events, abort timeout, re-arm) -----------
enum TrackerState { TRACKER_IDLE, TRACKER_WAIT_BEAM2, TRACKER_DONE, TRACKER_REVERSED };
enum TrackerEvent { TRACKER_NO_EVENT, TRACKER_SHOT, TRACKER_WRONG_DIRECTION, TRACKER_TIMEOUT };

struct Tracker {
  TrackerState state;
  uint32_t first_edge_us;
};

void tracker_init(Tracker *tracker);
// beam is 1 or 2. For TRACKER_SHOT, `delta_us` receives t(beam 2) - t(beam 1).
// Timestamps are micros() values: 32-bit wrap-around is handled.
TrackerEvent tracker_on_beam(Tracker *tracker, int beam, uint32_t t_us, uint32_t *delta_us);
// Call regularly. Detects the abort timeout (and re-arms by itself).
TrackerEvent tracker_poll(Tracker *tracker, uint32_t now_us);
// After a finished shot, go back to waiting for beam 1.
void tracker_rearm(Tracker *tracker);

// ---- Message formatting ---------------------------------------------------------------
struct ShotMessage {
  const char *event_id;
  const char *device_id;
  const char *session_id;
  const char *timestamp;  // ISO 8601 UTC
  float ball_speed_kmh;
  uint16_t pressure_reading;  // raw ADC value
  const char *target_zone;    // one of the 4 zone names, or "none"
  uint16_t impact_intensity;  // raw ADC value
  const char *firmware_version;
};

// Compact JSON with the field names of the cahier des charges (section 6). Returns the
// length, or 0 if the buffer is too small.
size_t build_shot_json(char *buf, size_t capacity, const ShotMessage &message);
// {"device_id","timestamp","level","message"}: for the status topic. The message must not
// contain quotes or backslashes (the firmware builds it from fixed strings).
size_t build_status_json(char *buf, size_t capacity, const char *device_id,
                         const char *timestamp, const char *level, const char *message);

// UUID version 4 text (36 chars + NUL) from 128 random bits.
void format_uuid_v4(char out[37], const uint32_t random_words[4]);
// "YYYY-MM-DDTHH:MM:SSZ" (20 chars + NUL) from a Unix time in seconds.
void format_iso8601_utc(char out[21], uint32_t unix_seconds);

}  // namespace sf

// IR break-beam (emitter + receiver pair), simulated for Wokwi.
//
// Output: OUT is HIGH while the beam is intact, LOW while the beam is cut.
//
// Demo problem: nobody can cut two beams 10 ms apart by hand. Two instances of this
// chip are wired together through their SYNC pin:
//   - the MASTER (attribute role = 1, default) owns the controls "ballSpeedKmh" and "shoot";
//   - the SLAVE  (attribute role = 2, set in diagram.json) ignores its own controls.
// When "shoot" goes 0 -> 1 on the master:
//   - the master cuts its beam and raises SYNC;
//   - SYNC falls after dt = D / v, and the slave cuts its beam on that falling edge.
// So beam 2 is cut exactly D / v after beam 1, with the real ball speed.
//
// Wokwi does not give this chip a pushbutton control: only "range" sliders exist, so
// "shoot" is a 0/1 slider. It is driven the same way from a Wokwi CI scenario.

#include "wokwi-api.h"

// Distance between the two beams. MUST match BEAM_SPACING_M in firmware/lib/shot_logic/src/shot_logic.h.
#define BEAM_SPACING_M 0.20f
// km/h = (D / dt) * 3.6, so dt_us = D * 3.6 * 1e6 / v_kmh.
#define DT_US_TIMES_KMH (BEAM_SPACING_M * 3.6f * 1000000.0f)
// How long a beam stays cut when the ball passes through it.
#define BEAM_CUT_US 10000
// Period of the "shoot" slider polling.
#define POLL_US 1000
#define ROLE_MASTER 1

static pin_t pin_out;
static pin_t pin_sync;
static uint32_t attr_role;
static uint32_t attr_speed;
static uint32_t attr_shoot;
static timer_t timer_poll;
static timer_t timer_pulse;
static timer_t timer_release;
static bool previous_shoot = false;
static bool is_master = true;

static void cut_beam(void) {
  pin_write(pin_out, LOW);
  timer_start(timer_release, BEAM_CUT_US, false);
}

static void on_release(void *user_data) {
  (void)user_data;
  pin_write(pin_out, HIGH);
}

static void on_pulse_end(void *user_data) {
  (void)user_data;
  pin_write(pin_sync, LOW);
}

static void on_sync_fall(void *user_data, pin_t pin, uint32_t value) {
  (void)user_data;
  (void)pin;
  (void)value;
  cut_beam();
}

static void on_poll(void *user_data) {
  (void)user_data;
  bool shoot = attr_read_float(attr_shoot) >= 0.5f;
  if (shoot && !previous_shoot) {
    float speed = attr_read_float(attr_speed);
    if (speed < 0.1f) {
      speed = 0.1f;
    }
    uint32_t dt_us = (uint32_t)(DT_US_TIMES_KMH / speed + 0.5f);
    pin_write(pin_sync, HIGH);
    cut_beam();
    timer_start(timer_pulse, dt_us, false);
  }
  previous_shoot = shoot;
}

void chip_init(void) {
  attr_role = attr_init_float("role", ROLE_MASTER);
  attr_speed = attr_init_float("ballSpeedKmh", 72.0f);
  attr_shoot = attr_init_float("shoot", 0.0f);
  is_master = ((uint32_t)attr_read_float(attr_role)) == ROLE_MASTER;

  pin_out = pin_init("OUT", OUTPUT_HIGH);

  const timer_config_t release_config = {.callback = on_release, .user_data = 0};
  timer_release = timer_init(&release_config);

  if (is_master) {
    pin_sync = pin_init("SYNC", OUTPUT_LOW);
    const timer_config_t pulse_config = {.callback = on_pulse_end, .user_data = 0};
    timer_pulse = timer_init(&pulse_config);
    const timer_config_t poll_config = {.callback = on_poll, .user_data = 0};
    timer_poll = timer_init(&poll_config);
    timer_start(timer_poll, POLL_US, true);
  } else {
    pin_sync = pin_init("SYNC", INPUT_PULLDOWN);
    const pin_watch_config_t watch = {
        .user_data = 0, .edge = FALLING, .pin_change = on_sync_fall};
    pin_watch(pin_sync, &watch);
  }
}

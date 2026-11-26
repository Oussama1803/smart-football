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

typedef struct {
  pin_t pin_out;
  pin_t pin_sync;
  uint32_t attr_speed;
  uint32_t attr_shoot;
  timer_t timer_poll;
  timer_t timer_pulse;
  timer_t timer_release;
  bool previous_shoot;
} chip_state_t;

// The freestanding build has no malloc. Reserve one distinct context per IR in
// diagram.json. Wokwi calls chip_init for each instance in the shared module.
#define MAX_IR_INSTANCES 2
static chip_state_t instances[MAX_IR_INSTANCES];
static uint32_t instance_count;

static void cut_beam(chip_state_t *chip) {
  pin_write(chip->pin_out, LOW);
  timer_start(chip->timer_release, BEAM_CUT_US, false);
}

static void on_release(void *user_data) {
  chip_state_t *chip = user_data;
  pin_write(chip->pin_out, HIGH);
}

static void on_pulse_end(void *user_data) {
  chip_state_t *chip = user_data;
  pin_write(chip->pin_sync, LOW);
}

static void on_sync_fall(void *user_data, pin_t pin, uint32_t value) {
  chip_state_t *chip = user_data;
  (void)pin;
  if (value == LOW) {
    cut_beam(chip);
  }
}

static void on_poll(void *user_data) {
  chip_state_t *chip = user_data;
  bool shoot = attr_read_float(chip->attr_shoot) >= 0.5f;
  if (shoot && !chip->previous_shoot) {
    float speed = attr_read_float(chip->attr_speed);
    if (speed < 0.1f) {
      speed = 0.1f;
    }
    uint32_t dt_us = (uint32_t)(DT_US_TIMES_KMH / speed + 0.5f);
    pin_write(chip->pin_sync, HIGH);
    cut_beam(chip);
    timer_start(chip->timer_pulse, dt_us, false);
  }
  chip->previous_shoot = shoot;
}

void chip_init(void) {
  // Fail visibly rather than alias an existing sensor if the diagram grows.
  if (instance_count >= MAX_IR_INSTANCES) {
    __builtin_trap();
  }
  chip_state_t *chip = &instances[instance_count++];
  chip->previous_shoot = false;
  const uint32_t attr_role = attr_init_float("role", ROLE_MASTER);
  chip->attr_speed = attr_init_float("ballSpeedKmh", 72.0f);
  chip->attr_shoot = attr_init_float("shoot", 0.0f);
  const bool is_master = ((uint32_t)attr_read_float(attr_role)) == ROLE_MASTER;

  chip->pin_out = pin_init("OUT", OUTPUT_HIGH);

  const timer_config_t release_config = {.callback = on_release, .user_data = chip};
  chip->timer_release = timer_init(&release_config);

  if (is_master) {
    chip->pin_sync = pin_init("SYNC", OUTPUT_LOW);
    const timer_config_t pulse_config = {.callback = on_pulse_end, .user_data = chip};
    chip->timer_pulse = timer_init(&pulse_config);
    const timer_config_t poll_config = {.callback = on_poll, .user_data = chip};
    chip->timer_poll = timer_init(&poll_config);
    timer_start(chip->timer_poll, POLL_US, true);
  } else {
    chip->pin_sync = pin_init("SYNC", INPUT_PULLDOWN);
    const pin_watch_config_t watch = {
        .user_data = chip, .edge = FALLING, .pin_change = on_sync_fall};
    pin_watch(chip->pin_sync, &watch);
  }
}

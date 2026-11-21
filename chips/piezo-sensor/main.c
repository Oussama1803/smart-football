// Piezoelectric sensor, simulated for Wokwi (one instance per goal zone).
//
// Output (analog): when "trigger" goes 0 -> 1, OUT jumps to "peakVolts" and then decays
// exponentially, like the envelope of a vibration after an impact. This is a simplified
// envelope, not the real oscillation of a piezo, and the value is a relative intensity,
// not a force in Newton.

#include "wokwi-api.h"

#define POLL_US 1000
#define DECAY_STEP_US 100
// exp(-DECAY_STEP / tau) with tau = 5 ms: exp(-0.1 / 5).
#define DECAY_FACTOR 0.980199f
#define SILENCE_VOLTS 0.005f

static pin_t pin_out;
static uint32_t attr_peak;
static uint32_t attr_trigger;
static timer_t timer_poll;
static timer_t timer_decay;
static float level_volts = 0.0f;
static bool previous_trigger = false;

static void on_decay(void *user_data) {
  (void)user_data;
  level_volts *= DECAY_FACTOR;
  if (level_volts < SILENCE_VOLTS) {
    level_volts = 0.0f;
    timer_stop(timer_decay);
  }
  pin_dac_write(pin_out, level_volts);
}

static void on_poll(void *user_data) {
  (void)user_data;
  bool trigger = attr_read_float(attr_trigger) >= 0.5f;
  if (trigger && !previous_trigger) {
    level_volts = attr_read_float(attr_peak);
    pin_dac_write(pin_out, level_volts);
    timer_start(timer_decay, DECAY_STEP_US, true);
  }
  previous_trigger = trigger;
}

void chip_init(void) {
  attr_peak = attr_init_float("peakVolts", 2.0f);
  attr_trigger = attr_init_float("trigger", 0.0f);

  pin_out = pin_init("OUT", ANALOG);
  pin_dac_write(pin_out, 0.0f);

  const timer_config_t decay_config = {.callback = on_decay, .user_data = 0};
  timer_decay = timer_init(&decay_config);
  const timer_config_t poll_config = {.callback = on_poll, .user_data = 0};
  timer_poll = timer_init(&poll_config);
  timer_start(timer_poll, POLL_US, true);
}

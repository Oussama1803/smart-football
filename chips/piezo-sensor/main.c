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

typedef struct {
  pin_t pin_out;
  uint32_t attr_peak;
  uint32_t attr_trigger;
  timer_t timer_poll;
  timer_t timer_decay;
  float level_volts;
  bool previous_trigger;
} chip_state_t;

// One context per zone in diagram.json; no libc allocator in this build.
#define MAX_PIEZO_INSTANCES 4
static chip_state_t instances[MAX_PIEZO_INSTANCES];
static uint32_t instance_count;

static void on_decay(void *user_data) {
  chip_state_t *chip = user_data;
  chip->level_volts *= DECAY_FACTOR;
  if (chip->level_volts < SILENCE_VOLTS) {
    chip->level_volts = 0.0f;
    timer_stop(chip->timer_decay);
  }
  pin_dac_write(chip->pin_out, chip->level_volts);
}

static void on_poll(void *user_data) {
  chip_state_t *chip = user_data;
  bool trigger = attr_read_float(chip->attr_trigger) >= 0.5f;
  if (trigger && !chip->previous_trigger) {
    chip->level_volts = attr_read_float(chip->attr_peak);
    pin_dac_write(chip->pin_out, chip->level_volts);
    timer_start(chip->timer_decay, DECAY_STEP_US, true);
  }
  chip->previous_trigger = trigger;
}

void chip_init(void) {
  if (instance_count >= MAX_PIEZO_INSTANCES) {
    __builtin_trap();
  }
  chip_state_t *chip = &instances[instance_count++];
  chip->level_volts = 0.0f;
  chip->previous_trigger = false;
  chip->attr_peak = attr_init_float("peakVolts", 2.0f);
  chip->attr_trigger = attr_init_float("trigger", 0.0f);

  chip->pin_out = pin_init("OUT", ANALOG);
  pin_dac_write(chip->pin_out, 0.0f);

  const timer_config_t decay_config = {.callback = on_decay, .user_data = chip};
  chip->timer_decay = timer_init(&decay_config);
  const timer_config_t poll_config = {.callback = on_poll, .user_data = chip};
  chip->timer_poll = timer_init(&poll_config);
  timer_start(chip->timer_poll, POLL_US, true);
}

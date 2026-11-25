// Test-only stimulus generator for the Smart Football Wokwi VS Code project.
// A rising edge on "run" emits beam 1, beam 2 10 ms later, and the selected
// piezo envelope 12 ms after beam 2. Production firmware and sensor chips stay
// untouched.
#include "../../../chips/wokwi-api.h"

#define POLL_US 1000
#define BEAM_DELTA_US 10000
#define BEAM_CUT_US 10000
#define IMPACT_AFTER_BEAM2_US 12000
#define DECAY_STEP_US 100
#define DECAY_FACTOR 0.980199f
#define SILENCE_VOLTS 0.005f
#define SEQUENCE_LOCK_US 300000

enum {
  SCENARIO_UPPER_LEFT = 0,
  SCENARIO_UPPER_RIGHT = 1,
  SCENARIO_LOWER_LEFT = 2,
  SCENARIO_LOWER_RIGHT = 3,
  SCENARIO_NO_IMPACT = 4,
  SCENARIO_SIMULTANEOUS = 5,
};

typedef struct {
  pin_t beam1;
  pin_t beam2;
  pin_t piezo[4];
  uint32_t attr_scenario;
  uint32_t attr_run;
  timer_t poll_timer;
  timer_t beam2_timer;
  timer_t beam1_release_timer;
  timer_t beam2_release_timer;
  timer_t impact_timer;
  timer_t decay_timer;
  timer_t unlock_timer;
  float level[4];
  uint32_t scenario;
  bool previous_run;
  bool locked;
} chip_state_t;

static chip_state_t state;

static void on_beam1_release(void *user_data) {
  chip_state_t *chip = user_data;
  pin_write(chip->beam1, HIGH);
}

static void on_beam2_release(void *user_data) {
  chip_state_t *chip = user_data;
  pin_write(chip->beam2, HIGH);
}

static void on_impact(void *user_data) {
  chip_state_t *chip = user_data;
  switch (chip->scenario) {
    case SCENARIO_UPPER_LEFT: chip->level[0] = 3.3f; break;
    case SCENARIO_UPPER_RIGHT: chip->level[1] = 3.3f; break;
    case SCENARIO_LOWER_LEFT: chip->level[2] = 3.3f; break;
    case SCENARIO_LOWER_RIGHT: chip->level[3] = 3.3f; break;
    case SCENARIO_SIMULTANEOUS:
      chip->level[0] = 1.0f;
      chip->level[3] = 3.3f;
      break;
    case SCENARIO_NO_IMPACT:
    default: break;
  }
  bool has_impact = false;
  for (int i = 0; i < 4; ++i) {
    pin_dac_write(chip->piezo[i], chip->level[i]);
    has_impact |= chip->level[i] > 0.0f;
  }
  if (has_impact) timer_start(chip->decay_timer, DECAY_STEP_US, true);
}

static void on_beam2(void *user_data) {
  chip_state_t *chip = user_data;
  pin_write(chip->beam2, LOW);
  timer_start(chip->beam2_release_timer, BEAM_CUT_US, false);
  timer_start(chip->impact_timer, IMPACT_AFTER_BEAM2_US, false);
}

static void on_decay(void *user_data) {
  chip_state_t *chip = user_data;
  bool has_level = false;
  for (int i = 0; i < 4; ++i) {
    chip->level[i] *= DECAY_FACTOR;
    if (chip->level[i] < SILENCE_VOLTS) chip->level[i] = 0.0f;
    pin_dac_write(chip->piezo[i], chip->level[i]);
    has_level |= chip->level[i] > 0.0f;
  }
  if (!has_level) timer_stop(chip->decay_timer);
}

static void on_unlock(void *user_data) {
  chip_state_t *chip = user_data;
  chip->locked = false;
}

static void on_poll(void *user_data) {
  chip_state_t *chip = user_data;
  const bool run = attr_read_float(chip->attr_run) >= 0.5f;
  if (run && !chip->previous_run && !chip->locked) {
    float selected = attr_read_float(chip->attr_scenario);
    if (selected < 0.0f) selected = 0.0f;
    if (selected > SCENARIO_SIMULTANEOUS) selected = SCENARIO_SIMULTANEOUS;
    chip->scenario = (uint32_t)(selected + 0.5f);
    chip->locked = true;
    for (int i = 0; i < 4; ++i) {
      chip->level[i] = 0.0f;
      pin_dac_write(chip->piezo[i], 0.0f);
    }
    pin_write(chip->beam1, LOW);
    timer_start(chip->beam1_release_timer, BEAM_CUT_US, false);
    timer_start(chip->beam2_timer, BEAM_DELTA_US, false);
    timer_start(chip->unlock_timer, SEQUENCE_LOCK_US, false);
  }
  chip->previous_run = run;
}

static void init_timer(timer_t *timer, void (*callback)(void *), chip_state_t *chip) {
  const timer_config_t config = {.callback = callback, .user_data = chip};
  *timer = timer_init(&config);
}

void chip_init(void) {
  chip_state_t *chip = &state;
  chip->previous_run = false;
  chip->locked = false;
  chip->scenario = SCENARIO_UPPER_LEFT;
  chip->attr_scenario = attr_init_float("scenario", SCENARIO_UPPER_LEFT);
  chip->attr_run = attr_init_float("run", 0.0f);
  chip->beam1 = pin_init("IR1", OUTPUT_HIGH);
  chip->beam2 = pin_init("IR2", OUTPUT_HIGH);
  chip->piezo[0] = pin_init("PZ1", ANALOG);
  chip->piezo[1] = pin_init("PZ2", ANALOG);
  chip->piezo[2] = pin_init("PZ3", ANALOG);
  chip->piezo[3] = pin_init("PZ4", ANALOG);
  for (int i = 0; i < 4; ++i) {
    chip->level[i] = 0.0f;
    pin_dac_write(chip->piezo[i], 0.0f);
  }
  init_timer(&chip->beam2_timer, on_beam2, chip);
  init_timer(&chip->beam1_release_timer, on_beam1_release, chip);
  init_timer(&chip->beam2_release_timer, on_beam2_release, chip);
  init_timer(&chip->impact_timer, on_impact, chip);
  init_timer(&chip->decay_timer, on_decay, chip);
  init_timer(&chip->unlock_timer, on_unlock, chip);
  init_timer(&chip->poll_timer, on_poll, chip);
  timer_start(chip->poll_timer, POLL_US, true);
}

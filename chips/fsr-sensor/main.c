// FSR (force sensitive resistor) in a voltage divider, simulated for Wokwi.
//
// The slider "pressure" is a relative pressure from 0 to 100 %, not a force and not a
// shot power. The FSR conductance grows linearly with it; the output is the divider
// voltage with a fixed resistor to GND:  Vout = Vcc * R_FIXED / (R_FIXED + R_fsr).

#include "wokwi-api.h"

#define POLL_US 1000
#define VCC_VOLTS 3.3f
#define R_FIXED_OHMS 10000.0f
// Conductance of the FSR with no pressure (about 10 Mohm) and at 100 % (1 mS = 1 kohm).
#define G_MIN_SIEMENS 0.0000001f
#define G_MAX_SIEMENS 0.001f

static pin_t pin_out;
static uint32_t attr_pressure;
static timer_t timer_poll;
static float last_pressure = -1.0f;

static void on_poll(void *user_data) {
  (void)user_data;
  float pressure = attr_read_float(attr_pressure);
  if (pressure == last_pressure) {
    return;
  }
  last_pressure = pressure;
  float conductance = G_MIN_SIEMENS + (pressure / 100.0f) * (G_MAX_SIEMENS - G_MIN_SIEMENS);
  float r_fsr = 1.0f / conductance;
  pin_dac_write(pin_out, VCC_VOLTS * R_FIXED_OHMS / (R_FIXED_OHMS + r_fsr));
}

void chip_init(void) {
  attr_pressure = attr_init_float("pressure", 0.0f);
  pin_out = pin_init("OUT", ANALOG);
  pin_dac_write(pin_out, 0.0f);

  const timer_config_t poll_config = {.callback = on_poll, .user_data = 0};
  timer_poll = timer_init(&poll_config);
  timer_start(timer_poll, POLL_US, true);
}

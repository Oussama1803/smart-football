// Hardware pins and timings of the goal ESP32. Named constants only, no magic numbers
// in main.cpp. Physics and speed limits live in lib/shot_logic/src/shot_logic.h.
#pragma once

#include <stddef.h>
#include <stdint.h>

// ---- Identity (placeholders until the backend manages sessions) --------------------
#define FIRMWARE_VERSION "0.1.0"
#define DEVICE_ID "goal-esp32-01"
#define SESSION_ID "session-001"

// ---- Pins (avoid the boot pins 0, 2, 5, 12, 15) ------------------------------------
constexpr uint8_t PIN_BEAM_1 = 25;  // IR break-beam 1, falling edge interrupt
constexpr uint8_t PIN_BEAM_2 = 26;  // IR break-beam 2, falling edge interrupt
// Piezos zones 1 to 4. ADC1 only: ADC2 does not work with Wi-Fi.
constexpr uint8_t PIN_PIEZO[4] = {32, 33, 34, 35};
constexpr uint8_t PIN_FSR = 36;  // VP, ADC1, input only
// Addition from our TD sheets (not in the cahier des charges): 4 LEDs and 1 buzzer.
constexpr uint8_t PIN_LED[4] = {16, 17, 18, 19};
constexpr uint8_t PIN_BUZZER = 23;

// ---- Impact detection ---------------------------------------------------------------
// The impact window opens when beam 2 is cut. EF2: zone within 100 ms of the impact.
constexpr uint32_t IMPACT_WINDOW_MS = 60;
// Raw ADC value (0..4095) below which a piezo is considered silent.
// To be calibrated in the simulation: ADC scale and noise are not verified yet.
constexpr uint16_t PIEZO_THRESHOLD_ADC = 300;

// ---- Actuators ------------------------------------------------------------------------
constexpr uint32_t LED_ON_MS = 1000;
constexpr uint32_t BUZZER_ON_MS = 150;
constexpr uint32_t BUZZER_FREQUENCY_HZ = 2000;
constexpr uint8_t BUZZER_LEDC_CHANNEL = 0;

// ---- Cycle ----------------------------------------------------------------------------
// Pause after a shot before the system re-arms on beam 1.
constexpr uint32_t REARM_DELAY_MS = 200;

constexpr unsigned long SERIAL_BAUD = 115200;
constexpr size_t JSON_BUFFER_SIZE = 384;

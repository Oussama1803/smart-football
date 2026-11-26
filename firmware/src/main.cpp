// Smart Football goal ESP32: acquisition and optional asynchronous MQTT/TLS.
// Measures speed (2 IR beams), reads the FSR at the strike, finds the hit zone (4 piezos),
// drives the LEDs and the buzzer, and prints the JSON that step 4 will publish over MQTT.
//
// Serial output: lines starting with '[' are logs, lines starting with '{' are JSON.
#include <Arduino.h>
#include <esp_task_wdt.h>
#include <esp_timer.h>

#include "config.h"
#include "shot_logic.h"
#include "network.h"

using namespace sf;

// ---- Beam edges: captured in the interrupt handlers, processed in loop() ------------

struct BeamEdge {
  uint8_t beam;
  uint32_t t_us;
};

constexpr uint8_t EDGE_QUEUE_SIZE = 8;
static volatile BeamEdge edge_queue[EDGE_QUEUE_SIZE];
static volatile uint8_t queue_head = 0;  // written by the interrupt handlers
static volatile uint8_t queue_tail = 0;  // written by loop()
static volatile uint32_t last_edge_us[2] = {0, 0};
static volatile bool has_last_edge[2] = {false, false};
static portMUX_TYPE edge_mux = portMUX_INITIALIZER_UNLOCKED;
static volatile uint32_t dropped_edges = 0;

static void IRAM_ATTR on_beam_edge(uint8_t beam) {
  const uint32_t now_us = micros();
  const uint8_t index = beam - 1;
  portENTER_CRITICAL_ISR(&edge_mux);
  if (!debounce_accept(has_last_edge[index], last_edge_us[index], now_us, DEBOUNCE_US)) {
    portEXIT_CRITICAL_ISR(&edge_mux);
    return;  // bounce
  }
  has_last_edge[index] = true;
  last_edge_us[index] = now_us;
  const uint8_t next = (queue_head + 1) % EDGE_QUEUE_SIZE;
  if (next == queue_tail) {
    ++dropped_edges;
    portEXIT_CRITICAL_ISR(&edge_mux);
    return;  // queue full: drop the edge
  }
  edge_queue[queue_head].beam = beam;
  edge_queue[queue_head].t_us = now_us;
  queue_head = next;
  portEXIT_CRITICAL_ISR(&edge_mux);
}

static void IRAM_ATTR on_beam_1_edge() { on_beam_edge(1); }
static void IRAM_ATTR on_beam_2_edge() { on_beam_edge(2); }

static bool pop_edge(BeamEdge *out) {
  portENTER_CRITICAL(&edge_mux);
  if (queue_tail == queue_head) {
    portEXIT_CRITICAL(&edge_mux);
    return false;
  }
  out->beam = edge_queue[queue_tail].beam;
  out->t_us = edge_queue[queue_tail].t_us;
  queue_tail = (queue_tail + 1) % EDGE_QUEUE_SIZE;
  portEXIT_CRITICAL(&edge_mux);
  return true;
}

// ---- Shot cycle -----------------------------------------------------------------------

enum CycleState { CYCLE_WAITING, CYCLE_COLLECTING_IMPACT, CYCLE_COOLDOWN };

static Tracker tracker;
static CycleState cycle = CYCLE_WAITING;
static uint32_t cycle_started_ms = 0;
static float shot_speed_kmh = 0.0f;
static uint16_t shot_pressure_raw = 0;
static uint16_t piezo_peaks[ZONE_COUNT];
static int64_t shot_captured_us = 0;

static int8_t lit_led = -1;
static uint32_t led_off_at_ms = 0;
static bool buzzer_on = false;
static uint32_t buzzer_off_at_ms = 0;

// Serial-only fallback before SNTP. Network shots wait for a trusted timestamp.
static void make_timestamp(char out[21]) {
  if (!network_timestamp(out, esp_timer_get_time())) format_iso8601_utc(out, millis() / 1000);
}

static void print_status(const char *level, const char *message) {
  char timestamp[21];
  make_timestamp(timestamp);
  char json[JSON_BUFFER_SIZE];
  if (build_status_json(json, sizeof(json), DEVICE_ID, timestamp, level, message) > 0) {
    Serial.println(json);
    network_submit_status(json);
  }
}

static void start_buzzer() {
  ledcWriteTone(BUZZER_LEDC_CHANNEL, BUZZER_FREQUENCY_HZ);
  buzzer_on = true;
  buzzer_off_at_ms = millis() + BUZZER_ON_MS;
}

static void light_zone_led(Zone zone) {
  for (int i = 0; i < ZONE_COUNT; i++) {
    digitalWrite(PIN_LED[i], i == (int)zone ? HIGH : LOW);
  }
  lit_led = (int8_t)zone;
  led_off_at_ms = millis() + LED_ON_MS;
}

static void service_actuators() {
  const uint32_t now = millis();
  if (lit_led >= 0 && (int32_t)(now - led_off_at_ms) >= 0) {
    digitalWrite(PIN_LED[lit_led], LOW);
    lit_led = -1;
  }
  if (buzzer_on && (int32_t)(now - buzzer_off_at_ms) >= 0) {
    ledcWriteTone(BUZZER_LEDC_CHANNEL, 0);
    buzzer_on = false;
  }
}

static void publish_shot() {
  uint16_t intensity = 0;
  const Zone zone = pick_zone(piezo_peaks, PIEZO_THRESHOLD_ADC, &intensity);

  if (zone != ZONE_NONE) {
    light_zone_led(zone);
    start_buzzer();
  }

  uint32_t random_words[4];
  for (int i = 0; i < 4; i++) {
    random_words[i] = esp_random();
  }
  char event_id[37];
  format_uuid_v4(event_id, random_words);
  char timestamp[21];
  const bool timestamp_ready = network_timestamp(timestamp, shot_captured_us);
  if (!timestamp_ready) format_iso8601_utc(timestamp, (uint32_t)(shot_captured_us / 1000000));

  ShotMessage message;
  message.event_id = event_id;
  message.device_id = DEVICE_ID;
  message.session_id = SESSION_ID;
  message.timestamp = timestamp;
  message.ball_speed_kmh = shot_speed_kmh;
  message.pressure_reading = shot_pressure_raw;
  message.target_zone = zone_name(zone);  // "none" when no piezo crossed the threshold
  message.impact_intensity = intensity;
  message.firmware_version = FIRMWARE_VERSION;

  char json[JSON_BUFFER_SIZE];
  if (build_shot_json(json, sizeof(json), message) > 0) {
    Serial.printf("[SHOT] %.1f km/h, zone %s, pressure raw %u, impact raw %u\n",
                  (double)shot_speed_kmh, zone_name(zone), (unsigned)shot_pressure_raw,
                  (unsigned)intensity);
    Serial.println(json);
    if (!network_submit_shot(json, shot_captured_us, timestamp_ready))
      print_status("error", "file MQTT pleine ou indisponible, tir non conserve");
  } else {
    print_status("error", "JSON trop long");
  }
}

static void handle_tracker_event(TrackerEvent event, uint32_t delta_us) {
  switch (event) {
    case TRACKER_SHOT: {
      const SpeedStatus status = check_delta_us(delta_us);
      if (status != SPEED_OK) {
        // SEC2: nothing is published, only a message on the status channel.
        char text[64];
        snprintf(text, sizeof(text), "vitesse rejetée (%s)",
                 status == SPEED_TOO_FAST ? "trop rapide" : "trop lente");
        print_status("error", text);
        tracker_rearm(&tracker);
        return;
      }
      shot_speed_kmh = speed_kmh_from_delta_us(delta_us);
      shot_captured_us = esp_timer_get_time();
      shot_pressure_raw = (uint16_t)analogRead(PIN_FSR);  // FSR read at the strike
      for (int i = 0; i < ZONE_COUNT; i++) {
        piezo_peaks[i] = 0;
      }
      cycle = CYCLE_COLLECTING_IMPACT;
      cycle_started_ms = millis();
      break;
    }
    case TRACKER_WRONG_DIRECTION:
      print_status("error", "sens de passage inverse, mesure ignorée");
      break;
    case TRACKER_TIMEOUT:
      print_status("warning", "mesure abandonnée, barrière 2 non coupée");
      break;
    default:
      break;
  }
}

static void sample_piezos() {
  for (int i = 0; i < ZONE_COUNT; i++) {
    const uint16_t value = (uint16_t)analogRead(PIN_PIEZO[i]);
    if (value > piezo_peaks[i]) {
      piezo_peaks[i] = value;
    }
  }
}

void setup() {
  Serial.begin(SERIAL_BAUD);

  pinMode(PIN_BEAM_1, INPUT_PULLUP);
  pinMode(PIN_BEAM_2, INPUT_PULLUP);
  for (int i = 0; i < ZONE_COUNT; i++) {
    pinMode(PIN_LED[i], OUTPUT);
    digitalWrite(PIN_LED[i], LOW);
  }
  ledcSetup(BUZZER_LEDC_CHANNEL, BUZZER_FREQUENCY_HZ, 8);
  ledcAttachPin(PIN_BUZZER, BUZZER_LEDC_CHANNEL);
  ledcWriteTone(BUZZER_LEDC_CHANNEL, 0);

  tracker_init(&tracker);
  attachInterrupt(digitalPinToInterrupt(PIN_BEAM_1), on_beam_1_edge, FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_BEAM_2), on_beam_2_edge, FALLING);

  Serial.printf("[BOOT] smart-football firmware %s, device %s\n", FIRMWARE_VERSION, DEVICE_ID);
  if (esp_task_wdt_init(3, true) != ESP_OK || esp_task_wdt_add(nullptr) != ESP_OK)
    Serial.println("[WDT] Initialization failed");
  network_begin();
}

void loop() {
  esp_task_wdt_reset();
#if defined(SF_WATCHDOG_TEST) && SF_WATCHDOG_TEST
  // Deliberately stop feeding this task only in the watchdog test build.
  if (Serial.available() && Serial.read() == '!') {
    Serial.println("[WDT] Test stall");
    for (;;) delay(1);
  }
#endif
  uint32_t lost;
  portENTER_CRITICAL(&edge_mux);
  lost = dropped_edges;
  dropped_edges = 0;
  if (lost) queue_tail = queue_head;
  portEXIT_CRITICAL(&edge_mux);
  if (lost) {
    tracker_init(&tracker);
    cycle = CYCLE_WAITING;
    print_status("error", "debordement interruptions, mesure abandonnee");
  }
  BeamEdge edge;
  while (pop_edge(&edge)) {
    uint32_t delta_us = 0;
    const TrackerEvent event = tracker_on_beam(&tracker, edge.beam, edge.t_us, &delta_us);
    handle_tracker_event(event, delta_us);
  }
  handle_tracker_event(tracker_poll(&tracker, micros()), 0);

  if (cycle == CYCLE_COLLECTING_IMPACT) {
    sample_piezos();
    if ((uint32_t)(millis() - cycle_started_ms) >= IMPACT_WINDOW_MS) {
      publish_shot();
      cycle = CYCLE_COOLDOWN;
      cycle_started_ms = millis();
    }
  } else if (cycle == CYCLE_COOLDOWN) {
    if ((uint32_t)(millis() - cycle_started_ms) >= REARM_DELAY_MS) {
      tracker_rearm(&tracker);
      cycle = CYCLE_WAITING;
    }
  }

  service_actuators();
  delay(1);  // Yield to FreeRTOS; ADC acquisition remains inside the 60 ms window.
}

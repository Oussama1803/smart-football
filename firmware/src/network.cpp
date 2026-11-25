#include <Arduino.h>
#include <esp_timer.h>
#include <time.h>
#include "config.h"
#include "delivery.h"
#include "network.h"
#include "shot_logic.h"

#ifndef SF_NETWORK_ENABLED
#define SF_NETWORK_ENABLED 0
#endif

#if SF_NETWORK_ENABLED
#include <WiFi.h>
#include <mqtt_client.h>
#include <esp_sntp.h>
#define SF_SECRETS_PROVISIONED 0
#if __has_include("secrets.h")
#include "secrets.h"
#undef SF_SECRETS_PROVISIONED
#define SF_SECRETS_PROVISIONED 1
#else
#include "secrets.h.example"
#endif

namespace {
sf::DeliveryQueue shots;
portMUX_TYPE state_mux = portMUX_INITIALIZER_UNLOCKED;
struct MqttNotice { int event; int id; };
struct StatusMessage { char json[JSON_BUFFER_SIZE]; int64_t captured_us; };
QueueHandle_t notices;
QueueHandle_t statuses;
bool notice_overflow = false;
bool enabled = false;
bool clock_ready = false;
int64_t sync_us = 0;
uint32_t sync_epoch = 0;
char events_topic[96];
char status_topic[96];
char last_will[JSON_BUFFER_SIZE];

void time_synchronized(struct timeval *tv) {
  // Do not trust a plausible RTC value alone: require an actual SNTP callback.
  if (tv->tv_sec < 1700000000 || (uint64_t)tv->tv_sec > UINT32_MAX) return;
  portENTER_CRITICAL(&state_mux);
  sync_us = esp_timer_get_time();
  sync_epoch = (uint32_t)tv->tv_sec;
  clock_ready = true;
  portEXIT_CRITICAL(&state_mux);
}

esp_err_t mqtt_event(esp_mqtt_event_handle_t event) {
  if (event->event_id == MQTT_EVENT_CONNECTED || event->event_id == MQTT_EVENT_DISCONNECTED ||
      event->event_id == MQTT_EVENT_PUBLISHED || event->event_id == MQTT_EVENT_ERROR ||
      event->event_id == MQTT_EVENT_DELETED) {
    const MqttNotice notice = {(int)event->event_id, event->msg_id};
    if (xQueueSend(notices, &notice, 0) != pdTRUE) {
      portENTER_CRITICAL(&state_mux);
      notice_overflow = true;
      portEXIT_CRITICAL(&state_mux);
    }
  }
  return ESP_OK;
}

esp_mqtt_client_handle_t start_mqtt() {
  esp_mqtt_client_config_t cfg = {};
  cfg.host = SF_MQTT_HOST;
  cfg.port = SF_MQTT_PORT;
  cfg.transport = MQTT_TRANSPORT_OVER_SSL;
  cfg.cert_pem = SF_MQTT_CA_PEM;
  cfg.skip_cert_common_name_check = false;
  cfg.client_id = DEVICE_ID;
  cfg.username = SF_MQTT_USERNAME;
  cfg.password = SF_MQTT_PASSWORD;
  cfg.event_handle = mqtt_event;
  cfg.lwt_topic = status_topic;
  cfg.lwt_msg = last_will;
  cfg.lwt_qos = 1;
  cfg.lwt_retain = 1;
  cfg.keepalive = 15;
  cfg.reconnect_timeout_ms = 1000;
  cfg.network_timeout_ms = 3000;
  cfg.buffer_size = 1024;
  cfg.task_prio = 1;
  auto client = esp_mqtt_client_init(&cfg);
  if (client && esp_mqtt_client_start(client) != ESP_OK) {
    esp_mqtt_client_destroy(client);
    client = nullptr;
  }
  return client;
}

void send_presence(esp_mqtt_client_handle_t client, const char *status) {
  char timestamp[21];
  if (!network_timestamp(timestamp, esp_timer_get_time())) return;
  char json[JSON_BUFFER_SIZE];
  snprintf(json, sizeof(json),
    "{\"device_id\":\"%s\",\"timestamp\":\"%s\",\"status\":\"%s\","
    "\"firmware_version\":\"%s\",\"uptime_seconds\":%lu}",
    DEVICE_ID, timestamp, status, FIRMWARE_VERSION, (unsigned long)(millis() / 1000));
  esp_mqtt_client_enqueue(client, status_topic, json, 0, 1, 1, true);
}

void network_task(void *) {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(SF_WIFI_SSID, SF_WIFI_PASSWORD);
  sntp_set_time_sync_notification_cb(time_synchronized);
  configTime(0, 0, SF_NTP_SERVER);
  esp_mqtt_client_handle_t client = nullptr;
  bool connected = false;
  uint32_t wifi_attempt = millis(), mqtt_attempt = millis() - 1000, heartbeat = 0;
  for (;;) {
    const uint32_t now = millis();
    if (WiFi.status() != WL_CONNECTED && sf::elapsed_ms(now, wifi_attempt, 5000)) {
      WiFi.reconnect();
      wifi_attempt = now;
    }
    bool ready, restart;
    portENTER_CRITICAL(&state_mux);
    ready = clock_ready;
    restart = notice_overflow;
    notice_overflow = false;
    portEXIT_CRITICAL(&state_mux);
    MqttNotice notice;
    while (xQueueReceive(notices, &notice, 0) == pdTRUE) {
      if (notice.event == MQTT_EVENT_CONNECTED) {
        connected = true;
        heartbeat = now;
        send_presence(client, "online");
      } else if (notice.event == MQTT_EVENT_DISCONNECTED) {
        connected = false;
      } else if (notice.event == MQTT_EVENT_PUBLISHED) {
        portENTER_CRITICAL(&state_mux);
        shots.acknowledge(notice.id);
        portEXIT_CRITICAL(&state_mux);
      } else if (notice.event == MQTT_EVENT_DELETED) {
        restart = true;
      } else if (notice.event == MQTT_EVENT_ERROR) {
        // Never log credentials, certificates, or payloads from error objects.
        Serial.println("[NET] MQTT/TLS connection failed; retrying with certificate verification");
      }
    }
    portENTER_CRITICAL(&state_mux);
    for (size_t i = 0; i < shots.size(); ++i) {
      auto &entry = shots.at(i);
      if (!entry.acknowledged && entry.message_id > 0 && sf::elapsed_ms(now, entry.sent_ms, 15000)) restart = true;
    }
    portEXIT_CRITICAL(&state_mux);
    if (restart && client) {
      // Only this task owns the client. Destroy before reusing message IDs.
      esp_mqtt_client_stop(client);
      esp_mqtt_client_destroy(client);
      client = nullptr;
      connected = false;
      xQueueReset(notices);
      portENTER_CRITICAL(&state_mux);
      shots.retry_unacknowledged();
      portEXIT_CRITICAL(&state_mux);
    }
    if (!client && ready && WiFi.status() == WL_CONNECTED && sf::elapsed_ms(now, mqtt_attempt, 1000)) {
      client = start_mqtt();
      mqtt_attempt = now;
    }
    if (client && connected && WiFi.status() == WL_CONNECTED) {
      for (size_t i = 0; i < sf::DELIVERY_CAPACITY; ++i) {
        sf::DeliveryEntry entry;
        bool pending = false;
        portENTER_CRITICAL(&state_mux);
        if (i < shots.size() && !shots.at(i).acknowledged && shots.at(i).message_id < 0) {
          entry = shots.at(i);
          pending = true;
        }
        portEXIT_CRITICAL(&state_mux);
        if (!pending) continue;
        if (!entry.timestamp_ready) {
          char timestamp[21];
          if (!network_timestamp(timestamp, entry.captured_us)) break;
          char *field = strstr(entry.json, "\"timestamp\":\"");
          if (!field) { Serial.println("[NET] Invalid queued timestamp field"); break; }
          memcpy(field + strlen("\"timestamp\":\""), timestamp, 20);
          entry.timestamp_ready = true;
        }
        const int id = esp_mqtt_client_enqueue(client, events_topic, entry.json, 0, 1, 0, true);
        if (id < 0) break;
        entry.message_id = id;
        entry.sent_ms = now;
        portENTER_CRITICAL(&state_mux);
        shots.at(i) = entry;
        portEXIT_CRITICAL(&state_mux);
      }
      // Bound non-shot outbox use, even if the broker never acknowledges.
      if (esp_mqtt_client_get_outbox_size(client) < 12000) {
        if (sf::elapsed_ms(now, heartbeat, 30000)) {
          send_presence(client, "heartbeat");
          heartbeat = now;
        }
        StatusMessage status;
        if (xQueueReceive(statuses, &status, 0) == pdTRUE) {
          char timestamp[21];
          char *field = strstr(status.json, "\"timestamp\":\"");
          if (field && network_timestamp(timestamp, status.captured_us))
            memcpy(field + strlen("\"timestamp\":\""), timestamp, 20);
          esp_mqtt_client_enqueue(client, status_topic, status.json, 0, 1, 0, true);
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}
}  // namespace

void network_begin() {
#if !SF_SECRETS_PROVISIONED
  Serial.println("[NET] Disabled: copy secrets.h.example to secrets.h and provision broker credentials and CA");
  return;
#endif
  // Keep the network path linked and checked even with an empty example config.
  volatile bool configured = SF_MQTT_HOST[0] && SF_MQTT_USERNAME[0] && SF_MQTT_PASSWORD[0] &&
      strstr(SF_MQTT_CA_PEM, "-----BEGIN CERTIFICATE-----");
  if (!configured) {
    Serial.println("[NET] Disabled: provision secrets.h with broker credentials and trusted CA");
    return;
  }
  snprintf(events_topic, sizeof(events_topic), "smartfootball/%s/events", DEVICE_ID);
  snprintf(status_topic, sizeof(status_topic), "smartfootball/%s/status", DEVICE_ID);
  // The broker publishes the Will later; do not pretend its time is known now.
  snprintf(last_will, sizeof(last_will), "{\"device_id\":\"%s\",\"status\":\"offline\"}", DEVICE_ID);
  notices = xQueueCreate(64, sizeof(MqttNotice));
  statuses = xQueueCreate(4, sizeof(StatusMessage));
  if (!notices || !statuses) { Serial.println("[NET] Queue allocation failed"); return; }
  enabled = xTaskCreatePinnedToCore(network_task, "sf-network", 8192, nullptr, 1, nullptr, 0) == pdPASS;
  if (!enabled) Serial.println("[NET] Task allocation failed");
}

bool network_submit_shot(const char *json, int64_t captured_us, bool timestamp_ready) {
  if (!enabled) return false;
  portENTER_CRITICAL(&state_mux);
  const bool accepted = shots.push(json, captured_us, timestamp_ready);
  portEXIT_CRITICAL(&state_mux);
  return accepted;
}

void network_submit_status(const char *json) {
  if (!enabled) return;
  StatusMessage message = {};
  if (strlen(json) >= sizeof(message.json)) return;
  strcpy(message.json, json);
  message.captured_us = esp_timer_get_time();
  if (xQueueSend(statuses, &message, 0) != pdTRUE) Serial.println("[NET] Status queue full");
}

bool network_timestamp(char out[21], int64_t captured_us) {
  portENTER_CRITICAL(&state_mux);
  const bool ready = clock_ready;
  const uint32_t epoch = sf::capture_epoch(captured_us, sync_us, sync_epoch);
  portEXIT_CRITICAL(&state_mux);
  if (!ready || !epoch) return false;
  sf::format_iso8601_utc(out, epoch);
  return true;
}
#else
void network_begin() {}
bool network_submit_shot(const char *, int64_t, bool) { return true; }
void network_submit_status(const char *) {}
bool network_timestamp(char[21], int64_t) { return false; }
#endif

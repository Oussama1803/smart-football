#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace sf {
constexpr size_t DELIVERY_CAPACITY = 20;
constexpr size_t DELIVERY_JSON_SIZE = 384;

struct DeliveryEntry {
  char json[DELIVERY_JSON_SIZE];
  int64_t captured_us;
  int message_id;
  uint32_t sent_ms;
  bool timestamp_ready;
  bool acknowledged;
};

// Caller provides synchronization. An entry is removed only after PUBACK.
class DeliveryQueue {
 public:
  bool push(const char *json, int64_t captured_us, bool timestamp_ready) {
    if (!json || strlen(json) >= DELIVERY_JSON_SIZE || size_ == DELIVERY_CAPACITY) return false;
    DeliveryEntry &entry = entries_[(head_ + size_) % DELIVERY_CAPACITY];
    entry = {};
    strcpy(entry.json, json);
    entry.captured_us = captured_us;
    entry.timestamp_ready = timestamp_ready;
    entry.message_id = -1;
    ++size_;
    return true;
  }
  size_t size() const { return size_; }
  DeliveryEntry &at(size_t offset) { return entries_[(head_ + offset) % DELIVERY_CAPACITY]; }
  bool acknowledge(int id) {
    if (id <= 0) return false;
    bool found = false;
    for (size_t i = 0; i < size_; ++i) {
      if (at(i).message_id == id) { at(i).acknowledged = true; found = true; }
    }
    while (size_ && at(0).acknowledged) {
      head_ = (head_ + 1) % DELIVERY_CAPACITY;
      --size_;
    }
    return found;
  }
  void retry_unacknowledged() {
    for (size_t i = 0; i < size_; ++i) if (!at(i).acknowledged) at(i).message_id = -1;
  }
 private:
  DeliveryEntry entries_[DELIVERY_CAPACITY] = {};
  size_t head_ = 0;
  size_t size_ = 0;
};

inline bool elapsed_ms(uint32_t now, uint32_t start, uint32_t delay) {
  return (uint32_t)(now - start) >= delay;
}

// Project the first trusted wall clock back onto monotonic capture time.
inline uint32_t capture_epoch(int64_t captured_us, int64_t sync_us, uint32_t sync_epoch) {
  const int64_t seconds = (int64_t)sync_epoch + (captured_us - sync_us) / 1000000;
  return seconds > 0 && seconds <= UINT32_MAX ? (uint32_t)seconds : 0;
}
}  // namespace sf

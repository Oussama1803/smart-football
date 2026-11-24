#include <unity.h>
#include "delivery.h"

void setUp() {}
void tearDown() {}

void test_offline_capacity_and_overflow_preserve_first_twenty() {
  sf::DeliveryQueue queue;
  for (int i = 0; i < 20; ++i) TEST_ASSERT_TRUE(queue.push("{\"event_id\":\"stable\"}", i, false));
  TEST_ASSERT_FALSE(queue.push("overflow", 20, false));
  TEST_ASSERT_EQUAL_UINT(20, queue.size());
  for (int i = 0; i < 20; ++i) TEST_ASSERT_EQUAL_INT64(i, queue.at(i).captured_us);
}
void test_puback_only_removes_acknowledged_messages() {
  sf::DeliveryQueue queue;
  queue.push("first", 1, true);
  queue.push("second", 2, true);
  queue.at(0).message_id = 10;
  queue.at(1).message_id = 11;
  TEST_ASSERT_FALSE(queue.acknowledge(-1));
  TEST_ASSERT_FALSE(queue.acknowledge(99));
  TEST_ASSERT_TRUE(queue.acknowledge(11));
  TEST_ASSERT_EQUAL_UINT(2, queue.size());
  TEST_ASSERT_TRUE(queue.acknowledge(10));
  TEST_ASSERT_EQUAL_UINT(0, queue.size());
  TEST_ASSERT_FALSE(queue.acknowledge(10));
}
void test_reconnect_preserves_identity_payload_and_timestamp() {
  sf::DeliveryQueue queue;
  queue.push("{\"event_id\":\"same-id\",\"timestamp\":\"2026-11-24T00:00:00Z\"}", 123, true);
  queue.at(0).message_id = 7;
  queue.retry_unacknowledged();
  TEST_ASSERT_EQUAL_INT(-1, queue.at(0).message_id);
  TEST_ASSERT_TRUE(queue.at(0).timestamp_ready);
  TEST_ASSERT_EQUAL_STRING("{\"event_id\":\"same-id\",\"timestamp\":\"2026-11-24T00:00:00Z\"}", queue.at(0).json);
  queue.at(0).message_id = 8;
  TEST_ASSERT_FALSE(queue.acknowledge(7));
  TEST_ASSERT_TRUE(queue.acknowledge(8));
}
void test_queue_reuses_slots_after_many_cycles() {
  sf::DeliveryQueue queue;
  for (int cycle = 0; cycle < 100; ++cycle) {
    for (int i = 0; i < 20; ++i) {
      TEST_ASSERT_TRUE(queue.push("payload", cycle * 20 + i, true));
      queue.at(i).message_id = i + 1;
    }
    for (int i = 1; i <= 20; ++i) TEST_ASSERT_TRUE(queue.acknowledge(i));
    TEST_ASSERT_EQUAL_UINT(0, queue.size());
  }
}
void test_invalid_payload_and_timestamp_projection() {
  sf::DeliveryQueue queue;
  char oversized[sf::DELIVERY_JSON_SIZE + 1];
  memset(oversized, 'x', sizeof(oversized));
  oversized[sizeof(oversized) - 1] = 0;
  TEST_ASSERT_FALSE(queue.push(nullptr, 0, false));
  TEST_ASSERT_FALSE(queue.push(oversized, 0, false));
  TEST_ASSERT_EQUAL_UINT32(1700000000, sf::capture_epoch(1000000, 6000000, 1700000005));
  TEST_ASSERT_EQUAL_UINT32(0, sf::capture_epoch(0, 6000000, 1));
}
void test_retry_and_heartbeat_deadlines_wrap_safely() {
  TEST_ASSERT_FALSE(sf::elapsed_ms(29999, 0, 30000));
  TEST_ASSERT_TRUE(sf::elapsed_ms(30000, 0, 30000));
  TEST_ASSERT_TRUE(sf::elapsed_ms(0x20u, 0xfffffff0u, 48));
  TEST_ASSERT_FALSE(sf::elapsed_ms(0x20u, 0xfffffff0u, 49));
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(test_offline_capacity_and_overflow_preserve_first_twenty);
  RUN_TEST(test_puback_only_removes_acknowledged_messages);
  RUN_TEST(test_reconnect_preserves_identity_payload_and_timestamp);
  RUN_TEST(test_queue_reuses_slots_after_many_cycles);
  RUN_TEST(test_invalid_payload_and_timestamp_projection);
  RUN_TEST(test_retry_and_heartbeat_deadlines_wrap_safely);
  return UNITY_END();
}

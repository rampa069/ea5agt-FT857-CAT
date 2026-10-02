#include <string.h>
#include <unity.h>

#include "rig_poller.h"

using namespace ft8x7;

// Radio simulada mínima: responde según el opcode y registra el orden de comandos.
class FakeRadio : public CatPort {
 public:
  bool online = true;
  uint8_t freqMode[5] = {0x01, 0x40, 0x74, 0x00, 0x01};  // 14.074 MHz USB
  uint8_t rx = 0x05;
  uint8_t tx = 0xFF;
  uint8_t ops[64] = {};
  size_t opCount = 0;

  void write(const uint8_t* data, size_t len) override {
    if (len == kCommandLength && opCount < sizeof(ops)) {
      ops[opCount++] = data[4];
    }
    pending_ = data[4];
  }

  size_t read(uint8_t* data, size_t len, uint32_t) override {
    if (!online) {
      return 0;
    }
    switch (pending_) {
      case 0x03: memcpy(data, freqMode, len); return len;
      case 0xE7: data[0] = rx; return 1;
      case 0xF7: data[0] = tx; return 1;
      default: return 0;
    }
  }

  void discardInput() override {}

 private:
  uint8_t pending_ = 0;
};

void setUp() {}
void tearDown() {}

static void runSteps(RigPoller& poller, uint32_t& now, int count, uint32_t stepMs = 100) {
  for (int i = 0; i < count; ++i) {
    poller.step(now);
    now += stepMs;
  }
}

static void test_schedule_order_in_receive() {
  FakeRadio radio;
  Ft8x7Cat cat(radio);
  RigPoller poller(cat);
  uint32_t now = 0;
  runSteps(poller, now, 6);

  const uint8_t expected[] = {0x03, 0xF7, 0xE7, 0xF7, 0xE7, 0x03};
  TEST_ASSERT_EQUAL(6, radio.opCount);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, radio.ops, 6);
}

static void test_state_filled_after_one_cycle() {
  FakeRadio radio;
  Ft8x7Cat cat(radio);
  RigPoller poller(cat);
  uint32_t now = 1000;
  runSteps(poller, now, 3);

  const RigState& s = poller.state();
  TEST_ASSERT_TRUE(s.linked);
  TEST_ASSERT_TRUE(s.haveFreq);
  TEST_ASSERT_EQUAL_UINT32(14074000UL, s.freq.hz);
  TEST_ASSERT_EQUAL(Mode::USB, s.freq.mode);
  TEST_ASSERT_EQUAL_UINT8(5, s.rx.sMeter);
  TEST_ASSERT_FALSE(s.tx.transmitting);
  TEST_ASSERT_EQUAL_UINT32(3, s.okCount);
}

static void test_respects_gap() {
  FakeRadio radio;
  Ft8x7Cat cat(radio);
  RigPoller poller(cat, 20);
  TEST_ASSERT_TRUE(poller.step(0));
  TEST_ASSERT_FALSE(poller.step(10));
  TEST_ASSERT_TRUE(poller.step(20));
}

static void test_skips_rx_status_while_transmitting() {
  FakeRadio radio;
  radio.tx = 0x07;  // TX, PO=7
  Ft8x7Cat cat(radio);
  RigPoller poller(cat);
  uint32_t now = 0;
  runSteps(poller, now, 5);

  const uint8_t expected[] = {0x03, 0xF7, 0xF7, 0xF7, 0xF7};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, radio.ops, 5);
  TEST_ASSERT_TRUE(poller.state().tx.transmitting);
  TEST_ASSERT_EQUAL_UINT8(7, poller.state().tx.poMeter);
}

static void test_link_lost_after_consecutive_errors_and_recovers() {
  FakeRadio radio;
  Ft8x7Cat cat(radio);
  RigPoller poller(cat, 20, 500);
  uint32_t now = 0;
  runSteps(poller, now, 3);
  TEST_ASSERT_TRUE(poller.state().linked);

  radio.online = false;
  runSteps(poller, now, 2);
  TEST_ASSERT_TRUE(poller.state().linked);  // dos fallos aún se toleran
  runSteps(poller, now, 1);
  TEST_ASSERT_FALSE(poller.state().linked);
  TEST_ASSERT_EQUAL_UINT32(3, poller.state().errorCount);
  TEST_ASSERT_EQUAL_UINT32(14074000UL, poller.state().freq.hz);  // conserva el último valor

  // Sin enlace, reintenta más despacio.
  TEST_ASSERT_FALSE(poller.step(now + 100));
  radio.online = true;
  TEST_ASSERT_TRUE(poller.step(now + 500));
  TEST_ASSERT_TRUE(poller.state().linked);
}

static void test_writes_have_priority_and_force_freq_read() {
  FakeRadio radio;
  Ft8x7Cat cat(radio);
  RigPoller poller(cat);
  uint32_t now = 0;
  runSteps(poller, now, 2);  // 0x03, 0xF7

  TEST_ASSERT_TRUE(poller.enqueue(makeToggleVfo()));
  runSteps(poller, now, 2);
  TEST_ASSERT_EQUAL_HEX8(0x81, radio.ops[2]);  // la escritura sale antes que el sondeo
  TEST_ASSERT_EQUAL_HEX8(0x03, radio.ops[3]);  // y luego se relee la frecuencia
  TEST_ASSERT_EQUAL_UINT32(1, poller.state().writeCount);
}

static void test_set_frequency_coalesces_and_updates_state() {
  FakeRadio radio;
  Ft8x7Cat cat(radio);
  RigPoller poller(cat);
  uint32_t now = 0;
  runSteps(poller, now, 1);  // ya hay frecuencia leída

  Command c;
  for (uint32_t hz = 14074100; hz <= 14074500; hz += 100) {
    TEST_ASSERT_TRUE(makeSetFrequency(hz, c));
    TEST_ASSERT_TRUE(poller.enqueue(c));
  }
  TEST_ASSERT_TRUE(makeSetMode(Mode::CW, c));
  TEST_ASSERT_TRUE(poller.enqueue(c));

  runSteps(poller, now, 2);
  TEST_ASSERT_EQUAL_HEX8(0x01, radio.ops[1]);
  TEST_ASSERT_EQUAL_HEX8(0x07, radio.ops[2]);
  TEST_ASSERT_EQUAL_UINT32(2, poller.state().writeCount);  // 5 frecuencias -> 1 envío
  TEST_ASSERT_EQUAL_UINT32(14074500UL, poller.state().freq.hz);
  TEST_ASSERT_EQUAL(Mode::CW, poller.state().freq.mode);
}

static void test_queue_full() {
  FakeRadio radio;
  Ft8x7Cat cat(radio);
  RigPoller poller(cat);
  for (size_t i = 0; i < RigPoller::kQueueSize; ++i) {
    TEST_ASSERT_TRUE(poller.enqueue(makeToggleVfo()));
  }
  TEST_ASSERT_FALSE(poller.enqueue(makeToggleVfo()));
  Command c;
  TEST_ASSERT_TRUE(makeSetFrequency(7074000, c));
  TEST_ASSERT_FALSE(poller.enqueue(c));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_schedule_order_in_receive);
  RUN_TEST(test_state_filled_after_one_cycle);
  RUN_TEST(test_respects_gap);
  RUN_TEST(test_skips_rx_status_while_transmitting);
  RUN_TEST(test_link_lost_after_consecutive_errors_and_recovers);
  RUN_TEST(test_writes_have_priority_and_force_freq_read);
  RUN_TEST(test_set_frequency_coalesces_and_updates_state);
  RUN_TEST(test_queue_full);
  return UNITY_END();
}

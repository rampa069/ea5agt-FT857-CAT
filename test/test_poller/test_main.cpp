#include <string.h>
#include <unity.h>

#include "rig_poller.h"
#include "switching_cat_port.h"

using namespace ft8x7;

// Radio simulada mínima: responde según el opcode y registra el orden de comandos.
class FakeRadio : public CatPort {
 public:
  bool online = true;
  uint8_t freqMode[5] = {0x01, 0x40, 0x74, 0x00, 0x01};  // 14.074 MHz USB
  uint8_t rx = 0x05;
  uint8_t tx = 0xFF;
  uint8_t eeprom[256] = {};
  bool eepromSupported = true;
  uint8_t meters[2] = {0x93, 0x25};  // PO 9, ALC 3, SWR 2, MOD 5
  bool metersSupported = true;
  uint8_t ops[64] = {};
  size_t opCount = 0;

  void write(const uint8_t* data, size_t len) override {
    if (len == kCommandLength && opCount < sizeof(ops)) {
      ops[opCount++] = data[4];
    }
    memcpy(pending_, data, kCommandLength);
  }

  size_t read(uint8_t* data, size_t len, uint32_t) override {
    if (!online) {
      return 0;
    }
    switch (pending_[4]) {
      case 0x03: memcpy(data, freqMode, len); return len;
      case 0xE7: data[0] = rx; return 1;
      case 0xF7: data[0] = tx; return 1;
      case 0xBB: {
        if (!eepromSupported) return 0;
        uint8_t addr = pending_[1];
        data[0] = eeprom[addr];
        data[1] = eeprom[addr + 1];
        return 2;
      }
      case 0xBD:
        if (!metersSupported || (tx & 0x80)) {  // en RX (o sin soporte) sólo 1 byte
          data[0] = 0xFF;
          return 1;
        }
        memcpy(data, meters, 2);
        return 2;
      default: return 0;
    }
  }

  void discardInput() override {}

  size_t count(uint8_t op) const {
    size_t n = 0;
    for (size_t i = 0; i < opCount; ++i) n += ops[i] == op;
    return n;
  }

 private:
  uint8_t pending_[kCommandLength] = {};
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

static void test_switching_port_follows_target() {
  FakeRadio cable, bluetooth;
  bluetooth.freqMode[1] = 0x45;  // 14.574 en el "Bluetooth" para distinguirlos
  SwitchingCatPort port;
  Ft8x7Cat cat(port);
  FreqMode fm;
  TEST_ASSERT_EQUAL(CatResult::Timeout, cat.readFreqMode(fm));  // sin destino
  port.setTarget(&cable);
  TEST_ASSERT_EQUAL(CatResult::Ok, cat.readFreqMode(fm));
  TEST_ASSERT_EQUAL_UINT32(14074000UL, fm.hz);
  port.setTarget(&bluetooth);
  TEST_ASSERT_EQUAL(CatResult::Ok, cat.readFreqMode(fm));
  TEST_ASSERT_EQUAL_UINT32(14574000UL, fm.hz);
  TEST_ASSERT_EQUAL(1, cable.opCount);
}

static PollExtras extras857() {
  PollExtras e;
  e.eeprom = true;
  e.layout = kEeprom857;
  e.txMeters = true;
  return e;
}

static void test_eeprom_reads_vfo_and_split_in_rx() {
  FakeRadio radio;
  radio.eeprom[0x68] = 0x01;  // VFO B
  radio.eeprom[0x8D] = 0x80;  // split ON
  Ft8x7Cat cat(radio);
  RigPoller poller(cat);
  poller.setExtras(extras857());
  uint32_t now = 0;
  runSteps(poller, now, 8);

  const RigState& s = poller.state();
  TEST_ASSERT_TRUE(s.haveVfo);
  TEST_ASSERT_TRUE(s.vfoB);
  TEST_ASSERT_TRUE(s.haveSplit);
  TEST_ASSERT_TRUE(s.split);
  TEST_ASSERT_EQUAL(2, radio.count(0xBB));  // una de cada, no más hasta pasado el intervalo
  TEST_ASSERT_EQUAL_HEX8(0x03, radio.ops[0]);  // la frecuencia siempre primero
}

static void test_eeprom_reread_after_write_and_periodic() {
  FakeRadio radio;
  Ft8x7Cat cat(radio);
  RigPoller poller(cat);
  poller.setExtras(extras857());
  uint32_t now = 0;
  runSteps(poller, now, 6, 50);
  TEST_ASSERT_FALSE(poller.state().vfoB);
  size_t before = radio.count(0xBB);

  radio.eeprom[0x68] = 0x01;  // el usuario pulsa A/B
  poller.enqueue(makeToggleVfo());
  runSteps(poller, now, 5, 50);
  TEST_ASSERT_TRUE(poller.state().vfoB);
  TEST_ASSERT_EQUAL(before + 2, radio.count(0xBB));

  runSteps(poller, now, 30, 50);  // 1,5 s: al menos una relectura periódica
  TEST_ASSERT_TRUE(radio.count(0xBB) >= before + 4);
}

static void test_eeprom_unsupported_disables_without_link_loss() {
  FakeRadio radio;
  radio.eepromSupported = false;  // algunos FT-857 no responden a 0xBB
  Ft8x7Cat cat(radio);
  RigPoller poller(cat);
  poller.setExtras(extras857());
  uint32_t now = 0;
  runSteps(poller, now, 40, 100);
  TEST_ASSERT_TRUE(poller.state().linked);
  TEST_ASSERT_TRUE(poller.state().eepromUnsupported);
  TEST_ASSERT_FALSE(poller.state().haveVfo);
  TEST_ASSERT_EQUAL(RigPoller::kExtraGiveUpErrors, radio.count(0xBB));
  TEST_ASSERT_EQUAL_UINT32(0, poller.state().errorCount);
}

static void test_meters_read_while_transmitting() {
  FakeRadio radio;
  radio.tx = 0x47;  // TX
  Ft8x7Cat cat(radio);
  RigPoller poller(cat);
  poller.setExtras(extras857());
  uint32_t now = 0;
  runSteps(poller, now, 5);
  const RigState& s = poller.state();
  TEST_ASSERT_TRUE(s.haveMeters);
  TEST_ASSERT_EQUAL_UINT8(9, s.meters.power);
  TEST_ASSERT_EQUAL_UINT8(3, s.meters.alc);
  TEST_ASSERT_EQUAL_UINT8(2, s.meters.swr);
  TEST_ASSERT_EQUAL_UINT8(5, s.meters.mod);
  size_t eepromReads = radio.count(0xBB);  // las iniciales, antes de saber que transmite
  runSteps(poller, now, 30);               // 3 s transmitiendo
  TEST_ASSERT_EQUAL(eepromReads, radio.count(0xBB));  // en TX no se lee la EEPROM

  radio.tx = 0xFF;  // vuelve a RX: los medidores dejan de valer
  runSteps(poller, now, 3);
  TEST_ASSERT_FALSE(poller.state().haveMeters);
}

static void test_meters_unsupported_fall_back_to_tx_status() {
  FakeRadio radio;
  radio.tx = 0x47;
  radio.metersSupported = false;
  Ft8x7Cat cat(radio);
  RigPoller poller(cat);
  poller.setExtras(extras857());
  uint32_t now = 0;
  runSteps(poller, now, 20);
  TEST_ASSERT_TRUE(poller.state().metersUnsupported);
  TEST_ASSERT_TRUE(poller.state().linked);
  TEST_ASSERT_EQUAL(RigPoller::kExtraGiveUpErrors, radio.count(0xBD));
  TEST_ASSERT_EQUAL_UINT8(7, poller.state().tx.poMeter);  // sigue el PO del estado TX
}

static void test_eeprom_byte_pick_and_meter_decode() {
  const uint8_t pair[2] = {0x11, 0x22};
  TEST_ASSERT_EQUAL_HEX8(0x11, pickEepromByte(0x68, pair));
  TEST_ASSERT_EQUAL_HEX8(0x22, pickEepromByte(0x55, pair));
  const uint8_t m[2] = {0xA1, 0x3F};
  TxMeters t = decodeTxMeters(m);
  TEST_ASSERT_EQUAL_UINT8(10, t.power);
  TEST_ASSERT_EQUAL_UINT8(1, t.alc);
  TEST_ASSERT_EQUAL_UINT8(3, t.swr);
  TEST_ASSERT_EQUAL_UINT8(15, t.mod);
  TEST_ASSERT_EQUAL_HEX8(0x54, makeReadEepromCommand(0x55).bytes[1]);
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
  RUN_TEST(test_switching_port_follows_target);
  RUN_TEST(test_eeprom_reads_vfo_and_split_in_rx);
  RUN_TEST(test_eeprom_reread_after_write_and_periodic);
  RUN_TEST(test_eeprom_unsupported_disables_without_link_loss);
  RUN_TEST(test_meters_read_while_transmitting);
  RUN_TEST(test_meters_unsupported_fall_back_to_tx_status);
  RUN_TEST(test_eeprom_byte_pick_and_meter_decode);
  return UNITY_END();
}

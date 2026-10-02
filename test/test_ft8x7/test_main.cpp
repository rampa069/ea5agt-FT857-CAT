#include <string.h>
#include <unity.h>

#include "ft8x7_cat.h"
#include "ft8x7_protocol.h"

using namespace ft8x7;

// Puerto simulado: guarda lo escrito y devuelve una respuesta preparada.
class MockPort : public CatPort {
 public:
  uint8_t written[16] = {};
  size_t writtenLen = 0;
  uint8_t reply[16] = {};
  size_t replyLen = 0;
  int discards = 0;

  void setReply(const uint8_t* data, size_t len) {
    memcpy(reply, data, len);
    replyLen = len;
  }

  void write(const uint8_t* data, size_t len) override {
    memcpy(written, data, len);
    writtenLen = len;
  }

  size_t read(uint8_t* data, size_t len, uint32_t) override {
    size_t n = len < replyLen ? len : replyLen;
    memcpy(data, reply, n);
    return n;
  }

  void discardInput() override { ++discards; }
};

void setUp() {}
void tearDown() {}

static void test_make_command_layout() {
  Command c = makeCommand(Opcode::ReadFreqMode);
  const uint8_t expected[] = {0x00, 0x00, 0x00, 0x00, 0x03};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, c.bytes, 5);
}

static void test_make_read_eeprom_aligns_even_address() {
  Command c = makeReadEepromCommand(0x0079);
  const uint8_t expected[] = {0x00, 0x78, 0x00, 0x00, 0xBB};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, c.bytes, 5);
}

static void test_bcd_manual_example_439_7_mhz() {
  const uint8_t bcd[] = {0x43, 0x97, 0x00, 0x00};
  uint32_t hz = 0;
  TEST_ASSERT_TRUE(decodeBcdFrequency(bcd, hz));
  TEST_ASSERT_EQUAL_UINT32(439700000UL, hz);
}

static void test_bcd_hf_with_10hz_resolution() {
  const uint8_t bcd[] = {0x01, 0x42, 0x34, 0x56};
  uint32_t hz = 0;
  TEST_ASSERT_TRUE(decodeBcdFrequency(bcd, hz));
  TEST_ASSERT_EQUAL_UINT32(14234560UL, hz);
}

static void test_bcd_rejects_invalid_nibble() {
  const uint8_t bcd[] = {0x01, 0x4A, 0x34, 0x56};
  uint32_t hz = 123;
  TEST_ASSERT_FALSE(decodeBcdFrequency(bcd, hz));
  TEST_ASSERT_EQUAL_UINT32(123, hz);
}

static void test_mode_codes() {
  TEST_ASSERT_EQUAL(Mode::LSB, decodeMode(0x00));
  TEST_ASSERT_EQUAL(Mode::USB, decodeMode(0x01));
  TEST_ASSERT_EQUAL(Mode::CW, decodeMode(0x02));
  TEST_ASSERT_EQUAL(Mode::CWR, decodeMode(0x03));
  TEST_ASSERT_EQUAL(Mode::AM, decodeMode(0x04));
  TEST_ASSERT_EQUAL(Mode::WFM, decodeMode(0x06));
  TEST_ASSERT_EQUAL(Mode::FM, decodeMode(0x08));
  TEST_ASSERT_EQUAL(Mode::DIG, decodeMode(0x0A));
  TEST_ASSERT_EQUAL(Mode::PKT, decodeMode(0x0C));
  TEST_ASSERT_EQUAL(Mode::Unknown, decodeMode(0x05));
  TEST_ASSERT_EQUAL_STRING("USB", modeName(Mode::USB));
}

static void test_freq_mode_narrow_flag_ft857() {
  const uint8_t resp[] = {0x00, 0x70, 0x30, 0x00, 0x82};  // 7.030 MHz CW estrecho
  FreqMode fm;
  TEST_ASSERT_TRUE(decodeFreqMode(resp, fm));
  TEST_ASSERT_EQUAL_UINT32(7030000UL, fm.hz);
  TEST_ASSERT_EQUAL(Mode::CW, fm.mode);
  TEST_ASSERT_TRUE(fm.narrow);
  TEST_ASSERT_EQUAL_HEX8(0x82, fm.rawMode);
}

static void test_rx_status_bits() {
  RxStatus s = decodeRxStatus(0x89);  // squelch cerrado, S9
  TEST_ASSERT_EQUAL_UINT8(9, s.sMeter);
  TEST_ASSERT_TRUE(s.squelched);
  TEST_ASSERT_FALSE(s.toneMismatch);
  TEST_ASSERT_FALSE(s.discriminatorOffCenter);

  s = decodeRxStatus(0x63);
  TEST_ASSERT_EQUAL_UINT8(3, s.sMeter);
  TEST_ASSERT_FALSE(s.squelched);
  TEST_ASSERT_TRUE(s.toneMismatch);
  TEST_ASSERT_TRUE(s.discriminatorOffCenter);
}

static void test_tx_status_in_receive_is_ff() {
  TxStatus s = decodeTxStatus(0xFF);
  TEST_ASSERT_FALSE(s.transmitting);
  TEST_ASSERT_EQUAL_UINT8(0, s.poMeter);
  TEST_ASSERT_FALSE(s.highSwr);
  TEST_ASSERT_FALSE(s.split);
}

static void test_tx_status_transmitting() {
  TxStatus s = decodeTxStatus(0x67);  // TX, SWR alta, split OFF, PO=7
  TEST_ASSERT_TRUE(s.transmitting);
  TEST_ASSERT_EQUAL_UINT8(7, s.poMeter);
  TEST_ASSERT_TRUE(s.highSwr);
  TEST_ASSERT_FALSE(s.split);

  s = decodeTxStatus(0x0A);  // TX, split ON, PO=10
  TEST_ASSERT_TRUE(s.transmitting);
  TEST_ASSERT_TRUE(s.split);
  TEST_ASSERT_FALSE(s.highSwr);
  TEST_ASSERT_EQUAL_UINT8(10, s.poMeter);
}

static void test_smeter_db() {
  TEST_ASSERT_EQUAL_INT(-54, sMeterDbOverS9(0));
  TEST_ASSERT_EQUAL_INT(-24, sMeterDbOverS9(5));
  TEST_ASSERT_EQUAL_INT(0, sMeterDbOverS9(9));
  TEST_ASSERT_EQUAL_INT(20, sMeterDbOverS9(11));
}

static void test_cat_read_freq_mode_ok() {
  MockPort port;
  const uint8_t reply[] = {0x14, 0x07, 0x40, 0x00, 0x01};
  port.setReply(reply, sizeof(reply));
  Ft8x7Cat cat(port);

  FreqMode fm;
  TEST_ASSERT_EQUAL(CatResult::Ok, cat.readFreqMode(fm));
  TEST_ASSERT_EQUAL_UINT32(140740000UL, fm.hz);
  TEST_ASSERT_EQUAL(Mode::USB, fm.mode);
  TEST_ASSERT_EQUAL(1, port.discards);
  TEST_ASSERT_EQUAL(5, port.writtenLen);
  TEST_ASSERT_EQUAL_HEX8(0x03, port.written[4]);
}

static void test_cat_timeout_on_short_reply() {
  MockPort port;
  const uint8_t reply[] = {0x14, 0x07};
  port.setReply(reply, sizeof(reply));
  Ft8x7Cat cat(port);

  FreqMode fm;
  TEST_ASSERT_EQUAL(CatResult::Timeout, cat.readFreqMode(fm));
}

static void test_cat_bad_data_on_invalid_bcd() {
  MockPort port;
  const uint8_t reply[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  port.setReply(reply, sizeof(reply));
  Ft8x7Cat cat(port);

  FreqMode fm;
  TEST_ASSERT_EQUAL(CatResult::BadData, cat.readFreqMode(fm));
}

static void test_cat_status_commands() {
  MockPort port;
  Ft8x7Cat cat(port);

  const uint8_t rx = 0x07;
  port.setReply(&rx, 1);
  RxStatus rs;
  TEST_ASSERT_EQUAL(CatResult::Ok, cat.readRxStatus(rs));
  TEST_ASSERT_EQUAL_HEX8(0xE7, port.written[4]);
  TEST_ASSERT_EQUAL_UINT8(7, rs.sMeter);

  const uint8_t tx = 0xFF;
  port.setReply(&tx, 1);
  TxStatus ts;
  TEST_ASSERT_EQUAL(CatResult::Ok, cat.readTxStatus(ts));
  TEST_ASSERT_EQUAL_HEX8(0xF7, port.written[4]);
  TEST_ASSERT_FALSE(ts.transmitting);
}

static void expectCommand(const uint8_t expected[5], const Command& c) {
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, c.bytes, 5);
}

static void test_set_frequency_manual_examples() {
  Command c;
  TEST_ASSERT_TRUE(makeSetFrequency(439700000UL, c));
  const uint8_t e1[] = {0x43, 0x97, 0x00, 0x00, 0x01};
  expectCommand(e1, c);
  TEST_ASSERT_TRUE(makeSetFrequency(14234560UL, c));
  const uint8_t e2[] = {0x01, 0x42, 0x34, 0x56, 0x01};
  expectCommand(e2, c);
  uint32_t hz = 0;
  TEST_ASSERT_TRUE(decodeSetFrequency(c, hz));
  TEST_ASSERT_EQUAL_UINT32(14234560UL, hz);
}

static void test_set_frequency_rejects_invalid() {
  Command c = makeToggleVfo();
  TEST_ASSERT_FALSE(makeSetFrequency(14074005UL, c));  // no múltiplo de 10 Hz
  TEST_ASSERT_FALSE(makeSetFrequency(1000000000UL, c));
  TEST_ASSERT_EQUAL_HEX8(0x81, c.bytes[4]);  // sin tocar
}

static void test_set_mode() {
  Command c;
  TEST_ASSERT_TRUE(makeSetMode(Mode::FM, c));
  const uint8_t e[] = {0x08, 0x00, 0x00, 0x00, 0x07};
  expectCommand(e, c);
  TEST_ASSERT_FALSE(makeSetMode(Mode::Unknown, c));
}

static void test_toggle_commands() {
  const uint8_t vfo[] = {0, 0, 0, 0, 0x81};
  expectCommand(vfo, makeToggleVfo());
  TEST_ASSERT_EQUAL_HEX8(0x02, makeSplit(true).bytes[4]);
  TEST_ASSERT_EQUAL_HEX8(0x82, makeSplit(false).bytes[4]);
  TEST_ASSERT_EQUAL_HEX8(0x05, makeClarifier(true).bytes[4]);
  TEST_ASSERT_EQUAL_HEX8(0x85, makeClarifier(false).bytes[4]);
  TEST_ASSERT_EQUAL_HEX8(0x00, makeLock(true).bytes[4]);
  TEST_ASSERT_EQUAL_HEX8(0x80, makeLock(false).bytes[4]);
}

static void test_clarifier_offset() {
  Command c;
  TEST_ASSERT_TRUE(makeClarifierOffset(-1230, c));  // -1,23 kHz
  const uint8_t e[] = {0x01, 0x00, 0x01, 0x23, 0xF5};
  expectCommand(e, c);
  TEST_ASSERT_TRUE(makeClarifierOffset(9990, c));
  const uint8_t e2[] = {0x00, 0x00, 0x09, 0x99, 0xF5};
  expectCommand(e2, c);
  TEST_ASSERT_FALSE(makeClarifierOffset(10000, c));
  TEST_ASSERT_FALSE(makeClarifierOffset(15, c));
}

static void test_repeater() {
  const uint8_t minus[] = {0x09, 0, 0, 0, 0x09};
  expectCommand(minus, makeRepeaterShift(RepeaterShift::Minus));
  TEST_ASSERT_EQUAL_HEX8(0x89, makeRepeaterShift(RepeaterShift::Simplex).bytes[0]);
  Command c;
  TEST_ASSERT_TRUE(makeRepeaterOffset(5432100UL, c));  // ejemplo del manual
  const uint8_t e[] = {0x05, 0x43, 0x21, 0x00, 0xF9};
  expectCommand(e, c);
  TEST_ASSERT_TRUE(makeRepeaterOffset(600000UL, c));
  const uint8_t e2[] = {0x00, 0x60, 0x00, 0x00, 0xF9};
  expectCommand(e2, c);
  TEST_ASSERT_FALSE(makeRepeaterOffset(100000000UL, c));
}

static void test_tones() {
  const uint8_t off[] = {0x8A, 0, 0, 0, 0x0A};
  expectCommand(off, makeToneMode(ToneMode::Off));
  Command c;
  TEST_ASSERT_TRUE(makeCtcssTone(885, c));
  const uint8_t ct[] = {0x08, 0x85, 0x00, 0x00, 0x0B};
  expectCommand(ct, c);
  TEST_ASSERT_TRUE(makeCtcssTone(2541, c));
  TEST_ASSERT_FALSE(makeCtcssTone(886, c));
  TEST_ASSERT_TRUE(makeDcsCode(23, c));
  const uint8_t dcs[] = {0x00, 0x23, 0x00, 0x00, 0x0C};
  expectCommand(dcs, c);
  TEST_ASSERT_TRUE(makeDcsCode(754, c));
  TEST_ASSERT_FALSE(makeDcsCode(24, c));
}

static void test_send_tolerates_missing_ack() {
  MockPort port;
  Ft8x7Cat cat(port);
  TEST_ASSERT_FALSE(cat.send(makeToggleVfo()));  // sin confirmación: no es error
  TEST_ASSERT_EQUAL_HEX8(0x81, port.written[4]);
  const uint8_t ack = 0x00;
  port.setReply(&ack, 1);
  TEST_ASSERT_TRUE(cat.send(makeSplit(true)));
  TEST_ASSERT_EQUAL(2, port.discards);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_make_command_layout);
  RUN_TEST(test_make_read_eeprom_aligns_even_address);
  RUN_TEST(test_bcd_manual_example_439_7_mhz);
  RUN_TEST(test_bcd_hf_with_10hz_resolution);
  RUN_TEST(test_bcd_rejects_invalid_nibble);
  RUN_TEST(test_mode_codes);
  RUN_TEST(test_freq_mode_narrow_flag_ft857);
  RUN_TEST(test_rx_status_bits);
  RUN_TEST(test_tx_status_in_receive_is_ff);
  RUN_TEST(test_tx_status_transmitting);
  RUN_TEST(test_smeter_db);
  RUN_TEST(test_cat_read_freq_mode_ok);
  RUN_TEST(test_cat_timeout_on_short_reply);
  RUN_TEST(test_cat_bad_data_on_invalid_bcd);
  RUN_TEST(test_cat_status_commands);
  RUN_TEST(test_set_frequency_manual_examples);
  RUN_TEST(test_set_frequency_rejects_invalid);
  RUN_TEST(test_set_mode);
  RUN_TEST(test_toggle_commands);
  RUN_TEST(test_clarifier_offset);
  RUN_TEST(test_repeater);
  RUN_TEST(test_tones);
  RUN_TEST(test_send_tolerates_missing_ack);
  return UNITY_END();
}

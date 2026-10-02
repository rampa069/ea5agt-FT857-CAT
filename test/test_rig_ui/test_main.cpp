#include <unity.h>

#include <initializer_list>

#include "rig_ui_logic.h"

using namespace rigui;

void setUp() {}
void tearDown() {}

static void test_band_lookup() {
  TEST_ASSERT_EQUAL_STRING("20m", kBands[bandIndexFor(14074000)].name);
  TEST_ASSERT_EQUAL_STRING("2m", kBands[bandIndexFor(145500000)].name);
  TEST_ASSERT_EQUAL_STRING("FM-BC", kBands[bandIndexFor(98000000)].name);
  TEST_ASSERT_EQUAL(kNoBand, bandIndexFor(15000000));
}

static void test_60m_by_model() {
  TEST_ASSERT_FALSE(bandAvailable(2, RigModel::FT817));
  TEST_ASSERT_TRUE(bandAvailable(2, RigModel::FT818));
  TEST_ASSERT_TRUE(bandAvailable(2, RigModel::FT857));
  TEST_ASSERT_TRUE(bandAvailable(3, RigModel::FT817));
}

static void test_rx_ranges() {
  TEST_ASSERT_TRUE(inRxRange(100000));
  TEST_ASSERT_TRUE(inRxRange(56000000));
  TEST_ASSERT_FALSE(inRxRange(60000000));
  TEST_ASSERT_TRUE(inRxRange(145500000));
  TEST_ASSERT_FALSE(inRxRange(200000000));
  TEST_ASSERT_TRUE(inRxRange(470000000));
}

static void test_tune_snaps_to_grid() {
  uint32_t out = 0;
  TEST_ASSERT_TRUE(tune(14074030, 1000, +1, out));
  TEST_ASSERT_EQUAL_UINT32(14075000, out);
  TEST_ASSERT_TRUE(tune(14074030, 1000, -1, out));
  TEST_ASSERT_EQUAL_UINT32(14074000, out);
  TEST_ASSERT_TRUE(tune(14074000, 1000, -1, out));
  TEST_ASSERT_EQUAL_UINT32(14073000, out);
  TEST_ASSERT_TRUE(tune(14074000, 10, +1, out));
  TEST_ASSERT_EQUAL_UINT32(14074010, out);
}

static void test_tune_rejects_out_of_range() {
  uint32_t out = 123;
  TEST_ASSERT_FALSE(tune(56000000, 100000, +1, out));
  TEST_ASSERT_FALSE(tune(100000, 100000, -1, out));
  TEST_ASSERT_EQUAL_UINT32(123, out);
}

static void test_step_labels() {
  TEST_ASSERT_EQUAL_STRING("10Hz", stepLabel(0));
  TEST_ASSERT_EQUAL_STRING("1k", stepLabel(2));
  TEST_ASSERT_EQUAL_STRING("1M", stepLabel(5));
}

static void test_band_memory() {
  BandMemory mem;
  mem.reset();
  uint32_t hz;
  Mode mode;
  mem.recall(5, hz, mode);  // 20m por defecto
  TEST_ASSERT_EQUAL_UINT32(14200000, hz);
  TEST_ASSERT_EQUAL(Mode::USB, mode);

  mem.remember(14074000, Mode::DIG);
  mem.recall(5, hz, mode);
  TEST_ASSERT_EQUAL_UINT32(14074000, hz);
  TEST_ASSERT_EQUAL(Mode::DIG, mode);

  mem.remember(15000000, Mode::AM);  // fuera de banda: no cambia nada
  mem.recall(5, hz, mode);
  TEST_ASSERT_EQUAL_UINT32(14074000, hz);
}

static void test_keypad() {
  KeypadEntry k;
  uint32_t hz = 0;
  TEST_ASSERT_FALSE(k.value(hz));
  for (char c : {'1', '4', '5', '.', '5'}) TEST_ASSERT_TRUE(k.press(c));
  TEST_ASSERT_FALSE(k.press('.'));
  TEST_ASSERT_EQUAL_STRING("145.5", k.text());
  TEST_ASSERT_TRUE(k.value(hz));
  TEST_ASSERT_EQUAL_UINT32(145500000, hz);

  k.clear();
  for (char c : {'7', '.', '0', '7', '4', '5', '6'}) k.press(c);
  TEST_ASSERT_TRUE(k.value(hz));
  TEST_ASSERT_EQUAL_UINT32(7074560, hz);

  k.backspace();
  TEST_ASSERT_EQUAL_STRING("7.0745", k.text());

  k.clear();
  for (char c : {'2', '0', '0'}) k.press(c);
  TEST_ASSERT_FALSE(k.value(hz));  // 200 MHz fuera de rango
}

static void test_keypad_rounds_to_10hz() {
  KeypadEntry k;
  for (char c : {'1', '4', '.', '0', '7', '4', '0', '0', '6'}) k.press(c);
  uint32_t hz = 0;
  TEST_ASSERT_TRUE(k.value(hz));
  TEST_ASSERT_EQUAL_UINT32(14074010, hz);
}

static void test_wrap_index() {
  TEST_ASSERT_EQUAL(0, wrapIndex(49, 1, 50));
  TEST_ASSERT_EQUAL(49, wrapIndex(0, -1, 50));
  TEST_ASSERT_EQUAL(5, wrapIndex(4, 1, 50));
}

static void test_model_names() {
  TEST_ASSERT_EQUAL_STRING("FT-817", modelName(RigModel::FT817));
  TEST_ASSERT_EQUAL_STRING("FT-897", modelName(RigModel::FT897));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_band_lookup);
  RUN_TEST(test_60m_by_model);
  RUN_TEST(test_rx_ranges);
  RUN_TEST(test_tune_snaps_to_grid);
  RUN_TEST(test_tune_rejects_out_of_range);
  RUN_TEST(test_step_labels);
  RUN_TEST(test_band_memory);
  RUN_TEST(test_keypad);
  RUN_TEST(test_keypad_rounds_to_10hz);
  RUN_TEST(test_wrap_index);
  RUN_TEST(test_model_names);
  return UNITY_END();
}

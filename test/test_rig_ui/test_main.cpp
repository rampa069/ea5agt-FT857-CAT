#include <unity.h>

#include <initializer_list>

#include <stdio.h>
#include <string.h>

#include "bt_types.h"
#include "rig_ui_logic.h"
#include "ui_settings.h"

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

static void test_settings_defaults_and_validation() {
  Settings st;
  st.setDefaults(TouchCal{185, 3816, 323, 3887, false}, true);
  TEST_ASSERT_TRUE(st.invertColors);
  TEST_ASSERT_TRUE(st.valid());
  TEST_ASSERT_EQUAL(Transport::Cable, st.transport);
  TEST_ASSERT_FALSE(st.btHaveDevice);
  TEST_ASSERT_EQUAL_STRING("1234", st.btPin);

  Settings bad = st;
  bad.version = 1;  // ajustes de una versión anterior: se descartan
  TEST_ASSERT_FALSE(bad.valid());
  bad = st;
  bad.transport = static_cast<Transport>(7);
  TEST_ASSERT_FALSE(bad.valid());
  bad = st;
  memset(bad.btPin, 'x', sizeof(bad.btPin));  // sin terminador
  TEST_ASSERT_FALSE(bad.valid());
}

static void test_settings_migrate_from_v2() {
  Settings saved;
  saved.setDefaults(TouchCal{185, 3816, 323, 3887, false}, false);
  saved.transport = Transport::Bluetooth;
  saved.btHaveDevice = true;
  saved.btDevice = BtDevice{{0x98, 0xD3, 0x31, 0xF5, 0xA2, 0x10}, "HC-06", -50};
  saved.version = 2;

  // Simula leer de NVS sólo los bytes que guardaba la versión 2.
  size_t stored = Settings::sizeOfVersion(2);
  Settings loaded;
  memset(&loaded, 0xAB, sizeof(loaded));
  memcpy(&loaded, &saved, stored);
  TEST_ASSERT_TRUE(loaded.migrate(stored, true));
  TEST_ASSERT_EQUAL(Settings::kVersion, loaded.version);
  TEST_ASSERT_TRUE(loaded.invertColors);
  TEST_ASSERT_TRUE(loaded.btHaveDevice);
  TEST_ASSERT_EQUAL_STRING("HC-06", loaded.btDevice.name);
  TEST_ASSERT_EQUAL(Transport::Bluetooth, loaded.transport);

  Settings junk;
  memset(&junk, 0, sizeof(junk));
  TEST_ASSERT_FALSE(junk.migrate(stored, true));  // versión 0: no se aprovecha
}

static BtFound found(const char* name, uint32_t cod, int8_t rssi) {
  BtFound f{};
  snprintf(f.device.name, sizeof(f.device.name), "%s", name);
  f.cod = cod;
  f.device.rssi = rssi;
  return f;
}

static void test_bt_adapter_score() {
  TEST_ASSERT_EQUAL(3, btAdapterScore(found("HC-06", 0x1F00, -70)));
  TEST_ASSERT_EQUAL(3, btAdapterScore(found("linvor", 0, -70)));
  TEST_ASSERT_EQUAL(3, btAdapterScore(found("FT857 CAT", 0x5A020C, -70)));
  TEST_ASSERT_EQUAL(2, btAdapterScore(found("", 0x001F00, -70)));   // HC-06 sin nombre resuelto
  TEST_ASSERT_EQUAL(1, btAdapterScore(found("", 0, -70)));
  TEST_ASSERT_EQUAL(0, btAdapterScore(found("Pixel 8", 0x5A020C, -40)));     // móvil
  TEST_ASSERT_EQUAL(0, btAdapterScore(found("JBL Flip", 0x240414, -40)));    // audio
}

static void test_bt_sort_puts_unnamed_adapter_before_phones() {
  // El caso de Miquel: el HC-06 no anuncia nombre y quedaba detrás de los dispositivos con nombre.
  BtFound list[5] = {
      found("Pixel 8", 0x5A020C, -40),
      found("Samsung TV", 0x20041C, -55),
      found("JBL Flip", 0x240414, -45),
      found("", 0x001F00, -75),  // el adaptador
      found("ea5iue-laptop", 0x6C010C, -60),
  };
  sortBtFound(list, 5);
  TEST_ASSERT_EQUAL_INT8(-75, list[0].device.rssi);
  TEST_ASSERT_EQUAL_STRING("", list[0].device.name);
  TEST_ASSERT_EQUAL_STRING("Pixel 8", list[1].device.name);  // luego por señal
  TEST_ASSERT_EQUAL_STRING("JBL Flip", list[2].device.name);
}

static void test_settings_migrate_from_v3() {
  Settings saved;
  saved.setDefaults(TouchCal{185, 3816, 323, 3887, false}, true);
  saved.version = 3;
  size_t stored = Settings::sizeOfVersion(3);
  Settings loaded;
  memset(&loaded, 0xAB, sizeof(loaded));
  memcpy(&loaded, &saved, stored);
  TEST_ASSERT_TRUE(loaded.migrate(stored, false));
  TEST_ASSERT_EQUAL(0, loaded.skin);
  TEST_ASSERT_TRUE(loaded.invertColors);  // se conserva lo que ya había
}

static void test_dial_range() {
  uint32_t lo, hi;
  dialRange(14074000, lo, hi);
  TEST_ASSERT_EQUAL_UINT32(14000000, lo);
  TEST_ASSERT_EQUAL_UINT32(14350000, hi);
  dialRange(15500000, lo, hi);  // fuera de banda: el MHz que la contiene
  TEST_ASSERT_EQUAL_UINT32(15000000, lo);
  TEST_ASSERT_EQUAL_UINT32(16000000, hi);
}

static void test_bt_address_format() {
  const uint8_t a[6] = {0x98, 0xD3, 0x31, 0xF5, 0xA2, 0x10};
  char buf[18];
  formatBtAddr(a, buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("98:D3:31:F5:A2:10", buf);
  TEST_ASSERT_EQUAL_STRING("Conectado", btStateName(BtState::Connected));
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
  RUN_TEST(test_settings_defaults_and_validation);
  RUN_TEST(test_settings_migrate_from_v2);
  RUN_TEST(test_bt_address_format);
  RUN_TEST(test_settings_migrate_from_v3);
  RUN_TEST(test_dial_range);
  RUN_TEST(test_bt_adapter_score);
  RUN_TEST(test_bt_sort_puts_unnamed_adapter_before_phones);
  return UNITY_END();
}

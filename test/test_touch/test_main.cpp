#include <unity.h>

#include "touch_filter.h"

using namespace rigui;

void setUp() {}
void tearDown() {}

static TouchEventType feedRun(TouchFilter& f, bool contact, uint32_t& now, int count, int stepMs = 10) {
  TouchEventType last = TouchEventType::None;
  for (int i = 0; i < count; ++i) {
    TouchEvent e = f.feed(contact, 100, 50, now);
    if (e) last = e.type;
    now += stepMs;
  }
  return last;
}

static void test_down_after_three_samples_then_up() {
  TouchFilter f;
  uint32_t now = 0;
  TEST_ASSERT_EQUAL(TouchEventType::None, f.feed(true, 100, 50, now).type);
  TEST_ASSERT_EQUAL(TouchEventType::None, f.feed(true, 102, 52, now += 10).type);
  TouchEvent e = f.feed(true, 104, 54, now += 10);
  TEST_ASSERT_EQUAL(TouchEventType::Down, e.type);
  TEST_ASSERT_EQUAL(102, e.x);
  TEST_ASSERT_EQUAL(52, e.y);
  now += 10;
  TEST_ASSERT_EQUAL(TouchEventType::Up, feedRun(f, false, now, 20));
}

static void test_short_dropouts_do_not_release() {
  TouchFilter f;
  uint32_t now = 0;
  feedRun(f, true, now, 3);
  TEST_ASSERT_TRUE(f.pressed());
  // Huecos de 50 ms sin contacto mientras se mantiene el dedo: sigue pulsado, sin Up ni Down.
  for (int i = 0; i < 5; ++i) {
    TEST_ASSERT_EQUAL(TouchEventType::None, feedRun(f, false, now, 5));
    feedRun(f, true, now, 1);
  }
  TEST_ASSERT_TRUE(f.pressed());
}

static void test_single_glitch_is_ignored() {
  TouchFilter f;
  uint32_t now = 0;
  feedRun(f, true, now, 1);
  TEST_ASSERT_EQUAL(TouchEventType::None, feedRun(f, false, now, 30));
  TEST_ASSERT_FALSE(f.pressed());
}

static void test_repeat_while_held() {
  TouchFilter f;
  uint32_t now = 0;
  feedRun(f, true, now, 3);
  int repeats = 0;
  for (int i = 0; i < 100; ++i) {  // 1 s
    if (f.feed(true, 100, 50, now).type == TouchEventType::Repeat) ++repeats;
    now += 10;
  }
  // primera repetición a 450 ms y luego cada 120 ms: 450, 570, 690, 810, 930
  TEST_ASSERT_EQUAL(5, repeats);
}

static void test_map_touch_with_board_calibration() {
  TouchCal cal{185, 3816, 323, 3887, false};
  int16_t x, y;
  mapTouch(cal, 185, 323, 320, 240, x, y);
  TEST_ASSERT_EQUAL(0, x);
  TEST_ASSERT_EQUAL(0, y);
  mapTouch(cal, 3816, 3887, 320, 240, x, y);
  TEST_ASSERT_EQUAL(319, x);
  TEST_ASSERT_EQUAL(239, y);
  mapTouch(cal, 10, 5000, 320, 240, x, y);
  TEST_ASSERT_EQUAL(0, x);
  TEST_ASSERT_EQUAL(239, y);
}

static void test_calibration_from_measured_points() {
  // Puntos medidos en la placa del usuario con el env touchtest.
  const RawPoint raw[5] = {{416, 650}, {3589, 594}, {3590, 3619}, {411, 3559}, {1969, 2128}};
  TouchCal cal;
  TEST_ASSERT_TRUE(computeCalibration(raw, 320, 240, 20, cal));
  TEST_ASSERT_FALSE(cal.swapXY);
  TEST_ASSERT_INT_WITHIN(5, 185, cal.xMin);
  TEST_ASSERT_INT_WITHIN(5, 3816, cal.xMax);
  TEST_ASSERT_INT_WITHIN(5, 323, cal.yMin);
  TEST_ASSERT_INT_WITHIN(5, 3887, cal.yMax);
}

static void test_calibration_rejects_bounced_points() {
  const RawPoint raw[5] = {{435, 730}, {408, 712}, {3573, 601}, {411, 3559}, {1969, 2128}};
  TouchCal cal{1, 2, 3, 4, false};
  TEST_ASSERT_FALSE(computeCalibration(raw, 320, 240, 20, cal));
  TEST_ASSERT_EQUAL(1, cal.xMin);
}

static void test_calibration_detects_swapped_axes() {
  const RawPoint raw[5] = {{650, 416}, {594, 3589}, {3619, 3590}, {3559, 411}, {2128, 1969}};
  TouchCal cal;
  TEST_ASSERT_TRUE(computeCalibration(raw, 320, 240, 20, cal));
  TEST_ASSERT_TRUE(cal.swapXY);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_down_after_three_samples_then_up);
  RUN_TEST(test_short_dropouts_do_not_release);
  RUN_TEST(test_single_glitch_is_ignored);
  RUN_TEST(test_repeat_while_held);
  RUN_TEST(test_map_touch_with_board_calibration);
  RUN_TEST(test_calibration_from_measured_points);
  RUN_TEST(test_calibration_rejects_bounced_points);
  RUN_TEST(test_calibration_detects_swapped_axes);
  return UNITY_END();
}

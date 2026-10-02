#include <unity.h>

#include "rig_format.h"

using namespace ft8x7;

void setUp() {}
void tearDown() {}

static void test_format_frequency() {
  char buf[16];
  formatFrequency(14074000, buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("14.074.00", buf);
  formatFrequency(145500000, buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("145.500.00", buf);
  formatFrequency(7030010, buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("7.030.01", buf);
  formatFrequency(100000, buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("0.100.00", buf);
}

static void test_format_smeter() {
  char buf[8];
  formatSMeter(0, buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("S0", buf);
  formatSMeter(9, buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("S9", buf);
  formatSMeter(11, buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("S9+20", buf);
  formatSMeter(15, buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("S9+60", buf);
}

static void test_format_mode() {
  char buf[8];
  FreqMode fm{7030000, Mode::CW, true, 0x82};
  formatMode(fm, buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("CW-N", buf);
  fm.narrow = false;
  formatMode(fm, buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("CW", buf);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_format_frequency);
  RUN_TEST(test_format_smeter);
  RUN_TEST(test_format_mode);
  return UNITY_END();
}

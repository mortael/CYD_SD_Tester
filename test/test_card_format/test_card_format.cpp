#include <cstring>

#include <unity.h>

#include "hardware/card_format.h"

void setUp() {}
void tearDown() {}

static void test_format_human_bytes_mib_range() {
  char out[24];
  formatHumanBytes(500ULL * 1024ULL * 1024ULL, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("500.0 MiB", out);
}

static void test_format_human_bytes_gib_range() {
  char out[24];
  formatHumanBytes(2ULL * 1073741824ULL, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("2.00 GiB", out);
}

static void test_format_human_bytes_boundary_is_gib() {
  char out[24];
  // Exactly 1 GiB should take the GiB branch, not MiB.
  formatHumanBytes(1073741824ULL, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("1.00 GiB", out);
}

static void test_format_human_bytes_zero() {
  char out[24];
  formatHumanBytes(0, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("0.0 MiB", out);
}

static void test_format_spi_clock_whole_mhz() {
  char out[20];
  formatSpiClock(40000000UL, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("40 MHz", out);
}

static void test_format_spi_clock_fractional_mhz() {
  char out[20];
  formatSpiClock(1500000UL, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("1.5 MHz", out);
}

static void test_format_spi_clock_khz_range() {
  char out[20];
  formatSpiClock(500000UL, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("500 kHz", out);
}

int main(int argc, char** argv) {
  UNITY_BEGIN();

  RUN_TEST(test_format_human_bytes_mib_range);
  RUN_TEST(test_format_human_bytes_gib_range);
  RUN_TEST(test_format_human_bytes_boundary_is_gib);
  RUN_TEST(test_format_human_bytes_zero);
  RUN_TEST(test_format_spi_clock_whole_mhz);
  RUN_TEST(test_format_spi_clock_fractional_mhz);
  RUN_TEST(test_format_spi_clock_khz_range);

  return UNITY_END();
}

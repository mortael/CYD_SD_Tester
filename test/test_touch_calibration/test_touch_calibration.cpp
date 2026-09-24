#include <unity.h>

#include "core/config.h"
#include "hardware/touch_calibration.h"

void setUp() {}
void tearDown() {}

static void test_min_raw_maps_to_screen_origin() {
  int sx = -1;
  int sy = -1;

  // Raw X/Y both at their calibration minimum should map to (0, 0).
  mapTouchToScreen(TS_X_MIN, TS_Y_MIN, sx, sy);

  TEST_ASSERT_EQUAL_INT(0, sx);
  TEST_ASSERT_EQUAL_INT(0, sy);
}

static void test_max_raw_maps_to_screen_far_corner() {
  int sx = -1;
  int sy = -1;

  mapTouchToScreen(TS_X_MAX, TS_Y_MAX, sx, sy);

  TEST_ASSERT_EQUAL_INT(SCREEN_W - 1, sx);
  TEST_ASSERT_EQUAL_INT(SCREEN_H - 1, sy);
}

static void test_axes_are_swapped() {
  // Raw X drives screen Y, and raw Y drives screen X — verify the
  // swap by moving only one raw axis at a time.
  int sx = -1;
  int sy = -1;
  mapTouchToScreen(TS_X_MIN, TS_Y_MAX, sx, sy);

  TEST_ASSERT_EQUAL_INT(SCREEN_W - 1, sx);
  TEST_ASSERT_EQUAL_INT(0, sy);
}

static void test_out_of_range_raw_values_are_clamped() {
  int sx = -1;
  int sy = -1;

  mapTouchToScreen(
      TS_X_MIN - 1000,
      TS_Y_MAX + 1000,
      sx,
      sy
  );

  TEST_ASSERT_EQUAL_INT(SCREEN_W - 1, sx);
  TEST_ASSERT_EQUAL_INT(0, sy);
}

static void test_midpoint_maps_near_screen_center() {
  int sx = -1;
  int sy = -1;

  const int midX = (TS_X_MIN + TS_X_MAX) / 2;
  const int midY = (TS_Y_MIN + TS_Y_MAX) / 2;

  mapTouchToScreen(midX, midY, sx, sy);

  TEST_ASSERT_INT_WITHIN(2, (SCREEN_W - 1) / 2, sx);
  TEST_ASSERT_INT_WITHIN(2, (SCREEN_H - 1) / 2, sy);
}

int main(int argc, char** argv) {
  UNITY_BEGIN();

  RUN_TEST(test_min_raw_maps_to_screen_origin);
  RUN_TEST(test_max_raw_maps_to_screen_far_corner);
  RUN_TEST(test_axes_are_swapped);
  RUN_TEST(test_out_of_range_raw_values_are_clamped);
  RUN_TEST(test_midpoint_maps_near_screen_center);

  return UNITY_END();
}

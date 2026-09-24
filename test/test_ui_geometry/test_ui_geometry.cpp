#include <unity.h>

#include "ui/ui_geometry.h"

void setUp() {}
void tearDown() {}

static void test_hit_test_rect_inside() {
  TEST_ASSERT_TRUE(hitTestRect(10, 20, 100, 50, 50, 40));
}

static void test_hit_test_rect_outside() {
  TEST_ASSERT_FALSE(hitTestRect(10, 20, 100, 50, 200, 200));
}

static void test_hit_test_rect_top_left_inclusive() {
  TEST_ASSERT_TRUE(hitTestRect(10, 20, 100, 50, 10, 20));
}

static void test_hit_test_rect_bottom_right_exclusive() {
  // x + w and y + h are just outside the button (half-open interval).
  TEST_ASSERT_FALSE(hitTestRect(10, 20, 100, 50, 110, 20));
  TEST_ASSERT_FALSE(hitTestRect(10, 20, 100, 50, 10, 70));
}

static void test_bar_fill_width_zero_value() {
  TEST_ASSERT_EQUAL_INT(0, computeBarFillWidth(0.0f, 10.0f, 190));
}

static void test_bar_fill_width_full_value() {
  TEST_ASSERT_EQUAL_INT(188, computeBarFillWidth(10.0f, 10.0f, 190));
}

static void test_bar_fill_width_half_value() {
  TEST_ASSERT_EQUAL_INT(94, computeBarFillWidth(5.0f, 10.0f, 190));
}

static void test_bar_fill_width_clamps_when_value_exceeds_max() {
  TEST_ASSERT_EQUAL_INT(188, computeBarFillWidth(50.0f, 10.0f, 190));
}

static void test_bar_fill_width_zero_max_returns_zero() {
  TEST_ASSERT_EQUAL_INT(0, computeBarFillWidth(5.0f, 0.0f, 190));
}

static SpiScaleResult makeScaleResult(
    const float* speeds,
    const bool* ok,
    size_t count
) {
  SpiScaleResult result;
  result.count = count;

  for (size_t i = 0; i < count; ++i) {
    result.points[i].hz = (uint32_t)(i + 1) * 10000000UL;
    result.points[i].ok = ok[i];
    result.points[i].rawReadMBs = speeds[i];
    result.points[i].efficiencyPercent = 50.0f;
  }

  return result;
}

static void test_scale_graph_max_speed_pads_the_fastest_point() {
  const float speeds[] = {1.0f, 2.0f, 1.5f};
  const bool ok[] = {true, true, true};
  const SpiScaleResult result =
      makeScaleResult(speeds, ok, 3);

  TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f * 1.15f, scaleGraphMaxSpeed(result));
}

static void test_scale_graph_max_speed_ignores_failed_points() {
  const float speeds[] = {1.0f, 99.0f};
  const bool ok[] = {true, false};
  const SpiScaleResult result =
      makeScaleResult(speeds, ok, 2);

  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f * 1.15f, scaleGraphMaxSpeed(result));
}

static void test_scale_graph_max_speed_floor_when_all_failed() {
  const float speeds[] = {0.0f, 0.0f};
  const bool ok[] = {false, false};
  const SpiScaleResult result =
      makeScaleResult(speeds, ok, 2);

  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.1f * 1.15f, scaleGraphMaxSpeed(result));
}

static void test_scale_point_coords_first_and_last_span_the_graph_width() {
  const ScaleGraph graph { 34, 48, 272, 122 };
  SpiScalePoint point;
  point.ok = true;
  point.rawReadMBs = 1.0f;

  const ScalePointXY first =
      scalePointCoords(graph, 2.0f, 0, 4, point);
  const ScalePointXY last =
      scalePointCoords(graph, 2.0f, 3, 4, point);

  TEST_ASSERT_EQUAL_INT(graph.x + 12, first.x);
  TEST_ASSERT_EQUAL_INT(graph.x + graph.w - 12, last.x);
}

static void test_scale_point_coords_not_plotted_when_failed() {
  const ScaleGraph graph { 34, 48, 272, 122 };
  SpiScalePoint point;
  point.ok = false;

  const ScalePointXY xy =
      scalePointCoords(graph, 2.0f, 0, 2, point);

  TEST_ASSERT_FALSE(xy.plotted);
}

static void test_scale_point_coords_not_plotted_when_max_speed_zero() {
  const ScaleGraph graph { 34, 48, 272, 122 };
  SpiScalePoint point;
  point.ok = true;
  point.rawReadMBs = 1.0f;

  const ScalePointXY xy =
      scalePointCoords(graph, 0.0f, 0, 2, point);

  TEST_ASSERT_FALSE(xy.plotted);
}

static void test_scale_point_coords_single_point_does_not_divide_by_zero() {
  const ScaleGraph graph { 34, 48, 272, 122 };
  SpiScalePoint point;
  point.ok = true;
  point.rawReadMBs = 1.0f;

  const ScalePointXY xy =
      scalePointCoords(graph, 2.0f, 0, 1, point);

  TEST_ASSERT_EQUAL_INT(graph.x + 12, xy.x);
}

static void test_build_scale_summary_uses_first_successful_point() {
  const float speeds[] = {0.0f, 2.5f, 3.0f};
  const bool ok[] = {false, true, true};
  const SpiScaleResult result =
      makeScaleResult(speeds, ok, 3);

  char summary[96];
  buildScaleSummary(result, summary, sizeof(summary));

  TEST_ASSERT_EQUAL_STRING("20MHz 2.50MB/s 50%", summary);
}

static void test_build_scale_summary_shows_first_point_error_when_all_failed() {
  const float speeds[] = {0.0f, 0.0f};
  const bool ok[] = {false, false};
  SpiScaleResult result =
      makeScaleResult(speeds, ok, 2);
  result.points[0].errorCode = 0x0A;
  result.points[0].errorData = 0x0B;

  char summary[96];
  buildScaleSummary(result, summary, sizeof(summary));

  TEST_ASSERT_EQUAL_STRING("10MHz FAILED 0A/0B", summary);
}

static void test_build_scale_summary_empty_when_no_points() {
  SpiScaleResult result;
  result.count = 0;

  char summary[96];
  buildScaleSummary(result, summary, sizeof(summary));

  TEST_ASSERT_EQUAL_STRING("", summary);
}

int main(int argc, char** argv) {
  UNITY_BEGIN();

  RUN_TEST(test_hit_test_rect_inside);
  RUN_TEST(test_hit_test_rect_outside);
  RUN_TEST(test_hit_test_rect_top_left_inclusive);
  RUN_TEST(test_hit_test_rect_bottom_right_exclusive);
  RUN_TEST(test_bar_fill_width_zero_value);
  RUN_TEST(test_bar_fill_width_full_value);
  RUN_TEST(test_bar_fill_width_half_value);
  RUN_TEST(test_bar_fill_width_clamps_when_value_exceeds_max);
  RUN_TEST(test_bar_fill_width_zero_max_returns_zero);
  RUN_TEST(test_scale_graph_max_speed_pads_the_fastest_point);
  RUN_TEST(test_scale_graph_max_speed_ignores_failed_points);
  RUN_TEST(test_scale_graph_max_speed_floor_when_all_failed);
  RUN_TEST(test_scale_point_coords_first_and_last_span_the_graph_width);
  RUN_TEST(test_scale_point_coords_not_plotted_when_failed);
  RUN_TEST(test_scale_point_coords_not_plotted_when_max_speed_zero);
  RUN_TEST(test_scale_point_coords_single_point_does_not_divide_by_zero);
  RUN_TEST(test_build_scale_summary_uses_first_successful_point);
  RUN_TEST(test_build_scale_summary_shows_first_point_error_when_all_failed);
  RUN_TEST(test_build_scale_summary_empty_when_no_points);

  return UNITY_END();
}

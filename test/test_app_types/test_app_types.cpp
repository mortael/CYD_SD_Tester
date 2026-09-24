#include <unity.h>

#include "core/app_types.h"

void setUp() {}
void tearDown() {}

static void test_attempt_result_defaults() {
  AttemptResult a;
  TEST_ASSERT_EQUAL_UINT32(0, a.hz);
  TEST_ASSERT_TRUE(AttemptStage::NotTried == a.stage);
  TEST_ASSERT_EQUAL_UINT8(0, a.errorCode);
  TEST_ASSERT_EQUAL_UINT8(0, a.errorData);
}

static void test_card_info_defaults_are_all_not_ok() {
  // A default-constructed CardInfo must read as "nothing found yet",
  // not as a stale PASS — display_ui and diagnostics branch on these
  // before a scan ever runs.
  CardInfo info;
  TEST_ASSERT_FALSE(info.cardOk);
  TEST_ASSERT_FALSE(info.volumeOk);
  TEST_ASSERT_FALSE(info.cidOk);
  TEST_ASSERT_FALSE(info.invalidManufacturerId);
  TEST_ASSERT_FALSE(info.identityAnomaly);
  TEST_ASSERT_TRUE(info.bytes == 0);
  TEST_ASSERT_EQUAL_UINT8(0, info.volumeErrorCode);
  TEST_ASSERT_EQUAL_UINT8(0, info.volumeErrorData);
}

static void test_benchmark_result_cleanup_defaults_true() {
  // Every other bool defaults to false ("not attempted / not passed"),
  // but cleanupOk defaults true: "no cleanup was needed" must not read
  // as "cleanup failed". Only an actual failed removeTestFile() should
  // flip this to false. See runQuickBenchmark() in benchmark.cpp.
  BenchmarkResult result;
  TEST_ASSERT_TRUE(result.cleanupOk);
  TEST_ASSERT_FALSE(result.preallocOk);
  TEST_ASSERT_FALSE(result.fileWriteOk);
  TEST_ASSERT_FALSE(result.fileReadOk);
  TEST_ASSERT_FALSE(result.verifyOk);
  TEST_ASSERT_FALSE(result.rawReadOk);
  TEST_ASSERT_EQUAL_UINT8(0, result.ioErrorCode);
  TEST_ASSERT_EQUAL_UINT8(0, result.ioErrorData);
}

static void test_spi_scale_result_defaults() {
  SpiScaleResult result;
  TEST_ASSERT_EQUAL_UINT32(0, result.count);

  for (size_t i = 0; i < SPI_SCALE_SPEED_COUNT; ++i) {
    TEST_ASSERT_FALSE(result.points[i].ok);
    TEST_ASSERT_EQUAL_UINT32(0, result.points[i].hz);
  }
}

int main(int argc, char** argv) {
  UNITY_BEGIN();

  RUN_TEST(test_attempt_result_defaults);
  RUN_TEST(test_card_info_defaults_are_all_not_ok);
  RUN_TEST(test_benchmark_result_cleanup_defaults_true);
  RUN_TEST(test_spi_scale_result_defaults);

  return UNITY_END();
}

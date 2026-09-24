#include <unity.h>

#include "benchmark/bench_math.h"

void setUp() {}
void tearDown() {}

static void test_crc32_known_vector() {
  // CRC-32/ISO-HDLC of ASCII "123456789" is the standard check value.
  const uint8_t data[] = "123456789";
  const uint32_t crc = crc32Update(0, data, 9);
  TEST_ASSERT_EQUAL_HEX32(0xCBF43926U, crc);
}

static void test_crc32_empty_input_is_zero() {
  TEST_ASSERT_EQUAL_HEX32(0, crc32Update(0, nullptr, 0));
}

static void test_crc32_matches_across_split_chunks() {
  const uint8_t data[] = {1, 2, 3, 4, 5, 6, 7, 8};
  const uint32_t whole = crc32Update(0, data, 8);

  uint32_t split = crc32Update(0, data, 3);
  split = crc32Update(split, data + 3, 5);

  TEST_ASSERT_EQUAL_HEX32(whole, split);
}

static void test_throughput_zero_time_returns_zero() {
  TEST_ASSERT_EQUAL_FLOAT(0.0f, throughputMBs(1000000, 0));
}

static void test_throughput_one_mb_per_second() {
  // 1,000,000 bytes in 1,000,000 microseconds == 1.0 MB/s (decimal MB).
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, throughputMBs(1000000, 1000000));
}

static void test_throughput_scales_with_bytes() {
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 2.0f, throughputMBs(2000000, 1000000));
}

static void test_prepare_pattern_is_deterministic() {
  uint8_t bufferA[16];
  uint8_t bufferB[16];

  preparePattern(bufferA, sizeof(bufferA));
  preparePattern(bufferB, sizeof(bufferB));

  TEST_ASSERT_EQUAL_UINT8_ARRAY(bufferA, bufferB, sizeof(bufferA));

  // Spot-check the formula directly: (i * 37 + 11) & 0xFF
  TEST_ASSERT_EQUAL_UINT8(11, bufferA[0]);
  TEST_ASSERT_EQUAL_UINT8((37 + 11) & 0xFF, bufferA[1]);
}

static void test_repeated_buffer_crc32_single_pass_matches_direct_crc() {
  uint8_t buffer[64];
  preparePattern(buffer, sizeof(buffer));

  const uint32_t direct = crc32Update(0, buffer, sizeof(buffer));
  const uint32_t repeated =
      repeatedBufferCrc32(buffer, sizeof(buffer), sizeof(buffer));

  TEST_ASSERT_EQUAL_HEX32(direct, repeated);
}

static void test_repeated_buffer_crc32_handles_partial_final_chunk() {
  uint8_t buffer[64];
  preparePattern(buffer, sizeof(buffer));

  // Total size not a multiple of the buffer size: last chunk is partial.
  const uint32_t repeated =
      repeatedBufferCrc32(buffer, sizeof(buffer), 100);

  uint32_t manual = crc32Update(0, buffer, 64);
  manual = crc32Update(manual, buffer, 36);

  TEST_ASSERT_EQUAL_HEX32(manual, repeated);
}

static void test_spi_efficiency_full_theoretical_rate() {
  // 20 MHz / 8 bits-per-byte = 2.5 MB/s theoretical. Achieving exactly
  // that measured rate is 100% efficiency.
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 100.0f, spiEfficiencyPercent(20000000UL, 2.5f));
}

static void test_spi_efficiency_half_theoretical_rate() {
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 50.0f, spiEfficiencyPercent(20000000UL, 1.25f));
}

static void test_spi_efficiency_zero_hz_is_zero() {
  TEST_ASSERT_EQUAL_FLOAT(0.0f, spiEfficiencyPercent(0, 5.0f));
}

int main(int argc, char** argv) {
  UNITY_BEGIN();

  RUN_TEST(test_crc32_known_vector);
  RUN_TEST(test_crc32_empty_input_is_zero);
  RUN_TEST(test_crc32_matches_across_split_chunks);
  RUN_TEST(test_throughput_zero_time_returns_zero);
  RUN_TEST(test_throughput_one_mb_per_second);
  RUN_TEST(test_throughput_scales_with_bytes);
  RUN_TEST(test_prepare_pattern_is_deterministic);
  RUN_TEST(test_repeated_buffer_crc32_single_pass_matches_direct_crc);
  RUN_TEST(test_repeated_buffer_crc32_handles_partial_final_chunk);
  RUN_TEST(test_spi_efficiency_full_theoretical_rate);
  RUN_TEST(test_spi_efficiency_half_theoretical_rate);
  RUN_TEST(test_spi_efficiency_zero_hz_is_zero);

  return UNITY_END();
}

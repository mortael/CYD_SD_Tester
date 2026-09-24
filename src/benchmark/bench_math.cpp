#include "bench_math.h"

uint32_t crc32Update(
    uint32_t crc,
    const uint8_t* data,
    size_t len
) {
  crc = ~crc;

  for (size_t i = 0;
       i < len;
       ++i) {
    crc ^= data[i];

    for (uint8_t bit = 0;
         bit < 8;
         ++bit) {
      const uint32_t mask =
          -(crc & 1U);

      crc =
          (crc >> 1) ^
          (0xEDB88320U & mask);
    }
  }

  return ~crc;
}

float throughputMBs(
    size_t bytes,
    uint64_t ioMicros
) {
  if (ioMicros == 0) {
    return 0.0f;
  }

  return
      (bytes / 1000000.0f) /
      (ioMicros / 1000000.0f);
}

void preparePattern(
    uint8_t* buffer,
    size_t bufferSize
) {
  for (size_t i = 0;
       i < bufferSize;
       ++i) {
    buffer[i] =
        (uint8_t)(
            (i * 37U + 11U) &
            0xFF
        );
  }
}

float spiEfficiencyPercent(
    uint32_t hz,
    float measuredMBs
) {
  const float theoreticalMBs =
      hz / 8.0f / 1000000.0f;

  if (theoreticalMBs <= 0.0f) {
    return 0.0f;
  }

  return
      (measuredMBs / theoreticalMBs) *
      100.0f;
}

uint32_t repeatedBufferCrc32(
    const uint8_t* buffer,
    size_t bufferSize,
    size_t totalSize
) {
  uint32_t crc = 0;
  size_t done = 0;

  while (done < totalSize) {
    const size_t remaining =
        totalSize - done;

    const size_t chunk =
        remaining < bufferSize
            ? remaining
            : bufferSize;

    crc =
        crc32Update(
            crc,
            buffer,
            chunk
        );

    done += chunk;
  }

  return crc;
}

#pragma once
#include <cstddef>
#include <cstdint>

// Pure benchmark math — no hardware/Arduino dependency, safe to
// unit-test on host (see test/test_bench_math).

uint32_t crc32Update(
    uint32_t crc,
    const uint8_t* data,
    size_t len
);

float throughputMBs(
    size_t bytes,
    uint64_t ioMicros
);

void preparePattern(
    uint8_t* buffer,
    size_t bufferSize
);

// CRC32 of `buffer` (bufferSize bytes) repeated until `totalSize` bytes
// have been processed — matches the content the benchmark actually
// writes to the test file (the pattern buffer written chunk by chunk).
uint32_t repeatedBufferCrc32(
    const uint8_t* buffer,
    size_t bufferSize,
    size_t totalSize
);

// Percentage of theoretical raw SPI byte rate (hz / 8 bits-per-byte,
// decimal MB/s) actually achieved by a measured raw read speed.
// Returns 0 if hz is 0.
float spiEfficiencyPercent(
    uint32_t hz,
    float measuredMBs
);

#pragma once
#include <SdFat.h>
#include <cstddef>
#include <cstdint>

#include "benchmark.h"

// Shared internals for the benchmark_*.cpp files. Not part of the
// public benchmark.h API.

// Raw sector I/O, used by both the quick file benchmark and the SPI
// scaling benchmark. (DMA buffer allocation is shared from
// ../hardware/sd_card.h instead, since hardware/ can't depend back on
// benchmark/.)
bool rawReadAtCurrentSpeed(
    uint8_t* buffer,
    size_t bytesToRead,
    float& speedMBs,
    uint8_t& errorCode,
    uint8_t& errorData
);

// Timed, chunked file I/O passes used by runFileBenchmarkSequence()
// (benchmark.cpp). Each records an SD error into errorCode/errorData
// via recordSdError() on failure.

bool timedFileWrite(
    FsFile& file,
    uint8_t* buffer,
    BenchmarkProgressFn onProgress,
    float& speedMBs,
    uint8_t& errorCode,
    uint8_t& errorData
);

bool timedFileRead(
    FsFile& file,
    uint8_t* buffer,
    BenchmarkProgressFn onProgress,
    float& speedMBs,
    uint8_t& errorCode,
    uint8_t& errorData
);

bool verifyFile(
    FsFile& file,
    uint8_t* buffer,
    BenchmarkProgressFn onProgress,
    uint32_t expectedCrc,
    uint32_t& actualCrc,
    uint8_t& errorCode,
    uint8_t& errorData
);

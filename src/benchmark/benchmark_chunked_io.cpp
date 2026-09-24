#include <Arduino.h>
#include <SdFat.h>

#include "../core/config.h"
#include "bench_math.h"
#include "../hardware/sd_card.h"
#include "benchmark_internal.h"

// crcAccum is only ever non-null for chunkReadAndCrc's own CRC
// accumulator; chunkWrite/chunkRead ignore it. A concrete uint32_t*
// (rather than void*) means the one real use isn't an unchecked cast.
using ChunkIoFn = int (*)(
    FsFile& file,
    uint8_t* buffer,
    size_t chunkSize,
    uint32_t* crcAccum
);

static int chunkWrite(
    FsFile& file,
    uint8_t* buffer,
    size_t chunkSize,
    uint32_t* /*crcAccum*/
) {
  return file.write(buffer, chunkSize);
}

static int chunkRead(
    FsFile& file,
    uint8_t* buffer,
    size_t chunkSize,
    uint32_t* /*crcAccum*/
) {
  return file.read(buffer, chunkSize);
}

static int chunkReadAndCrc(
    FsFile& file,
    uint8_t* buffer,
    size_t chunkSize,
    uint32_t* crcAccum
) {
  const int n =
      file.read(buffer, chunkSize);

  if (n == (int)chunkSize) {
    *crcAccum =
        crc32Update(
            *crcAccum,
            buffer,
            chunkSize
        );
  }

  return n;
}

// Shared chunked-I/O loop for the write/read/verify passes below. Only
// the actual SdFat call (chunkFn) and progress updates are timed out
// of chunkFn's control; per the project's benchmark timing invariant,
// ioMicrosOut accumulates only chunkFn's own time, never the
// onProgress() calls between chunks.
static bool runChunkedIo(
    FsFile& file,
    uint8_t* buffer,
    BenchmarkProgressFn onProgress,
    const char* title,
    const char* stage,
    bool showLiveRate,
    ChunkIoFn chunkFn,
    uint32_t* crcAccum,
    uint64_t& ioMicrosOut,
    uint8_t& errorCode,
    uint8_t& errorData
) {
  size_t done = 0;
  size_t nextProgress = 0;
  ioMicrosOut = 0;

  onProgress(
      title,
      stage,
      0,
      BENCH_TEST_SIZE,
      -1.0f
  );

  while (done <
         BENCH_TEST_SIZE) {
    const size_t chunk =
        min(
            BENCH_BUFFER_SIZE,
            BENCH_TEST_SIZE - done
        );

    const uint32_t t0 =
        micros();

    const int transferred =
        chunkFn(
            file,
            buffer,
            chunk,
            crcAccum
        );

    const uint32_t t1 =
        micros();

    ioMicrosOut +=
        (uint32_t)(t1 - t0);

    if (transferred !=
        (int)chunk) {
      recordSdError(
          getSd(),
          errorCode,
          errorData,
          stage
      );
      return false;
    }

    done += chunk;

    if (done >= nextProgress ||
        done == BENCH_TEST_SIZE) {
      onProgress(
          title,
          stage,
          done,
          BENCH_TEST_SIZE,
          showLiveRate
              ? throughputMBs(
                    done,
                    ioMicrosOut
                )
              : -1.0f
      );

      nextProgress =
          done +
          BENCH_PROGRESS_STEP;
    }
  }

  return true;
}

// timedFileWrite/timedFileRead below are both a single timed,
// full-speed pass over the file with a live throughput readout;
// they differ only in which chunk function/progress stage to use.
static bool timedFileTransfer(
    FsFile& file,
    uint8_t* buffer,
    BenchmarkProgressFn onProgress,
    const char* stage,
    ChunkIoFn chunkFn,
    float& speedMBs,
    uint8_t& errorCode,
    uint8_t& errorData
) {
  uint64_t ioMicros;

  const bool ok =
      runChunkedIo(
          file,
          buffer,
          onProgress,
          "BENCHMARK",
          stage,
          true,
          chunkFn,
          nullptr,
          ioMicros,
          errorCode,
          errorData
      );

  if (ok) {
    speedMBs =
        throughputMBs(
            BENCH_TEST_SIZE,
            ioMicros
        );
  }

  return ok;
}

bool timedFileWrite(
    FsFile& file,
    uint8_t* buffer,
    BenchmarkProgressFn onProgress,
    float& speedMBs,
    uint8_t& errorCode,
    uint8_t& errorData
) {
  return timedFileTransfer(
      file,
      buffer,
      onProgress,
      "File write",
      chunkWrite,
      speedMBs,
      errorCode,
      errorData
  );
}

bool timedFileRead(
    FsFile& file,
    uint8_t* buffer,
    BenchmarkProgressFn onProgress,
    float& speedMBs,
    uint8_t& errorCode,
    uint8_t& errorData
) {
  return timedFileTransfer(
      file,
      buffer,
      onProgress,
      "File read",
      chunkRead,
      speedMBs,
      errorCode,
      errorData
  );
}

bool verifyFile(
    FsFile& file,
    uint8_t* buffer,
    BenchmarkProgressFn onProgress,
    uint32_t expectedCrc,
    uint32_t& actualCrc,
    uint8_t& errorCode,
    uint8_t& errorData
) {
  actualCrc = 0;

  uint64_t ioMicros;

  const bool readOk =
      runChunkedIo(
          file,
          buffer,
          onProgress,
          "VERIFY",
          "CRC readback",
          false,
          chunkReadAndCrc,
          &actualCrc,
          ioMicros,
          errorCode,
          errorData
      );

  return
      readOk &&
      actualCrc == expectedCrc;
}

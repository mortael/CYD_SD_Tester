#include <Arduino.h>
#include <SdFat.h>

#include "../core/config.h"
#include "../core/app_types.h"
#include "bench_math.h"
#include "../hardware/sd_card.h"
#include "../hardware/card_format.h"
#include "benchmark_internal.h"
#include "benchmark.h"

static bool removeTestFile() {
  const CardInfo& card =
      getCardInfo();

  SdFs& sd =
      getSd();

  if (!card.volumeOk) {
    return false;
  }

  if (!sd.exists(TEST_FILE)) {
    return true;
  }

  return sd.remove(TEST_FILE);
}

// Preallocate -> write -> read -> verify, in order, stopping at the
// first failure. Every failure path (preallocate, each seek, and the
// timed steps themselves) records an SD error code into result, so
// result.ioErrorCode/ioErrorData always reflects the step that
// actually failed rather than only some of them.
static void runFileBenchmarkSequence(
    FsFile& file,
    uint8_t* buffer,
    BenchmarkProgressFn onProgress,
    BenchmarkResult& result
) {
  SdFs& sd =
      getSd();

  onProgress(
      "BENCHMARK",
      "Pre-allocating",
      0,
      BENCH_TEST_SIZE,
      -1.0f
  );

  const uint32_t start =
      millis();

  result.preallocOk =
      file.preAllocate(
          BENCH_TEST_SIZE
      );

  result.preallocMs =
      millis() - start;

  Serial.printf(
      "preAllocate(%u) = %s (%lu ms)\n",
      (unsigned)BENCH_TEST_SIZE,
      result.preallocOk
          ? "PASS"
          : "FAIL",
      (unsigned long)
          result.preallocMs
  );

  if (!result.preallocOk) {
    recordSdError(
        sd,
        result.ioErrorCode,
        result.ioErrorData,
        "PREALLOCATE"
    );
    return;
  }

  if (!file.seekSet(0)) {
    recordSdError(
        sd,
        result.ioErrorCode,
        result.ioErrorData,
        "SEEK BEFORE WRITE"
    );
    return;
  }

  result.fileWriteOk =
      timedFileWrite(
          file,
          buffer,
          onProgress,
          result.fileWriteMBs,
          result.ioErrorCode,
          result.ioErrorData
      );

  file.flush();

  if (!result.fileWriteOk) {
    return;  // timedFileWrite() already recorded the error.
  }

  if (!file.seekSet(0)) {
    recordSdError(
        sd,
        result.ioErrorCode,
        result.ioErrorData,
        "SEEK BEFORE READ"
    );
    return;
  }

  result.fileReadOk =
      timedFileRead(
          file,
          buffer,
          onProgress,
          result.fileReadMBs,
          result.ioErrorCode,
          result.ioErrorData
      );

  if (!result.fileReadOk) {
    return;  // timedFileRead() already recorded the error.
  }

  if (!file.seekSet(0)) {
    recordSdError(
        sd,
        result.ioErrorCode,
        result.ioErrorData,
        "SEEK BEFORE VERIFY"
    );
    return;
  }

  result.verifyOk =
      verifyFile(
          file,
          buffer,
          onProgress,
          result.expectedCrc,
          result.verifyCrc,
          result.ioErrorCode,
          result.ioErrorData
      );
}

static void printBenchmarkResult(
    const CardInfo& card,
    const BenchmarkResult& result
) {
  char spi[20];
  formatSpiClock(
      card.activeSpiHz,
      spi,
      sizeof(spi)
  );

  Serial.println();
  Serial.println(
      "=== BENCHMARK RESULT ==="
  );
  Serial.printf(
      "SPI: %s\n",
      spi
  );
  Serial.printf(
      "Preallocation: %s (%lu ms)\n",
      result.preallocOk
          ? "PASS"
          : "N/A/FAIL",
      (unsigned long)
          result.preallocMs
  );

  if (result.fileWriteOk) {
    Serial.printf(
        "File write: %.2f MB/s\n",
        result.fileWriteMBs
    );
  }

  if (result.fileReadOk) {
    Serial.printf(
        "File read: %.2f MB/s\n",
        result.fileReadMBs
    );
  }

  Serial.printf(
      "File CRC: %08lX / %08lX (%s)\n",
      (unsigned long)
          result.expectedCrc,
      (unsigned long)
          result.verifyCrc,
      result.verifyOk
          ? "PASS"
          : "FAIL/N/A"
  );

  if (result.rawReadOk) {
    Serial.printf(
        "Raw read: %.2f MB/s\n",
        result.rawReadMBs
    );
  } else {
    Serial.println(
        "Raw read: FAILED"
    );
  }

  if (!result.cleanupOk) {
    Serial.println(
        "Cleanup: FAILED (temp file may remain on card)"
    );
  }

  if (result.ioErrorCode ||
      result.ioErrorData) {
    Serial.printf(
        "I/O error: %02X/%02X\n",
        result.ioErrorCode,
        result.ioErrorData
    );
  }

  Serial.println(
      "========================"
  );
}

BenchmarkResult runQuickBenchmark(
    BenchmarkProgressFn onProgress
) {
  BenchmarkResult result;

  const CardInfo& card =
      getCardInfo();

  SdFs& sd =
      getSd();

  if (!card.cardOk) {
    return result;
  }

  uint8_t* buffer =
      allocateDmaBuffer(
          BENCH_BUFFER_SIZE,
          "Benchmark"
      );

  if (!buffer) {
    return result;
  }

  preparePattern(buffer, BENCH_BUFFER_SIZE);

  result.expectedCrc =
      repeatedBufferCrc32(
          buffer,
          BENCH_BUFFER_SIZE,
          BENCH_TEST_SIZE
      );

  if (card.volumeOk) {
    removeTestFile();

    FsFile file =
        sd.open(
            TEST_FILE,
            O_RDWR |
            O_CREAT |
            O_TRUNC
        );

    if (file) {
      runFileBenchmarkSequence(
          file,
          buffer,
          onProgress,
          result
      );

      file.close();

      result.cleanupOk =
          removeTestFile();
    } else {
      recordSdError(
          sd,
          result.ioErrorCode,
          result.ioErrorData,
          "OPEN"
      );
    }
  }

  onProgress(
      "BENCHMARK",
      "Raw sector read",
      0,
      BENCH_TEST_SIZE,
      -1.0f
  );

  result.rawReadOk =
      rawReadAtCurrentSpeed(
          buffer,
          BENCH_TEST_SIZE,
          result.rawReadMBs,
          result.ioErrorCode,
          result.ioErrorData
      );

  free(buffer);

  printBenchmarkResult(card, result);

  return result;
}

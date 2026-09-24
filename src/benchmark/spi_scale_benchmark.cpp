#include <Arduino.h>
#include <SdFat.h>

#include "../core/config.h"
#include "../core/app_types.h"
#include "bench_math.h"
#include "../hardware/sd_card.h"
#include "../hardware/card_format.h"
#include "benchmark_internal.h"
#include "benchmark.h"

SpiScaleResult runSpiScalingBenchmark() {
  SpiScaleResult result;
  result.count =
      SPI_SCALE_SPEED_COUNT;

  SdFs& sd =
      getSd();

  Serial.println();
  Serial.println(
      "=== SPI SCALING TEST ==="
  );

  uint8_t* buffer =
      allocateDmaBuffer(
          BENCH_BUFFER_SIZE,
          "SPI scale"
      );

  uint8_t* sector0 =
      allocateDmaBuffer(
          SECTOR_SIZE,
          "Scale qualify"
      );

  if (!buffer ||
      !sector0) {
    if (buffer) free(buffer);
    if (sector0) free(sector0);

    scanCard();
    return result;
  }

  for (size_t i = 0;
       i < SPI_SCALE_SPEED_COUNT;
       ++i) {
    SpiScalePoint& point =
        result.points[i];

    point.hz =
        SPI_SCALE_SPEEDS[i];

    char spiText[20];
    formatSpiClock(
        point.hz,
        spiText,
        sizeof(spiText)
    );

    sd.end();
    delay(10);

    Serial.printf(
        "Trying %s ... ",
        spiText
    );

    if (!sd.cardBegin(
            makeSdConfig(
                point.hz))) {
      recordSdError(
          sd,
          point.errorCode,
          point.errorData,
          "INIT"
      );
      continue;
    }

    if (!sd.card()->readSector(
            0,
            sector0)) {
      recordSdError(
          sd,
          point.errorCode,
          point.errorData,
          "READ"
      );
      continue;
    }

    if (!rawReadAtCurrentSpeed(
            buffer,
            SPI_SCALE_TEST_SIZE,
            point.rawReadMBs,
            point.errorCode,
            point.errorData)) {
      continue;
    }

    point.efficiencyPercent =
        spiEfficiencyPercent(
            point.hz,
            point.rawReadMBs
        );

    point.ok = true;

    Serial.printf(
        "%.2f MB/s  %.1f%%\n",
        point.rawReadMBs,
        point.efficiencyPercent
    );
  }

  Serial.println(
      "========================"
  );

  free(buffer);
  free(sector0);

  // Scaling changes the active SdFat card state.
  // Restore normal qualification/mount afterwards.
  scanCard();

  return result;
}

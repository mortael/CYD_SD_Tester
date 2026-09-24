#include <Arduino.h>
#include <SdFat.h>

#include "../core/config.h"
#include "bench_math.h"
#include "../hardware/sd_card.h"
#include "benchmark_internal.h"

bool rawReadAtCurrentSpeed(
    uint8_t* buffer,
    size_t bytesToRead,
    float& speedMBs,
    uint8_t& errorCode,
    uint8_t& errorData
) {
  SdFs& sd =
      getSd();

  if (!sd.card()) {
    return false;
  }

  const uint32_t totalSectors =
      sd.card()->sectorCount();

  if (totalSectors == 0) {
    return false;
  }

  const uint32_t wantedSectors =
      bytesToRead /
      SECTOR_SIZE;

  const uint32_t sectorsToRead =
      min(
          wantedSectors,
          totalSectors
      );

  uint32_t sector = 0;
  uint64_t ioMicros = 0;

  while (sector <
         sectorsToRead) {
    const uint32_t chunkSectors =
        min(
            (uint32_t)
                RAW_SECTORS_PER_CHUNK,
            sectorsToRead - sector
        );

    const uint32_t t0 =
        micros();

    const bool ok =
        sd.card()->readSectors(
            sector,
            buffer,
            chunkSectors
        );

    const uint32_t t1 =
        micros();

    ioMicros +=
        (uint32_t)(t1 - t0);

    if (!ok) {
      char label[32];
      snprintf(
          label,
          sizeof(label),
          "RAW READ LBA %lu",
          (unsigned long)sector
      );

      recordSdError(
          sd,
          errorCode,
          errorData,
          label
      );
      return false;
    }

    sector +=
        chunkSectors;
  }

  const size_t bytesRead =
      (size_t)sector *
      SECTOR_SIZE;

  speedMBs =
      throughputMBs(
          bytesRead,
          ioMicros
      );

  return true;
}

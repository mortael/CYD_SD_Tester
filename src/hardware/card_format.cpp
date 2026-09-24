#include "card_format.h"

#include <cmath>
#include <cstdio>

void formatHumanBytes(
    uint64_t bytes,
    char* out,
    size_t outSize
) {
  const double gib =
      bytes / 1073741824.0;

  if (gib >= 1.0) {
    snprintf(
        out,
        outSize,
        "%.2f GiB",
        gib
    );
  } else {
    snprintf(
        out,
        outSize,
        "%.1f MiB",
        bytes / 1048576.0
    );
  }
}

void formatSpiClock(
    uint32_t hz,
    char* out,
    size_t outSize
) {
  if (hz >= 1000000UL) {
    const float mhz =
        hz / 1000000.0f;

    if (fabsf(mhz - roundf(mhz)) <
        0.01f) {
      snprintf(
          out,
          outSize,
          "%.0f MHz",
          mhz
      );
    } else {
      snprintf(
          out,
          outSize,
          "%.1f MHz",
          mhz
      );
    }
  } else {
    snprintf(
        out,
        outSize,
        "%lu kHz",
        (unsigned long)(hz / 1000UL)
    );
  }
}

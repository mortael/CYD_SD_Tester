#include "touch_calibration.h"

#include "../core/config.h"

static long mapRange(
    long x,
    long inMin,
    long inMax,
    long outMin,
    long outMax
) {
  return
      (x - inMin) *
          (outMax - outMin) /
          (inMax - inMin) +
      outMin;
}

static int clampInt(
    int value,
    int lo,
    int hi
) {
  if (value < lo) return lo;
  if (value > hi) return hi;
  return value;
}

void mapTouchToScreen(
    int rawX,
    int rawY,
    int& screenX,
    int& screenY
) {
  screenX =
      (int)mapRange(
          rawY,
          TS_Y_MIN,
          TS_Y_MAX,
          0,
          SCREEN_W - 1
      );

  screenY =
      (int)mapRange(
          rawX,
          TS_X_MIN,
          TS_X_MAX,
          0,
          SCREEN_H - 1
      );

  screenX = clampInt(screenX, 0, SCREEN_W - 1);
  screenY = clampInt(screenY, 0, SCREEN_H - 1);
}

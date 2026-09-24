#include "ui_geometry.h"

#include <cstdio>

bool hitTestRect(
    int rectX,
    int rectY,
    int rectW,
    int rectH,
    int pointX,
    int pointY
) {
  return
      pointX >= rectX &&
      pointX < rectX + rectW &&
      pointY >= rectY &&
      pointY < rectY + rectH;
}

int computeBarFillWidth(
    float value,
    float maxValue,
    int barWidth
) {
  int fillW = 0;

  if (maxValue > 0.0f) {
    fillW =
        (int)(
            (barWidth - 2) *
            (value / maxValue)
        );
  }

  if (fillW < 0) return 0;
  if (fillW > barWidth - 2) return barWidth - 2;
  return fillW;
}

float scaleGraphMaxSpeed(
    const SpiScaleResult& result
) {
  float maxSpeed = 0.1f;

  for (size_t i = 0;
       i < result.count;
       ++i) {
    if (result.points[i].ok &&
        result.points[i].rawReadMBs >
            maxSpeed) {
      maxSpeed =
          result.points[i].rawReadMBs;
    }
  }

  return maxSpeed * 1.15f;
}

ScalePointXY scalePointCoords(
    const ScaleGraph& graph,
    float maxSpeed,
    size_t index,
    size_t count,
    const SpiScalePoint& point
) {
  const size_t denom =
      count > 1
          ? count - 1
          : 1;

  const int x =
      graph.x + 12 +
      (int)(
          index *
          (graph.w - 24) /
          denom
      );

  if (!point.ok ||
      maxSpeed <= 0.0f) {
    return {
        x,
        graph.y + graph.h - 8,
        false
    };
  }

  const int y =
      graph.y +
      graph.h - 8 -
      (int)(
          (graph.h - 20) *
          (point.rawReadMBs /
           maxSpeed)
      );

  return { x, y, true };
}

void buildScaleSummary(
    const SpiScaleResult& result,
    char* out,
    size_t outSize
) {
  out[0] = '\0';

  for (size_t i = 0;
       i < result.count;
       ++i) {
    if (result.points[i].ok) {
      snprintf(
          out,
          outSize,
          "%luMHz %.2fMB/s %.0f%%",
          (unsigned long)(
              result.points[i].hz /
              1000000UL
          ),
          result.points[i].rawReadMBs,
          result.points[i]
              .efficiencyPercent
      );
      return;
    }
  }

  // No speed qualified: surface the fastest attempted point's SD
  // error instead of leaving the summary blank, so a fully-failed
  // scan still gives a diagnostic starting point.
  if (result.count > 0) {
    snprintf(
        out,
        outSize,
        "%luMHz FAILED %02X/%02X",
        (unsigned long)(
            result.points[0].hz /
            1000000UL
        ),
        result.points[0].errorCode,
        result.points[0].errorData
    );
  }
}

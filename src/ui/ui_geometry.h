#pragma once
#include <cstddef>

#include "../core/app_types.h"

// Pure UI geometry math — no TFT_eSPI/Arduino dependency, safe to
// unit-test on host (see test/test_ui_geometry).

bool hitTestRect(
    int rectX,
    int rectY,
    int rectW,
    int rectH,
    int pointX,
    int pointY
);

// Pixel width of a progress/result bar's filled portion, given the
// current value against maxValue, clamped to the bar's interior
// (barWidth - 2, leaving a 1px border on each side). Returns 0 if
// maxValue <= 0.
int computeBarFillWidth(
    float value,
    float maxValue,
    int barWidth
);

struct ScaleGraph {
  int x;
  int y;
  int w;
  int h;
};

// Padded max speed used to scale the SPI-scale graph's vertical axis,
// so the tallest point never touches the top of the frame.
float scaleGraphMaxSpeed(
    const SpiScaleResult& result
);

struct ScalePointXY {
  int x;
  int y;
  bool plotted;
};

// Coordinate for one SPI-scale data point within the graph frame.
// `plotted` is false for a failed attempt, whose x-axis label still
// gets drawn but has no line/dot.
ScalePointXY scalePointCoords(
    const ScaleGraph& graph,
    float maxSpeed,
    size_t index,
    size_t count,
    const SpiScalePoint& point
);

// First successful point's summary line. If no speed qualified, falls
// back to the first attempted point's SD error code instead of an
// empty string; returns an empty string only when `result` has no
// points at all.
void buildScaleSummary(
    const SpiScaleResult& result,
    char* out,
    size_t outSize
);

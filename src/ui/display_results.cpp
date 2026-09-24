#include <Arduino.h>
#include <TFT_eSPI.h>

#include "../hardware/sd_card.h"
#include "../hardware/card_format.h"
#include "display_ui.h"
#include "display_ui_internal.h"
#include "ui_geometry.h"

static void drawHorizontalBar(
    int y,
    const char* label,
    float value,
    float maxValue,
    uint16_t color
) {
  char valueText[24];

  snprintf(
      valueText,
      sizeof(valueText),
      "%.2f",
      value
  );

  tft().setTextDatum(TL_DATUM);
  tft().setTextColor(
      TFT_LIGHTGREY,
      TFT_BLACK
  );
  tft().drawString(
      label,
      8,
      y,
      2
  );

  tft().setTextDatum(TR_DATUM);
  tft().setTextColor(
      TFT_WHITE,
      TFT_BLACK
  );
  tft().drawString(
      valueText,
      310,
      y,
      2
  );

  const int barX = 72;
  const int barY = y + 3;
  const int barW = 190;
  const int barH = 11;

  tft().drawRect(
      barX,
      barY,
      barW,
      barH,
      TFT_DARKGREY
  );

  const int fillW =
      computeBarFillWidth(
          value,
          maxValue,
          barW
      );

  if (fillW > 0) {
    tft().fillRect(
        barX + 1,
        barY + 1,
        fillW,
        barH - 2,
        color
    );
  }
}

void uiShowBenchmarkResult(
    const CardInfo& info,
    const BenchmarkResult& result
) {
  header("BENCH RESULT");

  float maxValue = 0.01f;

  if (result.fileWriteOk &&
      result.fileWriteMBs > maxValue) {
    maxValue =
        result.fileWriteMBs;
  }

  if (result.fileReadOk &&
      result.fileReadMBs > maxValue) {
    maxValue =
        result.fileReadMBs;
  }

  if (result.rawReadOk &&
      result.rawReadMBs > maxValue) {
    maxValue =
        result.rawReadMBs;
  }

  drawHorizontalBar(
      52,
      "Write",
      result.fileWriteOk
          ? result.fileWriteMBs
          : 0.0f,
      maxValue,
      TFT_GREEN
  );

  drawHorizontalBar(
      82,
      "File",
      result.fileReadOk
          ? result.fileReadMBs
          : 0.0f,
      maxValue,
      TFT_CYAN
  );

  drawHorizontalBar(
      112,
      "Raw",
      result.rawReadOk
          ? result.rawReadMBs
          : 0.0f,
      maxValue,
      TFT_YELLOW
  );

  char spi[20];
  char footer[96];

  formatSpiClock(
      info.activeSpiHz,
      spi,
      sizeof(spi)
  );

  snprintf(
      footer,
      sizeof(footer),
      "SPI %s   CRC %s",
      spi,
      result.verifyOk
          ? "PASS"
          : "N/A/FAIL"
  );

  tft().setTextDatum(TL_DATUM);
  tft().setTextColor(
      TFT_LIGHTGREY,
      TFT_BLACK
  );

  tft().drawString(
      footer,
      10,
      150,
      2
  );

  tft().drawString(
      "Values in MB/s",
      10,
      170,
      2
  );

  tft().drawString(
      "Tap to return",
      10,
      194,
      2
  );
}

static constexpr ScaleGraph SCALE_GRAPH {
  34, 48, 272, 122
};

static void drawScaleGraphFrame(
    const ScaleGraph& graph,
    float maxSpeed
) {
  tft().drawRect(
      graph.x,
      graph.y,
      graph.w,
      graph.h,
      TFT_DARKGREY
  );

  tft().setTextDatum(TR_DATUM);
  tft().setTextColor(
      TFT_LIGHTGREY,
      TFT_BLACK
  );

  char yText[16];

  snprintf(
      yText,
      sizeof(yText),
      "%.1f",
      maxSpeed
  );

  tft().drawString(
      yText,
      graph.x - 4,
      graph.y - 2,
      1
  );

  tft().drawString(
      "0",
      graph.x - 4,
      graph.y + graph.h - 8,
      1
  );
}

static void drawScalePoint(
    const ScaleGraph& graph,
    const SpiScalePoint& point,
    const ScalePointXY& xy,
    int lastX,
    int lastY
) {
  if (xy.plotted && lastX >= 0) {
    tft().drawLine(
        lastX,
        lastY,
        xy.x,
        xy.y,
        TFT_CYAN
    );
  }

  if (xy.plotted) {
    tft().fillCircle(
        xy.x,
        xy.y,
        3,
        TFT_YELLOW
    );
  }

  char xLabel[12];

  snprintf(
      xLabel,
      sizeof(xLabel),
      "%lu",
      (unsigned long)
          (point.hz /
           1000000UL)
  );

  tft().setTextDatum(MC_DATUM);
  tft().setTextColor(
      TFT_LIGHTGREY,
      TFT_BLACK
  );
  tft().drawString(
      xLabel,
      xy.x,
      graph.y + graph.h + 10,
      1
  );
}

void uiShowSpiScaleResult(
    const SpiScaleResult& result
) {
  header("SPI SCALE");

  const ScaleGraph& graph =
      SCALE_GRAPH;

  const float maxSpeed =
      scaleGraphMaxSpeed(result);

  drawScaleGraphFrame(
      graph,
      maxSpeed
  );

  int lastX = -1;
  int lastY = -1;

  for (size_t i = 0;
       i < result.count;
       ++i) {
    const SpiScalePoint& point =
        result.points[i];

    const ScalePointXY xy =
        scalePointCoords(
            graph,
            maxSpeed,
            i,
            result.count,
            point
        );

    drawScalePoint(
        graph,
        point,
        xy,
        lastX,
        lastY
    );

    if (xy.plotted) {
      lastX = xy.x;
      lastY = xy.y;
    }
  }

  tft().setTextDatum(TL_DATUM);
  tft().setTextColor(
      TFT_LIGHTGREY,
      TFT_BLACK
  );

  tft().drawString(
      "MHz ->    raw MB/s",
      36,
      188,
      2
  );

  char summary[96];
  buildScaleSummary(
      result,
      summary,
      sizeof(summary)
  );

  if (summary[0]) {
    tft().drawString(
        summary,
        36,
        210,
        1
    );
  }
}

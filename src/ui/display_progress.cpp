#include <Arduino.h>
#include <TFT_eSPI.h>

#include "display_ui.h"
#include "display_ui_internal.h"
#include "ui_geometry.h"

void uiShowProgress(
    const char* title,
    const char* stage,
    size_t done,
    size_t total,
    float liveMBs
) {
  // No hidden screen state:
  // done == 0 explicitly starts a new progress screen.
  if (done == 0) {
    header(title);
  }

  tft().fillRect(
      8,
      45,
      304,
      110,
      TFT_BLACK
  );

  tft().setTextDatum(TL_DATUM);
  tft().setTextColor(
      TFT_WHITE,
      TFT_BLACK
  );
  tft().drawString(
      stage,
      10,
      52,
      2
  );

  const float pct =
      total > 0
          ? 100.0f *
            (float)done /
            (float)total
          : 0.0f;

  char text[32];

  snprintf(
      text,
      sizeof(text),
      "%.0f%%",
      pct
  );

  tft().setTextDatum(TR_DATUM);
  tft().setTextColor(
      TFT_LIGHTGREY,
      TFT_BLACK
  );
  tft().drawString(
      text,
      310,
      52,
      2
  );

  const int barX = 10;
  const int barY = 82;
  const int barW = 300;
  const int barH = 18;

  tft().drawRect(
      barX,
      barY,
      barW,
      barH,
      TFT_DARKGREY
  );

  const int innerW =
      computeBarFillWidth(
          (float)done,
          (float)total,
          barW
      );

  tft().fillRect(
      barX + 1,
      barY + 1,
      innerW,
      barH - 2,
      TFT_GREEN
  );

  snprintf(
      text,
      sizeof(text),
      "%.1f / %.1f MiB",
      done / 1048576.0f,
      total / 1048576.0f
  );

  tft().setTextDatum(TL_DATUM);
  tft().setTextColor(
      TFT_LIGHTGREY,
      TFT_BLACK
  );
  tft().drawString(
      text,
      10,
      112,
      2
  );

  if (liveMBs >= 0.0f) {
    snprintf(
        text,
        sizeof(text),
        "%.2f MB/s",
        liveMBs
    );

    tft().setTextColor(
        TFT_CYAN,
        TFT_BLACK
    );
    tft().drawString(
        text,
        10,
        136,
        2
    );
  }
}

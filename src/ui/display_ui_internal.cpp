#include "display_ui_internal.h"

#include <Arduino.h>

#include "../core/config.h"
#include "../hardware/touch.h"
#include "display_ui.h"

TFT_eSPI& tft() {
  static TFT_eSPI instance;
  return instance;
}

void header(
    const char* title
) {
  tft().fillScreen(TFT_BLACK);

  char fullTitle[48];

  snprintf(
      fullTitle,
      sizeof(fullTitle),
      "%s %s",
      title,
      APP_VERSION
  );

  tft().setTextDatum(TL_DATUM);
  tft().setTextColor(
      TFT_YELLOW,
      TFT_BLACK
  );
  tft().drawString(
      fullTitle,
      8,
      6,
      4
  );

  tft().drawFastHLine(
      8,
      34,
      304,
      TFT_DARKGREY
  );
}

void lineRow(
    int y,
    const char* left,
    const char* right,
    uint16_t rightColor
) {
  tft().setTextDatum(TL_DATUM);
  tft().setTextColor(
      TFT_LIGHTGREY,
      TFT_BLACK
  );
  tft().drawString(
      left,
      10,
      y,
      2
  );

  tft().setTextDatum(TR_DATUM);
  tft().setTextColor(
      rightColor,
      TFT_BLACK
  );
  tft().drawString(
      right,
      310,
      y,
      2
  );
}

void uiWaitForTap() {
  delay(250);

  int x;
  int y;

  while (true) {
    if (readTouch(x, y)) {
      waitForTouchRelease();
      return;
    }

    delay(10);
  }
}

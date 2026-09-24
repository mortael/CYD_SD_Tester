#include "display_ui.h"

#include <Arduino.h>
#include <TFT_eSPI.h>

#include "../hardware/card_format.h"
#include "display_ui_internal.h"

void uiShowNecromancerResult(
    const NecromancerResult& result
) {
  header("NECROMANCER");

  char value[48];

  lineRow(
      44,
      "Left idle",
      result.ready ? "YES" : "NO",
      result.ready ? TFT_GREEN : TFT_RED
  );

  if (result.speedHz > 0) {
    formatSpiClock(
        result.speedHz,
        value,
        sizeof(value)
    );

    lineRow(
        64,
        "Raw SPI",
        value
    );
  }

  lineRow(
      84,
      "SPI mode",
      result.usedSpiMode3 ? "3" : "0"
  );

  lineRow(
      104,
      "CMD1 fallback",
      result.usedCmd1 ? "USED" : "NO"
  );

  lineRow(
      124,
      "CID / CSD",
      result.cidOk && result.csdOk
          ? "PASS"
          : "FAIL",
      result.cidOk && result.csdOk
          ? TFT_GREEN
          : TFT_ORANGE
  );

  lineRow(
      144,
      "Sector 0",
      result.sector0ReadOk
          ? "READABLE"
          : "NO",
      result.sector0ReadOk
          ? TFT_GREEN
          : TFT_ORANGE
  );

  if (result.destructiveWriteAttempted) {
    snprintf(
        value,
        sizeof(value),
        "%s/%s/%s",
        result.destructiveWriteOk ? "W" : "-",
        result.destructiveVerifyOk ? "V" : "-",
        result.restoreOk ? "R" : "!"
    );

    lineRow(
        164,
        "Write/verify/restore",
        value,
        result.restoreOk
            ? TFT_GREEN
            : TFT_RED
    );
  }

  tft().setTextDatum(TL_DATUM);
  tft().setTextColor(
      result.destructiveWriteAttempted &&
      !result.restoreOk
          ? TFT_RED
          : TFT_LIGHTGREY,
      TFT_BLACK
  );

  tft().drawString(
      result.destructiveWriteAttempted &&
      !result.restoreOk
          ? "WARNING: LBA0 restore failed!"
          : "Full details in Serial monitor.",
      10,
      194,
      2
  );

  tft().drawString(
      "Tap to return",
      10,
      216,
      2
  );
}

#include <Arduino.h>
#include <TFT_eSPI.h>

#include "../hardware/sd_card.h"
#include "../hardware/card_format.h"
#include "../hardware/card_identity.h"
#include "display_ui.h"
#include "display_ui_internal.h"

void uiShowDiagnostics(
    const CardInfo& info,
    const AttemptResult* attempts,
    size_t attemptCount
) {
  header("SD DIAGNOSTICS");

  int y = 42;

  for (size_t i = 0;
       i < attemptCount;
       ++i) {
    const AttemptResult& a =
        attempts[i];

    char left[20];
    char right[32];

    formatSpiClock(
        a.hz,
        left,
        sizeof(left)
    );

    if (a.stage ==
        AttemptStage::Passed) {
      snprintf(
          right,
          sizeof(right),
          "PASS"
      );
    } else if (
        a.stage ==
        AttemptStage::NotTried) {
      snprintf(
          right,
          sizeof(right),
          "-"
      );
    } else {
      snprintf(
          right,
          sizeof(right),
          "%s %02X/%02X",
          attemptStageName(
              a.stage
          ),
          a.errorCode,
          a.errorData
      );
    }

    lineRow(
        y,
        left,
        right,
        a.stage ==
                AttemptStage::Passed
            ? TFT_GREEN
            : (
                a.stage ==
                        AttemptStage::NotTried
                    ? TFT_DARKGREY
                    : TFT_RED
              )
    );

    y += 18;
  }

  if (info.cardOk) {
    char text[64];

    snprintf(
        text,
        sizeof(text),
        "MID %02X  %s",
        info.manufacturerId,
        info.vendorName
    );

    tft().setTextDatum(TL_DATUM);
    tft().setTextColor(
        TFT_LIGHTGREY,
        TFT_BLACK
    );

    tft().drawString(
        text,
        10,
        142,
        2
    );

    snprintf(
        text,
        sizeof(text),
        "%s r%u.%u  %u/%u",
        info.product,
        info.revisionMajor,
        info.revisionMinor,
        info.month,
        info.year
    );

    tft().drawString(
        text,
        10,
        160,
        2
    );

    tft().setTextColor(
        info.identityAnomaly
            ? TFT_ORANGE
            : TFT_GREEN,
        TFT_BLACK
    );

    snprintf(
        text,
        sizeof(text),
        "Identity: %s",
        identityCheckName(info)
    );

    tft().drawString(
        text,
        10,
        178,
        2
    );
  }

  tft().setTextDatum(TR_DATUM);
  tft().setTextColor(
      TFT_DARKGREY,
      TFT_BLACK
  );

  tft().drawString(
      "tap to return",
      310,
      216,
      2
  );
}

#include <Arduino.h>
#include <TFT_eSPI.h>

#include "../core/config.h"
#include "../hardware/sd_card.h"
#include "../hardware/card_format.h"
#include "../hardware/touch.h"
#include "../hardware/card_identity.h"
#include "display_ui.h"
#include "display_ui_internal.h"
#include "ui_geometry.h"

struct Button {
  int x;
  int y;
  int w;
  int h;
  const char* label;
};

static constexpr Button BTN_SCAN {
  4, 196, 74, 36, "SCAN"
};

static constexpr Button BTN_BENCH {
  82, 196, 74, 36, "BENCH"
};

static constexpr Button BTN_DIAG {
  160, 196, 74, 36, "DIAG"
};

static constexpr Button BTN_TOOLS {
  238, 196, 78, 36, "TOOLS"
};

static constexpr Button BTN_QUICK {
  18, 72, 284, 42, "QUICK BENCHMARK"
};

static constexpr Button BTN_SCALE {
  18, 124, 284, 42, "SPI SCALING"
};

static constexpr Button BTN_BACK {
  112, 188, 96, 38, "BACK"
};

static bool hit(
    const Button& button,
    int x,
    int y
) {
  return hitTestRect(
      button.x,
      button.y,
      button.w,
      button.h,
      x,
      y
  );
}

static void drawButton(
    const Button& button,
    uint16_t border =
        TFT_DARKGREY
) {
  tft().drawRoundRect(
      button.x,
      button.y,
      button.w,
      button.h,
      5,
      border
  );

  tft().setTextDatum(MC_DATUM);
  tft().setTextColor(
      TFT_WHITE,
      TFT_BLACK
  );

  tft().drawString(
      button.label,
      button.x + button.w / 2,
      button.y + button.h / 2,
      2
  );
}

void initDisplay() {
  pinMode(
      TFT_BACKLIGHT,
      OUTPUT
  );

  digitalWrite(
      TFT_BACKLIGHT,
      HIGH
  );

  tft().init();
  tft().setRotation(1);
  tft().fillScreen(TFT_BLACK);
}

void uiShowScanning() {
  header("CYD SD TESTER");

  tft().setTextDatum(TL_DATUM);
  tft().setTextColor(
      TFT_LIGHTGREY,
      TFT_BLACK
  );

  tft().drawString(
      "Scanning microSD...",
      10,
      55,
      2
  );
}

void uiDrawHome(
    const CardInfo& info
) {
  header("CYD SD TESTER");

  char value[32];

  lineRow(
      42,
      "Raw card",
      info.cardOk
          ? "DETECTED"
          : "NOT FOUND",
      info.cardOk
          ? TFT_GREEN
          : TFT_RED
  );

  if (info.cardOk) {
    formatHumanBytes(
        info.bytes,
        value,
        sizeof(value)
    );

    lineRow(
        62,
        "Capacity",
        value
    );

    lineRow(
        82,
        "Card type",
        cardTypeName(
            info.cardType,
            info.bytes
        )
    );

    lineRow(
        102,
        "Filesystem",
        info.volumeOk
            ? fsName(info.fsType)
            : "not mounted",
        info.volumeOk
            ? TFT_GREEN
            : TFT_ORANGE
    );

    formatSpiClock(
        info.activeSpiHz,
        value,
        sizeof(value)
    );

    lineRow(
        122,
        "SPI clock",
        value
    );

    lineRow(
        142,
        "Product",
        strlen(info.product)
            ? info.product
            : "unavailable"
    );

    tft().setTextDatum(TL_DATUM);
    tft().setTextColor(
        TFT_LIGHTGREY,
        TFT_BLACK
    );

    tft().drawString(
        "BENCH contains quick + SPI scaling",
        10,
        168,
        2
    );
  } else {
    tft().setTextDatum(TL_DATUM);
    tft().setTextColor(
        TFT_ORANGE,
        TFT_BLACK
    );

    tft().drawString(
        "No stable SPI speed found.",
        10,
        62,
        2
    );

    tft().setTextColor(
        TFT_LIGHTGREY,
        TFT_BLACK
    );

    tft().drawString(
        "Tap DIAG for details.",
        10,
        82,
        2
    );
  }

  drawButton(BTN_SCAN);
  drawButton(
      BTN_BENCH,
      info.cardOk
          ? TFT_GREEN
          : TFT_DARKGREY
  );
  drawButton(
      BTN_DIAG,
      TFT_CYAN
  );

  drawButton(
      BTN_TOOLS,
      TFT_ORANGE
  );
}

HomeAction uiHitTestHome(
    int x,
    int y
) {
  if (hit(
          BTN_SCAN,
          x,
          y)) {
    return
        HomeAction::Rescan;
  }

  if (hit(
          BTN_BENCH,
          x,
          y)) {
    return
        HomeAction::BenchmarkMenu;
  }

  if (hit(
          BTN_DIAG,
          x,
          y)) {
    return
        HomeAction::Diagnostics;
  }

  if (hit(
          BTN_TOOLS,
          x,
          y)) {
    return
        HomeAction::Tools;
  }

  return
      HomeAction::None;
}

void uiDrawBenchMenu() {
  header("BENCHMARK");

  tft().setTextDatum(TL_DATUM);
  tft().setTextColor(
      TFT_LIGHTGREY,
      TFT_BLACK
  );

  tft().drawString(
      "Choose a benchmark:",
      18,
      48,
      2
  );

  drawButton(
      BTN_QUICK,
      TFT_GREEN
  );

  drawButton(
      BTN_SCALE,
      TFT_CYAN
  );

  drawButton(
      BTN_BACK
  );
}

BenchMenuAction uiHitTestBenchMenu(
    int x,
    int y
) {
  if (hit(
          BTN_QUICK,
          x,
          y)) {
    return
        BenchMenuAction::Quick;
  }

  if (hit(
          BTN_SCALE,
          x,
          y)) {
    return
        BenchMenuAction::Scale;
  }

  if (hit(
          BTN_BACK,
          x,
          y)) {
    return
        BenchMenuAction::Back;
  }

  return
      BenchMenuAction::None;
}


static constexpr Button BTN_INSPECT {
  18, 62, 284, 42, "DISK INSPECTOR"
};

static constexpr Button BTN_NECRO {
  18, 112, 284, 42, "SD NECROMANCER"
};

static constexpr Button BTN_TOOLS_BACK {
  112, 188, 96, 38, "BACK"
};

static constexpr Button BTN_ARM {
  18, 164, 132, 48, "ARM"
};

static constexpr Button BTN_CANCEL {
  170, 164, 132, 48, "CANCEL"
};

static constexpr Button BTN_RUN {
  72, 164, 176, 48, "RUN NECROMANCER"
};

void uiDrawToolsMenu() {
  header("TOOLS");

  tft().setTextDatum(TL_DATUM);
  tft().setTextColor(
      TFT_LIGHTGREY,
      TFT_BLACK
  );

  tft().drawString(
      "Inspect first, experiment second",
      18,
      40,
      2
  );

  drawButton(
      BTN_INSPECT,
      TFT_CYAN
  );

  drawButton(
      BTN_NECRO,
      TFT_RED
  );

  drawButton(
      BTN_TOOLS_BACK
  );
}

ToolsMenuAction uiHitTestToolsMenu(
    int x,
    int y
) {
  if (hit(BTN_INSPECT, x, y)) {
    return ToolsMenuAction::DiskInspector;
  }

  if (hit(BTN_NECRO, x, y)) {
    return ToolsMenuAction::Necromancer;
  }

  if (hit(BTN_TOOLS_BACK, x, y)) {
    return ToolsMenuAction::Back;
  }

  return ToolsMenuAction::None;
}

bool uiConfirmNecromancer() {
  header("DANGER");

  tft().setTextDatum(TL_DATUM);
  tft().setTextColor(
      TFT_RED,
      TFT_BLACK
  );

  tft().drawString(
      "DESTRUCTIVE SD EXPERIMENT",
      14,
      48,
      2
  );

  tft().setTextColor(
      TFT_LIGHTGREY,
      TFT_BLACK
  );

  tft().drawString(
      "If raw init succeeds, sector 0",
      14,
      76,
      2
  );
  tft().drawString(
      "will be overwritten, verified,",
      14,
      96,
      2
  );
  tft().drawString(
      "then restored. Restore can fail.",
      14,
      116,
      2
  );

  drawButton(BTN_ARM, TFT_RED);
  drawButton(BTN_CANCEL);

  while (true) {
    int x;
    int y;

    if (!readTouch(x, y)) {
      delay(5);
      continue;
    }

    const bool arm =
        hit(BTN_ARM, x, y);
    const bool cancel =
        hit(BTN_CANCEL, x, y);

    waitForTouchRelease();

    if (cancel) {
      return false;
    }

    if (arm) {
      break;
    }
  }

  header("ARMED");

  tft().setTextDatum(TL_DATUM);
  tft().setTextColor(
      TFT_RED,
      TFT_BLACK
  );
  tft().drawString(
      "Last chance.",
      14,
      58,
      4
  );

  tft().setTextColor(
      TFT_LIGHTGREY,
      TFT_BLACK
  );
  tft().drawString(
      "Tap RUN to start raw recovery.",
      14,
      112,
      2
  );

  drawButton(BTN_RUN, TFT_RED);

  while (true) {
    int x;
    int y;

    if (!readTouch(x, y)) {
      delay(5);
      continue;
    }

    const bool run =
        hit(BTN_RUN, x, y);

    waitForTouchRelease();

    if (run) {
      return true;
    }
  }
}

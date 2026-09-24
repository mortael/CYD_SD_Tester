#include <Arduino.h>

#include "hardware/touch.h"
#include "hardware/sd_card.h"
#include "ui/display_ui.h"
#include "benchmark/benchmark.h"
#include "tools/sd_necromancer.h"

static void rescan() {
  uiShowScanning();
  scanCard();
  uiDrawHome(getCardInfo());
}

static void showDiagnostics() {
  uiShowDiagnostics(
      getCardInfo(),
      getAttempts(),
      getAttemptCount()
  );

  uiWaitForTap();
}

static BenchMenuAction waitForBenchMenuAction() {
  while (true) {
    int x;
    int y;

    if (!readTouch(x, y)) {
      delay(5);
      continue;
    }

    const BenchMenuAction action =
        uiHitTestBenchMenu(
            x,
            y
        );

    waitForTouchRelease();

    if (action !=
        BenchMenuAction::None) {
      return action;
    }
  }
}

static void runBenchMenu() {
  while (true) {
    uiDrawBenchMenu();

    const BenchMenuAction action =
        waitForBenchMenuAction();

    if (action ==
        BenchMenuAction::Back) {
      return;
    }

    if (action ==
        BenchMenuAction::Quick) {
      const BenchmarkResult result =
          runQuickBenchmark(uiShowProgress);

      uiShowBenchmarkResult(
          getCardInfo(),
          result
      );

      uiWaitForTap();
      continue;
    }

    if (action ==
        BenchMenuAction::Scale) {
      const SpiScaleResult result =
          runSpiScalingBenchmark();

      uiShowSpiScaleResult(
          result
      );

      uiWaitForTap();
    }
  }
}


static ToolsMenuAction waitForToolsMenuAction() {
  while (true) {
    int x;
    int y;

    if (!readTouch(x, y)) {
      delay(5);
      continue;
    }

    const ToolsMenuAction action =
        uiHitTestToolsMenu(
            x,
            y
        );

    waitForTouchRelease();

    if (action != ToolsMenuAction::None) {
      return action;
    }
  }
}

static void runToolsMenu() {
  while (true) {
    uiDrawToolsMenu();

    const ToolsMenuAction action =
        waitForToolsMenuAction();

    if (action == ToolsMenuAction::Back) {
      return;
    }

    if (action == ToolsMenuAction::DiskInspector) {
      uiRunDiskInspector();
      continue;
    }

    if (action == ToolsMenuAction::Necromancer) {
      if (!uiConfirmNecromancer()) {
        continue;
      }

      const NecromancerResult result =
          runSdNecromancer();

      uiShowNecromancerResult(result);
      uiWaitForTap();
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(150);

  initDisplay();
  initTouch();
  initSdHardware();

  rescan();
}

void loop() {
  static uint32_t lastTouchMs = 0;

  if (millis() -
          lastTouchMs <
      250) {
    delay(5);
    return;
  }

  int x;
  int y;

  if (!readTouch(x, y)) {
    delay(5);
    return;
  }

  lastTouchMs =
      millis();

  const HomeAction action =
      uiHitTestHome(
          x,
          y
      );

  waitForTouchRelease();

  switch (action) {
    case HomeAction::Rescan:
      rescan();
      break;

    case HomeAction::BenchmarkMenu:
      runBenchMenu();
      uiDrawHome(
          getCardInfo()
      );
      break;

    case HomeAction::Diagnostics:
      showDiagnostics();
      uiDrawHome(
          getCardInfo()
      );
      break;

    case HomeAction::Tools:
      runToolsMenu();
      uiDrawHome(
          getCardInfo()
      );
      break;

    case HomeAction::None:
      break;
  }
}

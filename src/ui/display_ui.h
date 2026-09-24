#pragma once
#include "../core/app_types.h"

enum class HomeAction {
  None,
  Rescan,
  BenchmarkMenu,
  Diagnostics,
  Tools
};

enum class BenchMenuAction {
  None,
  Quick,
  Scale,
  Back
};

void initDisplay();

void uiShowScanning();

void uiDrawHome(
    const CardInfo& info
);

HomeAction uiHitTestHome(
    int x,
    int y
);

void uiDrawBenchMenu();

BenchMenuAction uiHitTestBenchMenu(
    int x,
    int y
);

void uiShowProgress(
    const char* title,
    const char* stage,
    size_t done,
    size_t total,
    float liveMBs = -1.0f
);

void uiShowBenchmarkResult(
    const CardInfo& info,
    const BenchmarkResult& result
);

void uiShowSpiScaleResult(
    const SpiScaleResult& result
);

void uiShowDiagnostics(
    const CardInfo& info,
    const AttemptResult* attempts,
    size_t attemptCount
);

void uiWaitForTap();


enum class ToolsMenuAction {
  None,
  DiskInspector,
  Necromancer,
  Back
};

void uiDrawToolsMenu();
ToolsMenuAction uiHitTestToolsMenu(int x, int y);

// Read-only MBR/filesystem/raw-sector toolbox.
void uiRunDiskInspector();

// Two-step destructive arming screen. Returns true only after the
// user explicitly taps ARM and then RUN.
bool uiConfirmNecromancer();

void uiShowNecromancerResult(
    const NecromancerResult& result
);

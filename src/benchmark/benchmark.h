#pragma once
#include <cstddef>

#include "../core/app_types.h"

// Progress callback matching display_ui.h's uiShowProgress() shape,
// threaded in explicitly (rather than #including ui/display_ui.h
// directly) so benchmark/ doesn't depend upward on ui/ — main.cpp
// passes uiShowProgress itself as the argument.
using BenchmarkProgressFn = void (*)(
    const char* title,
    const char* stage,
    size_t done,
    size_t total,
    float liveMBs
);

// Runs preallocate/write/read/verify (on the mounted filesystem, if
// any) followed by a raw sector read, reporting progress through
// onProgress and a full result summary to Serial. No side effect on
// global card state — unlike runSpiScalingBenchmark() below, it never
// touches the SPI speed or re-scans the card.
BenchmarkResult runQuickBenchmark(
    BenchmarkProgressFn onProgress
);

// Side effect: re-runs the full SPI qualification/mount scan (scanCard())
// before returning, both on early failure and after the scaling loop
// finishes, to restore the global card/filesystem state the scaling
// passes disturb. Callers can assume getCardInfo()/getSd() reflect a
// freshly re-qualified card afterward, not the last scaled-speed state.
SpiScaleResult runSpiScalingBenchmark();

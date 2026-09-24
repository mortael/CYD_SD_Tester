# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

Standalone microSD diagnostic/benchmark firmware for the ESP32-2432S028 / ESP32-2432S028R
"Cheap Yellow Display" (CYD) board. PlatformIO + Arduino framework, with two environments:
`env:cyd` (the real ESP32 firmware) and `env:native` (host-side unit tests, see below).

## Build & test commands

```bash
pio run                     # build the firmware (env:cyd)
pio run -t upload           # build + flash over USB
pio device monitor           # serial monitor (115200 baud), or: pio run -t upload -t monitor
pio run -t clean             # clean build artifacts

pio test -e native           # run the host-native unit test suite
pio test -e native -f test_bench_math   # run just one test suite (-f matches by folder name)
```

If `pio` isn't on PATH, use the VS Code PlatformIO extension's build/upload buttons, or
`~/.platformio/penv/Scripts/platformio` on Windows. `env:native` needs a host C++ compiler
on PATH (gcc/g++ or clang); on Windows without one, PlatformIO's bundled MinGW toolchain
works: add `~/.platformio/packages/toolchain-gccmingw32/bin` to PATH before running
`pio test -e native`.

### Test suite

`test/` holds host-native Unity tests for every hardware-independent pure-logic module —
`test_bench_math`, `test_card_format`, `test_card_identity`, `test_touch_calibration`,
`test_ui_geometry`, `test_app_types`. They run on the host (no ESP32/hardware needed) via
`env:native` in `platformio.ini`, whose `build_src_filter` compiles in only those pure
`.cpp` files (not the Arduino/SdFat/TFT_eSPI-coupled ones). `.github/workflows/native-tests.yml`
runs `pio test -e native` on every push/PR.

Hardware-coupled files (`main.cpp`, everything in `hardware/`, `benchmark/`, `ui/` that
touches SdFat/TFT_eSPI/Arduino directly) have no automated coverage and would need a full
mock hardware layer to test — validate those by flashing to real hardware and observing
the display.

## Architecture

`main.cpp` is intentionally a thin event loop: `setup()` wires up the display/touch/SD
modules and does an initial scan; `loop()` polls touch input, maps taps to a `HomeAction`,
and dispatches to the relevant module. Everything else lives under `src/`, grouped into
domain subdirectories:

- **`core/`** — `config.h` (pins, display size, touch calibration, SPI speed ladders,
  benchmark buffer/test sizes — change hardware wiring or timing constants here, not
  inline elsewhere) and `app_types.h` (shared structs/enums: `CardInfo`,
  `AttemptResult`/`AttemptStage`, `BenchmarkResult`, `SpiScalePoint`/`SpiScaleResult`).
  Both are free of Arduino/hardware headers (`<cstdint>` only) so they — and anything
  that only needs them — can compile and be unit-tested on the host. Note two *separate*
  SPI speed ladders in `config.h`: `SPI_SPEEDS` (normal card scan/qualification:
  20/10/4/1 MHz) and `SPI_SCALE_SPEEDS` (the diagnostic SPI Scaling benchmark:
  40/20/10/4/1 MHz) — see "40 MHz" note below.
- **`hardware/`** — everything that actually talks to the SD card and touch controller,
  plus their pure-logic companions (co-located here because each is consumed by exactly
  one hardware file, not shared across domains):
  - `sd_card.*` — SPI init, speed qualification/fallback, CID/CSD parsing, filesystem
    mount/state, plus the shared `allocateDmaBuffer()` and `recordSdError()` helpers used
    by both this module and `benchmark/` (hardware/ has no dependency on benchmark/, so
    these live here rather than the reverse). All benchmark logic — including the SPI
    scaling test — moved out to `benchmark/` in v0.7; don't add benchmark code back into
    `sd_card.*`. Uses `SdFat` (`SdFs`) exclusively; `DEDICATED_SPI` is the fixed config.
  - `card_format.*` — pure formatting (`formatHumanBytes`, `formatSpiClock`).
  - `card_identity.*` — pure naming/lookup (`fsName`, `attemptStageName`, `resolveVendor`,
    `identityCheckName`). `cardTypeName()` stays in `sd_card.*` instead since it needs
    SdFat's `SD_CARD_TYPE_*` constants.
  - `touch.*` — XPT2046 touch controller over *software* SPI (bit-banged, separate from
    the SD/TFT SPI buses); owns debounce (`waitForTouchRelease()`) so `main.cpp` never
    touches `TOUCH_IRQ` directly.
  - `touch_calibration.*` — pure raw-ADC-to-screen-coordinate mapping (including the
    deliberate X/Y axis swap), split out of `touch.cpp` so it's host-testable.
- **`benchmark/`** — `runQuickBenchmark()` (file write/read/verify/raw-read,
  `benchmark.cpp` + `benchmark_raw.cpp`) and `runSpiScalingBenchmark()`
  (`spi_scale_benchmark.cpp`; re-runs `scanCard()` on exit to restore normal card state —
  documented on the declaration in `benchmark.h`, not just in a comment at the call site).
  `bench_math.*` holds the pure math (CRC32, throughput, SPI efficiency, pattern
  generation) with zero hardware dependency. **Timing discipline is load-bearing**: the
  shared `runChunkedIo()` helper in `benchmark.cpp` times only the actual SdFat call
  passed to it — UI progress updates and CRC verification happen outside the timed
  interval (CRC is a separate pass after the timed read). Any change to this loop must
  preserve that separation or the reported MB/s numbers become meaningless. Buffers come
  from `hardware/`'s `allocateDmaBuffer()` (internal DMA-capable RAM). Known-good
  performance path, fixed since v0.6.1: `DEDICATED_SPI` + `USE_SPI_ARRAY_TRANSFER=1` +
  DMA-capable internal buffers + no `yield()` inside transfer loops.
- **`ui/`** — all TFT_eSPI rendering, split by screen:
  - `display_ui.*` — home screen, the BENCHMARK submenu (Quick Benchmark / SPI Scaling /
    Back), and their hit-testing (`uiHitTestHome()`, `uiHitTestBenchMenu()`).
  - `display_ui_internal.h/.cpp` — internal-only primitives shared across screens: the
    single `tft()` accessor (lazy singleton — every screen file calls `tft()` rather than
    holding its own `TFT_eSPI` instance), `header()`, `lineRow()`, and `uiWaitForTap()`.
  - `display_progress.cpp` — the progress-bar screen (`uiShowProgress()`).
  - `display_results.cpp` — benchmark result screens (horizontal bars, SPI-scale line
    graph).
  - `display_diagnostics.cpp` — the diagnostics screen (`uiShowDiagnostics()`).
  - `ui_geometry.*` — pure geometry math (hit-testing, progress/result bar fill width),
    shared by the screens above and host-testable.

### Data flow

`main.cpp` loop → `uiHitTestHome()` → `HomeAction`. `BenchmarkMenu` enters a local
`runBenchMenu()` loop in `main.cpp` that redraws `uiDrawBenchMenu()` and dispatches on
`BenchMenuAction` (`Quick` → `runQuickBenchmark()`, `Scale` → `runSpiScalingBenchmark()`,
`Back` → return to home) until the user backs out. Each benchmark call returns a result
struct (`BenchmarkResult`/`SpiScaleResult`) passed to the matching `uiShow*Result()`
renderer, then `uiWaitForTap()` before redrawing. Card state itself is process-global,
exposed via `getCardInfo()`/`getSd()` rather than threaded through every call.

### Why normal qualification excludes 40 MHz (v0.7.1)

`SPI_SPEEDS` (normal scan/fallback) intentionally omits 40 MHz even though the
DMA-safe qualification read makes 40 MHz technically checkable — 40 MHz proved
unreliable enough in practice that testing it on every card scan wasn't worth the cost.
`SPI_SCALE_SPEEDS` (the SPI Scaling diagnostic) still includes 40 MHz on purpose, as an
opt-in experimental data point. If you touch either ladder, keep this asymmetry
intentional rather than "fixing" it into consistency.

### Naming note

`CardInfo::filesystemDataBytes` (renamed from `filesystemBytes` in v0.7) is derived
from filesystem cluster geometry, *not* full partition size — don't treat it as
card/partition capacity. The old "filesystem exceeds card" fake-card heuristic that
used to compare these was removed for being unreliable; a real capacity-consistency
check needs MBR/GPT + boot-sector parsing, which doesn't exist yet.

### Safety scope

Tests are non-destructive apart from temporary creation/removal of `/SDTEST.BIN` on the
mounted filesystem; raw benchmark tests are read-only. No repartitioning, formatting, or
full-card raw write is implemented — don't add destructive operations without explicit
user request, since this firmware is meant to run against arbitrary user SD cards.

## Version history

The README's changelog documents *why* several non-obvious design decisions exist
(touch module boundary, DMA-capable buffer allocation, the two separate SPI speed
ladders, timing isolation, the v0.7 module-scope cleanup). Skim it before changing
SD/benchmark/touch code — it often already explains a constraint that looks arbitrary
in the source.

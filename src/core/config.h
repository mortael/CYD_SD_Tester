#pragma once
#include <cstddef>
#include <cstdint>

static constexpr const char* APP_VERSION = "v0.9.0";

// ------------------------------------------------------------
// CYD pins
// ------------------------------------------------------------

// SD - VSPI
static constexpr int SD_CS   = 5;
static constexpr int SD_SCK  = 18;
static constexpr int SD_MISO = 19;
static constexpr int SD_MOSI = 23;

// Touch - software SPI
static constexpr int TOUCH_CS   = 33;
static constexpr int TOUCH_SCK  = 25;
static constexpr int TOUCH_MOSI = 32;
static constexpr int TOUCH_MISO = 39;
static constexpr int TOUCH_IRQ  = 36;

// TFT
static constexpr int TFT_BACKLIGHT = 21;

// Display after tft.setRotation(1)
static constexpr int SCREEN_W = 320;
static constexpr int SCREEN_H = 240;

// ------------------------------------------------------------
// Touch calibration
// ------------------------------------------------------------

static constexpr int TS_X_MIN = 250;
static constexpr int TS_X_MAX = 3850;
static constexpr int TS_Y_MIN = 250;
static constexpr int TS_Y_MAX = 3850;

// ------------------------------------------------------------
// SD SPI qualification
// ------------------------------------------------------------

static constexpr uint32_t SPI_SPEEDS[] = {
  20UL * 1000UL * 1000UL,
  10UL * 1000UL * 1000UL,
   4UL * 1000UL * 1000UL,
   1UL * 1000UL * 1000UL
};

static constexpr size_t SPI_SPEED_COUNT =
    sizeof(SPI_SPEEDS) / sizeof(SPI_SPEEDS[0]);

// ------------------------------------------------------------
// Benchmark
// ------------------------------------------------------------

static constexpr uint32_t SECTOR_SIZE = 512;

static constexpr size_t BENCH_TEST_SIZE =
    16UL * 1024UL * 1024UL;

static constexpr size_t BENCH_BUFFER_SIZE =
    32UL * 1024UL;

static constexpr size_t BENCH_PROGRESS_STEP =
    1UL * 1024UL * 1024UL;

static constexpr uint32_t RAW_SECTORS_PER_CHUNK =
    BENCH_BUFFER_SIZE / SECTOR_SIZE;

static constexpr const char* TEST_FILE =
    "/SDTEST.BIN";

// ------------------------------------------------------------
// SPI scaling benchmark
// ------------------------------------------------------------

static constexpr size_t SPI_SCALE_TEST_SIZE =
    4UL * 1024UL * 1024UL;

static constexpr uint32_t SPI_SCALE_SPEEDS[] = {
    40UL * 1000UL * 1000UL,
    20UL * 1000UL * 1000UL,
    10UL * 1000UL * 1000UL,
     4UL * 1000UL * 1000UL,
     1UL * 1000UL * 1000UL
};

static constexpr size_t SPI_SCALE_SPEED_COUNT =
    sizeof(SPI_SCALE_SPEEDS) /
    sizeof(SPI_SCALE_SPEEDS[0]);


// ------------------------------------------------------------
// SD Necromancer
// ------------------------------------------------------------

static constexpr uint32_t NECRO_SPEEDS[] = {
    400UL * 1000UL,
    250UL * 1000UL,
    100UL * 1000UL,
      1UL * 1000UL * 1000UL
};

static constexpr size_t NECRO_SPEED_COUNT =
    sizeof(NECRO_SPEEDS) / sizeof(NECRO_SPEEDS[0]);

static constexpr uint32_t NECRO_ACMD41_TIMEOUT_MS = 8000;
static constexpr uint32_t NECRO_CMD1_TIMEOUT_MS = 5000;

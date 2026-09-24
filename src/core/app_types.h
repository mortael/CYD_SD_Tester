#pragma once
#include <cstdint>
#include "config.h"

enum class AttemptStage : uint8_t {
  NotTried,
  InitFailed,
  CidFailed,
  CsdFailed,
  ReadFailed,
  Passed
};

struct AttemptResult {
  uint32_t hz = 0;
  AttemptStage stage = AttemptStage::NotTried;
  uint8_t errorCode = 0;
  uint8_t errorData = 0;
};

struct CardInfo {
  bool cardOk = false;
  bool volumeOk = false;
  bool cidOk = false;

  uint64_t bytes = 0;
  uint64_t filesystemDataBytes = 0;

  uint8_t fsType = 0;
  uint8_t cardType = 0;
  uint32_t activeSpiHz = 0;

  char product[6] = "";
  char oem[3] = "";

  uint8_t manufacturerId = 0;
  const char* vendorName = "Unknown";

  uint8_t revisionMajor = 0;
  uint8_t revisionMinor = 0;
  uint32_t serial = 0;
  uint8_t month = 0;
  uint16_t year = 0;

  bool invalidManufacturerId = false;
  bool identityAnomaly = false;

  uint8_t volumeErrorCode = 0;
  uint8_t volumeErrorData = 0;
};

struct BenchmarkResult {
  bool preallocOk = false;
  bool fileWriteOk = false;
  bool fileReadOk = false;
  bool verifyOk = false;
  bool rawReadOk = false;
  bool cleanupOk = true;

  uint32_t preallocMs = 0;

  float fileWriteMBs = 0.0f;
  float fileReadMBs = 0.0f;
  float rawReadMBs = 0.0f;

  uint32_t expectedCrc = 0;
  uint32_t verifyCrc = 0;

  uint8_t ioErrorCode = 0;
  uint8_t ioErrorData = 0;
};

struct SpiScalePoint {
  uint32_t hz = 0;
  bool ok = false;
  float rawReadMBs = 0.0f;
  float efficiencyPercent = 0.0f;
  uint8_t errorCode = 0;
  uint8_t errorData = 0;
};

struct SpiScaleResult {
  SpiScalePoint points[SPI_SCALE_SPEED_COUNT];
  size_t count = 0;
};


struct NecromancerResult {
  bool ready = false;
  bool usedCmd1 = false;
  bool usedSpiMode3 = false;
  uint32_t speedHz = 0;

  uint8_t cmd0R1 = 0xFF;
  uint8_t cmd8R1 = 0xFF;
  uint8_t acmd41R1 = 0xFF;
  uint8_t cmd1R1 = 0xFF;
  uint8_t cmd58R1 = 0xFF;

  bool ocrOk = false;
  uint32_t ocr = 0;

  bool cidOk = false;
  bool csdOk = false;
  uint8_t cid[16] = {};
  uint8_t csd[16] = {};

  bool sector0ReadOk = false;
  bool destructiveWriteAttempted = false;
  bool destructiveWriteOk = false;
  bool destructiveVerifyOk = false;
  bool restoreOk = false;

  uint8_t lastR1 = 0xFF;
};

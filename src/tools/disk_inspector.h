#pragma once
#include <cstdint>
#include <cstddef>

static constexpr size_t DISK_MAX_PARTITIONS = 4;

struct PartitionInfo {
  bool present = false;
  uint8_t type = 0;
  uint32_t startLba = 0;
  uint32_t sectorCount = 0;
  char fsName[16] = "unknown";
};

struct DiskInspectorResult {
  bool cardReady = false;
  bool sector0Ok = false;
  bool mbrSignatureOk = false;
  bool protectiveGpt = false;
  uint64_t cardBytes = 0;
  PartitionInfo partitions[DISK_MAX_PARTITIONS];
  size_t partitionCount = 0;
  uint8_t sector0[512] = {};
};

DiskInspectorResult inspectDisk();

bool readRawSector(
    uint32_t lba,
    uint8_t out[512]
);

const char* partitionTypeName(uint8_t type);

void detectFilesystemSignature(
    const uint8_t sector[512],
    char* out,
    size_t outSize
);

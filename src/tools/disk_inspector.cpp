#include <Arduino.h>
#include <cstring>

#include "../core/config.h"
#include "../hardware/sd_card.h"
#include "disk_inspector.h"

static uint32_t le32(const uint8_t* p) {
  return
      (uint32_t)p[0] |
      ((uint32_t)p[1] << 8) |
      ((uint32_t)p[2] << 16) |
      ((uint32_t)p[3] << 24);
}

static bool equalsBytes(
    const uint8_t* p,
    const char* text,
    size_t len
) {
  return memcmp(p, text, len) == 0;
}

void detectFilesystemSignature(
    const uint8_t sector[512],
    char* out,
    size_t outSize
) {
  if (!out || outSize == 0) {
    return;
  }

  const char* name = "unknown";

  if (equalsBytes(&sector[3], "EXFAT   ", 8)) {
    name = "exFAT";
  }
  else if (equalsBytes(&sector[3], "NTFS    ", 8)) {
    name = "NTFS";
  }
  else if (equalsBytes(&sector[82], "FAT32   ", 8)) {
    name = "FAT32";
  }
  else if (equalsBytes(&sector[54], "FAT16   ", 8)) {
    name = "FAT16";
  }
  else if (equalsBytes(&sector[54], "FAT12   ", 8)) {
    name = "FAT12";
  }

  strncpy(out, name, outSize - 1);
  out[outSize - 1] = '\0';
}

const char* partitionTypeName(uint8_t type) {
  switch (type) {
    case 0x01: return "FAT12";
    case 0x04: return "FAT16 <32M";
    case 0x06: return "FAT16";
    case 0x07: return "NTFS/exFAT";
    case 0x0B: return "FAT32";
    case 0x0C: return "FAT32 LBA";
    case 0x0E: return "FAT16 LBA";
    case 0x0F: return "Extended";
    case 0x82: return "Linux swap";
    case 0x83: return "Linux";
    case 0xEE: return "GPT protective";
    default:   return "Unknown";
  }
}

bool readRawSector(
    uint32_t lba,
    uint8_t out[512]
) {
  if (!out || !getCardInfo().cardOk) {
    return false;
  }

  SdFs& sd = getSd();

  if (!sd.card()) {
    return false;
  }

  return sd.card()->readSector(lba, out);
}

DiskInspectorResult inspectDisk() {
  DiskInspectorResult result;
  const CardInfo& card = getCardInfo();

  result.cardReady = card.cardOk;
  result.cardBytes = card.bytes;

  if (!card.cardOk) {
    Serial.println("Disk Inspector: no qualified card.");
    return result;
  }

  result.sector0Ok =
      readRawSector(0, result.sector0);

  if (!result.sector0Ok) {
    Serial.println("Disk Inspector: sector 0 read failed.");
    return result;
  }

  result.mbrSignatureOk =
      result.sector0[510] == 0x55 &&
      result.sector0[511] == 0xAA;

  Serial.println();
  Serial.println("=== DISK INSPECTOR ===");
  Serial.printf(
      "MBR signature: %s\n",
      result.mbrSignatureOk ? "55 AA" : "missing"
  );

  if (!result.mbrSignatureOk) {
    char fs[16];
    detectFilesystemSignature(
        result.sector0,
        fs,
        sizeof(fs)
    );
    Serial.printf(
        "LBA0 filesystem signature: %s\n",
        fs
    );
    Serial.println("======================");
    return result;
  }

  for (size_t i = 0; i < DISK_MAX_PARTITIONS; ++i) {
    const uint8_t* e =
        &result.sector0[446 + i * 16];

    const uint8_t type = e[4];
    const uint32_t start = le32(&e[8]);
    const uint32_t count = le32(&e[12]);

    if (type == 0 || count == 0) {
      continue;
    }

    PartitionInfo& part =
        result.partitions[result.partitionCount];

    part.present = true;
    part.type = type;
    part.startLba = start;
    part.sectorCount = count;

    if (type == 0xEE) {
      result.protectiveGpt = true;
    }

    uint8_t bootSector[512];
    if (readRawSector(start, bootSector)) {
      detectFilesystemSignature(
          bootSector,
          part.fsName,
          sizeof(part.fsName)
      );
    }

    Serial.printf(
        "P%u type=%02X (%s) start=%lu sectors=%lu fs=%s\n",
        (unsigned)(result.partitionCount + 1),
        type,
        partitionTypeName(type),
        (unsigned long)start,
        (unsigned long)count,
        part.fsName
    );

    ++result.partitionCount;

    if (result.partitionCount >= DISK_MAX_PARTITIONS) {
      break;
    }
  }

  Serial.printf(
      "Scheme: %s\n",
      result.protectiveGpt
          ? "GPT protective MBR detected"
          : "MBR"
  );
  Serial.println("======================");

  return result;
}

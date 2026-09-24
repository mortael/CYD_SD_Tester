#include <Arduino.h>
#include <SPI.h>
#include <SdFat.h>
#include "esp_heap_caps.h"

#include "../core/config.h"
#include "../core/app_types.h"
#include "card_format.h"
#include "card_identity.h"
#include "sd_card.h"

static SPIClass sdSPI(VSPI);
static SdFs sd;
static CardInfo info;
static AttemptResult attempts[SPI_SPEED_COUNT];

static cid_t cid;
static csd_t csd;

SdFs& getSd() {
  return sd;
}

SPIClass& getSdSpi() {
  return sdSPI;
}


const CardInfo& getCardInfo() {
  return info;
}

const AttemptResult* getAttempts() {
  return attempts;
}

size_t getAttemptCount() {
  return SPI_SPEED_COUNT;
}

SdSpiConfig makeSdConfig(
    uint32_t hz
) {
  return SdSpiConfig(
      SD_CS,
      DEDICATED_SPI,
      hz,
      &sdSPI
  );
}

const char* cardTypeName(
    uint8_t type,
    uint64_t capacityBytes
) {
  switch (type) {
    case SD_CARD_TYPE_SD1:
      return "SD1 / SDSC";

    case SD_CARD_TYPE_SD2:
      return "SD2 / SDSC";

    case SD_CARD_TYPE_SDHC:
      if (capacityBytes >
          32ULL * 1000ULL * 1000ULL * 1000ULL) {
        return "SDXC";
      }
      return "SDHC";

    default:
      return "Unknown";
  }
}

const char* fsName(
    uint8_t type
) {
  switch (type) {
    case FAT_TYPE_FAT12:
      return "FAT12";

    case FAT_TYPE_FAT16:
      return "FAT16";

    case FAT_TYPE_FAT32:
      return "FAT32";

    case FAT_TYPE_EXFAT:
      return "exFAT";

    default:
      return "unknown / none";
  }
}

void recordSdError(
    SdFs& sd,
    uint8_t& errorCode,
    uint8_t& errorData,
    const char* label
) {
  errorCode =
      sd.sdErrorCode();
  errorData =
      sd.sdErrorData();

  Serial.printf(
      "%s FAIL %02X/%02X\n",
      label,
      errorCode,
      errorData
  );
}

uint8_t* allocateDmaBuffer(
    size_t size,
    const char* label
) {
  uint8_t* buffer =
      (uint8_t*)heap_caps_malloc(
          size,
          MALLOC_CAP_DMA |
          MALLOC_CAP_INTERNAL
      );

  if (!buffer) {
    Serial.printf(
        "%s DMA buffer allocation failed.\n",
        label
    );
    return nullptr;
  }

  Serial.printf(
      "%s buffer: %u bytes, DMA=%s, INTERNAL=%s, address=%p\n",
      label,
      (unsigned)size,
      esp_ptr_dma_capable(buffer)
          ? "YES"
          : "NO",
      esp_ptr_internal(buffer)
          ? "YES"
          : "NO",
      buffer
  );

  return buffer;
}

static void clearAttempts() {
  for (size_t i = 0;
       i < SPI_SPEED_COUNT;
       ++i) {
    attempts[i] = {};
    attempts[i].hz = SPI_SPEEDS[i];
  }
}

void initSdHardware() {
  sdSPI.begin(
      SD_SCK,
      SD_MISO,
      SD_MOSI,
      SD_CS
  );

  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
}

static void fillCardIdentity() {
  info.cidOk = true;

  info.manufacturerId = cid.mid;
  info.vendorName =
      resolveVendor(cid.mid);

  info.oem[0] = cid.oid[0];
  info.oem[1] = cid.oid[1];
  info.oem[2] = '\0';

  for (uint8_t i = 0; i < 5; ++i) {
    info.product[i] = cid.pnm[i];
  }
  info.product[5] = '\0';

  info.revisionMajor = cid.prvN();
  info.revisionMinor = cid.prvM();
  info.serial = cid.psn();
  info.month = cid.mdtMonth();
  info.year = cid.mdtYear();
}

static void runConsistencyChecks() {
  info.invalidManufacturerId =
      (info.manufacturerId == 0x00);

  // This is data-area size, not full partition size.
  // Proper partition/volume-size checks belong in the future
  // partition parser.
  if (info.volumeOk) {
    info.filesystemDataBytes =
        (uint64_t)sd.vol()->clusterCount() *
        sd.vol()->sectorsPerCluster() *
        SECTOR_SIZE;
  } else {
    info.filesystemDataBytes = 0;
  }

  info.identityAnomaly =
      info.invalidManufacturerId;
}

static void printCardReport() {
  char capacity[24];
  char spi[20];
  char fsData[24];

  formatHumanBytes(
      info.bytes,
      capacity,
      sizeof(capacity)
  );

  formatSpiClock(
      info.activeSpiHz,
      spi,
      sizeof(spi)
  );

  Serial.println();
  Serial.println("=== CARD INFO ===");
  Serial.printf("SPI: %s\n", spi);
  Serial.printf(
      "Type: %s\n",
      cardTypeName(info.cardType, info.bytes)
  );
  Serial.printf(
      "Capacity: %s\n",
      capacity
  );

  if (info.cidOk) {
    Serial.printf(
        "MID: 0x%02X\n",
        info.manufacturerId
    );
    Serial.printf(
        "Vendor hint: %s\n",
        info.vendorName
    );
    Serial.printf(
        "OEM: %s\n",
        info.oem
    );
    Serial.printf(
        "Product: %s\n",
        info.product
    );
    Serial.printf(
        "Revision: %u.%u\n",
        info.revisionMajor,
        info.revisionMinor
    );
    Serial.printf(
        "Serial: 0x%08lX\n",
        (unsigned long)info.serial
    );
    Serial.printf(
        "Manufacturing date: %u/%u\n",
        info.month,
        info.year
    );
  }

  Serial.printf(
      "Filesystem: %s\n",
      info.volumeOk
          ? fsName(info.fsType)
          : "not mounted"
  );

  if (!info.volumeOk &&
      (info.volumeErrorCode ||
       info.volumeErrorData)) {
    Serial.printf(
        " - mount error: %02X/%02X\n",
        info.volumeErrorCode,
        info.volumeErrorData
    );
  }

  if (info.volumeOk) {
    formatHumanBytes(
        info.filesystemDataBytes,
        fsData,
        sizeof(fsData)
    );

    Serial.printf(
        "Filesystem data area: %s\n",
        fsData
    );
  }

  Serial.printf(
      "Identity consistency: %s\n",
      identityCheckName(info)
  );

  if (info.invalidManufacturerId) {
    Serial.println(
        " - anomaly: manufacturer ID is 0x00"
    );
  }

  Serial.println("=================");
}

bool scanCard() {
  sd.end();
  info = {};
  clearAttempts();

  Serial.println();
  Serial.println(
      "=== AUTO SPI QUALIFICATION ==="
  );

  uint8_t* sector0 =
      allocateDmaBuffer(
          SECTOR_SIZE,
          "Qualification"
      );

  if (!sector0) {
    return false;
  }

  for (size_t i = 0;
       i < SPI_SPEED_COUNT;
       ++i) {
    const uint32_t hz =
        SPI_SPEEDS[i];

    AttemptResult& a =
        attempts[i];

    char spi[20];
    formatSpiClock(
        hz,
        spi,
        sizeof(spi)
    );

    Serial.printf(
        "Trying %s ... ",
        spi
    );

    sd.end();
    delay(10);

    if (!sd.cardBegin(
            makeSdConfig(hz))) {
      a.stage =
          AttemptStage::InitFailed;
      recordSdError(
          sd,
          a.errorCode,
          a.errorData,
          "INIT"
      );
      continue;
    }

    if (!sd.card()->readCID(&cid)) {
      a.stage =
          AttemptStage::CidFailed;
      recordSdError(
          sd,
          a.errorCode,
          a.errorData,
          "CID"
      );
      continue;
    }

    if (!sd.card()->readCSD(&csd)) {
      a.stage =
          AttemptStage::CsdFailed;
      recordSdError(
          sd,
          a.errorCode,
          a.errorData,
          "CSD"
      );
      continue;
    }

    if (!sd.card()->readSector(
            0,
            sector0)) {
      a.stage =
          AttemptStage::ReadFailed;
      recordSdError(
          sd,
          a.errorCode,
          a.errorData,
          "READ"
      );
      continue;
    }

    a.stage =
        AttemptStage::Passed;

    info.cardOk = true;
    info.activeSpiHz = hz;
    info.cardType =
        sd.card()->type();

    info.bytes =
        (uint64_t)sd.card()
            ->sectorCount() *
        SECTOR_SIZE;

    fillCardIdentity();

    Serial.println("PASS");
    break;
  }

  free(sector0);

  if (!info.cardOk) {
    Serial.println(
        "No qualified SPI speed found."
    );
    Serial.println(
        "=============================="
    );
    return false;
  }

  if (sd.volumeBegin()) {
    info.volumeOk = true;
    info.fsType = sd.fatType();
  } else {
    info.volumeOk = false;
    info.fsType = 0;
    recordSdError(
        sd,
        info.volumeErrorCode,
        info.volumeErrorData,
        "VOLUME"
    );
  }

  runConsistencyChecks();
  printCardReport();

  return true;
}

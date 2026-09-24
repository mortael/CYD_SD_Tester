#include "sd_necromancer.h"
#include "sd_bitbang_probe.h"

#include <Arduino.h>
#include <SPI.h>
#include <cstring>

#include "../core/config.h"
#include "../hardware/sd_card.h"

namespace {

static constexpr uint8_t NECRO_CMD0  = 0;
static constexpr uint8_t NECRO_CMD1  = 1;
static constexpr uint8_t NECRO_CMD8  = 8;
static constexpr uint8_t NECRO_CMD9  = 9;
static constexpr uint8_t NECRO_CMD10 = 10;
static constexpr uint8_t NECRO_CMD13 = 13;
static constexpr uint8_t NECRO_CMD16 = 16;
static constexpr uint8_t NECRO_CMD17 = 17;
static constexpr uint8_t NECRO_CMD24 = 24;
static constexpr uint8_t NECRO_CMD30 = 30;
static constexpr uint8_t NECRO_CMD55 = 55;
static constexpr uint8_t NECRO_CMD58 = 58;
static constexpr uint8_t NECRO_CMD59 = 59;
static constexpr uint8_t NECRO_ACMD41 = 41;

static constexpr size_t TRACE_BYTES = 32;

struct RawBus {
  SPIClass& spi;
  uint32_t hz;
  uint8_t mode;

  SPISettings settings() const {
    return SPISettings(hz, MSBFIRST, mode);
  }
};

// SD command CRC7. Returned byte already includes the mandatory end bit.
static uint8_t crc7Byte(
    uint8_t crc,
    uint8_t data) {
  for (uint8_t i = 0; i < 8; ++i) {
    crc <<= 1;

    if ((data ^ crc) & 0x80U) {
      crc ^= 0x09U;
    }

    data <<= 1;
  }

  return crc & 0x7FU;
}

static uint8_t commandCrc(
    uint8_t cmd,
    uint32_t arg) {
  uint8_t crc = 0;

  crc = crc7Byte(
      crc,
      0x40U | cmd
  );
  crc = crc7Byte(
      crc,
      (uint8_t)(arg >> 24)
  );
  crc = crc7Byte(
      crc,
      (uint8_t)(arg >> 16)
  );
  crc = crc7Byte(
      crc,
      (uint8_t)(arg >> 8)
  );
  crc = crc7Byte(
      crc,
      (uint8_t)arg
  );

  return (uint8_t)((crc << 1) | 1U);
}

struct ResponseTrace {
  uint8_t bytes[TRACE_BYTES] = {};
  uint8_t r1 = 0xFF;
  int8_t r1Index = -1;
  bool all00 = true;
  bool allFF = true;
};

static void printHexBytes(
    const uint8_t* data,
    size_t length) {
  for (size_t i = 0; i < length; ++i) {
    if (i > 0) {
      Serial.print(' ');
    }

    if (data[i] < 0x10) {
      Serial.print('0');
    }

    Serial.print(data[i], HEX);
  }
}

static const char* traceClass(
    const ResponseTrace& trace) {
  if (trace.all00) {
    return "ALL-00 / probable MISO stuck low";
  }

  if (trace.allFF) {
    return "ALL-FF / no response or MISO high";
  }

  if (trace.r1Index >= 0) {
    return "R1 candidate present";
  }

  return "mixed / no valid R1 candidate";
}

static void printTrace(
    const char* label,
    const ResponseTrace& trace) {
  Serial.printf("  %s raw: ", label);
  printHexBytes(trace.bytes, TRACE_BYTES);
  Serial.println();

  Serial.printf(
      "  %s class: %s",
      label,
      traceClass(trace)
  );

  if (trace.r1Index >= 0) {
    Serial.printf(
        " (R1=%02X at byte %d)",
        trace.r1,
        trace.r1Index
    );
  }

  Serial.println();
}

static void deselect(RawBus& bus) {
  digitalWrite(SD_CS, HIGH);
  bus.spi.transfer(0xFF);
}

static void idleClocks(
    RawBus& bus,
    size_t bytes = 20) {
  bus.spi.beginTransaction(bus.settings());
  digitalWrite(SD_CS, HIGH);

  for (size_t i = 0; i < bytes; ++i) {
    bus.spi.transfer(0xFF);
  }

  bus.spi.endTransaction();
}

static void sampleBus(
    RawBus& bus,
    const char* label,
    bool selected) {
  uint8_t samples[16];

  bus.spi.beginTransaction(bus.settings());
  digitalWrite(SD_CS, selected ? LOW : HIGH);

  delayMicroseconds(10);

  const int pinLevel = digitalRead(SD_MISO);

  for (size_t i = 0; i < sizeof(samples); ++i) {
    samples[i] = bus.spi.transfer(0xFF);
  }

  digitalWrite(SD_CS, HIGH);
  bus.spi.endTransaction();

  bool all00 = true;
  bool allFF = true;

  for (uint8_t value : samples) {
    all00 &= value == 0x00;
    allFF &= value == 0xFF;
  }

  Serial.printf(
      "  BUS %-12s CS=%s MISO-pin=%s bytes=",
      label,
      selected ? "LOW" : "HIGH",
      pinLevel ? "HIGH" : "LOW"
  );
  printHexBytes(samples, sizeof(samples));

  if (all00) {
    Serial.print("  [ALL-00]");
  } else if (allFF) {
    Serial.print("  [ALL-FF]");
  } else {
    Serial.print("  [MIXED]");
  }

  Serial.println();
}

static ResponseTrace sendCommandTraceSelected(
    RawBus& bus,
    uint8_t cmd,
    uint32_t arg,
    uint8_t crc) {
  ResponseTrace trace;

  bus.spi.transfer(0xFF);
  bus.spi.transfer(0x40U | cmd);
  bus.spi.transfer((uint8_t)(arg >> 24));
  bus.spi.transfer((uint8_t)(arg >> 16));
  bus.spi.transfer((uint8_t)(arg >> 8));
  bus.spi.transfer((uint8_t)arg);
  bus.spi.transfer(crc);

  for (size_t i = 0; i < TRACE_BYTES; ++i) {
    const uint8_t value =
        bus.spi.transfer(0xFF);

    trace.bytes[i] = value;
    trace.all00 &= value == 0x00;
    trace.allFF &= value == 0xFF;

    // A real R1 byte has bit 7 clear. Do not accept an endless stream
    // of zeroes as proof of a valid R1; classification below handles it.
    if (trace.r1Index < 0 &&
        (value & 0x80U) == 0) {
      trace.r1 = value;
      trace.r1Index = (int8_t)i;
    }
  }

  if (trace.all00) {
    trace.r1 = 0xFF;
    trace.r1Index = -1;
  }

  return trace;
}

static ResponseTrace commandTrace(
    RawBus& bus,
    uint8_t cmd,
    uint32_t arg,
    uint8_t crc = 0) {
  if (crc == 0) {
    crc = commandCrc(cmd, arg);
  }
  bus.spi.beginTransaction(bus.settings());
  digitalWrite(SD_CS, LOW);

  const ResponseTrace trace =
      sendCommandTraceSelected(
          bus,
          cmd,
          arg,
          crc
      );

  deselect(bus);
  bus.spi.endTransaction();
  return trace;
}

static uint8_t commandSelected(
    RawBus& bus,
    uint8_t cmd,
    uint32_t arg,
    uint8_t crc = 0) {
  if (crc == 0) {
    crc = commandCrc(cmd, arg);
  }

  // IMPORTANT:
  // This routine is used when payload/data follows R1 (R3/R7, CID/CSD,
  // data tokens, etc.). It must stop immediately after the R1 byte.
  // The forensic trace routine intentionally reads 32 bytes and must not
  // be used here, otherwise it consumes the payload before the caller.
  bus.spi.transfer(0xFF);
  bus.spi.transfer(0x40U | cmd);
  bus.spi.transfer((uint8_t)(arg >> 24));
  bus.spi.transfer((uint8_t)(arg >> 16));
  bus.spi.transfer((uint8_t)(arg >> 8));
  bus.spi.transfer((uint8_t)arg);
  bus.spi.transfer(crc);

  for (uint8_t i = 0; i < 16; ++i) {
    const uint8_t r1 =
        bus.spi.transfer(0xFF);

    if ((r1 & 0x80U) == 0) {
      return r1;
    }
  }

  return 0xFF;
}

static uint8_t command(
    RawBus& bus,
    uint8_t cmd,
    uint32_t arg,
    uint8_t crc = 0,
    uint8_t* extra = nullptr,
    size_t extraLen = 0) {
  if (crc == 0) {
    crc = commandCrc(cmd, arg);
  }
  bus.spi.beginTransaction(bus.settings());
  digitalWrite(SD_CS, LOW);

  const uint8_t r1 =
      commandSelected(
          bus,
          cmd,
          arg,
          crc
      );

  for (size_t i = 0; i < extraLen; ++i) {
    extra[i] = bus.spi.transfer(0xFF);
  }

  deselect(bus);
  bus.spi.endTransaction();
  return r1;
}

static void forcedFailureProbe(
    RawBus& bus) {
  Serial.println(
      "  CMD0 never produced idle. Running forced downstream probes..."
  );

  ResponseTrace trace =
      commandTrace(bus, NECRO_CMD8, 0x000001AAUL, 0
      );
  printTrace("CMD8 forced", trace);

  trace = commandTrace(bus, NECRO_CMD58, 0, 0
  );
  printTrace("CMD58 forced", trace);

  trace = commandTrace(bus, NECRO_CMD55, 0, 0
  );
  printTrace("CMD55 forced", trace);

  trace = commandTrace(bus, NECRO_ACMD41, 0x40000000UL, 0
  );
  printTrace("ACMD41 forced", trace);

  trace = commandTrace(bus, NECRO_CMD1, 0x40000000UL, 0
  );
  printTrace("CMD1 forced", trace);
}

static bool waitDataToken(
    RawBus& bus,
    uint8_t token,
    uint32_t timeoutMs) {
  const uint32_t start = millis();

  while (millis() - start < timeoutMs) {
    const uint8_t b =
        bus.spi.transfer(0xFF);

    if (b == token) {
      return true;
    }

    if (b != 0xFF) {
      return false;
    }
  }

  return false;
}

static bool readRegister16(
    RawBus& bus,
    uint8_t cmd,
    uint8_t out[16],
    uint8_t& r1Out) {
  bus.spi.beginTransaction(bus.settings());
  digitalWrite(SD_CS, LOW);

  r1Out = commandSelected(bus, cmd, 0, 0);

  if (r1Out != 0x00 ||
      !waitDataToken(bus, 0xFE, 500)) {
    deselect(bus);
    bus.spi.endTransaction();
    return false;
  }

  for (size_t i = 0; i < 16; ++i) {
    out[i] = bus.spi.transfer(0xFF);
  }

  bus.spi.transfer(0xFF);
  bus.spi.transfer(0xFF);

  deselect(bus);
  bus.spi.endTransaction();
  return true;
}

static bool readSector0(
    RawBus& bus,
    uint8_t out[SECTOR_SIZE],
    uint8_t& r1Out) {
  bus.spi.beginTransaction(bus.settings());
  digitalWrite(SD_CS, LOW);

  r1Out =
      commandSelected(
          bus,
          NECRO_CMD17,
          0,
          0
      );

  if (r1Out != 0x00 ||
      !waitDataToken(bus, 0xFE, 1000)) {
    deselect(bus);
    bus.spi.endTransaction();
    return false;
  }

  for (size_t i = 0; i < SECTOR_SIZE; ++i) {
    out[i] = bus.spi.transfer(0xFF);
  }

  bus.spi.transfer(0xFF);
  bus.spi.transfer(0xFF);

  deselect(bus);
  bus.spi.endTransaction();
  return true;
}

static bool writeSector0(
    RawBus& bus,
    const uint8_t data[SECTOR_SIZE],
    uint8_t& r1Out) {
  bus.spi.beginTransaction(bus.settings());
  digitalWrite(SD_CS, LOW);

  r1Out =
      commandSelected(
          bus,
          NECRO_CMD24,
          0,
          0
      );

  if (r1Out != 0x00) {
    deselect(bus);
    bus.spi.endTransaction();
    return false;
  }

  bus.spi.transfer(0xFF);
  bus.spi.transfer(0xFE);

  for (size_t i = 0; i < SECTOR_SIZE; ++i) {
    bus.spi.transfer(data[i]);
  }

  bus.spi.transfer(0xFF);
  bus.spi.transfer(0xFF);

  const uint8_t dataResponse =
      bus.spi.transfer(0xFF);

  if ((dataResponse & 0x1FU) != 0x05U) {
    deselect(bus);
    bus.spi.endTransaction();
    return false;
  }

  const uint32_t start = millis();

  while (bus.spi.transfer(0xFF) == 0x00) {
    if (millis() - start > 3000) {
      deselect(bus);
      bus.spi.endTransaction();
      return false;
    }
  }

  deselect(bus);
  bus.spi.endTransaction();
  return true;
}

static uint32_t bytesToU32(
    const uint8_t b[4]) {
  return ((uint32_t)b[0] << 24) |
         ((uint32_t)b[1] << 16) |
         ((uint32_t)b[2] << 8) |
         (uint32_t)b[3];
}

static void makeDestructivePattern(
    uint8_t pattern[SECTOR_SIZE]) {
  for (size_t i = 0;
       i < SECTOR_SIZE;
       ++i) {
    pattern[i] =
        (uint8_t)(
            (i * 73U + 0x5AU) &
            0xFFU
        );
  }

  static constexpr char marker[] =
      "CYD SD NECROMANCER DESTRUCTIVE WRITE PROBE";

  memcpy(
      pattern,
      marker,
      sizeof(marker) - 1
  );

  pattern[510] = 0xDE;
  pattern[511] = 0xAD;
}



static bool pairedAcmd41(
    RawBus& bus,
    uint32_t arg,
    uint8_t& r55Out,
    uint8_t& r41Out) {
  bus.spi.beginTransaction(bus.settings());
  digitalWrite(SD_CS, LOW);

  // CMD55
  r55Out =
      commandSelected(
          bus,
          NECRO_CMD55,
          0,
          0
      );

  // Keep CS LOW and add one stuff byte between commands.
  bus.spi.transfer(0xFF);

  // ACMD41
  r41Out =
      commandSelected(
          bus,
          NECRO_ACMD41,
          arg,
          0
      );

  deselect(bus);
  bus.spi.endTransaction();

  return r41Out == 0x00;
}

static bool minimalInitAttempt(
    RawBus& bus,
    NecromancerResult& result,
    uint32_t preDelayMs,
    bool keepCsLowPair) {
  Serial.printf(
      "  === CLEAN INIT: wait=%lu ms, pair-CS=%s ===\n",
      (unsigned long)preDelayMs,
      keepCsLowPair ? "LOW" : "normal"
  );

  digitalWrite(SD_CS, HIGH);
  delay(preDelayMs);

  // More than the required startup clocks.
  idleClocks(bus, 32);

  const uint8_t r0 =
      command(
          bus,
          NECRO_CMD0,
          0,
          0
      );

  Serial.printf(
      "  CLEAN CMD0 -> %02X\n",
      r0
  );

  if (r0 != 0x01) {
    return false;
  }

  uint8_t r7[4] = {};
  const uint8_t r8 =
      command(
          bus,
          NECRO_CMD8,
          0x000001AAUL,
          0,
          r7,
          sizeof(r7)
      );

  Serial.printf(
      "  CLEAN CMD8 -> %02X R7=%02X%02X%02X%02X\n",
      r8,
      r7[0],
      r7[1],
      r7[2],
      r7[3]
  );

  // This damaged card is clearly SD v2 capable if it echoes 1AA.
  const uint32_t acmdArg =
      (r8 == 0x01 &&
       r7[2] == 0x01 &&
       r7[3] == 0xAA)
          ? 0x40000000UL
          : 0x00000000UL;

  const uint32_t started =
      millis();

  uint32_t loops = 0;
  uint8_t r55 = 0xFF;
  uint8_t r41 = 0xFF;

  while (millis() - started < 10000UL) {
    if (keepCsLowPair) {
      pairedAcmd41(
          bus,
          acmdArg,
          r55,
          r41
      );
    } else {
      r55 =
          command(
              bus,
              NECRO_CMD55,
              0,
              0
          );

      r41 =
          command(
              bus,
              NECRO_ACMD41,
              acmdArg,
              0
          );
    }

    if ((loops % 100U) == 0U) {
      Serial.printf(
          "  CLEAN loop %lu: CMD55=%02X ACMD41=%02X\n",
          (unsigned long)loops,
          r55,
          r41
      );
    }

    if (r41 == 0x00) {
      Serial.printf(
          "  CLEAN INIT READY after %lu loops\n",
          (unsigned long)(loops + 1)
      );

      result.cmd0R1 = r0;
      result.cmd8R1 = r8;
      result.acmd41R1 = 0x00;
      result.ready = true;

      uint8_t ocr[4] = {};
      result.cmd58R1 =
          command(
              bus,
              NECRO_CMD58,
              0,
              0,
              ocr,
              sizeof(ocr)
          );

      result.ocr =
          ((uint32_t)ocr[0] << 24) |
          ((uint32_t)ocr[1] << 16) |
          ((uint32_t)ocr[2] << 8) |
          (uint32_t)ocr[3];

      result.ocrOk =
          result.cmd58R1 == 0x00;

      Serial.printf(
          "  CLEAN CMD58 -> %02X OCR=%08lX\n",
          result.cmd58R1,
          (unsigned long)result.ocr
      );

      return true;
    }

    ++loops;

    // Mix short and occasional longer settling gaps.
    if ((loops % 64U) == 0U) {
      delay(50);
      idleClocks(bus, 4);
    } else {
      delay(10);
    }
  }

  Serial.printf(
      "  CLEAN INIT failed: CMD55=%02X ACMD41=%02X after %lu loops\n",
      r55,
      r41,
      (unsigned long)loops
  );

  return false;
}

static bool runCleanInitMatrix(
    RawBus& bus,
    NecromancerResult& result) {
  const uint32_t waits[] = {
      0UL,
      250UL,
      1000UL,
      3000UL
  };

  for (uint32_t waitMs : waits) {
    if (minimalInitAttempt(
            bus,
            result,
            waitMs,
            false)) {
      return true;
    }

    // Fully return to a benign bus state before the next variant.
    digitalWrite(SD_CS, HIGH);
    idleClocks(bus, 16);
    delay(100);
  }

  // Second matrix: keep CS asserted across CMD55 -> ACMD41.
  for (uint32_t waitMs : waits) {
    if (minimalInitAttempt(
            bus,
            result,
            waitMs,
            true)) {
      return true;
    }

    digitalWrite(SD_CS, HIGH);
    idleClocks(bus, 16);
    delay(100);
  }

  return false;
}

static void runIdleForensics(
    RawBus& bus) {
  Serial.println(
      "  === IDLE-STATE FORENSICS ==="
  );

  const ResponseTrace cmd8_1aa =
      commandTrace(bus, NECRO_CMD8, 0x000001AAUL, 0
      );
  printTrace("CMD8 1AA", cmd8_1aa);

  const ResponseTrace cmd8_1ab =
      commandTrace(bus, NECRO_CMD8, 0x000001ABUL, 0
      );
  printTrace("CMD8 1AB", cmd8_1ab);

  const ResponseTrace cmd8_2aa =
      commandTrace(bus, NECRO_CMD8, 0x000002AAUL, 0
      );
  printTrace("CMD8 2AA", cmd8_2aa);

  const ResponseTrace cmd58 =
      commandTrace(bus, NECRO_CMD58, 0, 0
      );
  printTrace("CMD58 OCR", cmd58);

  const ResponseTrace cmd13 =
      commandTrace(bus, NECRO_CMD13, 0, 0
      );
  printTrace("CMD13 status", cmd13);

  const ResponseTrace crcOn =
      commandTrace(bus, NECRO_CMD59, 1, 0
      );
  printTrace("CMD59 CRC on", crcOn);

  const ResponseTrace crcOff =
      commandTrace(bus, NECRO_CMD59, 0, 0
      );
  printTrace("CMD59 CRC off", crcOff);

  const ResponseTrace cmd16 =
      commandTrace(bus, NECRO_CMD16, 512, 0
      );
  printTrace("CMD16 512", cmd16);
}

static bool tryAcmd41Variant(
    RawBus& bus,
    uint32_t arg,
    const char* label,
    uint32_t timeoutMs,
    uint8_t& finalR1) {
  const uint32_t started =
      millis();

  uint32_t loops = 0;

  while (millis() - started <
         timeoutMs) {
    const uint8_t r55 =
        command(bus, NECRO_CMD55, 0, 0
        );

    const uint8_t r41 =
        command(bus, NECRO_ACMD41, arg, 0
        );

    finalR1 = r41;

    if ((loops % 100U) == 0U) {
      Serial.printf(
          "  %s loop %lu: CMD55=%02X ACMD41=%02X\n",
          label,
          (unsigned long)loops,
          r55,
          r41
      );
    }

    if (r41 == 0x00) {
      Serial.printf(
          "  %s -> READY after %lu loops\n",
          label,
          (unsigned long)(loops + 1)
      );
      return true;
    }

    ++loops;
    delay(10);
  }

  Serial.printf(
      "  %s final -> R1=%02X after %lu loops\n",
      label,
      finalR1,
      (unsigned long)loops
  );

  return false;
}

static bool initOne(
    RawBus& bus,
    NecromancerResult& result) {
  idleClocks(bus, 32);
  delay(2);

  sampleBus(bus, "deselected", false);
  sampleBus(bus, "selected", true);

  uint8_t r1 = 0xFF;
  bool sawCmd0Idle = false;

  for (uint8_t attempt = 0;
       attempt < 8;
       ++attempt) {
    const ResponseTrace trace =
        commandTrace(bus, NECRO_CMD0, 0, 0
        );

    char label[24];
    snprintf(
        label,
        sizeof(label),
        "CMD0 #%u",
        attempt + 1
    );

    printTrace(label, trace);

    r1 = trace.r1Index >= 0
        ? trace.r1
        : 0xFF;

    result.cmd0R1 = r1;

    if (r1 == 0x01) {
      sawCmd0Idle = true;
      break;
    }

    idleClocks(bus, 8);
    delay(5 + attempt * 5);
  }

  if (!sawCmd0Idle) {
    forcedFailureProbe(bus);
    return false;
  }

  runIdleForensics(bus);

  uint8_t r7[4] = {};
  result.cmd8R1 =
      command(bus, NECRO_CMD8, 0x000001AAUL, 0,
          r7,
          sizeof(r7)
      );

  Serial.printf(
      "  CMD8 -> R1=%02X R7=%02X%02X%02X%02X\n",
      result.cmd8R1,
      r7[0], r7[1], r7[2], r7[3]
  );

  const bool v2 =
      result.cmd8R1 == 0x01 &&
      r7[2] == 0x01 &&
      r7[3] == 0xAA;

  uint8_t ocrBytes[4] = {};
  result.cmd58R1 =
      command(bus, NECRO_CMD58, 0, 0,
          ocrBytes,
          4
      );

  Serial.printf(
      "  CMD58(idle) -> R1=%02X OCR=%08lX\n",
      result.cmd58R1,
      (unsigned long)bytesToU32(ocrBytes)
  );

  struct AcmdVariant {
    uint32_t arg;
    const char* label;
  };

  const AcmdVariant variants[] = {
      {0x00000000UL, "ACMD41 arg=0"},
      {0x40000000UL, "ACMD41 HCS"},
      {0x50000000UL, "ACMD41 HCS+XPC"},
      {0x41000000UL, "ACMD41 HCS+S18R"}
  };

  for (const AcmdVariant& variant : variants) {
    uint8_t finalR1 = 0xFF;

    if (tryAcmd41Variant(
            bus,
            variant.arg,
            variant.label,
            2500,
            finalR1)) {
      result.acmd41R1 = 0x00;
      result.ready = true;
      break;
    }

    result.acmd41R1 =
        finalR1;

    // Reassert idle clocks before trying the next argument variant.
    idleClocks(bus, 16);
  }

  Serial.printf(
      "  ACMD41 final -> R1=%02X\n",
      result.acmd41R1
  );

  if (!result.ready) {
    Serial.println(
        "  ACMD41 did not leave idle; trying CMD1 fallback..."
    );

    const uint32_t cmd1Start = millis();

    while (millis() - cmd1Start <
           NECRO_CMD1_TIMEOUT_MS) {
      result.cmd1R1 =
          command(bus, NECRO_CMD1, 0, 0
          );

      if (result.cmd1R1 == 0x00) {
        result.ready = true;
        result.usedCmd1 = true;
        break;
      }

      delay(10);
    }

    Serial.printf(
        "  CMD1 final -> R1=%02X\n",
        result.cmd1R1
    );
  }

  if (!result.ready) {
    return false;
  }

  const uint8_t crcOff =
      command(bus, NECRO_CMD59, 0, 0
      );

  Serial.printf(
      "  CMD59 CRC off -> R1=%02X\n",
      crcOff
  );

  memset(
      ocrBytes,
      0,
      sizeof(ocrBytes)
  );

  result.cmd58R1 =
      command(bus, NECRO_CMD58, 0, 0,
          ocrBytes,
          4
      );

  result.ocr =
      bytesToU32(ocrBytes);

  result.ocrOk =
      result.cmd58R1 == 0x00;

  Serial.printf(
      "  CMD58(ready) -> R1=%02X OCR=%08lX\n",
      result.cmd58R1,
      (unsigned long)result.ocr
  );

  uint8_t regR1 = 0xFF;

  result.cidOk =
      readRegister16(
          bus,
          NECRO_CMD10,
          result.cid,
          regR1
      );

  Serial.printf(
      "  CID -> %s R1=%02X\n",
      result.cidOk ? "PASS" : "FAIL",
      regR1
  );

  result.csdOk =
      readRegister16(
          bus,
          NECRO_CMD9,
          result.csd,
          regR1
      );

  Serial.printf(
      "  CSD -> %s R1=%02X\n",
      result.csdOk ? "PASS" : "FAIL",
      regR1
  );

  return true;
}


static void dumpSectorSummary(
    const char* label,
    const uint8_t sector[SECTOR_SIZE]) {
  Serial.printf("  %s first 64 bytes:\n  ", label);

  for (size_t i = 0; i < 64; ++i) {
    if (i && (i % 16) == 0) {
      Serial.print("\n  ");
    }

    if (sector[i] < 0x10) {
      Serial.print('0');
    }

    Serial.print(sector[i], HEX);
    Serial.print(' ');
  }

  Serial.println();

  Serial.printf(
      "  %s signature[510:511]=%02X %02X%s\n",
      label,
      sector[510],
      sector[511],
      (sector[510] == 0x55 && sector[511] == 0xAA)
          ? " (55 AA)"
          : ""
  );
}

static bool readSectorAt(
    RawBus& bus,
    uint32_t address,
    uint8_t out[SECTOR_SIZE],
    uint8_t& r1Out) {
  bus.spi.beginTransaction(bus.settings());
  digitalWrite(SD_CS, LOW);

  r1Out =
      commandSelected(
          bus,
          NECRO_CMD17,
          address,
          0
      );

  if (r1Out != 0x00 ||
      !waitDataToken(
          bus,
          0xFE,
          1000)) {
    deselect(bus);
    bus.spi.endTransaction();
    return false;
  }

  for (size_t i = 0; i < SECTOR_SIZE; ++i) {
    out[i] = bus.spi.transfer(0xFF);
  }

  const uint8_t crcHi = bus.spi.transfer(0xFF);
  const uint8_t crcLo = bus.spi.transfer(0xFF);

  deselect(bus);
  bus.spi.endTransaction();

  Serial.printf(
      "  CMD17 addr=%lu -> R1=%02X dataCRC=%02X%02X\n",
      (unsigned long)address,
      r1Out,
      crcHi,
      crcLo
  );

  return true;
}


static uint16_t crc16Ccitt(
    const uint8_t* data,
    size_t len) {
  uint16_t crc = 0;

  for (size_t i = 0; i < len; ++i) {
    crc ^= (uint16_t)data[i] << 8;

    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc & 0x8000U)
          ? (uint16_t)((crc << 1) ^ 0x1021U)
          : (uint16_t)(crc << 1);
    }
  }

  return crc;
}

static uint32_t extractBits(
    const uint8_t data[16],
    uint8_t msb,
    uint8_t lsb) {
  uint32_t value = 0;

  for (int bit = msb; bit >= lsb; --bit) {
    const int byteIndex =
        15 - (bit / 8);
    const int bitIndex =
        bit % 8;

    value <<= 1;
    value |=
        (data[byteIndex] >> bitIndex) & 1U;
  }

  return value;
}

static void decodeCid(
    const uint8_t cid[16]) {
  const uint8_t mid = cid[0];

  char oid[3] = {
      (char)cid[1],
      (char)cid[2],
      '\0'
  };

  char pnm[6];
  memcpy(pnm, &cid[3], 5);
  pnm[5] = '\0';

  const uint8_t prv = cid[8];
  const uint32_t psn =
      ((uint32_t)cid[9] << 24) |
      ((uint32_t)cid[10] << 16) |
      ((uint32_t)cid[11] << 8) |
      cid[12];

  const uint16_t mdt =
      ((uint16_t)(cid[13] & 0x0F) << 8) |
      cid[14];

  const uint16_t year =
      2000U + ((mdt >> 4) & 0xFFU);
  const uint8_t month =
      mdt & 0x0FU;

  Serial.println("  --- CID DECODE ---");
  Serial.printf(
      "  MID=%02X OID=%s PNM=%s PRV=%u.%u PSN=%08lX MDT=%04u-%02u\n",
      mid,
      oid,
      pnm,
      prv >> 4,
      prv & 0x0F,
      (unsigned long)psn,
      year,
      month
  );
}

static void decodeCsd(
    const uint8_t csd[16],
    uint32_t& blockCountOut) {
  const uint32_t csdStructure =
      extractBits(csd, 127, 126);

  const uint32_t permWp =
      extractBits(csd, 13, 13);
  const uint32_t tmpWp =
      extractBits(csd, 12, 12);
  const uint32_t copy =
      extractBits(csd, 14, 14);

  blockCountOut = 0;

  Serial.println("  --- CSD DECODE ---");
  Serial.printf(
      "  CSD_STRUCTURE=%lu COPY=%lu PERM_WP=%lu TMP_WP=%lu\n",
      (unsigned long)csdStructure,
      (unsigned long)copy,
      (unsigned long)permWp,
      (unsigned long)tmpWp
  );

  if (csdStructure == 1) {
    const uint32_t cSize =
        extractBits(csd, 69, 48);

    const uint64_t capacityBytes =
        ((uint64_t)cSize + 1ULL) *
        512ULL *
        1024ULL;

    blockCountOut =
        (uint32_t)(capacityBytes / 512ULL);

    Serial.printf(
        "  C_SIZE=%lu capacity=%llu bytes (%.3f GiB), blocks=%lu\n",
        (unsigned long)cSize,
        (unsigned long long)capacityBytes,
        (double)capacityBytes /
            (1024.0 * 1024.0 * 1024.0),
        (unsigned long)blockCountOut
    );
  }
}

static bool readSectorVerified(
    RawBus& bus,
    uint32_t lba,
    uint8_t out[SECTOR_SIZE],
    uint8_t& r1Out,
    bool highCapacity,
    bool& crcOkOut,
    bool& allFfOut,
    bool& all00Out) {
  const uint32_t arg =
      highCapacity
          ? lba
          : lba * 512UL;

  bus.spi.beginTransaction(bus.settings());
  digitalWrite(SD_CS, LOW);

  r1Out =
      commandSelected(
          bus,
          NECRO_CMD17,
          arg,
          0
      );

  if (r1Out != 0x00 ||
      !waitDataToken(
          bus,
          0xFE,
          1000)) {
    deselect(bus);
    bus.spi.endTransaction();

    crcOkOut = false;
    allFfOut = false;
    all00Out = false;
    return false;
  }

  for (size_t i = 0; i < SECTOR_SIZE; ++i) {
    out[i] =
        bus.spi.transfer(0xFF);
  }

  const uint16_t cardCrc =
      ((uint16_t)bus.spi.transfer(0xFF) << 8) |
      bus.spi.transfer(0xFF);

  deselect(bus);
  bus.spi.endTransaction();

  const uint16_t calcCrc =
      crc16Ccitt(out, SECTOR_SIZE);

  crcOkOut =
      calcCrc == cardCrc;

  allFfOut = true;
  all00Out = true;

  for (size_t i = 0; i < SECTOR_SIZE; ++i) {
    allFfOut &= out[i] == 0xFF;
    all00Out &= out[i] == 0x00;
  }

  Serial.printf(
      "  LBA %-10lu R1=%02X CRC card=%04X calc=%04X %s data=%s\n",
      (unsigned long)lba,
      r1Out,
      cardCrc,
      calcCrc,
      crcOkOut ? "CRC-OK" : "CRC-BAD",
      allFfOut
          ? "ALL-FF"
          : (all00Out ? "ALL-00" : "MIXED")
  );

  return true;
}


static bool quickCleanReinit(
    RawBus& bus) {
  digitalWrite(SD_CS, HIGH);
  idleClocks(bus, 16);
  delay(20);

  const uint8_t r0 =
      command(
          bus,
          NECRO_CMD0,
          0,
          0
      );

  if (r0 != 0x01) {
    Serial.printf(
        "  REINIT CMD0 failed: %02X\n",
        r0
    );
    return false;
  }

  uint8_t r7[4] = {};
  const uint8_t r8 =
      command(
          bus,
          NECRO_CMD8,
          0x000001AAUL,
          0,
          r7,
          sizeof(r7)
      );

  if (r8 != 0x01) {
    Serial.printf(
        "  REINIT CMD8 failed: %02X\n",
        r8
    );
    return false;
  }

  for (uint16_t i = 0; i < 100; ++i) {
    const uint8_t r55 =
        command(
            bus,
            NECRO_CMD55,
            0,
            0
        );

    const uint8_t r41 =
        command(
            bus,
            NECRO_ACMD41,
            0x40000000UL,
            0
        );

    if (r41 == 0x00) {
      Serial.printf(
          "  REINIT ready after %u loops (CMD55=%02X)\n",
          i + 1,
          r55
      );
      return true;
    }

    delay(10);
  }

  Serial.println(
      "  REINIT failed to leave idle."
  );
  return false;
}

static void sampleCapacity(
    RawBus& bus,
    uint32_t blockCount) {
  if (blockCount < 16) {
    Serial.println(
        "  Capacity sample skipped: invalid block count."
    );
    return;
  }

  Serial.println(
      "  === CAPACITY SAMPLE (fresh init per LBA) ==="
  );

  const uint32_t last =
      blockCount - 1;

  const uint32_t points[] = {
      0,
      1,
      2,
      2048,
      blockCount / 8,
      blockCount / 4,
      blockCount / 2,
      (blockCount * 3UL) / 4UL,
      blockCount - 2048,
      last
  };

  uint8_t sector[SECTOR_SIZE];

  for (uint32_t lba : points) {
    if (lba >= blockCount) {
      continue;
    }

    Serial.printf(
        "  -- sample LBA %lu --\n",
        (unsigned long)lba
    );

    if (!quickCleanReinit(bus)) {
      Serial.println(
          "  Sample skipped: re-init failed."
      );
      continue;
    }

    uint8_t r1 = 0xFF;
    bool crcOk = false;
    bool allFf = false;
    bool all00 = false;

    const bool ok =
        readSectorVerified(
            bus,
            lba,
            sector,
            r1,
            true,
            crcOk,
            allFf,
            all00
        );

    if (!ok) {
      Serial.printf(
          "  LBA %-10lu READ FAIL R1=%02X\n",
          (unsigned long)lba,
          r1
      );
    }
  }
}

static void probeWriteProtect(
    RawBus& bus) {
  Serial.println(
      "  === WRITE-PROTECT PROBES ==="
  );

  // CMD30 returns a 4-byte write-protect bitmap as a data block
  // on cards that implement the command.
  bus.spi.beginTransaction(bus.settings());
  digitalWrite(SD_CS, LOW);

  const uint8_t r30 =
      commandSelected(
          bus,
          NECRO_CMD30,
          0,
          0
      );

  Serial.printf(
      "  CMD30 SEND_WRITE_PROT -> R1=%02X\n",
      r30
  );

  if (r30 == 0x00 &&
      waitDataToken(
          bus,
          0xFE,
          500)) {
    uint8_t wp[4];

    for (uint8_t& b : wp) {
      b = bus.spi.transfer(0xFF);
    }

    const uint8_t crcHi =
        bus.spi.transfer(0xFF);
    const uint8_t crcLo =
        bus.spi.transfer(0xFF);

    Serial.printf(
        "  WP bitmap=%02X%02X%02X%02X CRC=%02X%02X\n",
        wp[0],
        wp[1],
        wp[2],
        wp[3],
        crcHi,
        crcLo
    );
  }

  deselect(bus);
  bus.spi.endTransaction();
}

static void postReadyTriage(
    RawBus& bus,
    NecromancerResult& result) {
  Serial.println(
      "  === POST-READY TRIAGE ==="
  );

  const ResponseTrace ocrTrace =
      commandTrace(
          bus,
          NECRO_CMD58,
          0,
          0
      );
  printTrace("POST CMD58", ocrTrace);

  bus.spi.beginTransaction(bus.settings());
  digitalWrite(SD_CS, LOW);

  const uint8_t r13 =
      commandSelected(
          bus,
          NECRO_CMD13,
          0,
          0
      );
  const uint8_t r2 =
      bus.spi.transfer(0xFF);

  deselect(bus);
  bus.spi.endTransaction();

  Serial.printf(
      "  POST CMD13 -> R1=%02X R2=%02X\n",
      r13,
      r2
  );

  uint8_t regR1 = 0xFF;

  result.cidOk =
      readRegister16(
          bus,
          NECRO_CMD10,
          result.cid,
          regR1
      );

  Serial.printf(
      "  POST CID -> %s R1=%02X\n",
      result.cidOk ? "PASS" : "FAIL",
      regR1
  );

  if (result.cidOk) {
    Serial.print("  CID: ");
    printHexBytes(result.cid, sizeof(result.cid));
    Serial.println();
  }

  result.csdOk =
      readRegister16(
          bus,
          NECRO_CMD9,
          result.csd,
          regR1
      );

  Serial.printf(
      "  POST CSD -> %s R1=%02X\n",
      result.csdOk ? "PASS" : "FAIL",
      regR1
  );

  // Preserve the earliest successful data-read proof before any
  // experimental command can destabilize this damaged controller.
  uint8_t firstSector0[SECTOR_SIZE];
  uint8_t firstR1 = 0xFF;
  bool firstCrcOk = false;
  bool firstAllFf = false;
  bool firstAll00 = false;

  if (readSectorVerified(
          bus,
          0,
          firstSector0,
          firstR1,
          true,
          firstCrcOk,
          firstAllFf,
          firstAll00)) {
    result.sector0ReadOk = true;
    Serial.println(
        "  STICKY sector0 proof: PASS"
    );
    dumpSectorSummary(
        "FIRST SECTOR0",
        firstSector0
    );
  } else {
    Serial.printf(
        "  STICKY sector0 proof: FAIL R1=%02X
",
        firstR1
    );
  }

  uint32_t decodedBlocks = 0;

  if (result.cidOk) {
    decodeCid(result.cid);
  }

  if (result.csdOk) {
    Serial.print("  CSD: ");
    printHexBytes(result.csd, sizeof(result.csd));
    Serial.println();

    decodeCsd(
        result.csd,
        decodedBlocks
    );
  }

  if (decodedBlocks > 0) {
    sampleCapacity(
        bus,
        decodedBlocks
    );
  }

  // Riskier/less essential command comes only after all useful reads.
  quickCleanReinit(bus);
  probeWriteProtect(bus);

  uint8_t sector0[SECTOR_SIZE];
  uint8_t sector1[SECTOR_SIZE];
  uint8_t r1 = 0xFF;

  if (readSectorAt(bus, 0, sector0, r1)) {
    result.sector0ReadOk = true;
    dumpSectorSummary("SECTOR0", sector0);
  } else {
    Serial.printf(
        "  POST sector0 read FAIL R1=%02X\n",
        r1
    );
  }

  if (readSectorAt(bus, 1, sector1, r1)) {
    dumpSectorSummary("SECTOR1", sector1);
  } else {
    Serial.printf(
        "  POST sector1/address1 read FAIL R1=%02X\n",
        r1
    );
  }

  const uint8_t r16 =
      command(
          bus,
          NECRO_CMD16,
          512,
          0
      );

  Serial.printf(
      "  POST CMD16(512) -> %02X\n",
      r16
  );

  const uint8_t r59 =
      command(
          bus,
          NECRO_CMD59,
          0,
          0
      );

  Serial.printf(
      "  POST CMD59 CRC off -> %02X\n",
      r59
  );

  uint8_t ocr[4] = {};
  const uint8_t r58 =
      command(
          bus,
          NECRO_CMD58,
          0,
          0,
          ocr,
          sizeof(ocr)
      );

  Serial.printf(
      "  POST CMD58 after CRC-off -> R1=%02X OCR=%02X%02X%02X%02X\n",
      r58,
      ocr[0],
      ocr[1],
      ocr[2],
      ocr[3]
  );

  if (r58 == 0x00) {
    result.cmd58R1 = r58;
    result.ocr =
        ((uint32_t)ocr[0] << 24) |
        ((uint32_t)ocr[1] << 16) |
        ((uint32_t)ocr[2] << 8) |
        (uint32_t)ocr[3];
    result.ocrOk = true;
  }
}

static bool destructiveProbe(
    RawBus& bus,
    NecromancerResult& result) {
  uint8_t original[SECTOR_SIZE];
  uint8_t pattern[SECTOR_SIZE];
  uint8_t verify[SECTOR_SIZE];

  uint8_t r1 = 0xFF;

  if (!readSector0(
          bus,
          original,
          r1)) {
    result.lastR1 = r1;
    // Do not clear an earlier proven-good sector0 read.
    return false;
  }

  result.sector0ReadOk = true;

  Serial.println(
      "  Sector 0 raw read: PASS"
  );
  Serial.println(
      "  *** DESTRUCTIVE PROBE: temporarily overwriting LBA 0 ***"
  );

  makeDestructivePattern(pattern);

  result.destructiveWriteAttempted = true;
  result.destructiveWriteOk =
      writeSector0(
          bus,
          pattern,
          r1
      );

  result.lastR1 = r1;

  Serial.printf(
      "  LBA0 pattern write: %s R1=%02X\n",
      result.destructiveWriteOk ? "PASS" : "FAIL",
      r1
  );

  if (!result.destructiveWriteOk) {
    return false;
  }

  if (readSector0(
          bus,
          verify,
          r1)) {
    result.destructiveVerifyOk =
        memcmp(
            verify,
            pattern,
            SECTOR_SIZE
        ) == 0;
  }

  Serial.printf(
      "  LBA0 pattern verify: %s\n",
      result.destructiveVerifyOk ? "PASS" : "FAIL"
  );

  result.restoreOk =
      writeSector0(
          bus,
          original,
          r1
      );

  if (result.restoreOk) {
    uint8_t restored[SECTOR_SIZE];

    result.restoreOk =
        readSector0(
            bus,
            restored,
            r1
        ) &&
        memcmp(
            restored,
            original,
            SECTOR_SIZE
        ) == 0;
  }

  Serial.printf(
      "  LBA0 restore+verify: %s\n",
      result.restoreOk ? "PASS" : "FAIL"
  );

  if (!result.restoreOk) {
    Serial.println(
        "  !!! WARNING: sector 0 may now be corrupted !!!"
    );
  }

  return
      result.destructiveWriteOk &&
      result.destructiveVerifyOk &&
      result.restoreOk;
}

}  // namespace

NecromancerResult runSdNecromancer() {
  NecromancerResult best;

  SdFs& sd = getSd();
  SPIClass& spi = getSdSpi();

  sd.end();

  // First bypass the ESP32 SPI peripheral entirely.
  // v0.8.2 showed static MISO=HIGH while SPIClass returned only 0x00.
  spi.end();
  runSdBitBangProbe();

  // Re-attach VSPI for the existing hardware-SPI forensic pass.
  initSdHardware();

  pinMode(SD_CS, OUTPUT);
  pinMode(SD_MISO, INPUT);
  digitalWrite(SD_CS, HIGH);

  Serial.println();
  Serial.println("=== SD NECROMANCER v0.8.12 ===");
  Serial.println(
      "Sticky read proof + fresh-init-per-LBA capacity sampling."
  );

  Serial.printf(
      "CRC sanity: CMD0=%02X (expect 95), CMD8(1AA)=%02X (expect 87)\n",
      commandCrc(NECRO_CMD0, 0),
      commandCrc(NECRO_CMD8, 0x000001AAUL)
  );
  Serial.println(
      "If a card becomes writable, LBA 0 WILL be overwritten and restored."
  );

  const uint8_t modes[] = {
      SPI_MODE0,
      SPI_MODE3
  };

  for (uint8_t modeIndex = 0;
       modeIndex < 2 && !best.ready;
       ++modeIndex) {
    const uint8_t mode =
        modes[modeIndex];

    Serial.printf(
        "\n--- SPI MODE %u ---\n",
        mode == SPI_MODE0 ? 0 : 3
    );

    for (size_t speedIndex = 0;
         speedIndex < NECRO_SPEED_COUNT;
         ++speedIndex) {
      NecromancerResult attempt;

      attempt.speedHz =
          NECRO_SPEEDS[speedIndex];
      attempt.usedSpiMode3 =
          mode == SPI_MODE3;

      RawBus bus {
          spi,
          attempt.speedHz,
          mode
      };

      Serial.printf(
          "\nTrying %lu Hz...\n",
          (unsigned long)attempt.speedHz
      );

      // v0.8.8: first try a spec-minimal init path before any invasive
      // forensic commands can perturb the controller.
      if (runCleanInitMatrix(
              bus,
              attempt)) {
        best = attempt;

        Serial.println(
            "  CARD LEFT IDLE STATE VIA CLEAN INIT."
        );

        postReadyTriage(
            bus,
            best
        );

        Serial.println(
            "  Fresh re-init before destructive probe..."
        );
        quickCleanReinit(bus);

        destructiveProbe(
            bus,
            best
        );

        break;
      }

      Serial.println(
          "  Clean init matrix failed; entering forensic init path..."
      );

      // Reset to SPI idle before the forensic path.
      digitalWrite(SD_CS, HIGH);
      idleClocks(bus, 32);
      delay(100);

      if (initOne(bus, attempt)) {
        best = attempt;

        Serial.println(
            "  CARD LEFT IDLE STATE VIA FORENSIC INIT."
        );

        postReadyTriage(
            bus,
            best
        );

        Serial.println(
            "  Fresh re-init before destructive probe..."
        );
        quickCleanReinit(bus);

        destructiveProbe(
            bus,
            best
        );

        break;
      }

      best = attempt;

      digitalWrite(SD_CS, HIGH);
      idleClocks(bus, 64);
      delay(100);
    }
  }

  Serial.println();
  Serial.println("=== NECROMANCER RESULT ===");
  Serial.printf(
      "Ready: %s\n",
      best.ready ? "YES" : "NO"
  );
  Serial.printf(
      "Speed: %lu Hz\n",
      (unsigned long)best.speedHz
  );
  Serial.printf(
      "SPI mode: %s\n",
      best.usedSpiMode3 ? "3" : "0"
  );
  Serial.printf(
      "CMD1 fallback: %s\n",
      best.usedCmd1 ? "YES" : "NO"
  );
  Serial.printf(
      "OCR: %s %08lX\n",
      best.ocrOk ? "PASS" : "N/A/FAIL",
      (unsigned long)best.ocr
  );
  Serial.printf(
      "CID/CSD: %s/%s\n",
      best.cidOk ? "PASS" : "FAIL",
      best.csdOk ? "PASS" : "FAIL"
  );
  Serial.printf(
      "Sector0 read: %s\n",
      best.sector0ReadOk ? "PASS" : "FAIL"
  );
  Serial.printf(
      "Write/verify/restore: %s/%s/%s\n",
      best.destructiveWriteOk ? "PASS" : "FAIL",
      best.destructiveVerifyOk ? "PASS" : "FAIL",
      best.restoreOk ? "PASS" : "FAIL"
  );
  Serial.println("==========================");

  scanCard();
  return best;
}

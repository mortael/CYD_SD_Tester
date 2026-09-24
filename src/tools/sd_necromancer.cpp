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
static constexpr uint8_t NECRO_CMD12 = 12;
static constexpr uint8_t NECRO_CMD13 = 13;
static constexpr uint8_t NECRO_CMD16 = 16;
static constexpr uint8_t NECRO_CMD17 = 17;
static constexpr uint8_t NECRO_CMD18 = 18;
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

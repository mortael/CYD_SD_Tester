#include "sd_bitbang_probe.h"

#include <Arduino.h>

#include "../core/config.h"

namespace {

static constexpr uint8_t NECRO_CMD0   = 0;
static constexpr uint8_t NECRO_CMD1   = 1;
static constexpr uint8_t NECRO_CMD8   = 8;
static constexpr uint8_t NECRO_CMD55  = 55;
static constexpr uint8_t NECRO_CMD58  = 58;
static constexpr uint8_t NECRO_ACMD41 = 41;

static constexpr size_t TRACE_BYTES = 32;


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

  crc = crc7Byte(crc, 0x40U | cmd);
  crc = crc7Byte(crc, (uint8_t)(arg >> 24));
  crc = crc7Byte(crc, (uint8_t)(arg >> 16));
  crc = crc7Byte(crc, (uint8_t)(arg >> 8));
  crc = crc7Byte(crc, (uint8_t)arg);

  return (uint8_t)((crc << 1) | 1U);
}

struct Trace {
  uint8_t bytes[TRACE_BYTES] = {};
  uint8_t r1 = 0xFF;
  int8_t r1Index = -1;
  bool all00 = true;
  bool allFF = true;
};

class BitBangSpi {
 public:
  explicit BitBangSpi(uint32_t requestedHz)
      : halfDelayUs_(halfDelayFor(requestedHz)) {}

  void begin() {
    pinMode(SD_CS, OUTPUT);
    pinMode(SD_SCK, OUTPUT);
    pinMode(SD_MOSI, OUTPUT);
    pinMode(SD_MISO, INPUT);

    digitalWrite(SD_CS, HIGH);
    digitalWrite(SD_SCK, LOW);
    digitalWrite(SD_MOSI, HIGH);
    delayMicroseconds(20);
  }

  uint8_t transfer(uint8_t out) {
    uint8_t in = 0;

    for (uint8_t bit = 0; bit < 8; ++bit) {
      digitalWrite(
          SD_MOSI,
          (out & 0x80U) ? HIGH : LOW
      );
      out <<= 1;

      delayMicroseconds(halfDelayUs_);

      // SPI mode 0: sample MISO on rising edge.
      digitalWrite(SD_SCK, HIGH);
      delayMicroseconds(halfDelayUs_);

      in <<= 1;
      if (digitalRead(SD_MISO)) {
        in |= 1U;
      }

      digitalWrite(SD_SCK, LOW);
    }

    return in;
  }

  void idleBytes(size_t count) {
    digitalWrite(SD_CS, HIGH);
    digitalWrite(SD_MOSI, HIGH);

    for (size_t i = 0; i < count; ++i) {
      transfer(0xFF);
    }
  }

 private:
  static uint16_t halfDelayFor(uint32_t hz) {
    // digitalWrite/digitalRead overhead is significant; these are
    // deliberately approximate "speed classes".
    if (hz <= 25000UL) {
      return 18;
    }

    if (hz <= 50000UL) {
      return 8;
    }

    return 3;
  }

  uint16_t halfDelayUs_;
};

static void printHex(
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

static const char* classify(
    const Trace& trace) {
  if (trace.all00) {
    return "ALL-00";
  }

  if (trace.allFF) {
    return "ALL-FF";
  }

  if (trace.r1Index >= 0) {
    return "R1 candidate";
  }

  return "MIXED";
}

static void printTrace(
    const char* label,
    const Trace& trace) {
  Serial.printf(
      "  BB %-15s raw: ",
      label
  );

  printHex(
      trace.bytes,
      TRACE_BYTES
  );
  Serial.println();

  Serial.printf(
      "  BB %-15s class: %s",
      label,
      classify(trace)
  );

  if (trace.r1Index >= 0) {
    Serial.printf(
        " R1=%02X@%d",
        trace.r1,
        trace.r1Index
    );
  }

  Serial.println();
}

static Trace commandTrace(
    BitBangSpi& bus,
    uint8_t cmd,
    uint32_t arg,
    uint8_t crc = 0) {
  Trace trace;

  if (crc == 0) {
    crc = commandCrc(cmd, arg);
  }

  digitalWrite(SD_CS, LOW);
  bus.transfer(0xFF);

  bus.transfer(0x40U | cmd);
  bus.transfer((uint8_t)(arg >> 24));
  bus.transfer((uint8_t)(arg >> 16));
  bus.transfer((uint8_t)(arg >> 8));
  bus.transfer((uint8_t)arg);
  bus.transfer(crc);

  for (size_t i = 0; i < TRACE_BYTES; ++i) {
    const uint8_t value =
        bus.transfer(0xFF);

    trace.bytes[i] = value;
    trace.all00 &= value == 0x00;
    trace.allFF &= value == 0xFF;

    if (trace.r1Index < 0 &&
        (value & 0x80U) == 0) {
      trace.r1 = value;
      trace.r1Index = (int8_t)i;
    }
  }

  // Don't mistake a permanently-low bus for R1=00.
  if (trace.all00) {
    trace.r1 = 0xFF;
    trace.r1Index = -1;
  }

  digitalWrite(SD_CS, HIGH);
  bus.transfer(0xFF);

  return trace;
}

static void sampleIdle(
    BitBangSpi& bus,
    bool selected) {
  uint8_t bytes[16];

  digitalWrite(
      SD_CS,
      selected ? LOW : HIGH
  );
  digitalWrite(SD_MOSI, HIGH);
  delayMicroseconds(20);

  const int staticLevel =
      digitalRead(SD_MISO);

  for (uint8_t& value : bytes) {
    value = bus.transfer(0xFF);
  }

  digitalWrite(SD_CS, HIGH);

  bool all00 = true;
  bool allFF = true;

  for (uint8_t value : bytes) {
    all00 &= value == 0x00;
    allFF &= value == 0xFF;
  }

  Serial.printf(
      "  BB bus CS=%s static-MISO=%s bytes=",
      selected ? "LOW" : "HIGH",
      staticLevel ? "HIGH" : "LOW"
  );

  printHex(
      bytes,
      sizeof(bytes)
  );

  Serial.printf(
      " [%s]\n",
      all00
          ? "ALL-00"
          : (allFF ? "ALL-FF" : "MIXED")
  );
}

static void forcedProbes(
    BitBangSpi& bus) {
  Trace trace =
      commandTrace(
          bus,
          NECRO_CMD8,
          0x000001AAUL,
          0
      );
  printTrace("CMD8", trace);

  trace =
      commandTrace(
          bus,
          NECRO_CMD58,
          0,
          0
      );
  printTrace("CMD58", trace);

  trace =
      commandTrace(
          bus,
          NECRO_CMD55,
          0,
          0
      );
  printTrace("CMD55", trace);

  trace =
      commandTrace(
          bus,
          NECRO_ACMD41,
          0x40000000UL,
          0
      );
  printTrace("ACMD41", trace);

  trace =
      commandTrace(
          bus,
          NECRO_CMD1,
          0x40000000UL,
          0
      );
  printTrace("CMD1", trace);
}

static void runAtSpeed(
    uint32_t requestedHz) {
  Serial.printf(
      "\n--- BIT-BANG SPI MODE 0, target ~%lu Hz ---\n",
      (unsigned long)requestedHz
  );

  BitBangSpi bus(requestedHz);
  bus.begin();

  // Plenty of clocks with CS high before NECRO_CMD0.
  bus.idleBytes(20);

  sampleIdle(bus, false);
  sampleIdle(bus, true);

  bool sawIdle = false;

  for (uint8_t attempt = 0;
       attempt < 8;
       ++attempt) {
    const Trace trace =
        commandTrace(
            bus,
            NECRO_CMD0,
            0,
            0
        );

    char label[20];
    snprintf(
        label,
        sizeof(label),
        "CMD0 #%u",
        attempt + 1
    );

    printTrace(
        label,
        trace
    );

    if (trace.r1Index >= 0 &&
        trace.r1 == 0x01) {
      sawIdle = true;
      break;
    }

    bus.idleBytes(4);
    delay(10);
  }

  Serial.println(
      sawIdle
          ? "  BB CMD0 reached IDLE (R1=01)."
          : "  BB CMD0 did not reach IDLE."
  );

  forcedProbes(bus);
}

}  // namespace

void runSdBitBangProbe() {
  Serial.println();
  Serial.println(
      "=== SOFTWARE-SPI CROSS-CHECK ==="
  );
  Serial.println(
      "VSPI detached; SD_MISO is sampled directly through GPIO."
  );

  const uint32_t speeds[] = {
      100000UL,
       50000UL,
       25000UL
  };

  for (uint32_t speed : speeds) {
    runAtSpeed(speed);
    delay(50);
  }

  digitalWrite(SD_CS, HIGH);
  digitalWrite(SD_SCK, LOW);
  digitalWrite(SD_MOSI, HIGH);

  Serial.println(
      "=== END SOFTWARE-SPI CROSS-CHECK ==="
  );
}

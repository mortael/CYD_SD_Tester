#include <Arduino.h>

#include "../core/config.h"
#include "touch.h"
#include "touch_calibration.h"

static uint8_t touchTransfer(
    uint8_t out
) {
  uint8_t in = 0;

  for (int bit = 7;
       bit >= 0;
       --bit) {
    digitalWrite(
        TOUCH_MOSI,
        (out >> bit) & 1
    );

    digitalWrite(
        TOUCH_SCK,
        HIGH
    );
    delayMicroseconds(1);

    in =
        (in << 1) |
        (digitalRead(TOUCH_MISO)
             ? 1
             : 0);

    digitalWrite(
        TOUCH_SCK,
        LOW
    );
    delayMicroseconds(1);
  }

  return in;
}

static uint16_t touchRead12(
    uint8_t command
) {
  digitalWrite(
      TOUCH_CS,
      LOW
  );

  touchTransfer(command);

  uint16_t value =
      ((uint16_t)touchTransfer(0x00) << 8);
  value |= touchTransfer(0x00);

  digitalWrite(
      TOUCH_CS,
      HIGH
  );

  return (value >> 3) & 0x0FFF;
}

void initTouch() {
  pinMode(TOUCH_CS, OUTPUT);
  pinMode(TOUCH_SCK, OUTPUT);
  pinMode(TOUCH_MOSI, OUTPUT);
  pinMode(TOUCH_MISO, INPUT);
  pinMode(TOUCH_IRQ, INPUT_PULLUP);

  digitalWrite(
      TOUCH_CS,
      HIGH
  );
  digitalWrite(
      TOUCH_SCK,
      LOW
  );
}

bool readTouch(
    int& screenX,
    int& screenY
) {
  if (digitalRead(TOUCH_IRQ) ==
      HIGH) {
    return false;
  }

  uint32_t xSum = 0;
  uint32_t ySum = 0;

  constexpr int samples = 5;

  for (int i = 0;
       i < samples;
       ++i) {
    xSum +=
        touchRead12(0xD0);
    ySum +=
        touchRead12(0x90);
  }

  const int rawX =
      xSum / samples;
  const int rawY =
      ySum / samples;

  mapTouchToScreen(
      rawX,
      rawY,
      screenX,
      screenY
  );

  return true;
}

void waitForTouchRelease() {
  while (digitalRead(TOUCH_IRQ) ==
         LOW) {
    delay(5);
  }
}

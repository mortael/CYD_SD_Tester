#include <Arduino.h>
#include <TFT_eSPI.h>

#include "../core/config.h"
#include "../hardware/touch.h"
#include "../hardware/card_format.h"
#include "../tools/disk_inspector.h"
#include "display_ui.h"
#include "display_ui_internal.h"
#include "ui_geometry.h"

struct InspectorButton {
  int x;
  int y;
  int w;
  int h;
  const char* label;
};

static constexpr InspectorButton BTN_RAW {
  8, 194, 92, 38, "RAW VIEW"
};

static constexpr InspectorButton BTN_PREV {
  106, 194, 48, 38, "<"
};

static constexpr InspectorButton BTN_NEXT {
  160, 194, 48, 38, ">"
};

static constexpr InspectorButton BTN_BACK_INSPECT {
  214, 194, 98, 38, "BACK"
};

static bool hitInspector(
    const InspectorButton& b,
    int x,
    int y
) {
  return hitTestRect(
      b.x, b.y, b.w, b.h, x, y
  );
}

static void drawInspectorButton(
    const InspectorButton& b,
    uint16_t border = TFT_DARKGREY
) {
  tft().drawRoundRect(
      b.x, b.y, b.w, b.h, 5, border
  );
  tft().setTextDatum(MC_DATUM);
  tft().setTextColor(TFT_WHITE, TFT_BLACK);
  tft().drawString(
      b.label,
      b.x + b.w / 2,
      b.y + b.h / 2,
      2
  );
}

static void drawPartitionInspector(
    const DiskInspectorResult& result
) {
  header("DISK INSPECTOR");

  char value[40];
  char cap[24];
  formatHumanBytes(
      result.cardBytes,
      cap,
      sizeof(cap)
  );

  lineRow(40, "Card", cap);
  lineRow(
      58,
      "Scheme",
      result.mbrSignatureOk
          ? (result.protectiveGpt ? "GPT protective" : "MBR")
          : "No MBR",
      result.mbrSignatureOk ? TFT_GREEN : TFT_ORANGE
  );

  if (!result.sector0Ok) {
    lineRow(82, "Sector 0", "READ FAIL", TFT_RED);
  } else if (result.partitionCount == 0) {
    char fs[16];
    detectFilesystemSignature(
        result.sector0,
        fs,
        sizeof(fs)
    );
    lineRow(82, "Partitions", "none");
    lineRow(104, "LBA0 FS", fs);
  } else {
    for (size_t i = 0; i < result.partitionCount && i < 4; ++i) {
      const PartitionInfo& p = result.partitions[i];
      const uint64_t bytes =
          (uint64_t)p.sectorCount * 512ULL;
      char size[18];
      formatHumanBytes(bytes, size, sizeof(size));

      snprintf(
          value,
          sizeof(value),
          "P%u %s %s",
          (unsigned)(i + 1),
          p.fsName,
          size
      );

      const int rowY = 82 + (int)i * 26;

      tft().setTextDatum(TL_DATUM);
      tft().setTextColor(TFT_WHITE, TFT_BLACK);
      tft().drawString(
          value,
          10,
          rowY,
          2
      );

      snprintf(
          value,
          sizeof(value),
          "LBA %lu  type %02X",
          (unsigned long)p.startLba,
          p.type
      );
      tft().setTextColor(TFT_LIGHTGREY, TFT_BLACK);
      tft().drawString(
          value,
          10,
          rowY + 14,
          1
      );
    }
  }

  drawInspectorButton(BTN_RAW, TFT_CYAN);
  drawInspectorButton(BTN_BACK_INSPECT);
}

static void drawRawSector(
    uint32_t lba,
    const uint8_t sector[512],
    bool ok
) {
  header("RAW SECTOR VIEW");

  char title[32];
  snprintf(
      title,
      sizeof(title),
      "LBA %lu",
      (unsigned long)lba
  );

  tft().setTextDatum(TL_DATUM);
  tft().setTextColor(
      ok ? TFT_CYAN : TFT_RED,
      TFT_BLACK
  );
  tft().drawString(title, 8, 38, 2);

  if (!ok) {
    tft().setTextColor(TFT_RED, TFT_BLACK);
    tft().drawString("READ FAILED", 8, 64, 4);
  } else {
    for (uint8_t row = 0; row < 8; ++row) {
      char line[64];
      const uint16_t off = row * 8;
      int pos = snprintf(
          line,
          sizeof(line),
          "%03X: ",
          off
      );

      for (uint8_t col = 0; col < 8; ++col) {
        pos += snprintf(
            line + pos,
            sizeof(line) - pos,
            "%02X ",
            sector[off + col]
        );
      }

      pos += snprintf(
          line + pos,
          sizeof(line) - pos,
          " |"
      );

      for (uint8_t col = 0; col < 8; ++col) {
        const uint8_t c = sector[off + col];
        pos += snprintf(
            line + pos,
            sizeof(line) - pos,
            "%c",
            (c >= 32 && c <= 126) ? (char)c : '.'
        );
      }

      snprintf(
          line + pos,
          sizeof(line) - pos,
          "|"
      );

      tft().setTextColor(TFT_WHITE, TFT_BLACK);
      tft().drawString(
          line,
          8,
          58 + row * 16,
          1
      );
    }
  }

  drawInspectorButton(BTN_PREV, TFT_CYAN);
  drawInspectorButton(BTN_NEXT, TFT_CYAN);
  drawInspectorButton(BTN_BACK_INSPECT);
}

void uiRunDiskInspector() {
  const DiskInspectorResult result =
      inspectDisk();

  drawPartitionInspector(result);

  while (true) {
    int x;
    int y;

    if (!readTouch(x, y)) {
      delay(5);
      continue;
    }

    const bool raw = hitInspector(BTN_RAW, x, y);
    const bool back = hitInspector(BTN_BACK_INSPECT, x, y);
    waitForTouchRelease();

    if (back) {
      return;
    }

    if (!raw) {
      continue;
    }

    uint32_t lba = 0;
    uint8_t sector[512];
    bool sectorOk = readRawSector(lba, sector);
    drawRawSector(lba, sector, sectorOk);

    while (true) {
      if (!readTouch(x, y)) {
        delay(5);
        continue;
      }

      const bool prev = hitInspector(BTN_PREV, x, y);
      const bool next = hitInspector(BTN_NEXT, x, y);
      const bool rawBack = hitInspector(BTN_BACK_INSPECT, x, y);
      waitForTouchRelease();

      if (rawBack) {
        drawPartitionInspector(result);
        break;
      }

      if (prev && lba > 0) {
        --lba;
      } else if (next) {
        const uint64_t sectors = result.cardBytes / 512ULL;
        if ((uint64_t)lba + 1ULL < sectors) {
          ++lba;
        }
      } else {
        continue;
      }

      sectorOk = readRawSector(lba, sector);
      drawRawSector(lba, sector, sectorOk);
    }
  }
}

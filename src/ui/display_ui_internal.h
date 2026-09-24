#pragma once
#include <TFT_eSPI.h>

// Shared internals for the display_ui.* screen files — the single TFT
// instance and drawing primitives common to more than one screen.
// Not part of the public display_ui.h API.

TFT_eSPI& tft();

void header(
    const char* title
);

void lineRow(
    int y,
    const char* left,
    const char* right,
    uint16_t rightColor = TFT_WHITE
);

#pragma once

// Pure coordinate-mapping — no hardware/Arduino dependency, safe to
// unit-test on host (see test/test_touch_calibration).
//
// Maps a raw XPT2046 ADC reading to a screen coordinate using the
// touch calibration constants in config.h, then clamps to the screen
// bounds. Note the X/Y swap: the touch controller's electrical X axis
// maps to the screen's Y axis and vice versa, because of how the touch
// panel is mounted relative to the display's rotation.
void mapTouchToScreen(
    int rawX,
    int rawY,
    int& screenX,
    int& screenY
);

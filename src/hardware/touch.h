#pragma once

void initTouch();

bool readTouch(
    int& screenX,
    int& screenY
);

void waitForTouchRelease();

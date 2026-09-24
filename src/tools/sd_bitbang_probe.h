#pragma once

// Bypass ESP32 SPIClass completely and sample SD_MISO directly
// via GPIO on each software-generated clock edge.
void runSdBitBangProbe();

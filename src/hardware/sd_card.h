#pragma once
#include <SdFat.h>
#include "../core/app_types.h"

void initSdHardware();

// Re-qualifies and mounts the card, refreshing the state exposed by
// getCardInfo()/getAttempts()/getAttemptCount() below. Returns whether
// a usable SPI speed was found; no current caller checks it, since
// scanCard()'s real output is that refreshed global state rather than
// a one-shot result.
//
// This differs deliberately from runSpiScalingBenchmark() (benchmark.h),
// which returns a self-contained SpiScaleResult: that result is shown
// once, immediately after the call, and never revisited, whereas the
// diagnostics screen needs to query per-attempt qualification history
// at an arbitrary later time (whenever the user taps DIAG) — a getter
// over persistent state fits that access pattern; embedding the same
// data in a return value would not, since nothing would hold onto it.
bool scanCard();

SdFs& getSd();
SPIClass& getSdSpi();
const CardInfo& getCardInfo();

const AttemptResult* getAttempts();
size_t getAttemptCount();

SdSpiConfig makeSdConfig(
    uint32_t hz
);

// Needs SD_CARD_TYPE_* from SdFat (see card_format.h/card_identity.h for
// the rest of this module's pure naming/formatting helpers). Takes
// capacity explicitly (SDHC vs SDXC is a capacity threshold) instead of
// reading global card state, so it always reflects the CardInfo the
// caller actually has.
const char* cardTypeName(
    uint8_t type,
    uint64_t capacityBytes
);

// Needs SdFat's FAT_TYPE_* constants, same reasoning as cardTypeName().
const char* fsName(
    uint8_t type
);

// Shared hardware-level helpers used by both the SD qualification path
// (this module) and the benchmark modules, which depend on hardware/
// but never the reverse.

// Reads the SD error code/data from `sd` after a failed operation,
// stores them, and prints a uniform "<label> FAIL XX/XX" line.
void recordSdError(
    SdFs& sd,
    uint8_t& errorCode,
    uint8_t& errorData,
    const char* label
);

// Allocates a DMA-capable, internal-RAM buffer for SD transfers.
// Returns nullptr (after logging) on failure.
uint8_t* allocateDmaBuffer(
    size_t size,
    const char* label
);

#pragma once
#include <cstddef>
#include <cstdint>

// Pure formatting helpers — no hardware/Arduino dependency, safe to
// unit-test on host (see test/test_card_format).

void formatHumanBytes(
    uint64_t bytes,
    char* out,
    size_t outSize
);

void formatSpiClock(
    uint32_t hz,
    char* out,
    size_t outSize
);

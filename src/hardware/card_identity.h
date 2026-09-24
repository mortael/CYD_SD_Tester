#pragma once
#include <cstdint>

#include "../core/app_types.h"

// Pure lookup/classification helpers — no hardware/Arduino dependency,
// safe to unit-test on host (see test/test_card_identity). fsName()
// lives in sd_card.h instead: it needs SdFat's FAT_TYPE_* constants,
// same reasoning as cardTypeName().

const char* attemptStageName(
    AttemptStage stage
);

const char* resolveVendor(
    uint8_t mid
);

const char* identityCheckName(
    const CardInfo& card
);

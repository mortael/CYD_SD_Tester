#include "card_identity.h"

const char* attemptStageName(
    AttemptStage stage
) {
  switch (stage) {
    case AttemptStage::NotTried:
      return "-";

    case AttemptStage::InitFailed:
      return "INIT";

    case AttemptStage::CidFailed:
      return "CID";

    case AttemptStage::CsdFailed:
      return "CSD";

    case AttemptStage::ReadFailed:
      return "READ";

    case AttemptStage::Passed:
      return "PASS";
  }

  return "?";
}

const char* resolveVendor(
    uint8_t mid
) {
  // Vendor hint only; not an authenticity guarantee.
  switch (mid) {
    case 0x01:
      return "Panasonic";

    case 0x02:
      return "Toshiba/Kioxia";

    case 0x03:
      return "SanDisk";

    case 0x08:
      return "Silicon Power";

    case 0x11:
      return "Sony";

    case 0x1B:
      return "Samsung";

    case 0x1D:
      return "ADATA";

    case 0x27:
      return "Phison";

    case 0x28:
      return "Lexar";

    case 0x31:
      return "Silicon Motion";

    case 0x41:
      return "Kingston";

    case 0x74:
      return "Transcend";

    case 0x82:
      return "Sony";

    default:
      return "Unknown";
  }
}

const char* identityCheckName(
    const CardInfo& card
) {
  if (!card.cardOk ||
      !card.cidOk) {
    return "UNKNOWN";
  }

  return
      card.identityAnomaly
          ? "ANOMALY"
          : "PASS";
}

#include <cstring>

#include <unity.h>

#include "hardware/card_identity.h"

void setUp() {}
void tearDown() {}

static void test_attempt_stage_name_all_stages() {
  TEST_ASSERT_EQUAL_STRING("-", attemptStageName(AttemptStage::NotTried));
  TEST_ASSERT_EQUAL_STRING("INIT", attemptStageName(AttemptStage::InitFailed));
  TEST_ASSERT_EQUAL_STRING("CID", attemptStageName(AttemptStage::CidFailed));
  TEST_ASSERT_EQUAL_STRING("CSD", attemptStageName(AttemptStage::CsdFailed));
  TEST_ASSERT_EQUAL_STRING("READ", attemptStageName(AttemptStage::ReadFailed));
  TEST_ASSERT_EQUAL_STRING("PASS", attemptStageName(AttemptStage::Passed));
}

static void test_resolve_vendor_known_ids() {
  TEST_ASSERT_EQUAL_STRING("SanDisk", resolveVendor(0x03));
  TEST_ASSERT_EQUAL_STRING("Samsung", resolveVendor(0x1B));
  TEST_ASSERT_EQUAL_STRING("Kingston", resolveVendor(0x41));
}

static void test_resolve_vendor_unknown_id() {
  TEST_ASSERT_EQUAL_STRING("Unknown", resolveVendor(0xFF));
}

static void test_identity_check_unknown_when_card_not_ok() {
  CardInfo card;
  card.cardOk = false;
  card.cidOk = true;
  TEST_ASSERT_EQUAL_STRING("UNKNOWN", identityCheckName(card));
}

static void test_identity_check_unknown_when_cid_not_ok() {
  CardInfo card;
  card.cardOk = true;
  card.cidOk = false;
  TEST_ASSERT_EQUAL_STRING("UNKNOWN", identityCheckName(card));
}

static void test_identity_check_pass() {
  CardInfo card;
  card.cardOk = true;
  card.cidOk = true;
  card.identityAnomaly = false;
  TEST_ASSERT_EQUAL_STRING("PASS", identityCheckName(card));
}

static void test_identity_check_anomaly() {
  CardInfo card;
  card.cardOk = true;
  card.cidOk = true;
  card.identityAnomaly = true;
  TEST_ASSERT_EQUAL_STRING("ANOMALY", identityCheckName(card));
}

int main(int argc, char** argv) {
  UNITY_BEGIN();

  RUN_TEST(test_attempt_stage_name_all_stages);
  RUN_TEST(test_resolve_vendor_known_ids);
  RUN_TEST(test_resolve_vendor_unknown_id);
  RUN_TEST(test_identity_check_unknown_when_card_not_ok);
  RUN_TEST(test_identity_check_unknown_when_cid_not_ok);
  RUN_TEST(test_identity_check_pass);
  RUN_TEST(test_identity_check_anomaly);

  return UNITY_END();
}

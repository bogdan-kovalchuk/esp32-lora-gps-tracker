#include <string.h>
#include <unity.h>

#include "protocol.h"

using tracker::protocol::TelemetryPayload;
using tracker::protocol::WireHeader;

void setUp() {}

void tearDown() {}

void testCrc16KnownVector() {
  const char *input = "123456789";
  TEST_ASSERT_EQUAL_HEX16(
      0x29B1,
      tracker::protocol::crc16Ccitt(
          reinterpret_cast<const uint8_t *>(input), strlen(input)));
}

void testWireLayoutIsStable() {
  TEST_ASSERT_EQUAL_UINT32(24, sizeof(tracker::protocol::WireHeader));
  TEST_ASSERT_EQUAL_UINT32(32, sizeof(tracker::protocol::TelemetryPayload));
  TEST_ASSERT_EQUAL_UINT32(72, sizeof(tracker::protocol::WirePacket));
}

void testHeaderValidation() {
  WireHeader header = {};
  uint8_t nonce[tracker::protocol::NONCE_SIZE] = {};
  tracker::protocol::initializeHeader(header, 0x12345678UL, 42UL, nonce);

  TEST_ASSERT_TRUE(tracker::protocol::isSupportedHeader(header));
  header.version++;
  TEST_ASSERT_FALSE(tracker::protocol::isSupportedHeader(header));
}

void testTelemetryCrcAndRanges() {
  TelemetryPayload payload = {};
  payload.flags =
      tracker::protocol::HAS_FIX | tracker::protocol::HAS_COURSE;
  payload.latitudeE7 = 481608330;
  payload.longitudeE7 = 244994440;
  payload.courseCdeg = 35999;
  tracker::protocol::sealTelemetry(payload);

  TEST_ASSERT_TRUE(tracker::protocol::isTelemetryValid(payload));

  payload.longitudeE7 = 1800000001L;
  tracker::protocol::sealTelemetry(payload);
  TEST_ASSERT_FALSE(tracker::protocol::isTelemetryValid(payload));

  payload.longitudeE7 = 244994440;
  tracker::protocol::sealTelemetry(payload);
  payload.latitudeE7++;
  TEST_ASSERT_FALSE(tracker::protocol::isTelemetryValid(payload));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(testCrc16KnownVector);
  RUN_TEST(testWireLayoutIsStable);
  RUN_TEST(testHeaderValidation);
  RUN_TEST(testTelemetryCrcAndRanges);
  return UNITY_END();
}

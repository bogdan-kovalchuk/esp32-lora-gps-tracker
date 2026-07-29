#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace tracker {
namespace protocol {

constexpr uint8_t MAGIC_0 = 'L';
constexpr uint8_t MAGIC_1 = 'T';
constexpr uint8_t VERSION = 1;
constexpr uint8_t MESSAGE_TELEMETRY = 1;
constexpr size_t NONCE_SIZE = 12;
constexpr size_t SESSION_ID_SIZE = 8;
constexpr size_t TAG_SIZE = 16;

enum TelemetryFlag : uint8_t {
  HAS_FIX = 1U << 0,
  HAS_TIME = 1U << 1,
  HAS_DATE = 1U << 2,
  HAS_ALTITUDE = 1U << 3,
  HAS_SPEED = 1U << 4,
  HAS_COURSE = 1U << 5,
  HAS_HDOP = 1U << 6,
  HAS_SATELLITES = 1U << 7
};

#pragma pack(push, 1)
struct WireHeader {
  uint8_t magic[2];
  uint8_t version;
  uint8_t messageType;
  uint32_t deviceId;
  uint32_t sequence;
  uint8_t nonce[NONCE_SIZE];
};

struct TelemetryPayload {
  uint8_t flags;
  uint8_t satellites;
  int32_t latitudeE7;
  int32_t longitudeE7;
  int32_t altitudeMm;
  uint16_t speedCms;
  uint16_t courseCdeg;
  uint16_t hdopCenti;
  uint16_t locationAgeMs;
  uint32_t timeOfDaySeconds;
  uint32_t dateYyyymmdd;
  uint16_t crc16;
};

struct WirePacket {
  WireHeader header;
  uint8_t ciphertext[sizeof(TelemetryPayload)];
  uint8_t tag[TAG_SIZE];
};
#pragma pack(pop)

static_assert(sizeof(WireHeader) == 24, "WireHeader layout changed");
static_assert(sizeof(TelemetryPayload) == 32, "TelemetryPayload layout changed");
static_assert(sizeof(WirePacket) == 72, "WirePacket layout changed");

inline uint16_t crc16Ccitt(const uint8_t *data, size_t length) {
  uint16_t crc = 0xFFFF;
  for (size_t index = 0; index < length; ++index) {
    crc ^= static_cast<uint16_t>(data[index]) << 8;
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc & 0x8000U)
                ? static_cast<uint16_t>((crc << 1) ^ 0x1021U)
                : static_cast<uint16_t>(crc << 1);
    }
  }
  return crc;
}

inline void initializeHeader(WireHeader &header, uint32_t deviceId,
                             uint32_t sequence, const uint8_t *nonce) {
  header.magic[0] = MAGIC_0;
  header.magic[1] = MAGIC_1;
  header.version = VERSION;
  header.messageType = MESSAGE_TELEMETRY;
  header.deviceId = deviceId;
  header.sequence = sequence;
  memcpy(header.nonce, nonce, NONCE_SIZE);
}

inline bool isSupportedHeader(const WireHeader &header) {
  return header.magic[0] == MAGIC_0 && header.magic[1] == MAGIC_1 &&
         header.version == VERSION &&
         header.messageType == MESSAGE_TELEMETRY;
}

inline bool hasFlag(const TelemetryPayload &payload, TelemetryFlag flag) {
  return (payload.flags & static_cast<uint8_t>(flag)) != 0;
}

inline void sealTelemetry(TelemetryPayload &payload) {
  payload.crc16 =
      crc16Ccitt(reinterpret_cast<const uint8_t *>(&payload),
                 sizeof(payload) - sizeof(payload.crc16));
}

inline bool isTelemetryValid(const TelemetryPayload &payload) {
  const uint16_t expected =
      crc16Ccitt(reinterpret_cast<const uint8_t *>(&payload),
                 sizeof(payload) - sizeof(payload.crc16));
  if (payload.crc16 != expected) {
    return false;
  }

  if (hasFlag(payload, HAS_FIX) &&
      (payload.latitudeE7 < -900000000L ||
       payload.latitudeE7 > 900000000L ||
       payload.longitudeE7 < -1800000000L ||
       payload.longitudeE7 > 1800000000L)) {
    return false;
  }

  if (hasFlag(payload, HAS_COURSE) && payload.courseCdeg > 36000U) {
    return false;
  }

  if (hasFlag(payload, HAS_TIME) && payload.timeOfDaySeconds > 86399UL) {
    return false;
  }

  return true;
}

}  // namespace protocol
}  // namespace tracker

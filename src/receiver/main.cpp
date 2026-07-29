#include <Arduino.h>
#include <AES.h>
#include <GCM.h>
#include <LoRa.h>
#include <string.h>

#include "lora_radio.h"
#include "protocol.h"
#include "secrets.h"

namespace {

using tracker::protocol::TelemetryPayload;
using tracker::protocol::WireHeader;
using tracker::protocol::WirePacket;

constexpr size_t SESSION_CACHE_SIZE = 4;

struct SessionState {
  bool used;
  uint8_t id[tracker::protocol::SESSION_ID_SIZE];
  uint32_t lastSequence;
};

struct ReceiverCounters {
  uint32_t accepted;
  uint32_t malformed;
  uint32_t authenticationFailures;
  uint32_t replayed;
};

GCM<AES256> cipher;
SessionState sessions[SESSION_CACHE_SIZE] = {};
size_t nextSessionSlot = 0;
ReceiverCounters counters = {};

void halt(const char *message) {
  Serial.println(message);
  while (true) {
    delay(1000);
  }
}

void drainPacket() {
  while (LoRa.available() > 0) {
    LoRa.read();
  }
}

bool readPacket(WirePacket &packet) {
  uint8_t *destination = reinterpret_cast<uint8_t *>(&packet);
  size_t bytesRead = 0;

  while (LoRa.available() > 0 && bytesRead < sizeof(packet)) {
    destination[bytesRead++] = static_cast<uint8_t>(LoRa.read());
  }
  drainPacket();
  return bytesRead == sizeof(packet);
}

bool decryptPacket(const WirePacket &packet, TelemetryPayload &payload) {
  if (!cipher.setIV(packet.header.nonce, sizeof(packet.header.nonce))) {
    return false;
  }

  cipher.addAuthData(&packet.header, sizeof(packet.header));
  cipher.decrypt(reinterpret_cast<uint8_t *>(&payload), packet.ciphertext,
                 sizeof(payload));

  if (!cipher.checkTag(packet.tag, sizeof(packet.tag))) {
    memset(&payload, 0, sizeof(payload));
    return false;
  }
  return true;
}

bool isReplayAndRemember(const WireHeader &header) {
  for (size_t index = 0; index < SESSION_CACHE_SIZE; ++index) {
    SessionState &session = sessions[index];
    if (session.used &&
        memcmp(session.id, header.nonce,
               tracker::protocol::SESSION_ID_SIZE) == 0) {
      if (header.sequence <= session.lastSequence) {
        return true;
      }
      session.lastSequence = header.sequence;
      return false;
    }
  }

  SessionState &newSession = sessions[nextSessionSlot];
  newSession.used = true;
  memcpy(newSession.id, header.nonce, tracker::protocol::SESSION_ID_SIZE);
  newSession.lastSequence = header.sequence;
  nextSessionSlot = (nextSessionSlot + 1U) % SESSION_CACHE_SIZE;
  return false;
}

void printUtc(const TelemetryPayload &payload) {
  const bool hasDate =
      tracker::protocol::hasFlag(payload, tracker::protocol::HAS_DATE);
  const bool hasTime =
      tracker::protocol::hasFlag(payload, tracker::protocol::HAS_TIME);
  if (!hasDate && !hasTime) {
    return;
  }

  Serial.print(" utc=");
  if (hasDate) {
    const uint32_t year = payload.dateYyyymmdd / 10000UL;
    const uint32_t month = (payload.dateYyyymmdd / 100UL) % 100UL;
    const uint32_t day = payload.dateYyyymmdd % 100UL;
    Serial.printf("%04lu-%02lu-%02lu", static_cast<unsigned long>(year),
                  static_cast<unsigned long>(month),
                  static_cast<unsigned long>(day));
  } else {
    Serial.print("---- -- --");
  }

  Serial.print("T");
  if (hasTime) {
    const uint32_t hour = payload.timeOfDaySeconds / 3600UL;
    const uint32_t minute = (payload.timeOfDaySeconds / 60UL) % 60UL;
    const uint32_t second = payload.timeOfDaySeconds % 60UL;
    Serial.printf("%02lu:%02lu:%02luZ", static_cast<unsigned long>(hour),
                  static_cast<unsigned long>(minute),
                  static_cast<unsigned long>(second));
  } else {
    Serial.print("--:--:--Z");
  }
}

void printTelemetry(const WireHeader &header,
                    const TelemetryPayload &payload, int rssi, float snr) {
  Serial.printf("RX id=%08lX seq=%lu fix=%u",
                static_cast<unsigned long>(header.deviceId),
                static_cast<unsigned long>(header.sequence),
                tracker::protocol::hasFlag(payload, tracker::protocol::HAS_FIX)
                    ? 1U
                    : 0U);

  if (tracker::protocol::hasFlag(payload, tracker::protocol::HAS_FIX)) {
    Serial.printf(" lat=%.7f lon=%.7f age_ms=%u",
                  payload.latitudeE7 / 10000000.0,
                  payload.longitudeE7 / 10000000.0,
                  static_cast<unsigned>(payload.locationAgeMs));
  }

  if (tracker::protocol::hasFlag(payload,
                                 tracker::protocol::HAS_ALTITUDE)) {
    Serial.printf(" alt_m=%.3f", payload.altitudeMm / 1000.0);
  }

  if (tracker::protocol::hasFlag(payload, tracker::protocol::HAS_SPEED)) {
    Serial.printf(" speed_mps=%.2f", payload.speedCms / 100.0);
  }

  if (tracker::protocol::hasFlag(payload, tracker::protocol::HAS_COURSE)) {
    Serial.printf(" course_deg=%.2f", payload.courseCdeg / 100.0);
  }

  if (tracker::protocol::hasFlag(payload,
                                 tracker::protocol::HAS_SATELLITES)) {
    Serial.printf(" sats=%u", static_cast<unsigned>(payload.satellites));
  }

  if (tracker::protocol::hasFlag(payload, tracker::protocol::HAS_HDOP)) {
    Serial.printf(" hdop=%.2f", payload.hdopCenti / 100.0);
  }

  printUtc(payload);
  Serial.printf(" rssi=%d snr=%.2f\n", rssi, snr);
}

void printCountersPeriodically() {
  const uint32_t now = millis();
  static uint32_t lastPrintMs = 0;
  if (now - lastPrintMs < 30000UL) {
    return;
  }
  lastPrintMs = now;

  Serial.printf(
      "STATS accepted=%lu malformed=%lu auth_failed=%lu replayed=%lu\n",
      static_cast<unsigned long>(counters.accepted),
      static_cast<unsigned long>(counters.malformed),
      static_cast<unsigned long>(counters.authenticationFailures),
      static_cast<unsigned long>(counters.replayed));
}

void processIncomingPacket(int packetSize) {
  const int rssi = LoRa.packetRssi();
  const float snr = LoRa.packetSnr();

  if (packetSize != static_cast<int>(sizeof(WirePacket))) {
    drainPacket();
    ++counters.malformed;
    Serial.printf("RX rejected=length expected=%u actual=%d rssi=%d\n",
                  static_cast<unsigned>(sizeof(WirePacket)), packetSize, rssi);
    return;
  }

  WirePacket packet = {};
  if (!readPacket(packet) ||
      !tracker::protocol::isSupportedHeader(packet.header)) {
    ++counters.malformed;
    Serial.println("RX rejected=header");
    return;
  }

  if (packet.header.deviceId != tracker::secrets::DEVICE_ID) {
    ++counters.malformed;
    Serial.printf("RX rejected=device id=%08lX\n",
                  static_cast<unsigned long>(packet.header.deviceId));
    return;
  }

  TelemetryPayload payload = {};
  if (!decryptPacket(packet, payload)) {
    ++counters.authenticationFailures;
    Serial.printf("RX rejected=authentication seq=%lu rssi=%d\n",
                  static_cast<unsigned long>(packet.header.sequence), rssi);
    return;
  }

  if (!tracker::protocol::isTelemetryValid(payload)) {
    ++counters.malformed;
    Serial.printf("RX rejected=payload seq=%lu\n",
                  static_cast<unsigned long>(packet.header.sequence));
    return;
  }

  if (isReplayAndRemember(packet.header)) {
    ++counters.replayed;
    Serial.printf("RX rejected=replay seq=%lu\n",
                  static_cast<unsigned long>(packet.header.sequence));
    return;
  }

  ++counters.accepted;
  printTelemetry(packet.header, payload, rssi, snr);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);

  tracker::disableUnusedRadios();
  if (!tracker::beginLoRa()) {
    halt("Fatal: SX1278 initialization failed");
  }

  if (!cipher.setKey(tracker::secrets::AES_KEY,
                     sizeof(tracker::secrets::AES_KEY))) {
    halt("Fatal: AES-256 key initialization failed");
  }

  Serial.printf("LoRa GPS receiver ready: device=%08lX packet=%u bytes\n",
                static_cast<unsigned long>(tracker::secrets::DEVICE_ID),
                static_cast<unsigned>(sizeof(WirePacket)));
}

void loop() {
  const int packetSize = LoRa.parsePacket();
  if (packetSize > 0) {
    processIncomingPacket(packetSize);
  }

  printCountersPeriodically();
  delay(2);
}

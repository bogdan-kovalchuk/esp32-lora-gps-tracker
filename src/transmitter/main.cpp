#include <Arduino.h>
#include <AES.h>
#include <GCM.h>
#include <HardwareSerial.h>
#include <LoRa.h>
#include <TinyGPS++.h>
#include <esp_system.h>
#include <limits.h>
#include <math.h>
#include <string.h>

#include "app_config.h"
#include "lora_radio.h"
#include "protocol.h"
#include "secrets.h"

namespace {

using tracker::protocol::TelemetryPayload;
using tracker::protocol::WirePacket;

TinyGPSPlus gps;
HardwareSerial gpsSerial(1);
GCM<AES256> cipher;

uint8_t sessionId[tracker::protocol::SESSION_ID_SIZE] = {};
uint32_t sequenceCounter = 0;

void halt(const char *message) {
  Serial.println(message);
  while (true) {
    delay(1000);
  }
}

void fillRandom(uint8_t *output, size_t length) {
  size_t index = 0;
  while (index < length) {
    uint32_t randomValue = esp_random();
    for (uint8_t byteIndex = 0;
         byteIndex < sizeof(randomValue) && index < length;
         ++byteIndex, ++index) {
      output[index] = static_cast<uint8_t>(randomValue & 0xFFU);
      randomValue >>= 8;
    }
  }
}

void makeNonce(uint32_t sequence, uint8_t *nonce) {
  memcpy(nonce, sessionId, sizeof(sessionId));
  for (uint8_t index = 0; index < 4; ++index) {
    nonce[sizeof(sessionId) + index] =
        static_cast<uint8_t>((sequence >> (index * 8U)) & 0xFFU);
  }
}

int32_t encodeSigned(double value, double multiplier) {
  const double scaled = value * multiplier;
  if (scaled >= static_cast<double>(INT32_MAX)) {
    return INT32_MAX;
  }
  if (scaled <= static_cast<double>(INT32_MIN)) {
    return INT32_MIN;
  }
  return static_cast<int32_t>(llround(scaled));
}

uint16_t encodeUnsigned(double value, double multiplier) {
  if (value <= 0.0) {
    return 0;
  }

  const double scaled = value * multiplier;
  if (scaled >= 65534.0) {
    return 0xFFFEU;
  }
  return static_cast<uint16_t>(lround(scaled));
}

uint16_t clampUnsigned16(uint32_t value) {
  return value >= 0xFFFEUL ? 0xFFFEU : static_cast<uint16_t>(value);
}

uint8_t clampSatellites(uint32_t value) {
  return value >= 0xFEUL ? 0xFEU : static_cast<uint8_t>(value);
}

TelemetryPayload makeTelemetry() {
  TelemetryPayload payload = {};
  payload.satellites = 0xFFU;
  payload.altitudeMm = INT32_MIN;
  payload.speedCms = 0xFFFFU;
  payload.courseCdeg = 0xFFFFU;
  payload.hdopCenti = 0xFFFFU;
  payload.locationAgeMs = 0xFFFFU;
  payload.timeOfDaySeconds = 0xFFFFFFFFUL;

  if (tracker::config::USE_DEMO_POSITION) {
    payload.flags = tracker::protocol::HAS_FIX;
    payload.latitudeE7 =
        encodeSigned(tracker::config::DEMO_LATITUDE, 10000000.0);
    payload.longitudeE7 =
        encodeSigned(tracker::config::DEMO_LONGITUDE, 10000000.0);
    payload.locationAgeMs = 0;
    tracker::protocol::sealTelemetry(payload);
    return payload;
  }

  const bool hasFix =
      gps.location.isValid() &&
      gps.location.age() <= tracker::config::MAX_FIX_AGE_MS;
  if (hasFix) {
    payload.flags |= tracker::protocol::HAS_FIX;
    payload.latitudeE7 = encodeSigned(gps.location.lat(), 10000000.0);
    payload.longitudeE7 = encodeSigned(gps.location.lng(), 10000000.0);
    payload.locationAgeMs =
        clampUnsigned16(static_cast<uint32_t>(gps.location.age()));
  }

  if (gps.altitude.isValid()) {
    payload.flags |= tracker::protocol::HAS_ALTITUDE;
    payload.altitudeMm = encodeSigned(gps.altitude.meters(), 1000.0);
  }

  if (gps.speed.isValid()) {
    payload.flags |= tracker::protocol::HAS_SPEED;
    payload.speedCms = encodeUnsigned(gps.speed.mps(), 100.0);
  }

  if (gps.course.isValid()) {
    payload.flags |= tracker::protocol::HAS_COURSE;
    payload.courseCdeg = encodeUnsigned(gps.course.deg(), 100.0);
  }

  if (gps.hdop.isValid()) {
    payload.flags |= tracker::protocol::HAS_HDOP;
    payload.hdopCenti = clampUnsigned16(gps.hdop.value());
  }

  if (gps.satellites.isValid()) {
    payload.flags |= tracker::protocol::HAS_SATELLITES;
    payload.satellites = clampSatellites(gps.satellites.value());
  }

  if (gps.time.isValid()) {
    payload.flags |= tracker::protocol::HAS_TIME;
    payload.timeOfDaySeconds =
        static_cast<uint32_t>(gps.time.hour()) * 3600UL +
        static_cast<uint32_t>(gps.time.minute()) * 60UL +
        static_cast<uint32_t>(gps.time.second());
  }

  if (gps.date.isValid()) {
    payload.flags |= tracker::protocol::HAS_DATE;
    payload.dateYyyymmdd =
        static_cast<uint32_t>(gps.date.year()) * 10000UL +
        static_cast<uint32_t>(gps.date.month()) * 100UL +
        static_cast<uint32_t>(gps.date.day());
  }

  tracker::protocol::sealTelemetry(payload);
  return payload;
}

bool encryptPacket(const TelemetryPayload &payload, uint32_t sequence,
                   WirePacket &packet) {
  uint8_t nonce[tracker::protocol::NONCE_SIZE];
  makeNonce(sequence, nonce);
  tracker::protocol::initializeHeader(packet.header, tracker::secrets::DEVICE_ID,
                                      sequence, nonce);

  if (!cipher.setIV(packet.header.nonce, sizeof(packet.header.nonce))) {
    return false;
  }

  cipher.addAuthData(&packet.header, sizeof(packet.header));
  cipher.encrypt(packet.ciphertext,
                 reinterpret_cast<const uint8_t *>(&payload),
                 sizeof(payload));
  cipher.computeTag(packet.tag, sizeof(packet.tag));
  return true;
}

void printTransmission(const TelemetryPayload &payload, uint32_t sequence,
                       bool sent) {
  Serial.printf("TX seq=%lu status=%s fix=%u",
                static_cast<unsigned long>(sequence), sent ? "sent" : "failed",
                tracker::protocol::hasFlag(payload, tracker::protocol::HAS_FIX)
                    ? 1U
                    : 0U);

  if (tracker::protocol::hasFlag(payload, tracker::protocol::HAS_FIX)) {
    Serial.printf(" lat=%.7f lon=%.7f",
                  payload.latitudeE7 / 10000000.0,
                  payload.longitudeE7 / 10000000.0);
  }

  if (tracker::protocol::hasFlag(payload,
                                 tracker::protocol::HAS_SATELLITES)) {
    Serial.printf(" sats=%u", static_cast<unsigned>(payload.satellites));
  }

  Serial.println();
}

void sendTelemetry() {
  const TelemetryPayload payload = makeTelemetry();
  const uint32_t sequence = sequenceCounter++;
  WirePacket packet = {};

  if (!encryptPacket(payload, sequence, packet)) {
    Serial.println("TX error=AES-GCM-IV");
    return;
  }

  LoRa.beginPacket();
  const size_t bytesWritten =
      LoRa.write(reinterpret_cast<const uint8_t *>(&packet), sizeof(packet));
  const bool sent = bytesWritten == sizeof(packet) && LoRa.endPacket() == 1;
  printTransmission(payload, sequence, sent);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);

  tracker::disableUnusedRadios();
  gpsSerial.begin(tracker::config::GPS_BAUD_RATE, SERIAL_8N1,
                  tracker::config::GPS_RX_PIN, tracker::config::GPS_TX_PIN);

  if (!tracker::beginLoRa()) {
    halt("Fatal: SX1278 initialization failed");
  }

  if (!cipher.setKey(tracker::secrets::AES_KEY,
                     sizeof(tracker::secrets::AES_KEY))) {
    halt("Fatal: AES-256 key initialization failed");
  }

  fillRandom(sessionId, sizeof(sessionId));
  Serial.printf("LoRa GPS transmitter ready: device=%08lX packet=%u bytes\n",
                static_cast<unsigned long>(tracker::secrets::DEVICE_ID),
                static_cast<unsigned>(sizeof(WirePacket)));
}

void loop() {
  while (gpsSerial.available() > 0) {
    gps.encode(static_cast<char>(gpsSerial.read()));
  }

  const uint32_t now = millis();
  static uint32_t lastTransmissionMs = 0;
  if (now - lastTransmissionMs >= tracker::config::TRANSMIT_INTERVAL_MS) {
    lastTransmissionMs = now;
    sendTelemetry();
  }

  static uint32_t lastGpsWarningMs = 0;
  if (!tracker::config::USE_DEMO_POSITION && now >= 5000UL &&
      gps.charsProcessed() < 10 &&
      now - lastGpsWarningMs >= 5000UL) {
    lastGpsWarningMs = now;
    Serial.println("GPS warning: no NMEA data; check power, baud rate and RX pin");
  }

  delay(2);
}

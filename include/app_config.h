#pragma once

#include <stdint.h>

namespace tracker {
namespace config {

// ESP32 DevKit V1 <-> SX1278
constexpr int LORA_CS_PIN = 5;
constexpr int LORA_RESET_PIN = 14;
constexpr int LORA_IRQ_PIN = 2;

// ESP32 UART1 <-> GPS
constexpr int GPS_RX_PIN = 16;
constexpr int GPS_TX_PIN = 17;
constexpr uint32_t GPS_BAUD_RATE = 9600;

// Both devices must use exactly the same LoRa modem settings.
constexpr long LORA_FREQUENCY_HZ = 433000000L;
constexpr uint8_t LORA_SYNC_WORD = 0x34;
constexpr uint8_t LORA_SPREADING_FACTOR = 9;
constexpr long LORA_SIGNAL_BANDWIDTH_HZ = 125000L;
constexpr uint8_t LORA_CODING_RATE_DENOMINATOR = 5;
constexpr long LORA_PREAMBLE_LENGTH = 8;
constexpr int8_t LORA_TX_POWER_DBM = 17;

constexpr uint32_t TRANSMIT_INTERVAL_MS = 1000;
constexpr uint32_t MAX_FIX_AGE_MS = 2000;

// Set to true to test the complete radio path without a GPS module.
// The demo position is Hoverla, Ukraine.
constexpr bool USE_DEMO_POSITION = false;
constexpr double DEMO_LATITUDE = 48.160833;
constexpr double DEMO_LONGITUDE = 24.499444;

}  // namespace config
}  // namespace tracker

#pragma once

#include <Arduino.h>
#include <LoRa.h>
#include <WiFi.h>
#include <esp_bt.h>

#include "app_config.h"

namespace tracker {

inline void disableUnusedRadios() {
  WiFi.mode(WIFI_OFF);
  WiFi.disconnect(true);

  if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED) {
    esp_bt_controller_disable();
  }
}

inline bool beginLoRa() {
  LoRa.setPins(config::LORA_CS_PIN, config::LORA_RESET_PIN,
               config::LORA_IRQ_PIN);
  if (!LoRa.begin(config::LORA_FREQUENCY_HZ)) {
    return false;
  }

  LoRa.setTxPower(config::LORA_TX_POWER_DBM);
  LoRa.setSpreadingFactor(config::LORA_SPREADING_FACTOR);
  LoRa.setSignalBandwidth(config::LORA_SIGNAL_BANDWIDTH_HZ);
  LoRa.setCodingRate4(config::LORA_CODING_RATE_DENOMINATOR);
  LoRa.setPreambleLength(config::LORA_PREAMBLE_LENGTH);
  LoRa.setSyncWord(config::LORA_SYNC_WORD);
  LoRa.enableCrc();
  return true;
}

}  // namespace tracker

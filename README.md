# ESP32 LoRa GPS Tracker

Encrypted point-to-point GPS telemetry over 433 MHz LoRa between two ESP32 boards.

## Prerequisites

- [PlatformIO CLI](https://docs.platformio.org/en/latest/core/installation.html) or VS Code with the PlatformIO extension
- GCC toolchain (only required to run native protocol tests)

## Features

- Separate transmitter and receiver firmware
- Position, altitude, speed, course, satellites, HDOP, UTC date/time
- AES-256-GCM authenticated encryption
- Versioned 72-byte binary packets with CRC16 and replay detection
- RSSI / SNR reporting on the receiver
- Demo mode for radio testing without a GPS module

## Hardware

| Component | Qty | Notes |
|---|---|---|
| ESP32 DevKit V1 | 2 | |
| SX1278 433 MHz + antenna | 2 | SPI, 3.3 V |
| NEO-6M or compatible UART GPS | 1 | NMEA 9600 baud |

### SX1278 wiring

| SX1278 | ESP32 |
|---|---:|
| VCC | 3.3 V |
| GND | GND |
| SCK | GPIO 18 |
| MISO | GPIO 19 |
| MOSI | GPIO 23 |
| NSS / CS | GPIO 5 |
| RESET | GPIO 14 |
| DIO0 | GPIO 2 |

### GPS wiring

| GPS | ESP32 transmitter |
|---|---:|
| TX | GPIO 16 (RX1) |
| RX | GPIO 17 (TX1, optional) |
| GND | GND |

## Project structure

```
├── include/
│   ├── app_config.h          pins, LoRa settings, demo mode
│   ├── lora_radio.h          SX1278 init helper
│   ├── protocol.h            packet layout, CRC16, validation
│   └── secrets.example.h     credential template
├── src/
│   ├── transmitter/main.cpp  GPS read → encrypt → send
│   └── receiver/main.cpp     receive → decrypt → print
├── test/
│   └── test_protocol/        native unit tests
├── docs/protocol.md          wire format reference
└── platformio.ini            build environments
```

## Setup

1. Copy the credential template:
   ```powershell
   Copy-Item include/secrets.example.h include/secrets.h
   ```
2. Set `DEVICE_ID` and `AES_KEY` in `include/secrets.h` — both devices must match.
3. Adjust pins, LoRa settings, or demo mode in `include/app_config.h`.

## Build and upload

```powershell
pio run                                              # build both targets
pio test -e native                                   # protocol tests
pio run -e transmitter -t upload                     # flash transmitter
pio run -e receiver -t upload                        # flash receiver
pio device monitor -b 115200                         # serial monitor
```

## Security notes

- Header is AES-GCM AAD; payload + CRC16 are encrypted.
- Nonce = random session ID (8 B) + sequence counter (4 B) → replay detection.
- Replay state is in-memory only; it resets on receiver restart.
- Never publish `secrets.h` or firmware built with production keys.
- Check your region's frequency, power, and duty-cycle limits.

Full packet layout: [docs/protocol.md](docs/protocol.md)

## License

[MIT](LICENSE)

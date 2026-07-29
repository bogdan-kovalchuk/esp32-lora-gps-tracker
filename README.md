# ESP32 LoRa GPS Tracker

Encrypted point-to-point GPS telemetry between two ESP32 boards and 433 MHz
SX1278 LoRa modules.

## Features

- Separate transmitter and receiver firmware.
- Position, altitude, speed, course, satellites, HDOP, UTC date, and time.
- AES-256-GCM authenticated encryption.
- Versioned 72-byte binary packets with CRC16 and replay detection.
- RSSI and SNR reporting on the receiver.
- Demo mode for radio testing without a GPS module.

## Hardware

- 2 x ESP32 DevKit V1
- 2 x 433 MHz SX1278 modules with antennas
- 1 x UART NMEA GPS module, such as the NEO-6M

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

The SX1278 uses 3.3 V. Attach an antenna before transmitting.

## Setup

Create the local credentials file:

```powershell
Copy-Item include/secrets.example.h include/secrets.h
```

Replace `DEVICE_ID` and `AES_KEY` in `include/secrets.h`. Both devices must use
the same values. The file is excluded from Git.

Hardware pins, LoRa settings, and demo mode are configured in
`include/app_config.h`. Radio settings must match on both devices.

## Build and upload

```powershell
# Build both firmware targets
pio run

# Run protocol tests
pio test -e native

# Upload one target at a time
pio run -e transmitter -t upload
pio run -e receiver -t upload

# Open the serial monitor
pio device monitor -b 115200
```

Set `USE_DEMO_POSITION` to `true` in `include/app_config.h` to transmit a fixed
test position without a GPS module.

## Protocol and security

The header is authenticated as AES-GCM additional data. The encrypted payload
contains telemetry and CRC16-CCITT. A random session ID and sequence counter
form the GCM nonce and provide in-memory replay detection.

Replay state does not survive a receiver restart. Never publish
`include/secrets.h` or firmware built with production credentials. Check the
permitted frequency, power, and duty cycle for your region.

See [docs/protocol.md](docs/protocol.md) for the packet layout.

## License

[MIT](LICENSE)

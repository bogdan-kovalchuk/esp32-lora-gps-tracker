# ESP32 LoRa GPS Tracker

A compact PlatformIO project for securely transmitting GPS telemetry between
two ESP32 boards through 433 MHz LoRa SX1278 modules.

The project provides two independent firmware targets:

- `transmitter` reads NMEA data from a GPS module, builds telemetry, and sends
  it over LoRa;
- `receiver` authenticates, decrypts, validates, and prints the telemetry
  together with RSSI and SNR.

## Features

- Latitude, longitude, altitude, speed, course, satellite count, HDOP, UTC
  date, and UTC time.
- Status packets are transmitted even when a valid GPS fix is unavailable.
- Built-in Hoverla demo coordinates for testing without a GPS module.
- AES-256-GCM encryption and authentication for the payload and header.
- Random session ID, packet sequence counter, and in-memory replay protection.
- CRC16-CCITT inside the encrypted telemetry and LoRa hardware CRC.
- Fixed-size, versioned binary protocol.
- Separate PlatformIO environments and native protocol unit tests.
- GitHub Actions builds both firmware targets and runs protocol tests on every
  push and pull request.

## Hardware

- 2 × ESP32 DevKit V1 boards.
- 2 × 433 MHz SX1278 modules with suitable antennas.
- 1 × UART NMEA GPS module, such as the NEO-6M.

### SX1278 to ESP32 wiring

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

Power the SX1278 from 3.3 V only. Never operate a transmitter without an
antenna.

### GPS to transmitter wiring

| GPS | ESP32 |
|---|---:|
| TX | GPIO 16 (RX1) |
| RX | GPIO 17 (TX1, optional) |
| GND | GND |

GPS power requirements depend on the specific breakout board. Its UART logic
level must be safe for the ESP32 3.3 V inputs.

## Configuration

1. Create the local credentials file:

   ```powershell
   Copy-Item include/secrets.example.h include/secrets.h
   ```

2. Replace these values in `include/secrets.h`:

   - `DEVICE_ID`: the identifier shared by the device pair;
   - `AES_KEY`: a randomly generated 32-byte key.

3. Adjust the pins and LoRa modem settings in `include/app_config.h` if
   necessary. The transmitter and receiver must use identical LoRa settings.

`include/secrets.h` is excluded from Git. The values in
`secrets.example.h` exist only for CI and local testing and are not secure
deployment credentials.

## Build

Build both firmware targets:

```powershell
pio run
```

Build each target separately:

```powershell
pio run -e transmitter
pio run -e receiver
```

Run the protocol unit tests:

```powershell
pio test -e native
```

## Upload

Connect the appropriate ESP32 board and run:

```powershell
pio run -e transmitter -t upload
pio run -e receiver -t upload
```

The serial monitor uses 115200 baud:

```powershell
pio device monitor -b 115200
```

Example receiver output:

```text
RX id=4C475031 seq=42 fix=1 lat=48.1608330 lon=24.4994440 age_ms=220 alt_m=2061.300 speed_mps=0.12 course_deg=15.40 sats=10 hdop=0.92 utc=2026-07-29T12:45:10Z rssi=-87 snr=7.25
```

## Test without a GPS module

Set `USE_DEMO_POSITION = true` in `include/app_config.h`. The transmitter will
send the coordinates of Hoverla once per second. Restore the value to `false`
before using a real GPS module.

## Project structure

```text
include/
  app_config.h       pins, interval, and shared LoRa settings
  lora_radio.h       shared SX1278 initialization
  protocol.h         wire format, flags, validation, and CRC
  secrets.example.h  template for local credentials
src/
  transmitter/       GPS -> AES-GCM -> LoRa
  receiver/          LoRa -> validation -> AES-GCM -> Serial
test/
  test_protocol/     wire format and CRC unit tests
docs/
  protocol.md        packet format specification
```

## Security and compatibility notes

- This protocol is not compatible with the legacy 16-byte AES-128/ECB
  implementation from `lora-track-gps`.
- A GCM nonce must never be reused with the same key. At startup, the
  transmitter generates a random 64-bit session ID and appends a 32-bit
  sequence counter.
- Replay state is stored only in receiver RAM and does not survive a receiver
  restart.
- Keep `include/secrets.h`, `.pio/`, firmware binaries, and local editor
  metadata out of public archives.
- Verify the permitted frequency, transmit power, and duty cycle for your
  region.

The complete packet format is documented in
[docs/protocol.md](docs/protocol.md).

## License

This project is available under the [MIT License](LICENSE). You may use, copy,
modify, publish, distribute, sublicense, and sell the software, provided that
the copyright notice and license text remain included.

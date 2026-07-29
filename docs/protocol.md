# LoRa GPS wire protocol v1

The protocol is designed for an ESP32 and SX1278 device pair. Each telemetry
message is a fixed-size 72-byte packet. All multi-byte numeric fields use
little-endian byte order.

## Wire packet

| Offset | Size | Field | Description |
|---:|---:|---|---|
| 0 | 2 | Magic | ASCII `LT` |
| 2 | 1 | Version | `1` |
| 3 | 1 | Message type | `1` for telemetry |
| 4 | 4 | Device ID | Identifier shared by the device pair |
| 8 | 4 | Sequence | Monotonic packet number within a session |
| 12 | 12 | Nonce | 8-byte session ID followed by 4-byte sequence |
| 24 | 32 | Ciphertext | Encrypted `TelemetryPayload` |
| 56 | 16 | GCM tag | 128-bit authentication tag |

The complete 24-byte header is sent in plaintext but included as AES-GCM
additional authenticated data. Changing the magic, version, device ID,
sequence, or nonce invalidates the authentication tag.

## Decrypted TelemetryPayload

| Offset | Size | Field | Scale or sentinel |
|---:|---:|---|---|
| 0 | 1 | Flags | Field-validity bit mask |
| 1 | 1 | Satellites | Count; `0xFF` means unavailable |
| 2 | 4 | Latitude | Degrees × 10⁷ |
| 6 | 4 | Longitude | Degrees × 10⁷ |
| 10 | 4 | Altitude | Millimeters; `INT32_MIN` means unavailable |
| 14 | 2 | Speed | Centimeters per second; `0xFFFF` means unavailable |
| 16 | 2 | Course | Degrees × 100; `0xFFFF` means unavailable |
| 18 | 2 | HDOP | HDOP × 100; `0xFFFF` means unavailable |
| 20 | 2 | Location age | Milliseconds; `0xFFFF` means unavailable |
| 22 | 4 | UTC time | Seconds after midnight; `UINT32_MAX` means unavailable |
| 26 | 4 | UTC date | `YYYYMMDD`; `0` means unavailable |
| 30 | 2 | CRC16 | CRC16-CCITT over bytes 0 through 29 |

### Flags

| Bit | Mask | Meaning |
|---:|---:|---|
| 0 | `0x01` | Location fix |
| 1 | `0x02` | UTC time |
| 2 | `0x04` | UTC date |
| 3 | `0x08` | Altitude |
| 4 | `0x10` | Speed |
| 5 | `0x20` | Course |
| 6 | `0x40` | HDOP |
| 7 | `0x80` | Satellite count |

## Receiver validation order

The receiver processes a packet in this order:

1. Validate the length, magic, protocol version, message type, and device ID.
2. Verify the AES-GCM authentication tag.
3. Verify CRC16 and telemetry value ranges.
4. Reject a sequence number already seen for the same session ID.
5. Use and print the plaintext only after all checks pass.

Increment `VERSION` whenever a field layout or cryptographic scheme changes in
a backward-incompatible way.

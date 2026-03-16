# ESP32-S3-Touch-AMOLED-1.75C Board Support

This folder contains board-specific configuration for the Waveshare ESP32-S3-Touch-AMOLED-1.75C development board.

## Board Specifications

| Feature | Specification |
|---------|---------------|
| MCU | ESP32-S3R8 (Dual-core Xtensa LX7 @ 240MHz) |
| Flash | 8MB QSPI Flash |
| PSRAM | 8MB Octal PSRAM |
| Display | 1.75" Round AMOLED, 466x466 pixels |
| Display Driver | SH8601 (QSPI interface) |
| Touch | CST816D (I2C) |
| Battery | 2-pin battery connector (3.7V LiPo) |
| Charging | USB Type-C charging |

## Pinout

### Display (SH8601 - QSPI)

| Pin | Function | Description |
|-----|----------|-------------|
| 47  | SCK      | SPI Clock |
| 48  | MOSI/D0  | SPI Data Out |
| 41  | MISO/D1  | SPI Data In (QSPI D1) |
| 40  | DC       | Data/Command |
| 38  | CS       | Chip Select |
| 39  | RST      | Reset |
| 9   | TE       | Tearing Effect |

### Touch (CST816D - I2C)

| Pin | Function | Description |
|-----|----------|-------------|
| 8   | SDA      | I2C Data |
| 7   | SCL      | I2C Clock |
| 4   | INT      | Interrupt |
| 5   | RST      | Reset |

### Audio (I2S)

| Pin | Function | Description |
|-----|----------|-------------|
| 16  | BCLK     | Bit Clock |
| 15  | WS       | Word Select |
| 17  | DOUT     | Data Output |

### Microphone (I2S)

| Pin | Function | Description |
|-----|----------|-------------|
| 10  | BCLK     | Bit Clock |
| 11  | WS       | Word Select |
| 12  | DIN      | Data Input |

### Other

| Pin | Function | Description |
|-----|----------|-------------|
| 13  | BOOT     | Boot button |
| 14  | USER     | User button |

## Display Configuration

The SH8601 AMOLED controller is configured for:
- Resolution: 466x466 pixels (round)
- Color depth: 16-bit RGB565
- Interface: QSPI @ 40MHz
- Frame buffer: ~434KB (allocated in PSRAM)

## Building for this Board

```bash
# Set target
idf.py set-target esp32s3

# Build
idf.py build

# Flash
idf.py -p /dev/ttyUSB0 flash

# Monitor
idf.py -p /dev/ttyUSB0 monitor
```

## Important Notes

1. **Display Buffer**: The 466x466 RGB565 display requires ~434KB of RAM. This is allocated from PSRAM.

2. **QSPI Mode**: The display uses QSPI (Quad SPI) mode for faster refresh rates.

3. **Touch Controller**: The CST816D touch controller uses I2C at 400kHz.

4. **Power Management**: The board supports battery operation. Display brightness can be adjusted to save power.

5. **Round Display**: The display is circular. Drawing functions should account for the round shape.

## References

- [Waveshare Wiki](https://www.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.75)
- [ESP32-S3 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf)
- [SH8601 Datasheet](https://www.waveshare.com/w/upload/5/5e/SH8601A.pdf)

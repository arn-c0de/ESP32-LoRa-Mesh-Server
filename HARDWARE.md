# Hardware — Pin Layout

> Scope: Hardware layout and pin assignment. Firmware is documented separately.

## Overview

- **Target Platform**: ESP32 LoRa32 (e.g., TTGO LoRa32 V2.1)
- **Display**: SSD1306 OLED (I2C)
- **Button**: GPIO 33 (Input, use internal pull-up)
- **Antenna**: U.FL (or integrated spring antenna) — always connect before TX
- **Default Frequency**: 868 MHz (EU); configurable in firmware

---

## Pin Assignment

| Function        | GPIO  | Notes |
|-----------------|:-----:|:---------|
| LoRa NSS / CS   | 5     | SPI Chip Select
| LoRa RST        | 14    | LoRa module reset
| LoRa DIO0       | 26    | RX/TX IRQ (RX done)
| LoRa SCK        | 18    | SPI Clock
| LoRa MISO       | 19    | SPI MISO
| LoRa MOSI       | 23    | SPI MOSI
| OLED SDA (I2C)  | 21    | SSD1306 SDA (I2C)
| OLED SCL (I2C)  | 22    | SSD1306 SCL (I2C)
| OLED_RESET      | -1    | Reset shared / not exposed on all boards — check hardware
| Button (PAIR)   | 33    | Input; use INPUT_PULLUP
| LED (onboard)   | 2     | Status LED (optional)
| VBAT_MON (ADC)  | 35    | ADC pin for battery monitoring (optional)
| USB Serial TX/RX| 1, 3  | Programming / Serial

---

## Recommendations & Notes

- **Button**: Hardware documented (GPIO33, Pull-up). Button behavior is defined in firmware.
- **OLED I2C Address**: Default is **0x3C** — if display does not respond, check 0x3D.
- **Antenna**: Never transmit without an antenna connected (risk of PA damage).
- **Frequency/Board**: Ensure board settings and antenna match your region (868 MHz EU / 915 MHz US).

---

## Boot-Critical Pins & Limitations

- **Use with care**: GPIO 0, 2, 12, 15 (affect boot configuration)
- **Input-only**: GPIO 34, 35, 36, 39 (no pull-ups/downs, no PWM)
- **Do NOT use**: GPIO 6–11 (Flash/SD pins) — risk of data loss

---

## Quick Hardware Tests

1. Power on → verify LED / USB serial communication (115200 baud).
2. I2C: Run I2C scanner → OLED visible at 0x3C.
3. LoRa: Run firmware test → expect "LoRa init success" and frequency check.
4. Button: Check for debounce; GPIO state when pressed = LOW (INPUT_PULLUP).

---

## Change Log

- This file is the final hardware pin specification. All firmware changes and button logic are documented in separate commits.

---

For questions or if you need an alternative pin layout, contact: arn-c0de@protonmail.com
- ✅ Charge at correct rate (1C maximum)
- ✅ Store at 3.7-3.8V for long term
- ❌ Don't over-discharge below 3.0V
- ❌ Don't puncture or short circuit
- ❌ Don't expose to extreme temperatures

### Legal Compliance

**868 MHz (Europe)**:
- ISM Band: 863-870 MHz
- Max power: 14 dBm ERP (25 mW)
- Duty cycle: <1% in some sub-bands
- License-free (check local regulations)

**915 MHz (USA)**:
- ISM Band: 902-928 MHz
- Max power: 30 dBm (1W)
- Frequency hopping may be required
- Part 15 rules apply

**433 MHz (Asia/Europe)**:
- ISM Band: 433-435 MHz
- Max power: 10 mW ERP (Europe)
- Check local regulations

**⚠️ IMPORTANT**: Verify regulations in your country before operation!

## Bill of Materials (BOM)

### Minimal Setup

| Component | Quantity | Est. Price |
|-----------|----------|------------|
| ESP32-WROOM (module) | 1 | €7-10 |
| LoRa module (SX127x) | 1 | €15 |
| Tactile Button (if not included) | 1 | €0.50 |
| USB Cable (Micro or Type-C) | 1 | €2-5 |
| **Total (per device)** | | **€25-31** |

### Complete Setup

| Component | Quantity | Est. Price |
|-----------|----------|------------|
| ESP32-WROOM (module) | 1 | €7-10 |
| LoRa module (SX127x) | 1 | €15 |
| External LoRa Antenna (868 MHz, 3dBi) | 1 | €5-10 |
| LiPo Battery (3.7V 2000mAh) | 1 | €8-12 |
| Plastic Enclosure | 1 | €3-5 |
| Tactile Button | 1 | €0.50 |
| Dupont Wires (if needed) | 10 | €2 |
| USB Cable | 1 | €2-5 |
| **Total (per device)** | | **€43-60** |

### Premium Setup (Maximum Range)

| Component | Quantity | Est. Price |
|-----------|----------|------------|
| ESP32-WROOM (module) | 1 | €7-10 |
| LoRa module (SX127x) | 1 | €15 |
| High-gain Antenna (868 MHz, 8-12 dBi) | 1 | €15-30 |
| IP65 Weatherproof Enclosure | 1 | €10-15 |
| 18650 Battery (3000mAh) | 2 | €12-20 |
| 18650 Battery Holder (2S) | 1 | €3-5 |
| Solar Panel (5V 5W) | 1 | €10-15 |
| TP4056 Charging Module | 1 | €2-3 |
| Antenna Extension Cable | 1 | €5-8 |
| Mounting Hardware | - | €5-10 |
| **Total (per device)** | | **€84-131** |


> **Preis-Hinweis:** Die oben angegebenen Schätzpreise stellen **Maximalwerte** dar. Auf Marktplätzen wie **AliExpress** sind viele Komponenten oft deutlich günstiger — ein einzelnes Gerät lässt sich häufig für **ca. €20–€30** zusammenstellen, und mit gebrauchten/alten Teilen sogar darunter. Typische Einzelpreise: **ESP32-WROOM ≈ €7–€10**, **LoRa-Modul ≈ €15**.

> **Pricing note:** The estimated prices above are **maximums**. On marketplaces like **AliExpress** components are frequently much cheaper — a single device can often be assembled for **around €20–€30**, or less using used parts. Typical per-item prices: **ESP32-WROOM ≈ €7–€10**, **LoRa module ≈ €15**.

## Suppliers

### Online Retailers

- **AliExpress**: Cheapest, long shipping
- **Banggood**: Good prices, faster shipping
- **Amazon**: Fast shipping, higher prices
- **Adafruit**: Quality components, educational
- **SparkFun**: Similar to Adafruit
- **Mouser/DigiKey**: Professional components

### Local Options

- Electronics hobby stores
- Makerspaces
- University electronics labs
- Ham radio clubs

## Tools Required

### Basic

- ✅ Computer with Arduino IDE
- ✅ USB cable
- ✅ Serial terminal (Arduino Serial Monitor)

### Recommended

- ✅ Multimeter (for testing connections)
- ✅ Soldering iron (for permanent connections)
- ✅ Wire strippers
- ✅ Small screwdrivers
- ✅ Hot glue gun

### Advanced

- ✅ Oscilloscope (for debugging)
- ✅ Logic analyzer (for I2C/SPI debugging)
- ✅ Spectrum analyzer (for RF testing)
- ✅ 3D printer (for enclosures)

---

**Questions?** Open an issue on GitHub or email arn-c0de@protonmail.com

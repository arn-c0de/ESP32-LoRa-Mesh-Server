# Hardware — Pin Layout (FINAL)

> Fokus: Nur Hardware-Layout und Pin‑Belegung. Firmware/Software wird separat neu geschrieben.

## Kurzüberblick
- Zielplattform: **ESP32 LoRa32 (z. B. TTGO LoRa32 V2.1)**
- Display: **SSD1306 OLED** (I2C)
- Button: **GPIO 33** (Input, use internal pull‑up)
- Antenne: **U.FL** (oder integrierte Federantenne) — immer angeschlossen betreiben
- Default-Frequenz: **868 MHz** (EU); anpassbar in Firmware

---

## Pin‑Belegung (definitiv)

| Funktion        | GPIO  | Hinweise |
|-----------------|:-----:|:---------|
| LoRa NSS / CS   | 5     | SPI Chip Select
| LoRa RST        | 14    | Reset für LoRa‑Modul
| LoRa DIO0       | 26    | RX/TX IRQ (RX done)
| LoRa SCK        | 18    | SPI Clock
| LoRa MISO       | 19    | SPI MISO
| LoRa MOSI       | 23    | SPI MOSI
| OLED SDA (I2C)  | 21    | SSD1306 SDA (I2C)
| OLED SCL (I2C)  | 22    | SSD1306 SCL (I2C)
| OLED_RESET      | -1    | Reset shared / not exposed on all Boards — prüfen Hardware
| Button (PAIR)   | 33    | Input; **use INPUT_PULLUP** (firmware‑verhalten separat festlegen)
| LED (onboard)   | 2     | Status LED (optional)
| VBAT_MON (ADC)  | 35    | ADC pin für Batterieüberwachung (optional)
| USB Serial TX/RX| 1, 3  | Programmierung / Serial

---

## Empfehlungen & Hinweise
- Button: **Nur Hardware** dokumentiert (GPIO33, Pull‑up). Firmware‑Verhalten (kurz/long press) wird in der neuen Software definiert.
  - Empfohlene Firmware‑Verhalten (optional): Kurzdruck → Pairing / ECDH; Langdruck → Bluetooth toggle / Reset (implementieren falls nötig).
- OLED I2C Adresse: **0x3C** (Standard) — falls Display nicht erkennt, prüfen 0x3D.
- Antenne: **Nie** ohne Antenne TX betreiben (schützt PA).
- Frequenz/Gerät: Stelle sicher, dass die Board‑Einstellung und Antenne zur Region passen (868 MHz EU / 915 MHz US).

---

## Boot‑kritische Pins & Einschränkungen
- **Vorsichtig verwenden**: GPIO 0, 2, 12, 15 (Boot‑Konfigurationen beeinflusst)
- **Input‑only**: GPIO 34, 35, 36, 39 (keine Pull‑ups/-downs, kein PWM)
- **NICHT verwenden**: GPIO 6–11 (Flash/SD pins) — Risiko für Datenverlust

---

## Kurze Tests (Hardware‑Checks)
1. Power on → LED / USB‑Serielle Kommunikation prüfen (115200 Baud).
2. I2C: Scan (I2C‑Scanner) → OLED bei 0x3C sichtbar.
3. LoRa: Firmware‑Testskript → "LoRa init success" und Frequency check.
4. Button: Hardware‑Bouncedebounce prüfen; GPIO‑state bei gedrückt = LOW (INPUT_PULLUP).

---

## Änderungsvermerk
- Diese Datei ist die finale Hardware‑Pin‑Spezifikation. Alle Firmware‑Änderungen und Button‑Logik werden in separaten Software‑Issues/Commits dokumentiert.

---

Bei Fragen oder wenn du eine alternative Pin‑Belegung brauchst, sag Bescheid.
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
| ESP32 LoRa32 V2.1 (868 MHz) | 2 | $20-30 |
| Tactile Button (if not included) | 2 | $0.50 |
| USB Cable (Micro or Type-C) | 2 | $2-5 |
| **Total** | | **$23-36** |

### Complete Setup

| Component | Quantity | Est. Price |
|-----------|----------|------------|
| ESP32 LoRa32 V2.1 (868 MHz) | 2 | $20-30 |
| External LoRa Antenna (868 MHz, 3dBi) | 2 | $5-10 |
| LiPo Battery (3.7V 2000mAh) | 2 | $8-12 |
| Plastic Enclosure | 2 | $3-5 |
| Tactile Button | 2 | $0.50 |
| Dupont Wires (if needed) | 10 | $2 |
| USB Cable | 2 | $2-5 |
| **Total** | | **$41-65** |

### Premium Setup (Maximum Range)

| Component | Quantity | Est. Price |
|-----------|----------|------------|
| ESP32 LoRa32 V2.1 (868 MHz) | 2 | $20-30 |
| High-gain Antenna (868 MHz, 8-12 dBi) | 2 | $15-30 |
| IP65 Weatherproof Enclosure | 2 | $10-15 |
| 18650 Battery (3000mAh) | 4 | $12-20 |
| 18650 Battery Holder (2S) | 2 | $3-5 |
| Solar Panel (5V 5W) | 2 | $10-15 |
| TP4056 Charging Module | 2 | $2-3 |
| Antenna Extension Cable | 2 | $5-8 |
| Mounting Hardware | - | $5-10 |
| **Total** | | **$82-136** |

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

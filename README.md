# ESP32 LoRa Mesh Server

Simple LoRa mesh network implementation for ESP32 LoRa32 (TTGO V2.1) with SX1276 radio module and SSD1306 OLED display.

## Features

- ✅ **LoRa Mesh Networking** with Node-ID and Hop-Count routing
- ✅ **OLED Display (128x32)** showing channel, frequency, power, RSSI, and last message
- ✅ **Serial Commands** for configuration (frequency, power, node ID, hops, etc.)
- ✅ **Automatic Rebroadcasting** for mesh forwarding
- ✅ **RadioLib** for reliable SX1276 control
- ✅ **Scrolling Text** for long messages on display

## Hardware

Based on [HARDWARE.md](HARDWARE.md) specifications:

- **Board**: ESP32 LoRa32 (TTGO LoRa32 V2.1)
- **Radio**: SX1276 LoRa module (868 MHz EU / 915 MHz US)
- **Display**: SSD1306 OLED 128x32 pixels (I2C 0x3C)
- **Antenna**: U.FL or integrated spring antenna (ALWAYS connect antenna before TX!)

### Pin Configuration

| Function | GPIO | Notes |
|----------|------|-------|
| LoRa NSS | 5 | SPI Chip Select |
| LoRa RST | 14 | Reset |
| LoRa DIO0 | 26 | RX/TX IRQ |
| LoRa SCK | 18 | SPI Clock |
| LoRa MISO | 19 | SPI MISO |
| LoRa MOSI | 23 | SPI MOSI |
| OLED SDA | 21 | I2C Data |
| OLED SCL | 22 | I2C Clock |
| Button | 33 | INPUT_PULLUP |
| LED | 2 | Status LED |

## Quick Start

### 1. Install arduino-cli

```bash
# Linux/macOS
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh
sudo mv bin/arduino-cli /usr/local/bin/

# Or via package manager
sudo apt install arduino-cli     # Debian/Ubuntu
brew install arduino-cli         # macOS
```

### 2. Build and Flash

```bash
# Make sure ESP32 is connected via USB
./build.sh
```

The script will:
- Check for arduino-cli installation
- Install ESP32 core if needed
- Install required libraries (RadioLib, Adafruit_SSD1306, Adafruit_GFX)
- Compile the sketch
- Auto-detect ESP32 board
- Flash firmware
- Optionally open serial monitor

### 3. Open Serial Monitor

```bash
# Using the helper script (autodetects port):
./serial_monitor.sh

# Or specify port manually:
./serial_monitor.sh /dev/ttyUSB0

# Or using arduino-cli directly
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200

# Or using screen
screen /dev/ttyUSB0 115200

# Or using minicom
minicom -D /dev/ttyUSB0 -b 115200
```

Type `/HELP` to see available commands (commands now accept a leading `/`) and try `/PING` to test reachability.

## Serial Commands (use leading `/`)

Commands now accept a leading `/` (e.g., `/HELP`). The most commonly used commands are listed below — use `/HELP` on the serial console for the latest list.

### Basic Commands

| Command | Description | Example |
|---------|-------------|---------|
| `/HELP` | Show command list | `/HELP` |
| `/STATUS` | Show system status | `/STATUS` |
| `/RESET` | Reset counters and display | `/RESET` |
| `/PING` | Broadcast PING and wait for PONG replies (5s timeout) | `/PING` |

### LoRa Configuration

| Command | Description | Example | Range |
|---------|-------------|---------|-------|
| `/FREQ:xxx` | Set frequency (MHz) | `/FREQ:868.0` | 410-525, 863-870, 902-928 |
| `/FREQ?` | Query current frequency | `/FREQ?` | - |
| `/POWER:xx` | Set TX power (dBm) | `/POWER:14` | 2-20 (max 14 for EU) |
| `/POWER?` | Query current power | `/POWER?` | - |
| `/SF:x` | Set spread factor | `/SF:7` | 6-12 |
| `/BW:xxx` | Set bandwidth (kHz) | `/BW:125` | 7.8-500 |

### Mesh Configuration

| Command | Description | Example | Range |
|---------|-------------|---------|-------|
| `/NODEID:x` | Set this node's ID | `/NODEID:1` | 1-255 |
| `/HOPS:x` | Set default hop count | `/HOPS:3` | 0-10 |

### Messaging

| Command | Description | Example |
|---------|-------------|---------|
| `/TX:message` or `/SEND:message` | Broadcast message to all nodes | `/TX:Hello World` |
| `/TXTO:id,msg` or `/SENDTO:id,msg` | Send message to specific node | `/TXTO:5,Hi Node 5` |

**Notes:**
- `/PING` sends a broadcast `PING` message; nodes automatically reply with `PONG` and the board will display RSSI and round-trip time for replies.
- Aliases `/SEND` and `/SENDTO` are available as convenience for `/TX` and `/TXTO`.

## Display Layout

The 128x32 OLED display shows:

```
┌────────────────────────────┐
│ N:1          868.0MHz      │  ← Node ID | Frequency
│ TX:5 RX:12        14dBm    │  ← TX/RX counters | Power
│ Fr:3 -65dBm                │  ← From Node | RSSI
│ Hello from mesh...         │  ← Last message (scrolls if long)
└────────────────────────────┘
```

## Ping / Reachability Testing

- Use `/PING` to broadcast a PING request. Nodes replying with `PONG` will be shown in the serial monitor with RSSI and RTT (round-trip time).
- PING waits up to 5 seconds for responses by default.
- Example: `/FREQ:433.775` then `/PING` to test reachability of a neighbor or remote station on 433.775 MHz.

## Testing a remote station (example: 433.775 MHz)

1. Ensure you're legally allowed to transmit on 433.775 MHz in your country and attach an antenna.
2. Set the radio to that frequency and typical settings:

```
/FREQ:433.775
/BW:125
/SF:7
/POWER:10
```

3. Send a test broadcast or PING:
```
/SEND:Hello from Node 1
/PING
```

4. Check the serial log for responses. If the remote station operates a public portal or log, you can verify reception there as well.

## Extended Command Examples

Praktische Beispiele mit erwartetem seriellen Output (Beispiele):

1) Nachbarschaft / Reichweite prüfen (433.775 MHz)

```
/FREQ:433.775
/BW:125
/SF:7
/POWER:10
/PING
```
Serielle Ausgabe (Beispiel):
```
> /PING
[PING] Broadcasting PING request...
[PING] Waiting for PONG responses (5s timeout)...
[PONG] Received from Node 23 (RSSI: -70 dBm, RTT: 120 ms)
```

2) Broadcast Nachricht senden

```
/SEND:Hello from Node 1
```
Serielle Ausgabe (Beispiel):
```
> /SEND:Hello from Node 1
[TX] Sending to BROADCAST (hops: 3): Hello from Node 1
[TX] Success
```

3) Gezielt Node anpingen (Unicast)

```
/TXTO:10,PING
```
Serielle Ausgabe (wenn antwortet):
```
> /TXTO:10,PING
[TX] Sending to 10 (hops: 3): PING
[PONG] Received from Node 10 (RSSI: -68 dBm, RTT: 90 ms)
```

4) Status prüfen

```
/STATUS
```

Hinweis: Befehle akzeptieren führendes `/` und sind nicht case-sensitiv. PING-Antworten werden automatisch verarbeitet und angezeigt.

## Mesh Network Protocol

### Packet Format

Each message is formatted as: `FromID:ToID:Hops:Data`

Example: `1:0:3:Hello` means:
- From Node 1
- To Node 0 (broadcast)
- 3 hops remaining
- Message: "Hello"

### Routing Behavior

1. **Broadcast (ToID=0)**: All nodes receive and rebroadcast
2. **Unicast (ToID=specific)**: Only target node processes, others forward
3. **Hop Decrement**: Each relay decrements hop count
4. **Stop Forwarding**: When hops reach 0, message is not rebroadcast
5. **Collision Avoidance**: Random 50-150ms delay before rebroadcast

## Example Usage

### Basic Test (2 Nodes)

**Node 1:**
```
NODEID:1
FREQ:868.0
POWER:14
TX:Hello from Node 1
```

**Node 2:**
```
NODEID:2
FREQ:868.0
POWER:14
TX:Response from Node 2
```

### Multi-Hop Mesh (3+ Nodes)

**Node 1 (Base):**
```
NODEID:1
HOPS:3
TX:Testing 3-hop mesh
```

**Node 2 (Relay):**
```
NODEID:2
HOPS:2
```

**Node 3 (End):**
```
NODEID:3
HOPS:1
```

Node 2 will automatically relay messages between Node 1 and Node 3.

### Long Range Settings

For maximum range, use:
```
SF:12           # Slowest but longest range
BW:125          # Standard bandwidth
POWER:14        # Max for EU (20 for US)
```

Note: Higher SF = longer range but slower data rate.

## Troubleshooting

### Build Issues

**Error: arduino-cli not found**
```bash
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh
sudo mv bin/arduino-cli /usr/local/bin/
```

**Error: ESP32 core not found**
```bash
arduino-cli config init
arduino-cli config add board_manager.additional_urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core install esp32:esp32
```

### Upload Issues

**Error: Permission denied**
```bash
sudo usermod -aG dialout $USER
# Log out and back in
```

**Error: Board not found**
```bash
# List available ports
ls -la /dev/ttyUSB* /dev/ttyACM*

# Try manual port in build.sh
nano build.sh
# Change: PORT="/dev/ttyUSB0"  # Update this line
```

**Error: Upload timeout**
1. Hold **BOOT** button on ESP32
2. Press **EN/RST** button
3. Release **BOOT** button
4. Run `./build.sh` again

### Runtime Issues

**Display shows "OLED not found"**
- Check I2C wiring (SDA=21, SCL=22)
- Try alternative address: 0x3D
- Run I2C scanner sketch

**LoRa init failed**
- Check SPI wiring
- Verify antenna is connected
- Check LoRa module power

**No messages received**
- Verify both nodes use same frequency
- Check antenna connections
- Verify not blocked by rate limiting
- Increase TX power
- Try lower spread factor (SF7)

**Messages not forwarding**
- Check hop count > 0
- Verify Node IDs are different
- Check serial output for rebroadcast logs

## Legal Compliance

⚠️ **Important**: Check local regulations before operation!

**868 MHz (Europe)**
- ISM Band: 863-870 MHz
- Max power: 14 dBm ERP (25 mW)
- Duty cycle: <1% in some sub-bands
- **Use**: `POWER:14` (max)

**915 MHz (USA)**
- ISM Band: 902-928 MHz
- Max power: 30 dBm (1W)
- **Use**: `POWER:20` (max)

**433 MHz (Asia/Europe)**
- ISM Band: 433-435 MHz
- Max power: 10 mW ERP
- **Use**: `POWER:10` (max)

## File Structure

```
.
├── ESP32-LoRa-Mesh-Server.ino   # Main sketch (refactored)
├── mesh_network.h               # Mesh networking, packet handling, ping/pong
├── commands.h                   # Serial command parser and handlers
├── display_ui.h                 # OLED display helpers
├── build.sh                     # Build and flash script
├── serial_monitor.sh            # Serial monitor helper
├── HARDWARE.md                  # Hardware specifications
└── README.md                    # This file
```

## Dependencies

Automatically installed by `build.sh`:

- **RadioLib** v7.5.0 - SX1276 driver
- **Adafruit SSD1306** v2.5.16 - OLED display driver
- **Adafruit GFX** v1.12.4 - Graphics library
- **Adafruit BusIO** v1.17.4 - I2C helper library

## License

This project is provided as-is for educational and experimental purposes.

## Safety Warnings

⚠️ **NEVER transmit without antenna connected** - damages PA!
⚠️ Verify frequency regulations in your country
⚠️ Respect duty cycle limits (EU: <1% in some bands)
⚠️ Use appropriate TX power for your region

## Support

For issues or questions:
- Check [HARDWARE.md](HARDWARE.md) for pin configurations
- Review troubleshooting section above
- Open issue on GitHub

---

**Questions?** Email: arn-c0de@protonmail.com

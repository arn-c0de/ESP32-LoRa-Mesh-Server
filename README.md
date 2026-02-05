[![GitHub Stars](https://img.shields.io/github/stars/arn-c0de/ESP32-LoRa-Mesh-Server?style=flat)](https://github.com/arn-c0de/ESP32-LoRa-Mesh-Server)
[![License](https://img.shields.io/github/license/arn-c0de/ESP32-LoRa-Mesh-Server?style=flat)](https://github.com/arn-c0de/ESP32-LoRa-Mesh-Server/blob/main/LICENSE)
[![Issues](https://img.shields.io/github/issues/arn-c0de/ESP32-LoRa-Mesh-Server?style=flat)](https://github.com/arn-c0de/ESP32-LoRa-Mesh-Server/issues)
[![ESP32](https://img.shields.io/badge/ESP32-LoRa32-blue?style=flat)](https://github.com/arn-c0de/ESP32-LoRa-Mesh-Server)
[![LoRa SX1276](https://img.shields.io/badge/LoRa-SX1276-green?style=flat)](https://github.com/arn-c0de/ESP32-LoRa-Mesh-Server)

# ESP32 LoRa Mesh Server

Professional, minimal, and documented LoRa mesh implementation for ESP32 LoRa32 (TTGO V2.1) using SX1276 and an SSD1306 OLED.

Repository: https://github.com/arn-c0de/ESP32-LoRa-Mesh-Server
Security contact: arn-c0de@protonmail.com

---

## Table of contents

1. [About](#1-about)
2. [Features](#2-features)
3. [Hardware](#3-hardware)
4. [Quick start](#4-quick-start)
5. [Commands (full list)](#5-commands-full-list)
6. [Encryption (AES-256-GCM)](#6-encryption-aes-256-gcm)
7. [Mesh protocol](#7-mesh-protocol-details)
8. [Display](#8-display)
9. [Examples](#9-examples)
10. [Troubleshooting](#10-troubleshooting)
11. [Legal & Safety](#11-legal--safety)
12. [File structure](#12-file-structure)
13. [Support & License](#13-support--license)

---

## 1. About

This project implements a simple, reliable LoRa mesh network with the following goals:

- Small memory footprint and straightforward command interface
- Automatic rebroadcast for multi-hop forwarding
- Optional end-to-end encrypted messages (AES-256-GCM)
- OLED status display and serial command interface

## 2. Features

- LoRa mesh routing using Node ID and remaining hop count
- OLED display (128x32) for node status and last message
- Serial command interface for configuration and messaging
- Optional AES-256-GCM encryption with passphrase-derived key
- Automatic installation of dependencies via `build.sh`

## 3. Hardware

Refer to [HARDWARE.md](HARDWARE.md) for detailed wiring, pin assignments, BOM, suppliers, and tools. Key pins:

- LoRa NSS: GPIO 5
- LoRa RST: GPIO 14
- LoRa DIO0: GPIO 26
- LoRa SCK/MISO/MOSI: GPIO 18/19/23
- OLED SDA/SCL: GPIO 21/22
- Button: GPIO 33 (INPUT_PULLUP)
- LED: GPIO 2

## 4. Quick start

1. Install `arduino-cli` (see instructions in original README).
2. Connect your ESP32 and run `./build.sh` to compile and flash.
3. Open serial monitor with `./serial_monitor.sh` or `arduino-cli monitor -p <port> -c baudrate=115200`.
4. Type `/HELP` on the serial console for the full command list.

## 5. Commands (full list)

Commands accept a leading `/` and separators `:`, `=` or a space. They are case-insensitive.

General
- `/HELP` — Show command list
- `/STATUS` or `/INFO` — Show system status
- `/RESET` — Reset counters and display

LoRa configuration
- `/FREQ:xxx` — Set frequency (MHz). Valid ranges: 410–525, 863–870 (EU), 902–928 (US)
- `/FREQ?` — Show current frequency
- `/POWER:xx` — Set TX power (2–20 dBm; max 14 dBm recommended for EU)
- `/POWER?` — Show TX power
- `/SF:x` — Set spread factor (6–12)
- `/BW:xxx` — Set bandwidth (kHz). Valid: 7.8, 10.4, 15.6, 20.8, 31.25, 41.7, 62.5, 125, 250, 500

Mesh configuration
- `/NODEID:x` — Set node ID (1–255)
- `/HOPS:x` — Set default hop count (0–10)

Messaging
- `/TX:message` or `/SEND:message` — Broadcast message (ToID=0)
- `/TXTO:id,msg` or `/SENDTO:id,msg` — Send to specific node

Device name
- `/NAME:DeviceName` — Set device name (prepended to outgoing messages)

Encryption (see section below)
- `/KEY:passphrase` — Derive and store AES-256 key from passphrase
- `/CLEARKEY` — Clear key from memory and NVS
- `/EINFO` — Show encryption status and fingerprint
- `/ESEND:message` — Send encrypted broadcast message (requires key)

Maintenance
- `/FORMAT` — Reset EEPROM/NVS to factory defaults (resets settings)

Examples
- `/PING` — Broadcast a ping and wait for PONG replies (5s timeout)
- `/TXTO:10,PING` — Send a unicast PING to Node 10

## 6. Encryption (AES-256-GCM)

Overview
- Algorithm: AES-256 in GCM mode (authenticated encryption)
- Key derivation: SHA-256(passphrase) → 32-byte key
- IV: 12 bytes random (per-message)
- Tag: 16 bytes (128-bit)
- Key is stored in NVS (Preferences) under namespace `mesh_crypto` as `aes256`

Usage
1. Set key: `/KEY:your-passphrase` — This derives a key and saves it in NVS.
2. Verify: `/EINFO` — Shows whether a key is set and the fingerprint (first 6 bytes in hex).
3. Send encrypted broadcast: `/ESEND:your message` — The message is encrypted and Base64-encoded.

Payload format (on the wire)
- Encrypted payload is sent as: `FLAG:<NodeID>:<Base64Blob>`
  - `FLAG` indicates an encrypted payload
  - `<NodeID>` the sender ID
  - `<Base64Blob>` = Base64( IV || Ciphertext || Tag )

Receiving nodes
- The receiver must have the same encryption key set (`/KEY:...`) to decrypt and verify.
- Decryption verifies the authentication tag; invalid or tampered payloads are rejected.

Security notes
- Use a strong passphrase. The passphrase is not stored — only the derived key is kept in NVS.
- Use `/CLEARKEY` to remove the key from memory and NVS when needed.
- If you require a formal vulnerability disclosure process, contact: arn-c0de@protonmail.com

## 7. Mesh protocol details

Packet format: `FromID:ToID:Hops:Data`
- FromID — Sender node ID
- ToID — Destination node ID (0 = broadcast)
- Hops — Remaining hops (decremented by relays)
- Data — Payload (plain text or encrypted blob)

Routing
- Broadcasts are rebroadcast until hops == 0
- Unicast messages are forwarded toward destination using hop count
- A small random delay (50–150 ms) is used before rebroadcast to reduce collisions

## 8. Display

The OLED shows node ID, frequency, TX/RX counters, TX power, RSSI of last message, and the last received message (scrolling if necessary).

## 9. Examples

Basic 2-node test
- Node 1: `/NODEID:1` `/FREQ:868.0` `/POWER:14` `/TX:Hello from Node 1`
- Node 2: `/NODEID:2` `/FREQ:868.0` `/POWER:14` `/TX:Response from Node 2`

Multi-hop
- Set `/HOPS:3` and observe automatic rebroadcasting through relay nodes.

## 10. Troubleshooting

Common issues and fixes are documented in the original README and in the `Troubleshooting` section of this file. Typical checks:
- Verify wiring (I2C/SPI)
- Check antenna connection before TX
- Confirm nodes share frequency / SF / BW
- Ensure Node IDs are unique when testing unicast

## 11. Legal & Safety

Always check local regulations before transmitting.
- 868 MHz (EU): 863–870 MHz, max 14 dBm ERP, duty-cycle restrictions apply
- 915 MHz (US): 902–928 MHz
- 433 MHz: region-specific limitations

NEVER transmit without an antenna connected — this may damage the power amplifier.

## 12. File structure

See top-level layout; important files include:
- `ESP32-LoRa-Mesh-Server.ino` — main sketch
- `mesh_network.h` — routing and packet handling
- `commands.h` — serial command parser and handlers
- `crypt.h` — encryption utilities (AES-256-GCM)

## 13. Support & License

- Repo: https://github.com/arn-c0de/ESP32-LoRa-Mesh-Server
- Security contact: arn-c0de@protonmail.com
- Issues & PRs: use GitHub Issues and Pull Requests


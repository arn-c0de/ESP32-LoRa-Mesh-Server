# Implementation Summary: HomeServer TCP Collector + Persistent Storage + API

## Overview

This update adds a **dual-path architecture** to the ESP32 LoRa Mesh project. Nodes now transmit via both **LoRa mesh** and **WiFi/TCP** to a 24/7 HomeServer (Raspberry Pi). The server persists all messages in SQLite, provides REST APIs, and pushes real-time updates via WebSocket.

---

## Part 1: Build Configuration (.env Injection)

### New Files
| File | Purpose |
|------|---------|
| `.env.example` | Template with all 14 configurable variables |

### Modified Files
| File | Changes |
|------|---------|
| `build.sh` | Added `.env` parser (lines 17-49) that reads key=value pairs and builds `-D` compiler flags. String values get escaped quotes (`-DWIFI_SSID="\"MyNetwork\""`). Compile command uses `--build-property "build.extra_flags=$BUILD_DEFINES"`. Falls back gracefully when no `.env` exists. |

### Configurable Variables
```
WIFI_SSID, WIFI_PASS, SERVER_IP, SERVER_PORT, TCP_ENABLED,
NODE_ID, DEVICE_NAME, ENCRYPTION_PASSPHRASE,
LORA_FREQ, LORA_POWER, LORA_SF, LORA_BW, LORA_CR, LORA_HOPS
```

---

## Part 2: ESP32 Firmware - WiFi/TCP Client

### New File: `wifi_tcp.h`

Complete WiFi/TCP client module with the following features:

| Feature | Details |
|---------|---------|
| **Compile-time defaults** | `#ifdef` guards for `WIFI_SSID`, `WIFI_PASS`, `SERVER_IP`, `SERVER_PORT`, `TCP_ENABLED` |
| **EEPROM persistence** | Bytes 47-162: SSID (47-79), password (80-143), server IP (144-159), port (160-161), TCP flag (162) |
| **WiFi connection** | Non-blocking `connectWiFi()` with exponential backoff reconnect (5s to 60s) |
| **TCP client** | `connectTCP()` with backoff (3s to 30s), sends `HELLO:<nodeID>:<deviceName>` on connect |
| **Circular buffer** | 50-message queue when TCP is disconnected, auto-drains on reconnect |
| **Heartbeat** | `HEARTBEAT:<nodeID>` sent every 30 seconds |
| **Main handlers** | `setupWiFi()` for setup(), `handleTCP()` for loop() |
| **Status** | `printTCPStatus()` prints full WiFi/TCP connection details |

### EEPROM Layout
```
Bytes 0-46:    EXISTING (init flag, device name, nodeID, freq, power, BW, SF, CR, hops)
Byte 47:       WiFi SSID length
Bytes 48-79:   WiFi SSID data (32 bytes)
Byte 80:       WiFi password length
Bytes 81-143:  WiFi password data (63 bytes)
Byte 144:      Server IP length
Bytes 145-159: Server IP data (15 bytes)
Bytes 160-161: Server port (uint16_t)
Byte 162:      TCP enabled flag
Bytes 163-255: FREE (93 bytes)
```

---

## Part 3: Firmware Modifications to Existing Files

### `ESP32-LoRa-Mesh-Server.ino`

| Change | Location |
|--------|----------|
| Added `#include "wifi_tcp.h"` | After line 47 |
| Replaced hardcoded defaults with `#ifdef` conditionals | Lines 67-101 (LORA_FREQ, LORA_POWER, LORA_BW, LORA_SF, LORA_CR, NODE_ID, LORA_HOPS) |
| Added `#ifdef DEVICE_NAME` fallback | In `loadDeviceName()` - uses compile-time name if EEPROM is empty |
| Added `#ifdef ENCRYPTION_PASSPHRASE` auto-set | After `loadEncryptionKey()` - auto-derives key if NVS is empty |
| Added `setupWiFi()` call | In `setup()` after `setupLoRa()` |
| Added `handleTCP()` call | In `loop()` after `checkPingTimeout()` |

### `mesh_network.h`

| Change | Location |
|--------|----------|
| Added extern declarations | `sendMessageTCP()` and `tcpEnabled` after line 32 |
| Forward sent packets to TCP | `sendMessageTCP(packet)` after `messagesSent++` in `sendMeshMessage()` |
| Forward received packets to TCP | `sendMessageTCP(msg)` after `messagesReceived++` in `receiveLoRaMessage()` |

### `commands.h`

**Case-sensitivity fix:**
- Preserves `originalCmd` before `toUpperCase()`
- Extracts `rawArgument` from original input
- Uses `rawArgument` for case-sensitive commands: `/NAME`, `/KEY`, `/TX`, `/TXTO`, `/ESEND`, `/WIFISSID`, `/WIFIPASS`, `/SERVERIP`

**New commands (8):**

| Command | Function |
|---------|----------|
| `/WIFISSID:name` | Set WiFi SSID (case-sensitive) |
| `/WIFIPASS:pass` | Set WiFi password (case-sensitive) |
| `/SERVERIP:x.x.x.x` | Set HomeServer IP address |
| `/SERVERPORT:5001` | Set HomeServer TCP port |
| `/TCPENABLE` | Enable WiFi/TCP and start connecting |
| `/TCPDISABLE` | Disable WiFi/TCP and disconnect |
| `/TCPSTATUS` | Show full WiFi/TCP connection status |
| `/RECONNECT` | Force WiFi/TCP reconnect |

**Updated commands:**
- `/HELP` - Added "WiFi/TCP HomeServer" section
- `/STATUS` - Added WiFi/TCP connection info
- `/FORMAT` - Resets WiFi/TCP runtime variables

### `display_ui.h`

| Change | Details |
|--------|---------|
| Added extern declarations | `wifiConnected`, `tcpConnected`, `tcpEnabled` |
| Shortened power display | "dBm" to "dB" (saves 1 character for WiFi indicator) |
| WiFi status indicator | Right edge of line 1: `W` = TCP connected, `w` = WiFi only, `.` = disconnected |

---

## Part 4: Python HomeServer (TCP-Server/)

### File Structure

```
TCP-Server/
├── main.py              # FastAPI app + async TCP server (lifespan startup)
├── tcp_server.py        # asyncio TCP listener, ESP32Connection handler
├── message_parser.py    # Parse mesh packets, HEARTBEAT, HELLO messages
├── crypto_handler.py    # AES-256-GCM decryption (mirrors ESP32 crypt.h)
├── database.py          # SQLAlchemy async engine + session factory
├── models.py            # Message + Node ORM models
├── schemas.py           # Pydantic response/request models
├── api_routes.py        # REST API endpoints
├── websocket_handler.py # WebSocketManager for real-time broadcast
├── auth.py              # X-API-Key header middleware
├── retention.py         # Background task: delete old messages
├── config.py            # YAML + env var config loader (dataclasses)
├── config.yaml          # Default configuration
├── requirements.txt     # Python dependencies
├── homeserver.service   # systemd unit file
├── .env.example         # Server-side environment variables
└── README.md            # Full documentation with curl examples
```

### TCP Server (port 5001)

- `asyncio.start_server` with line-buffered (`\n` delimited) protocol
- Handles three message types:
  - `HELLO:<NodeID>:<DeviceName>` - Node registration
  - `HEARTBEAT:<NodeID>` - Keep-alive
  - `<FromID>:<ToID>:<HopCount>:<Data>` - Mesh packets
- Tracks connected nodes, marks online/offline status
- Optional auto-decryption of encrypted messages

### HTTP Server (port 8000)

**REST API Endpoints:**

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/v1/messages` | GET | Paginated messages (filter: `from_id`, `to_id`, `since`) |
| `/api/v1/nodes` | GET | All known nodes with online status |
| `/api/v1/stats` | GET | Aggregate stats (total messages, online nodes, messages/hour) |
| `/api/v1/decrypt/{id}` | POST | On-demand decrypt with passphrase |
| `/health` | GET | Health check (TCP connections, uptime) |
| `/ws` | WS | Real-time message feed |

### Key Design Decisions

| Component | Implementation |
|-----------|---------------|
| **Database** | SQLAlchemy async + SQLite with WAL mode (`aiosqlite`) |
| **Encryption** | AES-256-GCM with SHA-256 key derivation (same as ESP32) |
| **WebSocket** | Broadcasts new messages to all connected clients |
| **Auth** | Optional `X-API-Key` header middleware (only on `/api/` routes) |
| **Rate limiting** | `slowapi` on API endpoints (configurable requests/minute) |
| **Retention** | Hourly cleanup of messages older than `max_age_days` |
| **Logging** | `structlog` with console or JSON format |
| **Deployment** | systemd service file included |

---

## .gitignore Updates

Added entries for:
- `TCP-Server/homeserver.db` (SQLite database)
- `TCP-Server/venv/` (Python virtual environment)
- `TCP-Server/__pycache__/` (Python bytecode cache)
- `TCP-Server/*.pyc` (compiled Python files)
- `TCP-Server/.env` (server-side secrets)

---

## Verification Checklist

- [ ] `./build.sh` compiles with and without `.env`
- [ ] ESP32 logs `[WiFi] Connected! IP: x.x.x.x`
- [ ] ESP32 logs `[TCP] Connected to x.x.x.x:5001`
- [ ] LoRa messages appear in both LoRa RX and HomeServer DB
- [ ] Disconnect server, send messages, reconnect - buffer drains
- [ ] `curl http://raspi:8000/api/v1/messages` returns stored messages
- [ ] `wscat -c ws://raspi:8000/ws` shows real-time messages
- [ ] `/ESEND:test` -> server stores encrypted, `POST /decrypt` returns plaintext
- [ ] `/WIFISSID:MyNetwork` stores case-correctly, `/TCPSTATUS` shows status
- [ ] Messages older than `max_age_days` auto-deleted
- [ ] systemd service auto-starts and survives restarts

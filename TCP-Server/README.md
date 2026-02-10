# ESP32 LoRa Mesh HomeServer

A Python backend that collects messages from ESP32 LoRa Mesh nodes via TCP, stores them in SQLite, and provides REST API + WebSocket access.

## Architecture

```
ESP32 Nodes ──(LoRa)──> Other ESP32 Nodes
     │
     └──(WiFi/TCP)──> HomeServer (Raspberry Pi)
                           ├── TCP Server (port 5001)
                           ├── REST API  (port 8000)
                           ├── WebSocket (port 8000/ws)
                           └── SQLite Database
```

## Quick Start

```bash
# 0. Start via helper script (creates venv, installs deps, runs server)
./start.sh

# or manual:
# 1. Create virtual environment
python3 -m venv venv
source venv/bin/activate

# 2. Install dependencies
pip install -r requirements.txt

# 3. Configure (optional - defaults work out of the box)
cp .env.example .env
# Edit .env or config.yaml as needed

# 4. Run
python main.py
```

The server starts:
- **TCP listener** on port 5001 (ESP32 nodes connect here)
- **HTTP API** on port 8000 (REST + WebSocket)

## Configuration

Configuration is loaded from `config.yaml` with environment variable overrides. See `config.yaml` for all options.

### Key Settings

| Setting | Env Var | Default | Description |
|---------|---------|---------|-------------|
| TCP port | `TCP_PORT` | 5001 | Port for ESP32 TCP connections |
| TCP shared secret | `TCP_SHARED_SECRET` | - | Optional pre-shared token for HELLO auth |
| TCP max line | `TCP_MAX_LINE_LENGTH` | 2048 | Max TCP line length (bytes) |
| TCP max data | `TCP_MAX_DATA_LENGTH` | 512 | Max mesh payload length |
| TCP max device name | `TCP_MAX_DEVICE_NAME_LENGTH` | 64 | Max HELLO device name length |
| TCP max connections | `TCP_MAX_CONNECTIONS` | 100 | Max concurrent TCP connections |
| HTTP port | `HTTP_PORT` | 8000 | Port for REST API / WebSocket |
| Database | `DATABASE_URL` | sqlite+aiosqlite:///homeserver.db | Database connection URL |
| Auth | `AUTH_ENABLED` | false | Enable API key authentication |
| API key | `API_KEY` | - | Required when auth enabled |
| Health auth | `HEALTH_REQUIRE_AUTH` | false | Require X-API-Key for /health |
| WS max connections | `WS_MAX_CONNECTIONS` | 100 | Max concurrent WebSocket connections |
| Encryption | `ENCRYPTION_PASSPHRASE` | - | Auto-decrypt encrypted messages |
| Store decrypted | `STORE_DECRYPTED` | false | Persist decrypted payloads in DB |
| Retention | `RETENTION_MAX_AGE_DAYS` | 30 | Auto-delete messages older than N days |
| Startup checks | `STARTUP_CHECKS_ENABLED` | true | Run self-tests and integrity checks on start |
| Startup loopback | `STARTUP_CHECKS_LOOPBACK_ENABLED` | true | Run TCP loopback integration test on start |
| Decrypt auth | `DECRYPT_REQUIRE_AUTH` | true | Require X-API-Key for decrypt endpoint |
| GUI enabled | `GUI_ENABLED` | true | Serve GUI static files |
| GUI path | `GUI_PATH` | gui | Path to GUI folder (relative to `main.py`) |
| GUI mount | `GUI_MOUNT_PATH` | / | URL mount path for GUI |

## REST API

### Get Messages
```bash
# All messages (paginated)
curl http://localhost:8000/api/v1/messages

# Filter by sender
curl http://localhost:8000/api/v1/messages?from_id=1

# Filter by recipient
curl http://localhost:8000/api/v1/messages?to_id=5

# Messages since timestamp
curl "http://localhost:8000/api/v1/messages?since=2025-01-01T00:00:00"

# Pagination
curl "http://localhost:8000/api/v1/messages?limit=10&offset=20"
```

### Get Nodes
```bash
curl http://localhost:8000/api/v1/nodes
```

### Get Stats
```bash
curl http://localhost:8000/api/v1/stats
```

### Decrypt Message
```bash
curl -X POST http://localhost:8000/api/v1/decrypt/42 \
  -H "Content-Type: application/json" \
  -d '{"passphrase": "my-secret-key"}'
```

### Health Check
```bash
curl http://localhost:8000/health
```

### With API Key Auth
```bash
curl -H "X-API-Key: your-secret-key" http://localhost:8000/api/v1/messages
```

## WebSocket

Connect to `ws://localhost:8000/ws` for real-time message feed.
If auth is enabled, include `X-API-Key` header or `?api_key=` query param.

```bash
# Using wscat
wscat -c ws://localhost:8000/ws
```

Messages are JSON objects pushed in real-time:
```json
{
  "type": "message",
  "id": 1,
  "from_id": 1,
  "to_id": 0,
  "hop_count": 3,
  "data": "Hello from Node 1",
  "is_encrypted": false,
  "decrypted_data": null,
  "timestamp": "2025-01-15T10:30:00+00:00"
}
```

## TCP Protocol

ESP32 nodes communicate with the server using newline-delimited text:

| Message | Format | Direction |
|---------|--------|-----------|
| Hello | `HELLO:<NodeID>:<DeviceName>[:<AuthToken>]\n` | Node -> Server |
| Heartbeat | `HEARTBEAT:<NodeID>\n` | Node -> Server |
| Mesh Packet | `<FromID>:<ToID>:<HopCount>:<Data>\n` | Node -> Server |

## Deployment (systemd)

```bash
# Copy service file
sudo cp homeserver.service /etc/systemd/system/

# Edit paths if needed
sudo nano /etc/systemd/system/homeserver.service

# Enable and start
sudo systemctl daemon-reload
sudo systemctl enable homeserver
sudo systemctl start homeserver

# Check status
sudo systemctl status homeserver

# View logs
journalctl -u homeserver -f
```

## Interactive API Docs

FastAPI auto-generates interactive docs:
- **Swagger UI**: http://localhost:8000/docs
- **ReDoc**: http://localhost:8000/redoc

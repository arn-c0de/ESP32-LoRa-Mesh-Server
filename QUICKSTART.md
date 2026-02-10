# Quick Start Guide

This guide covers ESP32 firmware setup + HomeServer backend setup.

## 1) ESP32 Firmware (LoRa Mesh)

### Requirements
- ESP32 LoRa32 (TTGO V2.1) + SX1276
- USB cable
- `arduino-cli`

### Build + Flash
```bash
cd /mnt/festplatte2/ProjectsGithub/ESP32-LoRa-Mesh-Server
./build.sh
```

### Optional: .env Build Defaults
Create `.env` from the template:
```bash
cp .env.example .env
```

Common fields (compile-time defaults):
- `WIFI_SSID`
- `WIFI_PASS`
- `SERVER_IP`
- `SERVER_PORT`
- `TCP_ENABLED`
- `TCP_SHARED_SECRET`
- `NODE_ID`
- `DEVICE_NAME`
- `ENCRYPTION_PASSPHRASE`
- `LORA_FREQ`, `LORA_POWER`, `LORA_SF`, `LORA_BW`, `LORA_CR`, `LORA_HOPS`

### Runtime Serial Commands (no reflash)
Connect serial monitor and use commands:
- `/WIFISSID:MyNetwork`
- `/WIFIPASS:MyPass`
- `/SERVERIP:192.168.1.10`
- `/SERVERPORT:5001`
- `/TCPENABLE`
- `/TCPTOKEN:my-tcp-secret` (shared secret)
- `/STATUS`

## 2) HomeServer (TCP Collector + API + GUI)

### Requirements
- Python 3.10+ recommended

### Start (automatic venv + deps)
```bash
cd /mnt/festplatte2/ProjectsGithub/ESP32-LoRa-Mesh-Server/TCP-Server
./start.sh
```

### Open GUI
- `http://localhost:8000`

## 3) Configuration (Server)

Configuration is read from `TCP-Server/config.yaml` with environment variable overrides.

### Full `config.yaml` Reference
```yaml
tcp:
  host: "0.0.0.0"
  port: 5001
  shared_secret: ""          # Optional HELLO token
  max_line_length: 2048
  max_data_length: 512
  max_device_name_length: 64
  max_connections: 100

http:
  host: "0.0.0.0"
  port: 8000

database:
  url: "sqlite+aiosqlite:///homeserver.db"

auth:
  enabled: false
  api_key: ""                # Set strong key when enabling auth
  require_for_decrypt: true
  require_for_health: false
  max_ws_connections: 100

encryption:
  passphrase: ""              # Same passphrase as ESP32 (optional)
  store_decrypted: false

retention:
  enabled: true
  max_age_days: 30
  check_interval_hours: 1

logging:
  level: "INFO"
  format: "console"

rate_limit:
  enabled: true
  requests_per_minute: 60

startup_checks:
  enabled: true
  loopback_enabled: true

gui:
  enabled: true
  path: "gui"
  mount_path: "/"
```

### Environment Variable Overrides
- `TCP_HOST`, `TCP_PORT`, `TCP_SHARED_SECRET`
- `TCP_MAX_LINE_LENGTH`, `TCP_MAX_DATA_LENGTH`, `TCP_MAX_DEVICE_NAME_LENGTH`, `TCP_MAX_CONNECTIONS`
- `HTTP_HOST`, `HTTP_PORT`
- `DATABASE_URL`
- `AUTH_ENABLED`, `API_KEY`, `DECRYPT_REQUIRE_AUTH`, `HEALTH_REQUIRE_AUTH`, `WS_MAX_CONNECTIONS`
- `ENCRYPTION_PASSPHRASE`, `STORE_DECRYPTED`
- `RETENTION_MAX_AGE_DAYS`
- `LOG_LEVEL`, `LOG_FORMAT`
- `STARTUP_CHECKS_ENABLED`, `STARTUP_CHECKS_LOOPBACK_ENABLED`
- `GUI_ENABLED`, `GUI_PATH`, `GUI_MOUNT_PATH`

## 4) Security Notes (Recommended)

- Set `auth.enabled: true` and a strong `api_key`.
- Set `tcp.shared_secret` and configure ESP32 with `/TCPTOKEN` or `TCP_SHARED_SECRET`.
- Keep `store_decrypted: false` unless required.
- Use TLS via reverse proxy for remote access.

## 5) Troubleshooting

- If server exits on startup: set `startup_checks.loopback_enabled: false`.
- If GUI loads but API 404: make sure GUI mount is after API routes (already handled).
- If ESP32 not connecting: verify `SERVER_IP`, `SERVER_PORT`, and WiFi credentials.

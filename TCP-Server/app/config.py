"""
Configuration loader for HomeServer.
Reads from config.yaml with environment variable overrides.
"""

import os
from dataclasses import dataclass, field
from pathlib import Path

import yaml


@dataclass
class TCPConfig:
    host: str = "0.0.0.0"
    port: int = 5001
    shared_secret: str = ""
    max_line_length: int = 2048
    max_data_length: int = 512
    max_device_name_length: int = 64
    max_connections: int = 100


@dataclass
class HTTPConfig:
    host: str = "0.0.0.0"
    port: int = 8000


@dataclass
class DatabaseConfig:
    url: str = "sqlite+aiosqlite:///homeserver.db"


@dataclass
class AuthConfig:
    enabled: bool = False
    api_key: str = ""
    require_for_decrypt: bool = True
    require_for_health: bool = False
    max_ws_connections: int = 100


@dataclass
class EncryptionConfig:
    passphrase: str = ""
    store_decrypted: bool = False


@dataclass
class RetentionConfig:
    enabled: bool = True
    max_age_days: int = 30
    check_interval_hours: int = 1


@dataclass
class LoggingConfig:
    level: str = "INFO"
    format: str = "console"  # "console" or "json"


@dataclass
class RateLimitConfig:
    enabled: bool = True
    requests_per_minute: int = 60


@dataclass
class StartupChecksConfig:
    enabled: bool = True
    loopback_enabled: bool = True


@dataclass
class GUIConfig:
    enabled: bool = True
    path: str = "gui"
    mount_path: str = "/"

@dataclass
class CORSConfig:
    enabled: bool = True
    allow_origins: list[str] = field(default_factory=lambda: ["*"])
    allow_methods: list[str] = field(default_factory=lambda: ["*"])
    allow_headers: list[str] = field(default_factory=lambda: ["*"])
    allow_credentials: bool = False


@dataclass
class AppConfig:
    tcp: TCPConfig = field(default_factory=TCPConfig)
    http: HTTPConfig = field(default_factory=HTTPConfig)
    database: DatabaseConfig = field(default_factory=DatabaseConfig)
    auth: AuthConfig = field(default_factory=AuthConfig)
    encryption: EncryptionConfig = field(default_factory=EncryptionConfig)
    retention: RetentionConfig = field(default_factory=RetentionConfig)
    logging: LoggingConfig = field(default_factory=LoggingConfig)
    rate_limit: RateLimitConfig = field(default_factory=RateLimitConfig)
    startup_checks: StartupChecksConfig = field(default_factory=StartupChecksConfig)
    gui: GUIConfig = field(default_factory=GUIConfig)
    cors: CORSConfig = field(default_factory=CORSConfig)


def load_config(config_path: str = None) -> AppConfig:
    """Load configuration from YAML file with environment variable overrides."""
    if config_path is None:
        config_path = Path(__file__).parent.parent / "config.yaml"

    config = AppConfig()

    # Load YAML if exists
    if Path(config_path).exists():
        with open(config_path) as f:
            data = yaml.safe_load(f) or {}

        if "tcp" in data:
            config.tcp = TCPConfig(**data["tcp"])
        if "http" in data:
            config.http = HTTPConfig(**data["http"])
        if "database" in data:
            config.database = DatabaseConfig(**data["database"])
        if "auth" in data:
            config.auth = AuthConfig(**data["auth"])
        if "encryption" in data:
            config.encryption = EncryptionConfig(**data["encryption"])
        if "retention" in data:
            config.retention = RetentionConfig(**data["retention"])
        if "logging" in data:
            config.logging = LoggingConfig(**data["logging"])
        if "rate_limit" in data:
            config.rate_limit = RateLimitConfig(**data["rate_limit"])
        if "startup_checks" in data:
            config.startup_checks = StartupChecksConfig(**data["startup_checks"])
        if "gui" in data:
            config.gui = GUIConfig(**data["gui"])
        if "cors" in data:
            config.cors = CORSConfig(**data["cors"])

    # Environment variable overrides
    if v := os.getenv("TCP_HOST"):
        config.tcp.host = v
    if v := os.getenv("TCP_PORT"):
        config.tcp.port = int(v)
    if v := os.getenv("TCP_SHARED_SECRET"):
        config.tcp.shared_secret = v
    if v := os.getenv("TCP_MAX_LINE_LENGTH"):
        config.tcp.max_line_length = int(v)
    if v := os.getenv("TCP_MAX_DATA_LENGTH"):
        config.tcp.max_data_length = int(v)
    if v := os.getenv("TCP_MAX_DEVICE_NAME_LENGTH"):
        config.tcp.max_device_name_length = int(v)
    if v := os.getenv("TCP_MAX_CONNECTIONS"):
        config.tcp.max_connections = int(v)
    if v := os.getenv("HTTP_HOST"):
        config.http.host = v
    if v := os.getenv("HTTP_PORT"):
        config.http.port = int(v)
    if v := os.getenv("DATABASE_URL"):
        config.database.url = v
    if v := os.getenv("AUTH_ENABLED"):
        config.auth.enabled = v.lower() in ("1", "true", "yes")
    if v := os.getenv("API_KEY"):
        config.auth.api_key = v
    if v := os.getenv("DECRYPT_REQUIRE_AUTH"):
        config.auth.require_for_decrypt = v.lower() in ("1", "true", "yes")
    if v := os.getenv("HEALTH_REQUIRE_AUTH"):
        config.auth.require_for_health = v.lower() in ("1", "true", "yes")
    if v := os.getenv("WS_MAX_CONNECTIONS"):
        config.auth.max_ws_connections = int(v)
    if v := os.getenv("ENCRYPTION_PASSPHRASE"):
        config.encryption.passphrase = v
    if v := os.getenv("STORE_DECRYPTED"):
        config.encryption.store_decrypted = v.lower() in ("1", "true", "yes")
    if v := os.getenv("RETENTION_MAX_AGE_DAYS"):
        config.retention.max_age_days = int(v)
    if v := os.getenv("LOG_LEVEL"):
        config.logging.level = v
    if v := os.getenv("LOG_FORMAT"):
        config.logging.format = v
    if v := os.getenv("STARTUP_CHECKS_ENABLED"):
        config.startup_checks.enabled = v.lower() in ("1", "true", "yes")
    if v := os.getenv("STARTUP_CHECKS_LOOPBACK_ENABLED"):
        config.startup_checks.loopback_enabled = v.lower() in ("1", "true", "yes")
    if v := os.getenv("GUI_ENABLED"):
        config.gui.enabled = v.lower() in ("1", "true", "yes")
    if v := os.getenv("GUI_PATH"):
        config.gui.path = v
    if v := os.getenv("GUI_MOUNT_PATH"):
        config.gui.mount_path = v
    if v := os.getenv("CORS_ENABLED"):
        config.cors.enabled = v.lower() in ("1", "true", "yes")
    if v := os.getenv("CORS_ALLOW_ORIGINS"):
        config.cors.allow_origins = [s.strip() for s in v.split(",") if s.strip()]
    if v := os.getenv("CORS_ALLOW_METHODS"):
        config.cors.allow_methods = [s.strip() for s in v.split(",") if s.strip()]
    if v := os.getenv("CORS_ALLOW_HEADERS"):
        config.cors.allow_headers = [s.strip() for s in v.split(",") if s.strip()]
    if v := os.getenv("CORS_ALLOW_CREDENTIALS"):
        config.cors.allow_credentials = v.lower() in ("1", "true", "yes")

    return config

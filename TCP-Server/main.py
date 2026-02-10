"""
HomeServer main application.
FastAPI + async TCP server for ESP32 LoRa Mesh network.
"""

import asyncio
from contextlib import asynccontextmanager

import structlog
import uvicorn
from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware
from fastapi.staticfiles import StaticFiles
from fastapi.responses import FileResponse
from pathlib import Path

from app.api_routes import create_health_endpoint, router as api_router
from app.auth import APIKeyMiddleware
from app.config import load_config
from app.database import close_db, init_db
from app.retention import retention_loop
from app.schemas import HealthResponse
from app.startup_checks import run_startup_checks
from app.tcp_server import start_tcp_server
from app.websocket_handler import ws_manager

# Load configuration
config = load_config()

# Configure structlog
structlog.configure(
    processors=[
        structlog.contextvars.merge_contextvars,
        structlog.processors.add_log_level,
        structlog.processors.StackInfoRenderer(),
        structlog.dev.set_exc_info,
        structlog.processors.TimeStamper(fmt="iso"),
        (
            structlog.dev.ConsoleRenderer()
            if config.logging.format == "console"
            else structlog.processors.JSONRenderer()
        ),
    ],
    wrapper_class=structlog.make_filtering_bound_logger(
        structlog.processors.NAME_TO_LEVEL.get(config.logging.level.lower(), 20)
    ),
    context_class=dict,
    logger_factory=structlog.PrintLoggerFactory(),
    cache_logger_on_first_use=True,
)

logger = structlog.get_logger()


@asynccontextmanager
async def lifespan(app: FastAPI):
    """Application lifespan: start TCP server and background tasks."""
    # Initialize database
    await init_db(config.database.url)
    logger.info("database_initialized", url=config.database.url)

    # Startup checks (self-tests + DB integrity)
    if config.startup_checks.enabled:
        await run_startup_checks(
            config.database.url,
            loopback_enabled=config.startup_checks.loopback_enabled,
        )

    # Start TCP server
    tcp_server = await start_tcp_server(
        config.tcp.host,
        config.tcp.port,
        config.encryption.passphrase,
        shared_secret=config.tcp.shared_secret,
        max_line_length=config.tcp.max_line_length,
        max_data_length=config.tcp.max_data_length,
        max_device_name_length=config.tcp.max_device_name_length,
        store_decrypted=config.encryption.store_decrypted,
        max_connections=config.tcp.max_connections,
    )

    # Start retention task
    retention_task = None
    if config.retention.enabled:
        retention_task = asyncio.create_task(
            retention_loop(
                config.retention.max_age_days,
                config.retention.check_interval_hours,
            )
        )

    logger.info(
        "homeserver_started",
        tcp_port=config.tcp.port,
        http_port=config.http.port,
        auth_enabled=config.auth.enabled,
    )

    yield

    # Shutdown
    logger.info("homeserver_shutting_down")
    tcp_server.close()
    await tcp_server.wait_closed()

    if retention_task:
        retention_task.cancel()
        try:
            await retention_task
        except asyncio.CancelledError:
            pass

    await close_db()


# Create FastAPI app
app = FastAPI(
    title="ESP32 LoRa Mesh HomeServer",
    version="1.0.0",
    lifespan=lifespan,
)


# Auth middleware
if config.auth.enabled:
    app.add_middleware(APIKeyMiddleware, api_key=config.auth.api_key)

# CORS middleware
if config.cors.enabled:
    app.add_middleware(
        CORSMiddleware,
        allow_origins=config.cors.allow_origins,
        allow_methods=config.cors.allow_methods,
        allow_headers=config.cors.allow_headers,
        allow_credentials=config.cors.allow_credentials,
    )

# Rate limiting
if config.rate_limit.enabled:
    try:
        from slowapi import Limiter, _rate_limit_exceeded_handler
        from slowapi.errors import RateLimitExceeded
        from slowapi.util import get_remote_address

        limiter = Limiter(key_func=get_remote_address, default_limits=[
            f"{config.rate_limit.requests_per_minute}/minute"
        ])
        app.state.limiter = limiter
        app.add_exception_handler(RateLimitExceeded, _rate_limit_exceeded_handler)
    except ImportError:
        logger.warning("slowapi not installed, rate limiting disabled")

# Include API routes
app.include_router(api_router)

# Health check endpoint
app.get("/health", response_model=HealthResponse)(create_health_endpoint())

# GUI (static files) - mount after API routes to avoid shadowing /api
if config.gui.enabled:
    gui_dir = Path(__file__).parent / config.gui.path
    if gui_dir.exists():
        app.mount(config.gui.mount_path, StaticFiles(directory=gui_dir, html=True), name="gui")
    else:
        logger.warning("gui_path_not_found", path=str(gui_dir))


# Configure WebSocket connection limits
ws_manager.max_connections = config.auth.max_ws_connections

# WebSocket endpoint
@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    if config.auth.enabled:
        key = websocket.headers.get("X-API-Key") or websocket.query_params.get("api_key")
        if key != config.auth.api_key:
            await websocket.close(code=1008)
            return
    await ws_manager.connect(websocket)
    try:
        while True:
            # Keep connection alive, read any client messages (unused for now)
            await websocket.receive_text()
    except WebSocketDisconnect:
        await ws_manager.disconnect(websocket)
    except Exception:
        await ws_manager.disconnect(websocket)


if __name__ == "__main__":
    uvicorn.run(
        "main:app",
        host=config.http.host,
        port=config.http.port,
        log_level=config.logging.level.lower(),
    )

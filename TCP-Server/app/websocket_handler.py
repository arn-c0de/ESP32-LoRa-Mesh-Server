"""
WebSocket manager for real-time message broadcasting.
"""

import asyncio
import json
from datetime import datetime, timezone
from typing import Any

from fastapi import WebSocket
import structlog

logger = structlog.get_logger()


class WebSocketManager:
    """Manages WebSocket connections and broadcasts messages to all connected clients."""

    def __init__(self, max_connections: int = 100):
        self.active_connections: list[WebSocket] = []
        self._lock = asyncio.Lock()
        self.max_connections = max_connections

    async def connect(self, websocket: WebSocket):
        async with self._lock:
            if len(self.active_connections) >= self.max_connections:
                await websocket.close(code=1008)
                return
        await websocket.accept()
        async with self._lock:
            self.active_connections.append(websocket)
        logger.info("websocket_connected", total=len(self.active_connections))

    async def disconnect(self, websocket: WebSocket):
        async with self._lock:
            if websocket in self.active_connections:
                self.active_connections.remove(websocket)
        logger.info("websocket_disconnected", total=len(self.active_connections))

    async def broadcast(self, data: dict[str, Any]):
        """Broadcast a message to all connected WebSocket clients."""
        if not self.active_connections:
            return

        # Serialize datetime objects
        payload = _serialize(data)
        message = json.dumps(payload)

        async with self._lock:
            stale = []
            for conn in self.active_connections:
                try:
                    await conn.send_text(message)
                except Exception:
                    stale.append(conn)

            for conn in stale:
                self.active_connections.remove(conn)

    @property
    def connection_count(self) -> int:
        return len(self.active_connections)


def _serialize(obj: Any) -> Any:
    """Recursively serialize objects for JSON."""
    if isinstance(obj, datetime):
        return obj.isoformat()
    if isinstance(obj, dict):
        return {k: _serialize(v) for k, v in obj.items()}
    if isinstance(obj, list):
        return [_serialize(v) for v in obj]
    return obj


# Global instance
ws_manager = WebSocketManager()

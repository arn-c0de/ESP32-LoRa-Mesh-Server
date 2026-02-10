"""
REST API routes for HomeServer.
"""

import time
from datetime import datetime, timedelta, timezone
from typing import Optional

from fastapi import APIRouter, Depends, Query, Request, HTTPException
from sqlalchemy import func, select
from sqlalchemy.ext.asyncio import AsyncSession

from app.config import load_config
from app.crypto_handler import decrypt_message, try_decrypt_data
from app.database import get_session
from app.models import Message, Node
from app.schemas import (
    CommandRequest,
    CommandResponse,
    DecryptRequest,
    DecryptResponse,
    HealthResponse,
    MessageResponse,
    NodeResponse,
    StatsResponse,
)
from app.tcp_server import connected_nodes
from app.websocket_handler import ws_manager

router = APIRouter(prefix="/api/v1")
start_time = time.time()
config = load_config()


async def get_db():
    async with get_session() as session:
        yield session


@router.get("/messages", response_model=list[MessageResponse])
async def get_messages(
    from_id: Optional[int] = Query(None, description="Filter by sender node ID"),
    to_id: Optional[int] = Query(None, description="Filter by recipient node ID"),
    since: Optional[datetime] = Query(None, description="Messages after this timestamp"),
    limit: int = Query(50, ge=1, le=500),
    offset: int = Query(0, ge=0),
    db: AsyncSession = Depends(get_db),
):
    """Get paginated messages with optional filters."""
    query = select(Message).order_by(Message.timestamp.desc())

    if from_id is not None:
        query = query.where(Message.from_id == from_id)
    if to_id is not None:
        query = query.where(Message.to_id == to_id)
    if since is not None:
        query = query.where(Message.timestamp >= since)

    query = query.offset(offset).limit(limit)
    result = await db.execute(query)
    return result.scalars().all()


@router.get("/nodes", response_model=list[NodeResponse])
async def get_nodes(db: AsyncSession = Depends(get_db)):
    """Get all known nodes with online status."""
    result = await db.execute(select(Node).order_by(Node.id))
    return result.scalars().all()


@router.get("/stats", response_model=StatsResponse)
async def get_stats(db: AsyncSession = Depends(get_db)):
    """Get aggregate statistics."""
    total_msgs = await db.execute(select(func.count(Message.id)))
    total_nodes = await db.execute(select(func.count(Node.id)))
    online_nodes = await db.execute(
        select(func.count(Node.id)).where(Node.is_online == True)  # noqa: E712
    )

    one_hour_ago = datetime.now(timezone.utc) - timedelta(hours=1)
    msgs_last_hour = await db.execute(
        select(func.count(Message.id)).where(Message.timestamp >= one_hour_ago)
    )

    return StatsResponse(
        total_messages=total_msgs.scalar() or 0,
        total_nodes=total_nodes.scalar() or 0,
        online_nodes=online_nodes.scalar() or 0,
        messages_last_hour=msgs_last_hour.scalar() or 0,
        uptime_seconds=time.time() - start_time,
    )


@router.post("/decrypt/{message_id}", response_model=DecryptResponse)
async def decrypt_message_endpoint(
    message_id: int,
    request: DecryptRequest,
    http_request: Request,
    db: AsyncSession = Depends(get_db),
):
    """Decrypt a stored encrypted message with a passphrase."""
    if config.auth.require_for_decrypt:
        if not config.auth.api_key:
            return DecryptResponse(
                message_id=message_id,
                success=False,
                error="Auth not configured",
            )
        key = http_request.headers.get("X-API-Key", "")
        if key != config.auth.api_key:
            return DecryptResponse(
                message_id=message_id,
                success=False,
                error="Unauthorized",
            )
    result = await db.execute(select(Message).where(Message.id == message_id))
    msg = result.scalar_one_or_none()

    if not msg:
        return DecryptResponse(
            message_id=message_id, success=False, error="Message not found"
        )

    if not msg.is_encrypted:
        return DecryptResponse(
            message_id=message_id, success=False, error="Message is not encrypted"
        )

    decrypted = try_decrypt_data(msg.data, request.passphrase)
    if decrypted is None:
        return DecryptResponse(
            message_id=message_id,
            success=False,
            error="Decryption failed (wrong passphrase or corrupted data)",
        )

    # Store decrypted data
    msg.decrypted_data = decrypted
    await db.commit()

    return DecryptResponse(
        message_id=message_id, decrypted_data=decrypted, success=True
    )


@router.post("/command", response_model=CommandResponse)
async def send_command(
    payload: CommandRequest,
):
    """Send a command to a connected node (or broadcast)."""
    if not payload.command:
        return CommandResponse(success=False, error="Command is empty")

    # Only allow sending to a connected node or broadcast (0)
    if payload.target_id != 0:
        key = f"node_{payload.target_id}"
        conn = connected_nodes.get(key)
        if conn is None:
            return CommandResponse(success=False, error="Target node not connected")

        await conn.send_command(payload.command)
        return CommandResponse(success=True)

    # Broadcast to all connected nodes
    for conn in list(connected_nodes.values()):
        try:
            await conn.send_command(payload.command)
        except Exception:
            continue

    return CommandResponse(success=True)


# Health check (outside /api/v1 prefix, added in main.py)
def create_health_endpoint():
    """Create health check endpoint (added to app directly, not router)."""

    async def health(request: Request):
        if config.auth.enabled and config.auth.require_for_health:
            if not config.auth.api_key:
                raise HTTPException(status_code=401, detail="Auth not configured")
            key = request.headers.get("X-API-Key", "")
            if key != config.auth.api_key:
                raise HTTPException(status_code=401, detail="Unauthorized")
        return HealthResponse(
            status="ok",
            tcp_connections=len(connected_nodes),
            database="connected",
            uptime_seconds=time.time() - start_time,
        )

    return health

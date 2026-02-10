"""
Pydantic schemas for API request/response models.
"""

from datetime import datetime
from typing import Optional

from pydantic import BaseModel


class MessageResponse(BaseModel):
    id: int
    from_id: int
    to_id: int
    hop_count: int
    data: str
    raw_packet: str
    is_encrypted: bool
    decrypted_data: Optional[str] = None
    source_node: Optional[str] = None
    timestamp: datetime

    model_config = {"from_attributes": True}


class NodeResponse(BaseModel):
    id: int
    device_name: Optional[str] = None
    last_seen: datetime
    ip_address: Optional[str] = None
    messages_sent: int
    messages_received: int
    is_online: bool

    model_config = {"from_attributes": True}


class StatsResponse(BaseModel):
    total_messages: int
    total_nodes: int
    online_nodes: int
    messages_last_hour: int
    uptime_seconds: float


class DecryptRequest(BaseModel):
    passphrase: str


class DecryptResponse(BaseModel):
    message_id: int
    decrypted_data: Optional[str] = None
    success: bool
    error: Optional[str] = None


class HealthResponse(BaseModel):
    status: str
    tcp_connections: int
    database: str
    uptime_seconds: float


class CommandRequest(BaseModel):
    target_id: int
    command: str


class CommandResponse(BaseModel):
    success: bool
    error: Optional[str] = None

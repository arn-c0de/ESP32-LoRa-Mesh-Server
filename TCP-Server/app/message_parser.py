"""
Parse ESP32 mesh network messages.
Formats:
  - Mesh packet: FromID:ToID:HopCount:Data
  - Heartbeat: HEARTBEAT:<NodeID>
  - Hello: HELLO:<NodeID>:<DeviceName>
"""

from dataclasses import dataclass
from enum import Enum
from typing import Optional


class MessageType(Enum):
    MESH_PACKET = "mesh_packet"
    HEARTBEAT = "heartbeat"
    HELLO = "hello"
    UNKNOWN = "unknown"


@dataclass
class ParsedMessage:
    msg_type: MessageType
    from_id: Optional[int] = None
    to_id: Optional[int] = None
    hop_count: Optional[int] = None
    data: Optional[str] = None
    device_name: Optional[str] = None
    auth_token: Optional[str] = None
    raw: str = ""


def parse_message(raw: str) -> ParsedMessage:
    """Parse a raw message from an ESP32 node."""
    raw = raw.strip()

    if not raw:
        return ParsedMessage(msg_type=MessageType.UNKNOWN, raw=raw)

    # HEARTBEAT:<NodeID>
    if raw.startswith("HEARTBEAT:"):
        parts = raw.split(":", 1)
        try:
            node_id = int(parts[1])
            return ParsedMessage(
                msg_type=MessageType.HEARTBEAT, from_id=node_id, raw=raw
            )
        except (IndexError, ValueError):
            return ParsedMessage(msg_type=MessageType.UNKNOWN, raw=raw)

    # HELLO:<NodeID>:<DeviceName>[:<AuthToken>]
    if raw.startswith("HELLO:"):
        parts = raw.split(":", 3)
        try:
            node_id = int(parts[1])
            device_name = parts[2] if len(parts) > 2 else ""
            auth_token = parts[3] if len(parts) > 3 else None
            return ParsedMessage(
                msg_type=MessageType.HELLO,
                from_id=node_id,
                device_name=device_name,
                auth_token=auth_token,
                raw=raw,
            )
        except (IndexError, ValueError):
            return ParsedMessage(msg_type=MessageType.UNKNOWN, raw=raw)

    # Mesh packet: FromID:ToID:HopCount:Data
    parts = raw.split(":", 3)
    if len(parts) >= 4:
        try:
            from_id = int(parts[0])
            to_id = int(parts[1])
            hop_count = int(parts[2])
            data = parts[3]
            return ParsedMessage(
                msg_type=MessageType.MESH_PACKET,
                from_id=from_id,
                to_id=to_id,
                hop_count=hop_count,
                data=data,
                raw=raw,
            )
        except ValueError:
            pass

    return ParsedMessage(msg_type=MessageType.UNKNOWN, raw=raw)

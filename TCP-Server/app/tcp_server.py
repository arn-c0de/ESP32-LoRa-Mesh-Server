"""
Async TCP server for receiving ESP32 mesh messages.
Handles HELLO, HEARTBEAT, and mesh packet messages.
"""

import asyncio
from datetime import datetime, timezone

import structlog
from sqlalchemy import select

from app.crypto_handler import try_decrypt_data
from app.database import get_session
from app.message_parser import MessageType, parse_message
from app.models import Message, Node
from app.websocket_handler import ws_manager

logger = structlog.get_logger()

# Track connected ESP32 nodes
connected_nodes: dict[str, "ESP32Connection"] = {}
active_connections = 0
active_lock = asyncio.Lock()

# Defaults used if not overridden by config
DEFAULT_MAX_LINE_LENGTH = 2048
DEFAULT_MAX_DATA_LENGTH = 512
DEFAULT_MAX_DEVICE_NAME_LENGTH = 64


class ESP32Connection:
    """Represents a connected ESP32 node."""

    def __init__(
        self,
        reader: asyncio.StreamReader,
        writer: asyncio.StreamWriter,
        encryption_passphrase: str = "",
        shared_secret: str = "",
        max_line_length: int = DEFAULT_MAX_LINE_LENGTH,
        max_data_length: int = DEFAULT_MAX_DATA_LENGTH,
        max_device_name_length: int = DEFAULT_MAX_DEVICE_NAME_LENGTH,
        store_decrypted: bool = False,
        max_connections: int = 100,
    ):
        self.reader = reader
        self.writer = writer
        self.node_id: int | None = None
        self.device_name: str = ""
        self.addr = writer.get_extra_info("peername")
        self.encryption_passphrase = encryption_passphrase
        self.shared_secret = shared_secret
        self.max_line_length = max_line_length
        self.max_data_length = max_data_length
        self.max_device_name_length = max_device_name_length
        self.store_decrypted = store_decrypted
        self.max_connections = max_connections
        self.last_seen = asyncio.get_event_loop().time()

    async def handle(self):
        """Handle incoming data from this connection."""
        addr_str = f"{self.addr[0]}:{self.addr[1]}" if self.addr else "unknown"
        logger.info("tcp_client_connected", addr=addr_str)

        try:
            global active_connections
            async with active_lock:
                if active_connections >= self.max_connections:
                    logger.warning("tcp_connection_limit", addr=addr_str)
                    self.writer.close()
                    return
                active_connections += 1

            while True:
                data = await self.reader.readline()
                if not data:
                    break
                if len(data) > self.max_line_length:
                    logger.warning("tcp_line_too_long", addr=addr_str, length=len(data))
                    break

                line = data.decode("utf-8", errors="replace").strip()
                if not line:
                    continue

                self.last_seen = asyncio.get_event_loop().time()
                logger.debug("tcp_received", addr=addr_str, data=line)
                await self._process_message(line, addr_str)

        except asyncio.CancelledError:
            pass
        except ConnectionResetError:
            logger.info("tcp_client_reset", addr=addr_str)
        except Exception as e:
            logger.error("tcp_client_error", addr=addr_str, error=str(e))
        finally:
            await self._cleanup(addr_str)
            async with active_lock:
                if active_connections > 0:
                    active_connections -= 1

    async def _process_message(self, line: str, addr_str: str):
        parsed = parse_message(line)

        if parsed.msg_type == MessageType.HELLO:
            if self.shared_secret:
                if not parsed.auth_token or parsed.auth_token != self.shared_secret:
                    logger.warning("node_auth_failed", addr=addr_str)
                    self.writer.close()
                    return
            self.node_id = parsed.from_id
            self.device_name = parsed.device_name or ""
            if len(self.device_name) > self.max_device_name_length:
                self.device_name = self.device_name[: self.max_device_name_length]
                logger.warning(
                    "device_name_truncated",
                    node_id=self.node_id,
                    max_len=self.max_device_name_length,
                )
            key = f"node_{self.node_id}"
            connected_nodes[key] = self
            logger.info("node_hello", node_id=self.node_id, device_name=self.device_name, addr=addr_str)
            await self._upsert_node(addr_str)

        elif parsed.msg_type == MessageType.HEARTBEAT:
            logger.debug("node_heartbeat", node_id=parsed.from_id)
            await self._update_node_seen(parsed.from_id)

        elif parsed.msg_type == MessageType.MESH_PACKET:
            if parsed.data and len(parsed.data) > self.max_data_length:
                logger.warning(
                    "mesh_packet_too_large",
                    from_id=parsed.from_id,
                    length=len(parsed.data),
                    max_len=self.max_data_length,
                )
                return
            logger.info(
                "mesh_packet",
                from_id=parsed.from_id,
                to_id=parsed.to_id,
                hops=parsed.hop_count,
                data_len=len(parsed.data) if parsed.data else 0,
            )
            await self._store_message(parsed, addr_str)

        else:
            logger.warning("unknown_message", addr=addr_str, raw=line[:100])

    async def _upsert_node(self, addr_str: str):
        """Insert or update a node record."""
        async with get_session() as session:
            result = await session.execute(select(Node).where(Node.id == self.node_id))
            node = result.scalar_one_or_none()
            if node:
                node.device_name = self.device_name
                node.last_seen = datetime.now(timezone.utc)
                node.ip_address = addr_str
                node.is_online = True
            else:
                node = Node(
                    id=self.node_id,
                    device_name=self.device_name,
                    last_seen=datetime.now(timezone.utc),
                    ip_address=addr_str,
                    is_online=True,
                )
                session.add(node)
            await session.commit()

    async def _update_node_seen(self, node_id: int):
        """Update node last_seen timestamp."""
        async with get_session() as session:
            result = await session.execute(select(Node).where(Node.id == node_id))
            node = result.scalar_one_or_none()
            if node:
                node.last_seen = datetime.now(timezone.utc)
                node.is_online = True
                await session.commit()

    async def _store_message(self, parsed, addr_str: str):
        """Store a mesh message in the database and broadcast via WebSocket."""
        is_encrypted = parsed.data.startswith("FLAG:") if parsed.data else False
        decrypted = None

        if is_encrypted and self.encryption_passphrase and self.store_decrypted:
            decrypted = try_decrypt_data(parsed.data, self.encryption_passphrase)

        msg = Message(
            from_id=parsed.from_id,
            to_id=parsed.to_id,
            hop_count=parsed.hop_count,
            data=parsed.data or "",
            raw_packet=parsed.raw,
            is_encrypted=is_encrypted,
            decrypted_data=decrypted,
            source_node=str(self.node_id) if self.node_id else addr_str,
            timestamp=datetime.now(timezone.utc),
        )

        async with get_session() as session:
            session.add(msg)
            await session.commit()
            await session.refresh(msg)

            # Update node message counts
            result = await session.execute(
                select(Node).where(Node.id == parsed.from_id)
            )
            node = result.scalar_one_or_none()
            if node:
                node.messages_sent += 1
                node.last_seen = datetime.now(timezone.utc)
                await session.commit()

        # Broadcast to WebSocket clients
        await ws_manager.broadcast(
            {
                "type": "message",
                "id": msg.id,
                "from_id": msg.from_id,
                "to_id": msg.to_id,
                "hop_count": msg.hop_count,
                "data": msg.data,
                "is_encrypted": msg.is_encrypted,
                "decrypted_data": msg.decrypted_data,
                "timestamp": msg.timestamp,
            }
        )

    async def _cleanup(self, addr_str: str):
        """Clean up on disconnect."""
        logger.info("tcp_client_disconnected", addr=addr_str, node_id=self.node_id)

        if self.node_id:
            key = f"node_{self.node_id}"
            # Only remove if this connection is still the active one.
            if connected_nodes.get(key) is self:
                connected_nodes.pop(key, None)

            # Mark node as offline
            if connected_nodes.get(key) is None:
                async with get_session() as session:
                    result = await session.execute(
                        select(Node).where(Node.id == self.node_id)
                    )
                    node = result.scalar_one_or_none()
                    if node:
                        node.is_online = False
                        await session.commit()

        self.writer.close()
        try:
            await self.writer.wait_closed()
        except Exception:
            pass

    async def send_command(self, command: str):
        """Send a command line to this TCP connection."""
        if not self.writer.is_closing():
            self.writer.write((command.strip() + "\n").encode("utf-8"))
            await self.writer.drain()


async def start_tcp_server(
    host: str,
    port: int,
    encryption_passphrase: str = "",
    shared_secret: str = "",
    max_line_length: int = DEFAULT_MAX_LINE_LENGTH,
    max_data_length: int = DEFAULT_MAX_DATA_LENGTH,
    max_device_name_length: int = DEFAULT_MAX_DEVICE_NAME_LENGTH,
    store_decrypted: bool = False,
    max_connections: int = 100,
):
    """Start the async TCP server."""

    async def handle_client(reader: asyncio.StreamReader, writer: asyncio.StreamWriter):
        conn = ESP32Connection(
            reader,
            writer,
            encryption_passphrase=encryption_passphrase,
            shared_secret=shared_secret,
            max_line_length=max_line_length,
            max_data_length=max_data_length,
            max_device_name_length=max_device_name_length,
            store_decrypted=store_decrypted,
            max_connections=max_connections,
        )
        await conn.handle()

    server = await asyncio.start_server(handle_client, host, port)
    if server.sockets:
        addr = server.sockets[0].getsockname()
        logger.info("tcp_server_started", host=addr[0], port=addr[1])
    else:
        logger.warning("tcp_server_started_no_socket")
    return server

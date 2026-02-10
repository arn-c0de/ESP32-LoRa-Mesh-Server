"""
Startup checks for HomeServer.
Includes a lightweight parser self-test and DB integrity check.
"""

import asyncio
import structlog
from sqlalchemy import delete, select, text

from app import database
from app.self_test import run_self_tests
from app.tcp_server import connected_nodes, start_tcp_server
from app.database import get_session
from app.models import Message, Node

logger = structlog.get_logger()


async def _run_tcp_loopback_test():
    """Start a temporary TCP server and validate basic end-to-end flow."""
    test_node_id = 250
    test_device = "startup-check"
    test_payload = "startup-check-msg"

    server = await start_tcp_server(
        "127.0.0.1",
        0,
        encryption_passphrase="",
        shared_secret="",
        store_decrypted=False,
        max_connections=10,
    )
    if not server.sockets:
        server.close()
        await server.wait_closed()
        logger.warning("startup_loopback_skipped_no_socket")
        return
    port = server.sockets[0].getsockname()[1]

    reader, writer = await asyncio.open_connection("127.0.0.1", port)
    writer.write(f"HELLO:{test_node_id}:{test_device}\n".encode("utf-8"))
    writer.write(f"HEARTBEAT:{test_node_id}\n".encode("utf-8"))
    writer.write(f"{test_node_id}:0:1:{test_payload}\n".encode("utf-8"))
    await writer.drain()

    # Allow server to process and poll for DB write (async timing)
    msg = None
    node = None
    # Cleanup any old test data before running
    async with get_session() as session:
        await session.execute(delete(Message).where(Message.from_id == test_node_id))
        await session.execute(delete(Node).where(Node.id == test_node_id))
        await session.commit()
    for _ in range(20):
        await asyncio.sleep(0.1)
        async with get_session() as session:
            msg_result = await session.execute(
                select(Message)
                .where(Message.from_id == test_node_id, Message.data == test_payload)
                .limit(1)
            )
            msg = msg_result.scalar_one_or_none()
            node_result = await session.execute(select(Node).where(Node.id == test_node_id))
            node = node_result.scalar_one_or_none()
        if msg is not None and node is not None and node.is_online is True:
            break

    if msg is None:
        raise RuntimeError("Loopback test: message not stored")
    if node is None or node.is_online is not True:
        raise RuntimeError("Loopback test: node not marked online")

    # Close connection and ensure offline status
    writer.close()
    await writer.wait_closed()
    for _ in range(20):
        await asyncio.sleep(0.1)
        async with get_session() as session:
            node_result = await session.execute(select(Node).where(Node.id == test_node_id))
            node = node_result.scalar_one_or_none()
        if node is not None and node.is_online is False:
            break
    if node is None or node.is_online is not False:
        raise RuntimeError("Loopback test: node not marked offline after disconnect")

    # Cleanup test data (best-effort)
    await session.execute(delete(Message).where(Message.from_id == test_node_id))
    await session.execute(delete(Node).where(Node.id == test_node_id))
    await session.commit()

    # Cleanup server and in-memory connection tracking
    server.close()
    await server.wait_closed()
    connected_nodes.pop(f"node_{test_node_id}", None)


async def run_startup_checks(database_url: str, loopback_enabled: bool = True):
    """Run self-tests and database integrity checks."""
    logger.info("startup_checks_begin")

    # Parser self-tests (fast, deterministic)
    try:
        run_self_tests()
        logger.info("startup_self_tests_ok")
    except Exception as exc:
        logger.error("startup_self_tests_failed", error=str(exc))
        raise

    # Database integrity check (SQLite only)
    if database_url.startswith("sqlite"):
        if database.engine is None:
            raise RuntimeError("Database engine not initialized for integrity check")
        async with database.engine.connect() as conn:
            result = await conn.execute(text("PRAGMA integrity_check"))
            row = result.fetchone()
            status = row[0] if row else None
            if status != "ok":
                logger.error("startup_db_integrity_failed", status=status)
                raise RuntimeError(f"SQLite integrity_check failed: {status}")
            logger.info("startup_db_integrity_ok")

    # TCP loopback integration test
    if loopback_enabled:
        try:
            await _run_tcp_loopback_test()
            logger.info("startup_loopback_ok")
        except Exception as exc:
            logger.warning("startup_loopback_failed", error=str(exc))

    logger.info("startup_checks_ok")

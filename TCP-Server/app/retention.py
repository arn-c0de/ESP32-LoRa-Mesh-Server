"""
Background task for message retention / cleanup.
Deletes messages older than max_age_days.
"""

import asyncio
from datetime import datetime, timedelta, timezone

import structlog
from sqlalchemy import delete

from app.database import get_session
from app.models import Message

logger = structlog.get_logger()


async def retention_loop(max_age_days: int, check_interval_hours: int):
    """Periodically delete old messages."""
    interval_seconds = check_interval_hours * 3600
    logger.info(
        "retention_started",
        max_age_days=max_age_days,
        check_interval_hours=check_interval_hours,
    )

    while True:
        try:
            await asyncio.sleep(interval_seconds)
            cutoff = datetime.now(timezone.utc) - timedelta(days=max_age_days)

            async with get_session() as session:
                result = await session.execute(
                    delete(Message).where(Message.timestamp < cutoff)
                )
                deleted = result.rowcount
                await session.commit()

            if deleted > 0:
                logger.info("retention_cleanup", deleted=deleted, cutoff=cutoff.isoformat())

        except asyncio.CancelledError:
            logger.info("retention_stopped")
            break
        except Exception as e:
            logger.error("retention_error", error=str(e))

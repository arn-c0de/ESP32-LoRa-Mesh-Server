"""
Async SQLAlchemy database engine and session factory.
"""

from sqlalchemy.ext.asyncio import AsyncSession, async_sessionmaker, create_async_engine

from app.models import Base

engine = None
async_session_factory = None


async def init_db(database_url: str):
    """Initialize database engine and create tables."""
    global engine, async_session_factory

    # Enable WAL mode for SQLite
    connect_args = {}
    if "sqlite" in database_url:
        connect_args["check_same_thread"] = False

    engine = create_async_engine(database_url, echo=False, connect_args=connect_args)
    async_session_factory = async_sessionmaker(engine, class_=AsyncSession, expire_on_commit=False)

    async with engine.begin() as conn:
        await conn.run_sync(Base.metadata.create_all)

        # Enable WAL mode for SQLite
        if "sqlite" in database_url:
            await conn.execute(
                __import__("sqlalchemy").text("PRAGMA journal_mode=WAL")
            )


def get_session() -> AsyncSession:
    """Get a new async database session."""
    return async_session_factory()


async def close_db():
    """Close database engine."""
    global engine
    if engine:
        await engine.dispose()

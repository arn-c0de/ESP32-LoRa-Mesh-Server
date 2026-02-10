"""
SQLAlchemy ORM models for HomeServer database.
"""

from datetime import datetime, timezone

from sqlalchemy import Boolean, Column, DateTime, Integer, String, Text
from sqlalchemy.orm import DeclarativeBase


class Base(DeclarativeBase):
    pass


class Message(Base):
    __tablename__ = "messages"

    id = Column(Integer, primary_key=True, autoincrement=True)
    from_id = Column(Integer, nullable=False, index=True)
    to_id = Column(Integer, nullable=False, index=True)
    hop_count = Column(Integer, nullable=False)
    data = Column(Text, nullable=False)
    raw_packet = Column(Text, nullable=False)
    is_encrypted = Column(Boolean, default=False)
    decrypted_data = Column(Text, nullable=True)
    source_node = Column(String(64), nullable=True)  # Node that sent via TCP
    timestamp = Column(
        DateTime, default=lambda: datetime.now(timezone.utc), index=True
    )


class Node(Base):
    __tablename__ = "nodes"

    id = Column(Integer, primary_key=True)  # Node ID from mesh
    device_name = Column(String(64), nullable=True)
    last_seen = Column(DateTime, default=lambda: datetime.now(timezone.utc))
    ip_address = Column(String(45), nullable=True)
    messages_sent = Column(Integer, default=0)
    messages_received = Column(Integer, default=0)
    is_online = Column(Boolean, default=True)

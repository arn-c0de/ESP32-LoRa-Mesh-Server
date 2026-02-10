"""
Minimal self-test for TCP server message parsing and crypto handling.
Run: python self_test.py
"""

import os
import sys

# Allow running as script: python app/self_test.py
if __package__ is None:
    sys.path.append(os.path.dirname(os.path.dirname(__file__)))

import base64
import hashlib
from app.message_parser import MessageType, parse_message
from cryptography.hazmat.primitives.ciphers.aead import AESGCM

from app.crypto_handler import decrypt_message, try_decrypt_data


def assert_equal(actual, expected, label):
    if actual != expected:
        raise AssertionError(f"{label}: expected {expected!r}, got {actual!r}")


def test_hello():
    msg = "HELLO:7:NodeAlpha"
    parsed = parse_message(msg)
    assert_equal(parsed.msg_type, MessageType.HELLO, "HELLO type")
    assert_equal(parsed.from_id, 7, "HELLO from_id")
    assert_equal(parsed.device_name, "NodeAlpha", "HELLO device_name")


def test_heartbeat():
    msg = "HEARTBEAT:12"
    parsed = parse_message(msg)
    assert_equal(parsed.msg_type, MessageType.HEARTBEAT, "HEARTBEAT type")
    assert_equal(parsed.from_id, 12, "HEARTBEAT from_id")


def test_mesh_packet():
    msg = "1:0:3:Hello world"
    parsed = parse_message(msg)
    assert_equal(parsed.msg_type, MessageType.MESH_PACKET, "MESH type")
    assert_equal(parsed.from_id, 1, "MESH from_id")
    assert_equal(parsed.to_id, 0, "MESH to_id")
    assert_equal(parsed.hop_count, 3, "MESH hop_count")
    assert_equal(parsed.data, "Hello world", "MESH data")


def test_unknown():
    msg = "BROKEN"
    parsed = parse_message(msg)
    assert_equal(parsed.msg_type, MessageType.UNKNOWN, "UNKNOWN type")


def test_decrypt_flag_message():
    passphrase = "test-passphrase"
    plaintext = "Hello secure world"
    sender = "42"

    key = hashlib.sha256(passphrase.encode()).digest()
    iv = bytes([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11])
    aesgcm = AESGCM(key)
    ciphertext = aesgcm.encrypt(iv, plaintext.encode(), None)
    blob = base64.b64encode(iv + ciphertext).decode("utf-8")

    # Direct decrypt_message test
    direct = decrypt_message(blob, passphrase)
    assert_equal(direct, plaintext, "decrypt_message")

    # FLAG envelope test
    data = f"FLAG:{sender}:{blob}"
    decoded = try_decrypt_data(data, passphrase)
    assert_equal(decoded, plaintext, "try_decrypt_data")


def run_self_tests() -> bool:
    tests = [test_hello, test_heartbeat, test_mesh_packet, test_unknown, test_decrypt_flag_message]
    for t in tests:
        t()
    return True


def main():
    run_self_tests()
    print("OK: all self-tests passed")


if __name__ == "__main__":
    main()

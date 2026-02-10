"""
AES-256-GCM decryption handler.
Mirrors the ESP32 crypt.h implementation:
  - Key derivation: SHA-256(passphrase)
  - Payload format: Base64(IV[12] || Ciphertext || Tag[16])
"""

import base64
import hashlib
from typing import Optional

from cryptography.hazmat.primitives.ciphers.aead import AESGCM


def derive_key(passphrase: str) -> bytes:
    """Derive AES-256 key from passphrase using SHA-256 (same as ESP32)."""
    return hashlib.sha256(passphrase.encode()).digest()


def decrypt_message(base64_blob: str, passphrase: str) -> Optional[str]:
    """
    Decrypt a FLAG message payload.
    The base64_blob is the Base64-encoded binary: IV(12) || Ciphertext || Tag(16).
    Returns decrypted plaintext or None on failure.
    """
    try:
        binary = base64.b64decode(base64_blob)
    except Exception:
        return None

    iv_size = 12
    tag_size = 16

    if len(binary) < iv_size + tag_size:
        return None

    iv = binary[:iv_size]
    ciphertext = binary[iv_size:-tag_size]
    tag = binary[-tag_size:]

    key = derive_key(passphrase)

    try:
        aesgcm = AESGCM(key)
        # AESGCM expects ciphertext + tag concatenated
        plaintext = aesgcm.decrypt(iv, ciphertext + tag, None)
        return plaintext.decode("utf-8")
    except Exception:
        return None


def try_decrypt_data(data: str, passphrase: str) -> Optional[str]:
    """
    Try to decrypt a mesh message data field.
    If it starts with FLAG:<senderInfo>:<base64blob>, extract and decrypt.
    Returns decrypted text or None.
    """
    if not data.startswith("FLAG:") or not passphrase:
        return None

    # FLAG:<SenderInfo>:<Base64Blob>
    rest = data[5:]  # Skip "FLAG:"
    colon_pos = rest.find(":")
    if colon_pos < 0:
        return None

    base64_blob = rest[colon_pos + 1 :]
    return decrypt_message(base64_blob, passphrase)

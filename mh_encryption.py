#!/usr/bin/env python3
"""
Encryption utilities for secure credential storage.

Uses Fernet (symmetric encryption) to encrypt sensitive data at rest.
"""

import os
import base64
import logging
from cryptography.fernet import Fernet
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.kdf.pbkdf2 import PBKDF2HMAC

logger = logging.getLogger(__name__)

# Default master key file location
MASTER_KEY_FILE = "data/master.key"


def get_master_key():
    """
    Get or generate master encryption key.
    """
    key_path = os.path.join(os.path.dirname(__file__), MASTER_KEY_FILE)
    key_dir = os.path.dirname(key_path)
    
    # Create directory if it doesn't exist
    os.makedirs(key_dir, exist_ok=True)
    
    if os.path.exists(key_path):
        # Read existing key
        with open(key_path, 'rb') as f:
            key = f.read()
    else:
        # Generate new key
        key = Fernet.generate_key()
        # Store key with restricted permissions (owner read/write only)
        with open(key_path, 'wb') as f:
            f.write(key)
        os.chmod(key_path, 0o600)  # rw------- (owner only)
        logger.info(f"Generated new master encryption key at {key_path}")
    
    return key


def get_encryption_key():
    """
    Derive encryption key from master key using PBKDF2.
    This allows key rotation without re-encrypting all data.
    """
    master_key = get_master_key()
    # Use a salt derived from the master key for consistency
    salt = master_key[:16]  # Use first 16 bytes as salt
    
    kdf = PBKDF2HMAC(
        algorithm=hashes.SHA256(),
        length=32,
        salt=salt,
        iterations=100000,
    )
    key = base64.urlsafe_b64encode(kdf.derive(master_key))
    return key


def encrypt_data(data):
    """
    Encrypt string or dict data.
    
    Args:
        data: String or dict to encrypt
        
    Returns:
        Encrypted string (base64 encoded)
    """
    if isinstance(data, dict):
        import json
        data = json.dumps(data)
    
    if not isinstance(data, str):
        data = str(data)
    
    key = get_encryption_key()
    f = Fernet(key)
    encrypted = f.encrypt(data.encode('utf-8'))
    return base64.b64encode(encrypted).decode('utf-8')


def decrypt_data(encrypted_data):
    """
    Decrypt encrypted data.
    
    Args:
        encrypted_data: Base64-encoded encrypted string
        
    Returns:
        Decrypted string
    """
    try:
        key = get_encryption_key()
        f = Fernet(key)
        encrypted_bytes = base64.b64decode(encrypted_data.encode('utf-8'))
        decrypted = f.decrypt(encrypted_bytes)
        return decrypted.decode('utf-8')
    except Exception as e:
        logger.error(f"Decryption failed: {e}")
        raise


def is_encrypted(data):
    """
    Check if data appears to be encrypted (base64 format check).
    
    Args:
        data: String to check
        
    Returns:
        True if data looks encrypted, False otherwise
    """
    if not isinstance(data, str):
        return False
    
    try:
        # Try to decode as base64
        base64.b64decode(data)
        # If it's valid base64 and reasonably long, assume encrypted
        return len(data) > 20
    except:
        return False


def encrypt_json_file(file_path, data):
    """
    Encrypt and write JSON data to file.
    
    Args:
        file_path: Path to file
        data: Dict to encrypt and write
    """
    encrypted = encrypt_data(data)
    with open(file_path, 'w') as f:
        f.write(encrypted)
    # Restrict file permissions
    os.chmod(file_path, 0o600)  # rw------- (owner only)


def decrypt_json_file(file_path):
    """
    Read and decrypt JSON file.
    
    Args:
        file_path: Path to encrypted file
        
    Returns:
        Decrypted dict
    """
    import json
    
    with open(file_path, 'r') as f:
        encrypted = f.read().strip()
    
    # Check if file is encrypted or plaintext (for backward compatibility)
    if is_encrypted(encrypted):
        decrypted = decrypt_data(encrypted)
        return json.loads(decrypted)
    else:
        # Legacy plaintext file - try to parse as JSON
        logger.warning(f"Reading plaintext file {file_path} - consider encrypting it")
        return json.loads(encrypted)


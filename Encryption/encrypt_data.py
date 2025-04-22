from cryptography.hazmat.primitives.asymmetric import padding
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from cryptography.hazmat.primitives import keywrap
from cryptography.hazmat.backends import default_backend
from pathlib import Path
import os

def encrypt_data():
    base_path = Path(__file__).parent

    # Load public key
    with open(base_path / "public_key.pem", "rb") as f:
        public_key = serialization.load_pem_public_key(f.read(), backend=default_backend())

    # Load data
    with open(base_path / "data.txt", "rb") as f:
        plaintext = f.read()

    # Generate AES key and IV
    aes_key = os.urandom(32)  # AES-256
    iv = os.urandom(16)       # 128-bit IV

    # Encrypt data with AES
    cipher = Cipher(algorithms.AES(aes_key), modes.CFB(iv), backend=default_backend())
    encryptor = cipher.encryptor()
    encrypted_data = encryptor.update(plaintext) + encryptor.finalize()

    # Encrypt AES key with RSA public key
    encrypted_key = public_key.encrypt(
        aes_key,
        padding.OAEP(
            mgf=padding.MGF1(algorithm=hashes.SHA256()),
            algorithm=hashes.SHA256(),
            label=None
        )
    )

    # Save encrypted AES key + IV + data
    with open(base_path / "encrypted_data.bin", "wb") as f:
        f.write(len(encrypted_key).to_bytes(2, "big"))
        f.write(encrypted_key)
        f.write(iv)
        f.write(encrypted_data)

    print("[+] Encryption complete. File saved as 'encrypted_data.bin'")

if __name__ == "__main__":
    encrypt_data()

from cryptography.hazmat.primitives.asymmetric import padding
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from cryptography.hazmat.backends import default_backend
from pathlib import Path

def decrypt_data():
    base_path = Path(__file__).parent

    # Load private key
    with open(base_path / "private_key.pem", "rb") as f:
        private_key = serialization.load_pem_private_key(f.read(), password=None, backend=default_backend())

    # Read the encrypted file
    with open(base_path / "encrypted_data.bin", "rb") as f:
        key_len = int.from_bytes(f.read(2), "big")
        encrypted_key = f.read(key_len)
        iv = f.read(16)
        encrypted_data = f.read()

    # Decrypt AES key with RSA private key
    aes_key = private_key.decrypt(
        encrypted_key,
        padding.OAEP(
            mgf=padding.MGF1(algorithm=hashes.SHA256()),
            algorithm=hashes.SHA256(),
            label=None
        )
    )

    # Decrypt the actual data
    cipher = Cipher(algorithms.AES(aes_key), modes.CFB(iv), backend=default_backend())
    decryptor = cipher.decryptor()
    decrypted_data = decryptor.update(encrypted_data) + decryptor.finalize()

    # Save or print
    with open(base_path / "decrypted_output.txt", "wb") as f:
        f.write(decrypted_data)

    print("[+] Decryption complete. Output written to 'decrypted_output.txt'")

if __name__ == "__main__":
    decrypt_data()

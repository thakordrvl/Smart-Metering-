import os
import secrets
import argparse

def generate_secure_key(size=16):
    """Generate cryptographically secure random bytes using secrets module"""
    return secrets.token_bytes(size)

def format_c_array(name: str, data: bytes) -> str:
    """Format bytes as a C/C++ array declaration"""
    elems = ", ".join(f"0x{b:02X}" for b in data)
    return f"const byte {name}[{len(data)}] = {{ {elems} }};"

def format_python_bytes(name: str, data: bytes) -> str:
    """Format bytes as a Python bytes array"""
    elems = ", ".join(f"0x{b:02X}" for b in data)
    return f"{name} = bytes([{elems}])"

def main():
    parser = argparse.ArgumentParser(description="Generate AES keys and IVs for ESP and Python")
    parser.add_argument("--size", type=int, default=16, 
                        help="Key size in bytes (16 for AES-128, 24 for AES-192, 32 for AES-256)")
    parser.add_argument("--output", type=str, default=None,
                        help="Output file (default: print to console)")
    args = parser.parse_args()
    
    # Generate random bytes for AES key and IV
    key = generate_secure_key(args.size)
    iv = generate_secure_key(16)  # IV is always 16 bytes for AES
    
    output = []
    output.append("// ---- AES Key and IV for ESP32/ESP8266 (C/C++) ----")
    output.append(format_c_array("AES_KEY", key))
    output.append(format_c_array("AES_IV", iv))
    output.append("\n# ---- AES Key and IV for Python Server ----")
    output.append(format_python_bytes("AES_KEY", key))
    output.append(format_python_bytes("AES_IV", iv))
    output.append("\n# The key is stored in a secure format. Keep this information confidential.")
    output.append(f"# Key strength: AES-{len(key)*8} ({len(key)} bytes)")
    
    if args.output:
        with open(args.output, 'w') as f:
            f.write('\n'.join(output))
        print(f"Key material written to {args.output}")
    else:
        print('\n'.join(output))

if __name__ == "__main__":
    main()
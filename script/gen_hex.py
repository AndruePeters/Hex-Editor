#!/usr/bin/env python3
import struct
import zlib
import time
import random
import string

def calculate_crc32(data: bytes) -> int:
    return zlib.crc32(data) & 0xFFFFFFFF

def build_packet(packet_type: int, payload: bytes, corrupt_crc: bool = False) -> bytes:
    header = struct.pack(">IH", packet_type, len(payload))
    packet_data = header + payload
    
    crc = calculate_crc32(packet_data)
    if corrupt_crc:
        crc = crc ^ 0xFFFFFFFF 
        
    return packet_data + struct.pack(">I", crc)

def create_string_packet(text: str, corrupt: bool = False) -> bytes:
    payload = text.encode('ascii')[:65535]
    return build_packet(0x00000000, payload, corrupt)

def create_sensor_packet(x: float, y: float, z: float) -> bytes:
    payload = struct.pack("<fff", x, y, z)
    return build_packet(0x00000001, payload)

def create_timestamp_packet(ts: int) -> bytes:
    payload = struct.pack(">Q", ts)
    return build_packet(0x00000002, payload)

def generate_large_file(filename: str, num_packets: int = 15000, inject_errors: bool = False):
    file_data = bytearray()
    
    # 1. 32-Byte Header
    if inject_errors:
        file_data.extend(struct.pack(">I", 0xDEADBEEF) * 3 + struct.pack(">I", 0xDEADDEAD) + struct.pack(">I", 0xDEADBEEF) * 4)
    else:
        file_data.extend(struct.pack(">I", 0xDEADBEEF) * 8)
        
    # 2. Generate Packets
    print(f"Generating {num_packets} packets for {filename}...")
    for i in range(num_packets):
        choice = random.random()
        corrupt_this = inject_errors and (random.random() < 0.05) 
        
        if choice < 0.1:
            # 10% Large String Packets (100 to 300 characters)
            length = random.randint(100, 300)
            random_text = ''.join(random.choices(string.ascii_letters + string.digits + " \n.,", k=length))
            file_data.extend(create_string_packet(f"LARGE_BLOCK_{i}: " + random_text, corrupt=corrupt_this))
        elif choice < 0.5:
            # 40% Small String Packets
            file_data.extend(create_string_packet(f"System status message sequence {i}", corrupt=corrupt_this))
        elif choice < 0.75:
            # 25% Sensor Packets
            file_data.extend(create_sensor_packet(random.uniform(-100, 100), random.uniform(-100, 100), random.uniform(0, 10)))
        else:
            # 25% Timestamp Packets
            file_data.extend(create_timestamp_packet(int(time.time()) + i * 60))
            
    # 3. File Footer CRC
    file_crc = calculate_crc32(bytes(file_data))
    if inject_errors:
        file_crc = file_crc ^ 0x0000FFFF
        
    file_data.extend(struct.pack(">I", file_crc))

    with open(filename, "wb") as f:
        f.write(file_data)
        
    print(f"Completed {filename} ({len(file_data) / 1024:.2f} KB)")

if __name__ == "__main__":
    # Generates roughly ~1500 KB to ~2000 KB files
    generate_large_file("valid_large_data.bin", num_packets=15000, inject_errors=False)
    generate_large_file("corrupted_large_data.bin", num_packets=15000, inject_errors=True)
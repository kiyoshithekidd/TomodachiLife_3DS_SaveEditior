import sys
import os

def decompress_lz11(data):
    if len(data) == 0: return b""
    header = data[0]
    if header != 0x11: return data
    decompressed_size = int.from_bytes(data[1:4], 'little')
    if decompressed_size == 0:
        decompressed_size = int.from_bytes(data[4:8], 'little')
        src_pos = 8
    else:
        src_pos = 4
    out = bytearray()
    while src_pos < len(data) and len(out) < decompressed_size:
        flag = data[src_pos]
        src_pos += 1
        for i in range(8):
            if len(out) >= decompressed_size or src_pos >= len(data): break
            if (flag & 0x80) != 0:
                b1 = data[src_pos]
                src_pos += 1
                length = b1 >> 4
                if length == 0:
                    b2 = data[src_pos]
                    b3 = data[src_pos+1]
                    src_pos += 2
                    length = ((b1 & 0x0F) << 4) | (b2 >> 4)
                    length += 0x11
                    disp = ((b2 & 0x0F) << 8) | b3
                elif length == 1:
                    b2 = data[src_pos]
                    b3 = data[src_pos+1]
                    b4 = data[src_pos+2]
                    src_pos += 3
                    length = ((b1 & 0x0F) << 12) | (b2 << 4) | (b3 >> 4)
                    length += 0x111
                    disp = ((b3 & 0x0F) << 8) | b4
                else:
                    b2 = data[src_pos]
                    src_pos += 1
                    length += 1
                    disp = ((b1 & 0x0F) << 8) | b2
                disp += 1
                for _ in range(length): out.append(out[-disp])
            else:
                out.append(data[src_pos])
                src_pos += 1
            flag <<= 1
    return bytes(out)

def parse_msbt(data):
    if data[0:8] != b'MsgStdBn': return []
    num_sections = int.from_bytes(data[14:16], 'little')
    pos = 32
    txt2_data = None
    for _ in range(num_sections):
        magic = data[pos:pos+4]
        size = int.from_bytes(data[pos+4:pos+8], 'little')
        pos += 16 
        if magic == b'TXT2':
            txt2_data = data[pos:pos+size]
            break
        pos += size
        if pos % 16 != 0: pos += 16 - (pos % 16)
    if not txt2_data: return []
    string_count = int.from_bytes(txt2_data[0:4], 'little')
    offsets = [int.from_bytes(txt2_data[4 + i*4: 8 + i*4], 'little') for i in range(string_count)]
    strings = []
    for i in range(string_count):
        start = offsets[i]
        end = offsets[i+1] if i < string_count - 1 else len(txt2_data)
        text = txt2_data[start:end].decode('utf-16-le', errors='ignore').split('\x00')[0]
        cleaned = ''.join(c for c in text if c.isprintable()).replace('"', '\\"')
        if not cleaned.strip(): cleaned = f"Unknown_{i}"
        strings.append(cleaned)
    return strings

def process_file(path, out_name):
    if not os.path.exists(path): return
    data = open(path, "rb").read()
    dec = decompress_lz11(data)
    idx = 0
    all_strings = []
    while True:
        idx = dec.find(b'MsgStdBn', idx)
        if idx == -1: break
        all_strings.extend(parse_msbt(dec[idx:]))
        idx += 8
    
    with open(out_name, "w", encoding='utf-8') as f:
        f.write("#pragma once\n\n")
        f.write(f"const char* {out_name.split('.')[0].upper()}_NAMES[] = {{\n")
        for s in all_strings: f.write(f'    "{s}",\n')
        f.write("};\n")
    print(f"Wrote {len(all_strings)} items to {out_name}")

def main():
    base = r"c:\Users\Kiyos\Documents\Antigravity_Projects\romfs\message"
    process_file(os.path.join(base, "Room", "Room_US_English_LZ.bin"), "room_data.h")
    process_file(os.path.join(base, "Material", "Material_US_English_LZ.bin"), "treasure_data.h")

if __name__ == "__main__":
    main()

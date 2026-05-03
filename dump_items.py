import sys
import os

def decompress_lz11(data):
    if len(data) == 0: return b""
    header = data[0]
    if header != 0x11:
        # Might not be compressed
        return data
    
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
            if len(out) >= decompressed_size or src_pos >= len(data):
                break
                
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
                for _ in range(length):
                    out.append(out[-disp])
            else:
                out.append(data[src_pos])
                src_pos += 1
            flag <<= 1
            
    return bytes(out)

def parse_msbt(data):
    if data[0:8] != b'MsgStdBn':
        raise ValueError("Not an MSBT file")
        
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
        if pos % 16 != 0:
            pos += 16 - (pos % 16)
            
    if not txt2_data:
        raise ValueError("TXT2 section not found")
        
    string_count = int.from_bytes(txt2_data[0:4], 'little')
    offsets = []
    for i in range(string_count):
        offsets.append(int.from_bytes(txt2_data[4 + i*4: 8 + i*4], 'little'))
        
    strings = []
    for i in range(string_count):
        start = offsets[i]
        if i < string_count - 1:
            end = offsets[i+1]
        else:
            end = len(txt2_data)
            
        raw_str = txt2_data[start:end]
        text = raw_str.decode('utf-16-le', errors='ignore').split('\x00')[0]
        cleaned = ''.join(c for c in text if c.isprintable())
        cleaned = cleaned.replace('"', '\\"')
        
        if not cleaned.strip():
            cleaned = f"Unknown_Item_{i}"
            
        strings.append(cleaned)
            
    return strings

def extract_msbts_from_darc(data):
    offsets = []
    idx = 0
    while True:
        idx = data.find(b'MsgStdBn', idx)
        if idx == -1:
            break
        offsets.append(idx)
        idx += 8
    
    all_strings = []
    for offset in offsets:
        try:
            strings = parse_msbt(data[offset:])
            all_strings.extend(strings)
        except Exception as e:
            print(f"Failed to parse MSBT at offset {offset}: {e}")
            
    return all_strings

def main():
    romfs_dir = r"c:\Users\Kiyos\Documents\Antigravity_Projects\romfs"
    item_file = os.path.join(romfs_dir, "message", "Item", "Item_US_English_LZ.bin")
    clothes_file = os.path.join(romfs_dir, "message", "Clothes", "Clothes_US_English_LZ.bin")
    
    out_dir = r"c:\Users\Kiyos\Documents\Antigravity_Projects\Tomodachi_Life_Save_Editor\TomodachiLife_3DS_SaveEditor\include"
    if not os.path.exists(out_dir):
        os.makedirs(out_dir)

    # Process Items
    if os.path.exists(item_file):
        print("Decompressing Items...")
        data = open(item_file, "rb").read()
        print(f"Raw hex: {data[:32].hex()}")
        dec = decompress_lz11(data)
        print(f"Dec hex: {dec[:32].hex()}")
        try:
            strings = extract_msbts_from_darc(dec)
            if not strings:
                print("No MSBTs found inside the DARC archive.")
            else:
                with open(os.path.join(out_dir, "item_dataset.h"), "w", encoding='utf-8') as f:
                    f.write("#pragma once\n\n")
                    f.write(f"const int ITEM_COUNT = {len(strings)};\n")
                    f.write("const char* ITEM_NAMES[] = {\n")
                    for s in strings:
                        f.write(f'    "{s}",\n')
                    f.write("};\n")
                print(f"Generated item_dataset.h with {len(strings)} items.")
        except Exception as e:
            print(f"Error parsing items: {e}")
    else:
        print(f"File not found: {item_file}")

    # Process Clothes
    if os.path.exists(clothes_file):
        print("Decompressing Clothes...")
        data = open(clothes_file, "rb").read()
        print(f"Raw Clothes hex: {data[:32].hex()}")
        dec = decompress_lz11(data)
        print(f"Dec Clothes hex: {dec[:32].hex()}")
        try:
            strings = extract_msbts_from_darc(dec)
            if not strings:
                print("No MSBTs found inside the Clothes DARC archive.")
            else:
                with open(os.path.join(out_dir, "clothes_dataset.h"), "w", encoding='utf-8') as f:
                    f.write("#pragma once\n\n")
                    f.write(f"const int CLOTHES_COUNT = {len(strings)};\n")
                    f.write("const char* CLOTHES_NAMES[] = {\n")
                    for s in strings:
                        f.write(f'    "{s}",\n')
                    f.write("};\n")
                print(f"Generated clothes_dataset.h with {len(strings)} clothes.")
        except Exception as e:
            print(f"Error parsing clothes: {e}")
    else:
        print(f"File not found: {clothes_file}")

if __name__ == "__main__":
    main()

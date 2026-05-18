import csv
import re
import os

def build_treasures():
    csv_path = r'c:\Users\Kiyos\Documents\Antigravity_Projects\Tomodachi_Life_Save_Editor\TomodachiLife_3DS_SaveEditor\scratch\treasures.csv'
    out_path = r'c:\Users\Kiyos\Documents\Antigravity_Projects\Tomodachi_Life_Save_Editor\TomodachiLife_3DS_SaveEditor\include\treasures_dataset.h'
    
    with open(csv_path, 'r', encoding='utf-8') as f:
        content = f.read()
        
    # Manual parsing of treasures.csv to extract columns
    rows = []
    current_row = []
    current_field = ""
    in_quotes = False
    
    for c in content:
        if c == '"':
            in_quotes = not in_quotes
        elif c == ',' and not in_quotes:
            current_row.append(current_field.strip())
            current_field = ""
        elif c == '\n' and not in_quotes:
            current_row.append(current_field.strip())
            if current_row:
                rows.append(current_row)
            current_row = []
            current_field = ""
        else:
            current_field += c
    if current_field:
        current_row.append(current_field.strip())
        rows.append(current_row)
        
    items_us = []
    items_eu = []
    
    for row in rows:
        if len(row) > 2:
            # First clean name: remove quotes and grab first line
            name = row[0].replace('"', '').split('\n')[0].strip()
            name = name.split(' (')[0] # Remove region tags
            
            us_idx_str = row[1]
            eu_idx_str = row[2]
            
            if us_idx_str.isdigit():
                items_us.append((int(us_idx_str), name))
            if eu_idx_str.isdigit():
                items_eu.append((int(eu_idx_str), name))
                
    # Standard save block is 166 bytes (0 to 165)
    # The indexes in CSV are 1-based, so Slot 0 corresponds to CSV Index 1
    arr_len = 166
    
    final_us = [f"Unknown_{i}" for i in range(arr_len)]
    for idx, name in items_us:
        if 0 <= idx - 1 < arr_len:
            final_us[idx - 1] = name
            
    final_eu = [f"Unknown_{i}" for i in range(arr_len)]
    for idx, name in items_eu:
        if 0 <= idx - 1 < arr_len:
            final_eu[idx - 1] = name
            
    with open(out_path, 'w', encoding='utf-8') as f:
        f.write("#pragma once\n\n")
        f.write(f"const int TREASURES_COUNT = {arr_len};\n")
        
        f.write("const char* TREASURES_NAMES_US[] = {\n")
        for s in final_us:
            f.write(f'    "{s}",\n')
        f.write("};\n\n")
        
        f.write("const char* TREASURES_NAMES_EU[] = {\n")
        for s in final_eu:
            f.write(f'    "{s}",\n')
        f.write("};\n")

def build_interiors():
    room_data = r'c:\Users\Kiyos\Documents\Antigravity_Projects\Tomodachi_Life_Save_Editor\TomodachiLife_3DS_SaveEditor\room_data.h'
    out_path = r'c:\Users\Kiyos\Documents\Antigravity_Projects\Tomodachi_Life_Save_Editor\TomodachiLife_3DS_SaveEditor\include\interiors_dataset.h'
    
    with open(room_data, 'r', encoding='utf-8') as f:
        lines = f.readlines()
        
    start_idx = -1
    names = []
    for i, line in enumerate(lines):
        if '"empty"' in line:
            start_idx = i
            break
            
    if start_idx != -1:
        for i in range(102):
            if start_idx + i < len(lines):
                name = lines[start_idx + i].strip().strip(',').strip('"')
                names.append(name)
                
    with open(out_path, 'w', encoding='utf-8') as f:
        f.write("#pragma once\n\n")
        f.write(f"const int INTERIORS_COUNT = {len(names)};\n")
        f.write("const char* INTERIORS_NAMES[] = {\n")
        for s in names:
            f.write(f'    "{s}",\n')
        f.write("};\n")

if __name__ == '__main__':
    build_treasures()
    build_interiors()
    print("Successfully built datasets.")

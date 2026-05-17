import csv
import re
import os

def build_treasures():
    csv_path = r'c:\Users\Kiyos\Documents\Antigravity_Projects\Tomodachi_Life_Save_Editor\TomodachiLife_3DS_SaveEditor\scratch\treasures.csv'
    out_path = r'c:\Users\Kiyos\Documents\Antigravity_Projects\Tomodachi_Life_Save_Editor\TomodachiLife_3DS_SaveEditor\include\treasures_dataset.h'
    
    items = []
    with open(csv_path, 'r', encoding='utf-8') as f:
        content = f.read()
        
    # Manual parsing of treasures.csv to extract Column 2
    # Format: Name,Col1,Col2,...
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
        
    for row in rows:
        if len(row) > 2 and row[2].isdigit():
            name = row[0].replace('"', '').split('\n')[0].strip()
            name = name.split(' (')[0] # Remove region tags like (US)
            idx = int(row[2])
            items.append((idx, name))
            
    # Pad array up to max index
    if not items:
        return
        
    max_idx = max(idx for idx, name in items)
    
    # Is it 1-based or 0-based? Let's assume 0-based array in C++
    # So if Gold coin is 57, and Hourglass is 64
    final_array = [f"Unknown_{i}" for i in range(max_idx + 1)]
    for idx, name in items:
        final_array[idx] = name
        
    with open(out_path, 'w', encoding='utf-8') as f:
        f.write("#pragma once\n\n")
        f.write(f"const int TREASURES_COUNT = {len(final_array)};\n")
        f.write("const char* TREASURES_NAMES[] = {\n")
        for s in final_array:
            f.write(f'    "{s}",\n')
        f.write("};\n")

def build_interiors():
    room_data = r'c:\Users\Kiyos\Documents\Antigravity_Projects\Tomodachi_Life_Save_Editor\TomodachiLife_3DS_SaveEditor\room_data.h'
    out_path = r'c:\Users\Kiyos\Documents\Antigravity_Projects\Tomodachi_Life_Save_Editor\TomodachiLife_3DS_SaveEditor\include\interiors_dataset.h'
    
    with open(room_data, 'r', encoding='utf-8') as f:
        lines = f.readlines()
        
    # Extract the block starting from 'empty' (line 244) for 102 items
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

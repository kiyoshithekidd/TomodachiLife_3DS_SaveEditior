# Tomodachi Life 3DS Save Editor (Homebrew)

A native 3DS homebrew application to edit Tomodachi Life save files (`savedataArc.txt`) directly on your console.

## Quick Start (Installation)

If you have a modded 3DS and just want to use the editor:

1. **Download**: Grab the latest `TomodachiLife_3DS_SaveEditor.3dsx` from the [Releases](https://github.com/yourusername/repo/releases) page.
2. **Copy**: Put the `.3dsx` file into the `/3ds/` folder on your SD card.
3. **Backup**: Use **Checkpoint** or **JKSM** to export a backup of your `savedataArc.txt` before editing.
4. **Launch**: Open the Homebrew Launcher on your 3DS and select the Tomodachi Life Save Editor.
5. **Save**: After making changes, press **START** to save and exit.

## Features

| Category | Items | Status |
|----------|-------|--------|
| **Money** | Edit island funds | ✅ Working |
| **Food** | 231 items (US) | ✅ Working |
| **Clothes** | 447 items with 8 color variants | ✅ Working |
| **Hats** | 171 items with 8 color variants | 🔧 In Progress |
| **Interiors** | 102 room styles | 🔧 In Progress |
| **Goods** | 18 useful items | ✅ Working |
| **Treasures** | 166 collectible items | 🔧 In Progress |
| **Special Unlock** | Bulk-unlock SpotPass/StreetPass items | ✅ Working |

### Controls
- **D-Pad**: Navigate menus and lists
- **A**: Select item / Confirm quantity
- **B**: Back
- **X**: Unlock All (within a category list)
- **Left/Right**: Adjust quantity by ±10
- **START**: Save changes and exit

### Granular Quantity Control
Unlike the legacy PC editor which only offered bulk "Unlock All", this editor lets you choose a specific quantity (1–99) for each individual item.

## Technical Details

| Category | Save Offset | Bytes/Item |
|----------|-------------|------------|
| Clothes | `0x0030` | 8 (1 per color) |
| Hats | `0x0E30` | 8 (1 per color) |
| Interiors | `0x1778` | 1 |
| Food | `0x17F0` | 1 |
| Goods | `0x18F0` | 1 |
| Treasures | `0x1902` | 1 |
| Money | `0x1E4BB8` | 4 (u32) |

- **Region**: Currently US-only. EU/JP/KR support is planned.
- **Language**: C++ using `libctru` and `citro2d`.
- **Credits**: Save offsets and item mapping referenced from the [VB.NET Tomodachi Life Save Editor by Brionjv](https://github.com/Brionjv/Tomodachi-Life-Save-Editor).

## Known Issues

- Interiors, Hats, and Treasures categories write to the save file but items may not appear in-game. Investigation ongoing.
- 14 of 166 treasure names are placeholders (regional items without confirmed US names).
- 5 of 102 interior names are placeholders.

---

## Changelog

### v0.3.0 — Inventory Overhaul (2026-05-04)
- **Granular quantity control**: Choose 1–99 for any item instead of bulk "Unlock All"
- **Interiors**: Added 102 interior names (US) with per-item editing
- **Treasures**: Added 166 treasure names (US) with per-item editing
- **Goods**: Added 18 useful items (medicine, travel tickets, etc.) as a new menu category
- **Hats title fix**: Hats list now displays "Hats" instead of generic "Items"
- **Goods menu entry**: Goods is now accessible from the main menu
- **Back-navigation fix**: Pressing B in any category returns to the correct menu position

### v0.2.0 — Food & Clothes (2026-05-03)
- **Food inventory**: Full 231-item US food list with 1:1 save-file mapping
- **Clothes**: 447 clothing items with 8-color variant support
- **Hats**: 171 hat items with color selection
- **Unlock All**: X button for bulk-unlock within any category list
- **Color picker**: Select individual colors or "All Colors" for clothes/hats
- **Filtering**: Automatically hides unknown/metadata items from lists

### v0.1.0 — Foundation
- **Money editor**: Add $100 increments or reset to $0
- **Island name display**: Reads and displays UTF-16 island name
- **Save & commit**: Press START to safely write and commit save data
- **RAII wrappers**: Crash-safe file and archive handling
- **Special unlock**: Bulk-write SpotPass/StreetPass clothes and hats

---

## Developer Instructions (Building from Source)

### Prerequisites
1. Install [devkitPro](https://devkitpro.org/wiki/Getting_Started).
2. Ensure `3ds-dev` is installed via pacman (`pacman -S 3ds-dev`).

### Compilation
- **Windows**: Run the included `build.bat`.
- **Linux/macOS**: Run `make` in the terminal.

The resulting `TomodachiLife_3DS_SaveEditor.3dsx` will be generated in the root directory.

### Project Structure
```
├── source/
│   └── main.cpp              # All application logic
├── include/
│   ├── food_names.h           # 231 US food items
│   ├── clothes_dataset.h      # 6932 clothes/hats entries
│   ├── interiors_dataset.h    # 102 US interior names
│   ├── treasures_dataset.h    # 166 US treasure names
│   └── goods_dataset.h        # 18 US goods items
├── Makefile
├── build.bat                  # Windows build helper
└── LICENSE
```

## License
MIT License - See the source code for details.
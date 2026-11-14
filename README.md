# Tomodachi Life 3DS Save Editor (Homebrew)

A native 3DS homebrew application to edit *Tomodachi Life* save files (`savedataArc.txt`) directly on your console. Supports both physical **Cartridge** and **SD Digital** copies across regions (**US, EU, JP, KR**), as well as raw **Checkpoint** exports.

## Quick Start (Installation)

If you have a modded 3DS and want to use the editor:

1. **Download**: Grab the latest `TomodachiLife_3DS_SaveEditor.3dsx` from the [Releases](https://github.com/kiyoshithekidd/TomodachiLife_3DS_SaveEditior/releases) page.
2. **Copy**: Put the `.3dsx` file into the `/3ds/` folder on your SD card.
3. **Save Detection & Automatic Backups**:
   - The editor automatically probes inserted **Cartridges** first, then **SD Digital** titles (`US`, `EU`, `JP`, `KR`), and finally falls back to a raw Checkpoint file at `sdmc:/3ds/tlse/savedataArc.txt`.
   - Before writing any change during a session, the editor automatically backs up your unmodified save to `sdmc:/3ds/tlse/savedataArc.bak`.
   - *(Recommended)* Still keep a manual **Checkpoint** or **JKSM** backup for extra peace of mind!
4. **Launch**: Open the Homebrew Launcher on your 3DS and select **Tomodachi Life Save Editor**.
5. **Save**: After making changes, press **START** to commit and exit.

## Features

| Category | Items | Status |
|----------|-------|--------|
| **Money** | Edit island funds (`+$100`, `±$10,000`, `Max $999,999.99`, `Reset $0`) | ✅ Working |
| **Food** | 231 items (US) / 252 items (EU) | ✅ Working |
| **Clothes** | 447 items (US/EU) with per-item valid color counts (1–8 colors) | ✅ Working |
| **Hats** | 219 items (US) / 215 items (EU) with per-item valid color counts (1–8 colors) | ✅ Working |
| **Interiors** | 102 room styles (US/EU) aligned to internal game IDs | ✅ Working |
| **Goods** | 18 useful items (US) / 17 items (EU) | ✅ Working |
| **Treasures** | 152 collectible items (US) / 155 items (EU) | ✅ Working |
| **Special Unlock** | Bulk-unlock SpotPass/StreetPass Clothes, Hats & Special Foods (`x99`) | ✅ Working |

### Controls

#### Main Menu
- **D-Pad Up / Down**: Navigate menu options
- **A**: Select option / Trigger **Unlock All Special Items**
- **L / R**: Switch item/offset region profile (`US` / `EU`)
- **START**: Commit save changes and exit

#### Money Editor
- **A**: Add `+$100.00`
- **D-Pad Left / Right**: Subtract / Add `±$10,000.00`
- **X**: Set Max Money (`$999,999.99`)
- **Y**: Reset Money to `$0.00`
- **B**: Return to Main Menu

#### Item Lists & Quantity Editor
- **D-Pad Up / Down**: Scroll items / Adjust quantity by `±1`
- **D-Pad Left / Right**: Adjust quantity by `±10` (in `Set Item Quantity` screen)
- **A**: Select item / color / Set exact item quantity (`1–99`)
- **X**: Unlock All items in the current category (`x99`, writing only valid color variants)
- **B**: Back / Cancel

### Granular Set-Quantity & Color Safety
- **Exact Quantity Preview**: When editing an individual item, the screen displays both your **Current Owned** count and the **New Quantity** (`1–99`) that will replace it.
- **Per-Item Valid Color Protection**: In *Tomodachi Life*, every clothing and hat item occupies an 8-byte block, but unused color bytes (`validColors..7`) must remain `0x00` or the game displays phantom "???" variants and freezes when equipping. Both single-item edits and **Unlock All (`X`)** strictly enforce each item's exact valid color count (`1..8`).

## Technical Details

| Category | US Save Offset | EU Save Offset | Bytes/Item |
|----------|----------------|----------------|------------|
| Clothes | `0x0030` | `0x0030` | 8 (1 per color, clamped to valid colors) |
| Hats | `0x0FF8` | `0x1010` | 8 (1 per color, clamped to valid colors) |
| Interiors | `0x1778` | `0x17A0` | 1 |
| Food | `0x17F0` | `0x1818` | 1 |
| Goods | `0x18F0` | `0x1920` | 1 |
| Treasures | `0x1902` | `0x1931` | 1 |
| Special Foods | `0x19E2` (`+0x19E4..0x1A12`) | `0x1A14` (`+0x1A16..0x1A44`) | 1 (48 slots) |
| Money | `0x1E4BC0` | `0x1E4BC8` | 4 (`u32`, max `99999999` cents) |
| Island Name | `0x1E4BCC` | `0x1E4BD4` | 22 (UTF-16LE) |

*(JP Money/Island Name offsets `0x14F2A0`/`0x14F2AC` and KR offsets `0x1EFA00`/`0x1EFA0C` are also automatically detected when probing JP `0x00035100` or KR `0x0008C500` saves.)*

- **Language**: C++17 using `libctru` and `citro2d`.
- **Credits**: Save offsets and item mapping referenced from the [VB.NET Tomodachi Life Save Editor by Brionjv](https://github.com/Brionjv/Tomodachi-Life-Save-Editor) and verified against RomFS MSBT message dumps.

---

## Changelog

### v0.6.0 — Complete QA Overhaul, Safe Special Unlocks & Multi-Region Detection (2026-11-14)
- **Fixed US Hats Base Offset (`#29`, `#20`)**: Corrected US Hats `saveBase` in `updateRegionOffsets()` from `0xE30` (SpotPass Clothes area) to `0xFF8`.
- **Fixed Clothes & Hats MSBT Name Alignment (`#30`, `#19`, `#24`)**: Corrected `clothes_dataset.h` 0-based start indices to `984` (`"regular clothes"`, 447 items) and `2600` (`"bear hat"`, 219 items), eliminating the off-by-one shift across every clothing and hat item.
- **Per-Item Valid Color Clamping (`#31`, `#12`, `#20`, `#26`)**: Added verified `CLOTHES_COLOR_COUNTS_US/EU` and `HATS_COLOR_COUNTS_US/EU` tables (`1..8` colors per item) so `Unlock All (X)`, `All Colors`, and color selection never write `99` into unused color bytes (`validColors..7`), preventing phantom `"???"` colors and save freezes.
- **Interiors & Treasures Alignment (`#32`, `#33`, `#21`)**: Aligned `INTERIORS_NAMES` slot 0 (`0x1778`) to `"children's"` through `"cinema"` (`0x17DD`), filtered out internal placeholder `"Unknown_108"`, and trimmed `TREASURES_COUNT` to `152` (`0x1902..0x1999`) so post-treasure bytes (`0x199A..0x19A7`) are never overwritten.
- **Re-enabled Safe "Unlock All Special Items" (`#35`, `#5`, `#27`)**: Replaced contiguous gap writes with verified per-region `SPOTPASS_CLOTHES_SLOTS_US/EU`, `SPOTPASS_HATS_SLOTS_US/EU`, and `unlockSpecialFoods()` (`0x19E2` + `0x19E4..0x1A12` US / `0x1A14` + `0x1A16..0x1A44` EU).
- **Multi-Region Save Detection & SD Fallback (`#34`, `#17`, `#18`, `#22`)**: Added automatic Cartridge-first then SD-Digital archive probing across `US` (`0x0008C300`), `EU` (`0x0008C400`), `JP` (`0x00035100`), and `KR` (`0x0008C500`), plus direct file fallback for `sdmc:/3ds/tlse/savedataArc.txt` and an explicit error screen if no save is found.
- **Automatic Pre-Write Save Backup (`#23`)**: Automatically streams a full backup to `sdmc:/3ds/tlse/savedataArc.bak` before the first write of a session.
- **Money Cap, Cursor Fix & UI Polish (`#36`, `#37`, `#15`, `#25`)**: Clamped money at `$999,999.99` with `±$10,000` (`Left/Right`) and `Max` (`X`) shortcuts, fixed the Main Menu cursor off-by-one wrap (`0..7`), added live `Current Owned` vs. `New Quantity` display in `Set Item Quantity`, and fitted all text layouts cleanly inside `400x240` / `320x240`.

### v0.5.1 — Clothing Alignment (2026-05-18)
- Adjusted clothing dataset alignment (`PR #14`).

### v0.5.0 — Region Toggle & Offset Fixes (2026-05-17)
- Added `L/R` US/EU region toggle and EU item datasets (`food`, `interiors`, `goods`, `treasures`).

### v0.4.0 — Inventory Offset Fixes (2026-05-11)
- **Treasures fixed**: Rebuilt the treasure dataset using the true internal game IDs extracted directly from the RomFS dump. Items now map correctly (e.g. "Gold coin" no longer gives an hourglass).
- **Interiors fixed**: Rebuilt the interiors dataset from the RomFS `Room_US_English_LZ.bin` file. Interior names now match the exact in-game order (e.g. "Rain" no longer gives "Arabian").
- **Hats offset corrected**: Hat memory block base offset corrected from `0xE30` to `0xFF8`, and array start index corrected from `1431` to `2601` (the "bear hat"). Hats should now apply correctly.
- **Treasures count updated**: Dataset count updated from 166 → 153 to reflect the verified US item list.
- **Special Unlock disabled**: The "Unlock All Special Items" feature is temporarily disabled to prevent save file corruption on fresh saves. The bottom screen now shows a clear "NOT WORKING (WIP)" notice when hovering over this option.

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
2. Ensure `3ds-dev` (`devkitARM`, `libctru`, `citro3d`, `citro2d`, `3dsxtool`, `smdhtool`) is installed via pacman (`pacman -S 3ds-dev`).

### Compilation
- **Windows**: Run the included `build.bat`.
- **Linux/macOS**: Run `make` in the terminal.

The resulting `TomodachiLife_3DS_SaveEditor.3dsx` will be generated in the root directory.

### Project Structure
```
├── source/
│   └── main.cpp                  # All application logic, save probing, backup & UI
├── include/
│   ├── food_names.h               # 231 US food items
│   ├── food_names_eu.h            # 252 EU food items
│   ├── clothes_dataset.h          # 6932 US clothes/hats entries
│   ├── clothes_dataset_eu.h       # 6904 EU clothes/hats entries
│   ├── interiors_dataset.h        # 102 US interior names
│   ├── interiors_dataset_eu.h     # 102 EU interior names
│   ├── treasures_dataset.h        # 152 US treasure names
│   ├── treasures_dataset_eu.h     # 155 EU treasure names
│   ├── goods_dataset.h            # 18 US goods items
│   └── goods_dataset_eu.h         # 17 EU goods items
├── Makefile
├── build.bat                      # Windows build helper
└── LICENSE
```

## License
MIT License - See the source code for details.
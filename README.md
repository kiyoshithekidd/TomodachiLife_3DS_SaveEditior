# Tomodachi Life 3DS Save Editor (Homebrew)

A native 3DS homebrew application to edit *Tomodachi Life* save files (`savedataArc.txt`) directly on your console. Supports both physical **Cartridge** and **SD Digital** copies across regions (**US, EU, JP, KR**), as well as raw **Checkpoint** exports.

## Quick Start (Installation)

If you have a modded 3DS and want to use the editor:

1. **Download**: Grab the latest `TomodachiLife_3DS_SaveEditor.3dsx` or `TomodachiLife_3DS_SaveEditor.cia` from the [Releases](https://github.com/kiyoshithekidd/TomodachiLife_3DS_SaveEditior/releases) page.
2. **Install**:
   - **`.cia` (Home Menu)**: Copy `TomodachiLife_3DS_SaveEditor.cia` to your SD card, install it with **FBI**, and launch it directly from the 3DS Home Menu.
   - **`.3dsx` (Homebrew Launcher)**: Copy `TomodachiLife_3DS_SaveEditor.3dsx` into `/3ds/` on your SD card and launch it from the Homebrew Launcher.
3. **Save Detection & Automatic Backups**:
   - The editor automatically probes inserted **Cartridges** first, then **SD Digital** titles (`US`, `EU`, `JP`, `KR`), and finally falls back to a raw Checkpoint file at `sdmc:/3ds/tlse/savedataArc.txt`.
   - Before writing any change during a session, the editor automatically backs up your unmodified save to `sdmc:/3ds/tlse/savedataArc.bak`.
   - *(Recommended)* Still keep a manual **Checkpoint** or **JKSM** backup for extra peace of mind!
4. **Save**: After making changes, press **START** to commit and exit.

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
| **Mii Editor** | Profile (Name, Birthday, Catchphrase, Sharing/Copying flags), Voice & Personality, Status (Lv.1–99, EXP, Stomach, Pocket Money), Equipped Items, Favorite Foods, Level-Up Gifts, `.tlmii`/`.tlamii` Export & Import, Clone & Delete Mii | ✅ Working |
| **Relationships** | View & edit Mii-to-Mii Relationship Types (with symmetric Spouse/Sweetheart enforcement) & Feelings (`0–200`), Resolve All Fights, Max All Feelings | ✅ Working |
| **Island & Config** | Edit Island Name, Problems Solved, Weddings, StreetPass Export Color, and unlock all 24 Island Buildings & 10 Rankings | ✅ Working |
| **Travelers & Concert** | Edit StreetPass Travelers (`Nickname`, `Message`, `On Island`, `Level`, `EXP`), Clear Traveler Slots, and edit Concert Hall Song Titles/Genres/Lyrics | ✅ Working |
| **Interactions & Extras** | Edit/Clear Mii Interactions, Fill/Empty All Stomachs, Max All Miis Lv.99, Reset Room Layout / News Flashes, Repair Malformed Interactions, Verify & Repair Mii CFSD CRC16-XMODEM Checksums | ✅ Working |
| **Touch & Keyboard UI** | Full Bottom-Screen Touch Navigation (tap rows/buttons/action bar) + native 3DS Software Keyboard (`swkbd`) for UTF-16 text fields | ✅ Working |

### Controls

#### Global & Touch Screen
- **D-Pad Up / Down**: Navigate list items
- **A**: Select / Edit / Confirm
- **B**: Save & Back / Cancel
- **Touch Screen**: Tap any visible row, Main Menu tile, or Bottom Action Bar button (`Back`, `Action`, `Save & Exit`)
- **START**: Commit save changes and exit

#### Main Menu
- **L / R**: Switch item/offset region profile (`US` / `EU`)
- **Y**: Restore from `sdmc:/3ds/tlse/savedataArc.bak` (press twice to confirm)

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

#### Mii Editor & Relationships
- **Mii Select**: `[A]` Edit Mii, `[X]` Max All Miis (Lv.99 + Pocket Money), `[L]` Export `all_miis.tlamii`, `[R]` Import `all_miis.tlamii`
- **Mii Submenu**: Edit Profile / Voice & Personality / Status / Equipped / Foods / Gifts, Export/Import `.tlmii`, Clone Mii, or Delete Mii (two-press confirmation)
- **Relationships**: `[A]` Select Mii pair, `[Y]` Resolve All Fights, `[X]` Max All Acquainted Feelings (`200`)

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
| Mii Table (`CFSD` + Status) | `0x1C70` (`0x660` stride) | `0x1C70` (`0x660` stride) | `0x660` per slot (CRC16-XMODEM at `+0x5E`) |
| Money | `0x1E4BC0` | `0x1E4BC8` | 4 (`u32`, max `99999999` cents) |
| Island Name | `0x1E4BCC` | `0x1E4BD4` | 22 (UTF-16LE) |

*(JP and KR region offsets are also automatically detected when probing JP `0x00035100` or KR `0x0008C500` saves.)*

- **Language**: C++11/17 using `libctru`, `citro3d`, and `citro2d`.
- **Credits**: Save offsets and item mapping referenced from the [VB.NET Tomodachi Life Save Editor by Brionjv](https://github.com/Brionjv/Tomodachi-Life-Save-Editor) and verified against RomFS MSBT message dumps and live save files.

---

## Changelog

### v0.9.2 — Home Menu `.cia` Build Support (2026-11-18)
- **`.cia` Packaging (`#59`)**: Added `make cia` target (`bannertool` + `makerom`), `app.rsf` (UniqueId `0xF8C30` with SDMC & save archive access), custom Home Menu banner (`gfx/banner.png` + `gfx/banner.wav`), and automated `.cia` release uploads alongside `.3dsx`.

### v0.9.1 — Loading Feedback, Traveler Editor, CFSD CRC16 & Mii Flags (2026-11-18)
- **Loading & Backup Progress Screens (`#55`)**: Added immediate busy frames during startup archive probing, the first-edit ~2 MB `savedataArc.bak` backup, and bulk SD/save operations so the 3DS never appears frozen.
- **Restored Bundle 2 Input Handlers (`#52`)**: Restored `STATE_MII_SELECT` `[L]/[R]` `.tlamii` export/import and `STATE_MII_SUBMENU` `Clone`/`Delete Mii` controls.
- **Travelers / StreetPass Port Editor (`#56`)**: Added `STATE_TRAVELER_EDIT` to edit Traveler `Nickname`, `Message`, `On Island`, `Level`, and `Experience`.
- **Mii CFSD CRC16-XMODEM Checksums (`#57`)**: Auto-recalculates CFSD CRC16-XMODEM (`poly=0x1021`, 2-byte BE at `+0x5E`) on every Mii write and adds `"Verify & Repair Mii Checksums"` to `Extras`.
- **Mii Catchphrase & Permission Flags (`#58`)**: Added `Catchphrase` (16 UTF-16LE chars), `Sharing Allowed`, and `Copying Allowed` toggles to `Mii Profile`.

### v0.9.0 — Full Feature Parity & Touch Controls (2026-11-18)
- **Bundle 1 (`#50`, `#51`, `#53`)**: Added Equipped Items, Favorite Foods, Level-Up Gifts, Relationships `Resolve All Fights` / `Max All Feelings`, and `Island Info` (`Export Color`, `Problems Solved`, `Weddings`).
- **Bundle 2 (`#52`)**: Added bulk `.tlamii` export/import, `Clone Mii`, and `Delete Mii` with relationship matrix cleanup.
- **Bundle 3 (`#54`)**: Added full bottom-screen Touch Controls (tappable Main Menu tiles, list rows, and bottom action bar buttons).

### v0.8.0 — Mii Editor, Relationships, Island, Travelers, Concert & Extras (2026-11-18)
- Added full Mii Editor (`Profile`, `Voice & Personality`, `Status`, `.tlmii`), Relationships Matrix Editor, Island Buildings/Rankings Unlocker, Travelers List, Concert Hall Song Editor, Mii Interactions Editor, and Quick Extras & Save Repair.
- Added native 3DS Software Keyboard (`swkbd`) for editing UTF-16 strings (Island Name, Mii names, Song titles/lyrics).

### v0.7.1 — Fix Unlock All Save Corruption & Backup Restore (2026-11-15)
- Fixed per-item valid color masks on `Unlock All` (`X`) and added `[Y]` Restore from Backup on the Main Menu.

### v0.7.0 — Visual Assets & *Tomodachi Life* UI Overhaul (2026-11-14)
- **Custom 3DS Homebrew Icon (`icon.png`)**: Embedded a custom `48×48` *Tomodachi Life* island tower icon into the `.3dsx` SMDH metadata (`#28`).
- **`tex3ds` Sprite Atlas (`romfs:/gfx/sprites.t3x`)**: Added custom PNG sprites compiled into RomFS via `tex3ds` (`#40`), including daylight island vista backdrops, Warm Notebook Cream cards, shop category icons, regional badges, and color swatch rings.

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
- Corrected Treasure, Interior, and Hat dataset offsets and disabled unsafe Special Unlock pending per-slot mapping.

### v0.3.0 — Inventory Overhaul (2026-05-04)
- Added granular quantity control (`1–99`), Interiors, Treasures, and Goods.

### v0.2.0 — Food & Clothes (2026-05-03)
- Added Food, Clothes, and Hats with color selection.

### v0.1.0 — Foundation
- Initial Money editor, Island name display, and RAII save/archive handling.

---

## Developer Instructions (Building from Source)

### Prerequisites
1. Install [devkitPro](https://devkitpro.org/wiki/Getting_Started).
2. Ensure `3ds-dev` (`devkitARM`, `libctru`, `citro3d`, `citro2d`, `3dsxtool`, `smdhtool`, `tex3ds`) is installed via pacman (`pacman -S 3ds-dev`).

### Compilation
- **Windows**: Run the included `build.bat`.
- **Linux/macOS (`.3dsx`)**: Run `make` in the terminal.
- **Linux/macOS (`.cia`)**: Ensure `bannertool` and `makerom` are on your `PATH` and run `make cia`.

The resulting `TomodachiLife_3DS_SaveEditor.3dsx` (and `.cia` when building `make cia`) will be generated in the root directory.

### Project Structure
```
├── source/
│   └── main.cpp                  # Application logic, touch/button UI, swkbd & rendering
├── include/
│   ├── save_io.h                 # SafeFile/SafeArchive RAII, auto-backup, UTF-16 helpers
│   ├── mii_data.h                # Mii CFSD/Status/Foods/Gifts, CRC16-XMODEM, .tlmii/.tlamii
│   ├── relationships_data.h      # Relationship matrix, symmetric Spouse/Sweetheart, fights
│   ├── island_data.h             # Island info/config, Travelers, Concert songs, Interactions
│   ├── food_names.h              # US & EU food items
│   ├── clothes_dataset.h         # US & EU clothes/hats entries & valid color counts
│   ├── interiors_dataset.h       # US & EU interior names
│   ├── treasures_dataset.h       # US & EU treasure names
│   └── goods_dataset.h           # US & EU goods items
├── gfx/                          # PNG sprites, tex3ds atlas (.t3s), banner.png & banner.wav
├── app.rsf                       # CIA Rom Specification File (Title ID & permissions)
├── Makefile
├── build.bat                     # Windows build helper
└── LICENSE
```

## License
MIT License - See the source code for details.
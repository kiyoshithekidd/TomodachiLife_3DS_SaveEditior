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
- **Money Editor**: Instantly add funds to your island.
- **Food Inventory**: Add any of the 231 food items with 1:1 save-file mapping (US Version).
- **Clothes & Colors**: Add clothing items with support for specific color variants or "All Colors" bulk writing.
- **Dynamic UI**: Clean, scrolled lists that filter out "Unknown" items and metadata tags.
- **Title Casing**: Professional item presentation (e.g., "Cowboy Duds" instead of "cowboy duds").

## Technical Details
- **Memory Map**: US Food starts at `0x17F0`, Clothes at `0x30`.
- **Language**: C++ using `libctru` and `citro2d`.
- **Credits**: Item name mapping and save offsets referenced from the [VB.NET Tomodachi Life Save Editor by Brionjv](https://github.com/Brionjv/Tomodachi-Life-Save-Editor).

---

## Developer Instructions (Building from Source)

If you want to fork this project or contribute:

### Prerequisites
1. Install [devkitPro](https://devkitpro.org/wiki/Getting_Started).
2. Ensure `3ds-dev` is installed via pacman (`pacman -S 3ds-dev`).

### Compilation
- **Windows**: Run the included `build.bat`.
- **Linux/macOS**: Run `make` in the terminal.

The resulting `TomodachiLife_3DS_SaveEditor.3dsx` will be generated in the root directory.

## License
MIT License - See the source code for details.
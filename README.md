# Tomodachi Life 3DS Save Editor

A native homebrew application for the Nintendo 3DS that allows you to read, modify, and manage your *Tomodachi Life* save data directly on your console without needing a PC!

## 🎯 Project Goal
The goal of this project is to port the functionality of the original Windows-based (VB.NET) Tomodachi Life Save Editor into a lightweight, stable C++ homebrew application. By running directly on the 3DS hardware via the Homebrew Launcher, this tool will eliminate the need to extract your save data to an SD card, edit it on a computer, and re-inject it.

## 🚀 Current Status
**Early Prototype Stage**
The application is currently in its foundational phase. 
*   [x] Safe ARM11 hardware initialization and exit lifecycles.
*   [x] Direct `ARCHIVE_USER_SAVEDATA` mounting (supports both digital SD copies and physical cartridges).
*   [x] Basic save data parsing (Island Name & Current Money offsets).
*   [x] Citro2D GUI rendering implementation.
*   [ ] Write capabilities and editing UI.
*   [ ] Full Mii and Item database mapping.

*Currently hardcoded for North American Title ID: `000400000008C300`*

## 🛠️ Built With
*   **C++11** - Core application logic.
*   **devkitARM / libctru** - 3DS hardware interactions and file system mounting.
*   **citro2d / citro3d** - 2D Graphics and Text Rendering.

## ⚠️ Disclaimer
**Always backup your save data before using any save editor.** This project is in active development. Editing save data carries an inherent risk of data corruption if incorrect hex values are written to the `savedata.arc` file.

## 📝 Building from Source
To compile this project, you must have [devkitPro](https://devkitpro.org/) installed along with the `3ds-dev` packages.

1. Clone the repository.
2. Open a terminal in the root directory.
3. Run `make`.
4. Copy the resulting `.3dsx` file to the `/3ds/` directory on your console's SD card.
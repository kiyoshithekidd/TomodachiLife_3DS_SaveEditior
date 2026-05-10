#include <3ds.h>
#include <citro2d.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <memory>
#include <vector>
#include "food_names.h"
#include "clothes_dataset.h"
#include "interiors_dataset.h"
#include "treasures_dataset.h"
#include "goods_dataset.h"

// ── Helpers ──────────────────────────────────────────────────────────

void capitalizeString(char* str) {
    bool newWord = true;
    for (int i = 0; str[i] != '\0'; i++) {
        if (newWord && isalpha((unsigned char)str[i])) {
            str[i] = toupper((unsigned char)str[i]);
            newWord = false;
        } else if (isspace((unsigned char)str[i]) || str[i] == '-') {
            newWord = true;
        }
    }
}

// ── State Machine ────────────────────────────────────────────────────

enum MenuState {
    STATE_MAIN_MENU,
    STATE_MONEY_EDIT,
    STATE_CATEGORY_FOOD,
    STATE_CATEGORY_CLOTHES,
    STATE_CATEGORY_HATS,
    STATE_CATEGORY_INTERIORS,
    STATE_CATEGORY_GOODS,
    STATE_CATEGORY_TREASURES,
    STATE_COLOR_SELECT,
    STATE_QUANTITY_SELECT
};

// ── Category Info ────────────────────────────────────────────────────
// Stores everything needed for a category: its name list, save offsets, etc.

struct CategoryInfo {
    const char** names;       // pointer to the name array
    int nameCount;            // total items in the name array 
    int nameOffset;           // offset into the array (0 for direct arrays)
    u32 saveBase;             // base save file offset
    int bytesPerItem;         // 1 for food/interiors/goods/treasures, 8 for clothes
    bool hasColors;           // true if items have color variants
    bool hasSpecialLogic;     // true if it uses the SpotPass/StreetPass scattered offsets
    int maxItems;             // total number of items in the name list
    std::vector<int> filteredIndices;  // populated at init time
};

CategoryInfo categories[10]; // FOOD, CLOTHES, HATS, INTERIORS, GOODS, TREASURES, SPECIAL_FOOD
int selectedColor = -1;
int savedItemCursor = 0;

void initCategory(CategoryInfo& cat, const char** names, int count, int offset, int maxItems,
                  u32 saveBase, int bytesPerItem, bool hasColors, bool needsFiltering) {
    cat.names = names;
    cat.nameCount = count;
    cat.nameOffset = offset;
    cat.saveBase = saveBase;
    cat.bytesPerItem = bytesPerItem;
    cat.hasColors = hasColors;
    cat.hasSpecialLogic = needsFiltering;
    cat.maxItems = maxItems;
    cat.filteredIndices.clear();
    
    for (int i = 0; i < maxItems; i++) {
        if (offset + i >= count) break;
        if (needsFiltering) {
            const char* name = names[offset + i];
            if (strstr(name, "Unknown_Item") != NULL) continue;
            if (strstr(name, "NSPthe")       != NULL) continue;
            if (strstr(name, "NPPthe")       != NULL) continue;
            if (strstr(name, "NSSthe")       != NULL) continue;
            if (strlen(name) > 60)           continue;
        }
        cat.filteredIndices.push_back(i);
    }
}

CategoryInfo* getCategoryForState(MenuState state) {
    switch (state) {
        case STATE_CATEGORY_FOOD:      return &categories[0];
        case STATE_CATEGORY_CLOTHES:   return &categories[1];
        case STATE_CATEGORY_HATS:      return &categories[2];
        case STATE_CATEGORY_INTERIORS: return &categories[3];
        case STATE_CATEGORY_GOODS:     return &categories[4];
        case STATE_CATEGORY_TREASURES: return &categories[5];
        default:                       return &categories[0];
    }
}

const char* getTitleForState(MenuState state) {
    switch (state) {
        case STATE_CATEGORY_FOOD:      return "Food";
        case STATE_CATEGORY_CLOTHES:   return "Clothes";
        case STATE_CATEGORY_HATS:      return "Hats";
        case STATE_CATEGORY_INTERIORS: return "Interiors";
        case STATE_CATEGORY_GOODS:     return "Goods";
        case STATE_CATEGORY_TREASURES: return "Treasures";
        default:                       return "Items";
    }
}

// ── RAII wrappers ────────────────────────────────────────────────────

class SafeArchive {
private:
    FS_Archive mArchive = 0;
public:
    SafeArchive(FS_ArchiveID id, FS_Path path) {
        if (R_FAILED(FSUSER_OpenArchive(&mArchive, id, path))) {
            mArchive = 0;
        }
    }

    ~SafeArchive() {
        if (mArchive) {
            FSUSER_CloseArchive(mArchive);
        }
    }

    // Must be called to ensure save data writes aren't lost
    Result commit() {
        if (!mArchive) return -1;
        return FSUSER_ControlArchive(mArchive, ARCHIVE_ACTION_COMMIT_SAVE_DATA, NULL, 0, NULL, 0);
    }
    
    FS_Archive get() const { return mArchive; }
    
    // Delete copy constructors to prevent double-closing of handle
    SafeArchive(const SafeArchive&) = delete;
    SafeArchive& operator=(const SafeArchive&) = delete;
};

class SafeFile {
private:
    Handle mHandle = 0;
public:
    SafeFile(FS_Archive archive, FS_Path path, u32 flags, u32 attributes) {
        if (R_FAILED(FSUSER_OpenFile(&mHandle, archive, path, flags, attributes))) {
            mHandle = 0;
        }
    }
    
    ~SafeFile() {
        if (mHandle) {
            FSFILE_Close(mHandle);
        }
    }
    
    Result write(u64 offset, const void* data, u32 size, u32* bytesWritten) {
        if (!mHandle) return -1;
        return FSFILE_Write(mHandle, bytesWritten, offset, data, size, FS_WRITE_FLUSH);
    }
    
    Result read(u64 offset, void* data, u32 size, u32* bytesRead) {
        if (!mHandle) return -1;
        return FSFILE_Read(mHandle, bytesRead, offset, data, size);
    }

    Handle get() const { return mHandle; }

    // Delete copy constructors to prevent double-closing of handle
    SafeFile(const SafeFile&) = delete;
    SafeFile& operator=(const SafeFile&) = delete;
};

// ── Special Items Logic ──────────────────────────────────────────────

void unlockSpecialItems(SafeFile* file, CategoryInfo* cat, u8 val) {
    if (!cat->hasSpecialLogic) return;
    
    u32 bytesWritten = 0;
    for (int i = 0; i < cat->maxItems; i++) {
        if (cat->nameOffset + i >= cat->nameCount) break;
        const char* name = cat->names[cat->nameOffset + i];
        
        bool isSpecial = false;
        if (strstr(name, "Unknown_Item") != NULL) isSpecial = true;
        if (strstr(name, "NSPthe")       != NULL) isSpecial = true;
        if (strstr(name, "NPPthe")       != NULL) isSpecial = true;
        if (strstr(name, "NSSthe")       != NULL) isSpecial = true;
        if (strlen(name) > 60)           isSpecial = true;
        
        if (isSpecial) {
            if (cat->hasColors) {
                for (int c = 0; c < cat->bytesPerItem; c++) {
                    file->write(cat->saveBase + (i * cat->bytesPerItem) + c, &val, 1, &bytesWritten);
                }
            } else {
                file->write(cat->saveBase + (i * cat->bytesPerItem), &val, 1, &bytesWritten);
            }
        }
    }
}

// ── Services ─────────────────────────────────────────────────────────

u32 old_time_limit;

void initServices() {
    // 1. Core Services
    hidInit();            // Input scanning
    gfxInitDefault();     // Graphics
    
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();
    
    // 2. CPU Time Limit Adjustment (CRITICAL for stability)
    APT_GetAppCpuTimeLimit(&old_time_limit);
    APT_SetAppCpuTimeLimit(30); 
    
    // 3. Hardware & OS Services
    romfsInit();          // Read-only filesystem (app assets)
    cfguInit();           // System configuration
    amInit();             // App Manager (CIA installs)
    acInit();             // WiFi/Internet Status
    
    // 4. Graphics Setup
    gfxSetScreenFormat(GFX_BOTTOM, GSP_BGR8_OES);
    gfxSetDoubleBuffering(GFX_BOTTOM, true);
}

void exitServices() {
    // Exit OS Services
    acExit();
    amExit();
    cfguExit();
    romfsExit();
    
    // Restore original CPU Time Limit
    if (old_time_limit != UINT32_MAX) {
        APT_SetAppCpuTimeLimit(old_time_limit);
    }
    
    // Exit Core Services last
    C2D_Fini();
    C3D_Fini();
    gfxExit();
    hidExit();
}

// ── Main ─────────────────────────────────────────────────────────────

int main(int argc, char** argv) {
    initServices();

    C3D_RenderTarget* topTarget = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    C3D_RenderTarget* bottomTarget = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    C2D_TextBuf staticBuf = C2D_TextBufNew(4096);
    C2D_TextBuf bottomBuf = C2D_TextBufNew(4096);

    // North American Tomodachi Life Title ID: 000400000008C300
    // Low ID: 0008C300, High ID: 00040000
    u32 pathDataSD[3] = {MEDIATYPE_SD, 0x0008C300, 0x00040000};
    FS_Path arcPathSD = {PATH_BINARY, 12, (const void*)pathDataSD};
    std::unique_ptr<SafeArchive> archive(new SafeArchive(ARCHIVE_USER_SAVEDATA, arcPathSD));

    if (archive->get() == 0) {
        // Try game card if not found on SD
        u32 pathDataCart[3] = {MEDIATYPE_GAME_CARD, 0x0008C300, 0x00040000};
        FS_Path arcPathCart = {PATH_BINARY, 12, (const void*)pathDataCart};
        archive.reset(new SafeArchive(ARCHIVE_USER_SAVEDATA, arcPathCart));
    }

    char displayString[2048] = "Tomodachi Life Save Editor\n\n";
    char bottomString[2048] = "";
    bool fileOpenSuccess = false;
    u32 currentMoney = 0;
    char islandNameUTF8[32] = {0};
    std::unique_ptr<SafeFile> file = nullptr;

    if (archive->get() != 0) {
        FS_Path filePath = fsMakePath(PATH_ASCII, "/savedataArc.txt");
        file.reset(new SafeFile(archive->get(), filePath, FS_OPEN_READ | FS_OPEN_WRITE, 0));

        if (file->get() != 0) {
            fileOpenSuccess = true;
            u32 bytesRead = 0;
            
            // Read Money (offset 0x1E4BB8)
            file->read(0x1E4BB8, &currentMoney, sizeof(u32), &bytesRead);
            
            // Read Island Name (offset 0x1E4BCC, 20 bytes for 10 UTF-16 chars)
            u16 islandNameUTF16[11] = {0}; 
            file->read(0x1E4BCC, islandNameUTF16, 20, &bytesRead);

            // Convert UTF-16 string to UTF-8 for citro2d rendering
            utf16_to_utf8((uint8_t*)islandNameUTF8, islandNameUTF16, 31);
        }
    }

    // ── Initialize categories ────────────────────────────────────────
    // Food: index 0 is "Nothing", save at 0x17F0+ID
    initCategory(categories[0], FOOD_NAMES, FOOD_COUNT, 0, FOOD_COUNT,
                 0x17F0, 1, false, false);
    // Clothes: standard block at 0x30+(ID*8)
    initCategory(categories[1], CLOTHES_NAMES, CLOTHES_COUNT, 984, 447,
                 0x30, 8, true, true);
    // Hats: standard block at 0xE30+(ID*8)
    initCategory(categories[2], CLOTHES_NAMES, CLOTHES_COUNT, 1431, 171,
                 0xE30, 8, true, true);
    // Interiors: save at 0x1778+ID, 102 items (US)
    initCategory(categories[3], INTERIORS_NAMES, INTERIORS_COUNT, 0, INTERIORS_COUNT,
                 0x1778, 1, false, false);
    // Goods: save at 0x18F0+ID, 18 items (US)
    initCategory(categories[4], GOODS_NAMES, GOODS_COUNT, 0, GOODS_COUNT,
                 0x18F0, 1, false, false);
    // Treasures: save at 0x1902+ID, 166 items (US)
    initCategory(categories[5], TREASURES_NAMES, TREASURES_COUNT, 0, TREASURES_COUNT,
                 0x1902, 1, false, false);

    // ── State variables ──────────────────────────────────────────────
    MenuState currentState = STATE_MAIN_MENU;
    int cursorIndex = 0;
    int scrollOffset = 0;
    int quantityToGive = 1;
    int colorCursor = 0;           // for the color selection menu
    MenuState previousCategory = STATE_MAIN_MENU;
    const int MAX_VISIBLE_ITEMS = 12;
    const int MAIN_MENU_ITEMS = 8;  // Money, Food, Clothes, Hats, Interiors, Goods, Treasures, Special Unlock

    while (aptMainLoop()) {
        hidScanInput(); 
        u32 kDown = hidKeysDown();
        
        if (kDown & KEY_START) {
            if (fileOpenSuccess && archive->get() != 0) {
                archive->commit(); 
            }
            break;
        }

        // ═══════════════════ INPUT LOGIC ═══════════════════

        if (currentState == STATE_MAIN_MENU) {
            if (kDown & KEY_DUP)   cursorIndex = (cursorIndex > 0) ? cursorIndex - 1 : MAIN_MENU_ITEMS;
            if (kDown & KEY_DDOWN) cursorIndex = (cursorIndex < MAIN_MENU_ITEMS) ? cursorIndex + 1 : 0;
            if (kDown & KEY_A) {
                if (cursorIndex == 0) {
                    currentState = STATE_MONEY_EDIT;
                }
                else if (cursorIndex == 1) {
                    currentState = STATE_CATEGORY_FOOD;
                    cursorIndex = 0; scrollOffset = 0;
                }
                else if (cursorIndex == 2) {
                    currentState = STATE_CATEGORY_CLOTHES;
                    cursorIndex = 0; scrollOffset = 0;
                }
                else if (cursorIndex == 3) {
                    currentState = STATE_CATEGORY_HATS;
                    cursorIndex = 0; scrollOffset = 0;
                }
                else if (cursorIndex == 4) {
                    currentState = STATE_CATEGORY_INTERIORS;
                    cursorIndex = 0; scrollOffset = 0;
                }
                else if (cursorIndex == 5) {
                    currentState = STATE_CATEGORY_GOODS;
                    cursorIndex = 0; scrollOffset = 0;
                }
                else if (cursorIndex == 6) {
                    currentState = STATE_CATEGORY_TREASURES;
                    cursorIndex = 0; scrollOffset = 0;
                }
                else if (cursorIndex == 7) {
                    // Quick Action: Unlock All Special Items
                    if (fileOpenSuccess) {
                        unlockSpecialItems(file.get(), &categories[1], 99);
                        unlockSpecialItems(file.get(), &categories[2], 99);
                        // Also unlock special foods if offset is known
                        // u8 val = 99;
                        // for (int i = 0; i < 40; i++) file->write(0x19A8 + i, &val, 1, &bytesRead);
                    }
                }
            }
        }
        else if (currentState == STATE_MONEY_EDIT) {
            if (kDown & KEY_B) {
                currentState = STATE_MAIN_MENU;
                cursorIndex = 0;
            }
            if (fileOpenSuccess) {
                bool valueChanged = false;
                if (kDown & KEY_A) { currentMoney += 10000; valueChanged = true; } 
                else if (kDown & KEY_Y) { currentMoney = 0; valueChanged = true; }

                if (valueChanged) {
                    u32 bytesWritten = 0;
                    file->write(0x1E4BB8, &currentMoney, sizeof(u32), &bytesWritten);
                }
            }
        }
        else if (currentState >= STATE_CATEGORY_FOOD && currentState <= STATE_CATEGORY_TREASURES) {
            CategoryInfo* cat = getCategoryForState(currentState);
            int maxItems = (int)cat->filteredIndices.size();

            if (kDown & KEY_DUP) {
                if (cursorIndex > 0) cursorIndex--;
                if (cursorIndex < scrollOffset) scrollOffset = cursorIndex;
            }
            if (kDown & KEY_DDOWN) {
                if (cursorIndex < maxItems - 1) cursorIndex++;
                if (cursorIndex >= scrollOffset + MAX_VISIBLE_ITEMS) scrollOffset = cursorIndex - MAX_VISIBLE_ITEMS + 1;
            }
            if (kDown & KEY_B) {
                // Return to main menu, restore cursor to whichever menu item launched us
                int menuIdx = 0;
                if (currentState == STATE_CATEGORY_FOOD) menuIdx = 1;
                else if (currentState == STATE_CATEGORY_CLOTHES) menuIdx = 2;
                else if (currentState == STATE_CATEGORY_HATS) menuIdx = 3;
                else if (currentState == STATE_CATEGORY_INTERIORS) menuIdx = 4;
                else if (currentState == STATE_CATEGORY_GOODS) menuIdx = 5;
                else if (currentState == STATE_CATEGORY_TREASURES) menuIdx = 6;
                currentState = STATE_MAIN_MENU;
                cursorIndex = menuIdx;
                scrollOffset = 0;
            }
            if (kDown & KEY_X && fileOpenSuccess) {
                // Unlock All for current category
                u8 val = 99;
                u32 bytesWritten = 0;
                for (int i = 0; i < (int)cat->filteredIndices.size(); i++) {
                    int originalIdx = cat->filteredIndices[i];
                    if (cat->hasColors) {
                        for (int c = 0; c < cat->bytesPerItem; c++) {
                            file->write(cat->saveBase + (originalIdx * cat->bytesPerItem) + c, &val, 1, &bytesWritten);
                        }
                    } else {
                        file->write(cat->saveBase + originalIdx, &val, 1, &bytesWritten);
                    }
                }
            }
            if (kDown & KEY_A && maxItems > 0) {
                savedItemCursor = cursorIndex;  // save position for returning
                if (currentState == STATE_CATEGORY_CLOTHES || currentState == STATE_CATEGORY_HATS) {
                    previousCategory = currentState;
                    currentState = STATE_COLOR_SELECT;
                    colorCursor = 0;
                } else {
                    previousCategory = currentState;
                    currentState = STATE_QUANTITY_SELECT;
                    quantityToGive = 1;
                }
            }
        }
        else if (currentState == STATE_COLOR_SELECT) {
            if (kDown & KEY_DUP)   colorCursor = (colorCursor > 0) ? colorCursor - 1 : 8;
            if (kDown & KEY_DDOWN) colorCursor = (colorCursor < 8) ? colorCursor + 1 : 0;
            
            if (kDown & KEY_B) {
                currentState = previousCategory;
                cursorIndex = savedItemCursor;
            }
            if (kDown & KEY_A) {
                selectedColor = (colorCursor == 0) ? -1 : colorCursor - 1;
                currentState = STATE_QUANTITY_SELECT;
                quantityToGive = 1;
            }
        }
        else if (currentState == STATE_QUANTITY_SELECT) {
            if (kDown & KEY_DUP)   quantityToGive = (quantityToGive < 99) ? quantityToGive + 1 : 99;
            if (kDown & KEY_DDOWN) quantityToGive = (quantityToGive > 1) ? quantityToGive - 1 : 1;
            if (kDown & KEY_RIGHT) quantityToGive = (quantityToGive + 10 <= 99) ? quantityToGive + 10 : 99;
            if (kDown & KEY_LEFT)  quantityToGive = (quantityToGive - 10 >= 1) ? quantityToGive - 10 : 1;

            if (kDown & KEY_B) {
                if (previousCategory == STATE_CATEGORY_CLOTHES || previousCategory == STATE_CATEGORY_HATS) {
                    currentState = STATE_COLOR_SELECT;
                } else {
                    currentState = previousCategory;
                    cursorIndex = savedItemCursor;
                }
            }
            if (kDown & KEY_A) {
                if (fileOpenSuccess) {
                    CategoryInfo* cat = getCategoryForState(previousCategory);
                    int originalIdx = cat->filteredIndices[savedItemCursor];
                    u32 bytesWritten = 0;
                    u8 val = (u8)quantityToGive;

                    if (cat->hasColors) {
                        if (selectedColor == -1) {
                            // Write all color variants
                            for (int i = 0; i < cat->bytesPerItem; i++) {
                                file->write(cat->saveBase + (originalIdx * cat->bytesPerItem) + i, &val, 1, &bytesWritten);
                            }
                        } else {
                            file->write(cat->saveBase + (originalIdx * cat->bytesPerItem) + selectedColor, &val, 1, &bytesWritten);
                        }
                    } else {
                        file->write(cat->saveBase + originalIdx, &val, 1, &bytesWritten);
                    }
                }
                currentState = previousCategory;
                cursorIndex = savedItemCursor;
            }
        }

        // ═══════════════════ RENDERING ═══════════════════

        C2D_TextBufClear(staticBuf);
        C2D_TextBufClear(bottomBuf);
        
        if (currentState == STATE_MAIN_MENU) {
            u32 dollars = currentMoney / 100;
            u32 cents = currentMoney % 100;
            snprintf(displayString, sizeof(displayString), 
                "Tomodachi Life Save Editor\n\n"
                "Island: %s\n"
                "Money: $%lu.%02lu\n\n"
                "%s Edit Money\n"
                "%s Add Food\n"
                "%s Add Clothes\n"
                "%s Add Hats\n"
                "%s Add Interiors\n"
                "%s Add Goods\n"
                "%s Add Treasures\n"
                "%s Unlock All Special Items\n",
                islandNameUTF8, dollars, cents,
                (cursorIndex == 0) ? "->" : "  ",
                (cursorIndex == 1) ? "->" : "  ",
                (cursorIndex == 2) ? "->" : "  ",
                (cursorIndex == 3) ? "->" : "  ",
                (cursorIndex == 4) ? "->" : "  ",
                (cursorIndex == 5) ? "->" : "  ",
                (cursorIndex == 6) ? "->" : "  ",
                (cursorIndex == 7) ? "->" : "  ");
            if (cursorIndex == 7) {
                snprintf(bottomString, sizeof(bottomString),
                    "Press A to select\n"
                    "Press START to save & exit\n\n"
                    "WARNING: May corrupt fresh saves!\n"
                    "Please have a backup save data.");
            } else {
                snprintf(bottomString, sizeof(bottomString),
                    "Press A to select\n"
                    "Press START to save & exit");
            }
        }
        else if (currentState == STATE_MONEY_EDIT) {
            u32 dollars = currentMoney / 100;
            u32 cents = currentMoney % 100;
            if (fileOpenSuccess) {
                snprintf(displayString, sizeof(displayString), 
                    "Money Editor\n\n"
                    "Island Name: %s\n"
                    "> Current Money: $%lu.%02lu <\n",
                    islandNameUTF8, dollars, cents);
                snprintf(bottomString, sizeof(bottomString),
                    "Press A to add $100.00\n"
                    "Press Y to reset to $0.00\n"
                    "Press B to go back");
            } else {
                snprintf(displayString, sizeof(displayString), "Error: Save file not loaded.\n");
                snprintf(bottomString, sizeof(bottomString), "Press B to go back.");
            }
        }
        else if (currentState >= STATE_CATEGORY_FOOD && currentState <= STATE_CATEGORY_TREASURES) {
            CategoryInfo* cat = getCategoryForState(currentState);
            const char* title = getTitleForState(currentState);
            int maxItems = (int)cat->filteredIndices.size();
            
            char listStr[2048] = {0};
            snprintf(listStr, sizeof(listStr), "%s List (%d items)\n\n", title, maxItems);
            
            for (int i = scrollOffset; i < scrollOffset + MAX_VISIBLE_ITEMS && i < maxItems; i++) {
                int originalIdx = cat->filteredIndices[i];
                char itemName[128];
                if (cat->names != NULL) {
                    strncpy(itemName, cat->names[originalIdx + cat->nameOffset], sizeof(itemName) - 1);
                    itemName[sizeof(itemName) - 1] = '\0';
                    capitalizeString(itemName);
                } else {
                    snprintf(itemName, sizeof(itemName), "Item #%d", originalIdx + 1);
                }
                
                char temp[256];
                snprintf(temp, sizeof(temp), "%s [%03d] %.50s\n", (i == cursorIndex) ? "->" : "  ", i + 1, itemName);
                strncat(listStr, temp, sizeof(listStr) - strlen(listStr) - 1);
            }
            snprintf(displayString, sizeof(displayString), "%s", listStr);
            snprintf(bottomString, sizeof(bottomString), "Press A to select\nPress X to Unlock All\nPress B to go back");
        }
        else if (currentState == STATE_COLOR_SELECT) {
            CategoryInfo* cat = getCategoryForState(previousCategory);
            int originalIdx = cat->filteredIndices[savedItemCursor];
            char itemName[128];
            if (cat->names != NULL) {
                strncpy(itemName, cat->names[originalIdx + cat->nameOffset], sizeof(itemName) - 1);
                itemName[sizeof(itemName) - 1] = '\0';
                capitalizeString(itemName);
            } else {
                snprintf(itemName, sizeof(itemName), "Item #%d", originalIdx + 1);
            }

            const char* colors[] = {"All Colors", "Color 1", "Color 2", "Color 3", "Color 4", "Color 5", "Color 6", "Color 7", "Color 8"};
            char colorStr[512];
            snprintf(colorStr, sizeof(colorStr), "Item: %s\n\nSelect Color:\n\n", itemName);
            for (int i = 0; i < 9; i++) {
                char temp[64];
                snprintf(temp, sizeof(temp), "%s %s\n", (i == colorCursor) ? "->" : "  ", colors[i]);
                strcat(colorStr, temp);
            }
            snprintf(displayString, sizeof(displayString), "%s", colorStr);
            snprintf(bottomString, sizeof(bottomString), "Press A to select\nPress B to go back");
        }
        else if (currentState == STATE_QUANTITY_SELECT) {
            CategoryInfo* cat = getCategoryForState(previousCategory);
            int originalIdx = cat->filteredIndices[savedItemCursor];
            
            char itemName[128];
            if (cat->names != NULL) {
                strncpy(itemName, cat->names[originalIdx + cat->nameOffset], sizeof(itemName) - 1);
                itemName[sizeof(itemName) - 1] = '\0';
                capitalizeString(itemName);
            } else {
                snprintf(itemName, sizeof(itemName), "Item #%d", originalIdx + 1);
            }
            itemName[sizeof(itemName) - 1] = '\0';
            capitalizeString(itemName);
            
            snprintf(displayString, sizeof(displayString), 
                "Add Item:\n%s\n\nQuantity: %d", 
                itemName, quantityToGive);
            snprintf(bottomString, sizeof(bottomString), 
                "Up/Down: +/- 1\nLeft/Right: +/- 10\nPress A to Confirm\nPress B to Cancel");
        }

        C2D_Text infoText;
        C2D_TextParse(&infoText, staticBuf, displayString);
        C2D_TextOptimize(&infoText);
        
        C2D_Text bottomText;
        C2D_TextParse(&bottomText, bottomBuf, bottomString);
        C2D_TextOptimize(&bottomText);
        
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        
        C2D_TargetClear(topTarget, C2D_Color32(0, 0, 0, 255));
        C2D_SceneBegin(topTarget);
        C2D_DrawText(&infoText, C2D_WithColor, 10.0f, 10.0f, 0.5f, 0.5f, 0.5f, C2D_Color32(255, 255, 255, 255));
        
        C2D_TargetClear(bottomTarget, C2D_Color32(0, 0, 0, 255));
        C2D_SceneBegin(bottomTarget);
        float textW = 0, textH = 0;
        C2D_TextGetDimensions(&bottomText, 0.5f, 0.5f, &textW, &textH);
        float textX = (320.0f - textW) / 2.0f;
        float textY = (240.0f - textH) / 2.0f;
        C2D_DrawText(&bottomText, C2D_WithColor, textX, textY, 0.5f, 0.5f, 0.5f, C2D_Color32(255, 255, 255, 255));
        
        C3D_FrameEnd(0);
    }

    // Free resources
    C2D_TextBufDelete(staticBuf);
    C2D_TextBufDelete(bottomBuf);
    exitServices();
    
    return 0;
}

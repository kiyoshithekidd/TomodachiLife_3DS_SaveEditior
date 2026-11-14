#include <3ds.h>
#include <citro2d.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <memory>
#include <vector>
#include <sys/stat.h>
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

// Global variables for Region Support
enum SaveRegion {
    REGION_US = 0,
    REGION_EU = 1,
    REGION_JP = 2,
    REGION_KR = 3
};
SaveRegion currentRegion = REGION_US;

struct CategoryInfo {
    const char** names;       // pointer to the name array
    int nameCount;            // total items in the name array 
    int nameOffset;           // offset into the array (0 for direct arrays)
    u32 saveBase;             // base save file offset
    int bytesPerItem;         // 1 for food/interiors/goods/treasures, 8 for clothes
    bool hasColors;           // true if items have color variants
    bool hasSpecialLogic;     // true if it uses the SpotPass/StreetPass scattered offsets
    int maxItems;             // total number of items in the name list
    const u8* colorCounts;    // per-item valid color variant counts (NULL if uniform)
    std::vector<int> filteredIndices;  // populated at init time
};

// Per-item valid color variant counts (1..8) for Clothes (447 slots at 0x30..0xE27)
// and Hats (219 slots at 0xFF8..0x16CF). Trailing bytes beyond an item's valid color
// count must remain 0xFD (253) in savedataArc.txt to prevent blank previews and corruption.
const u8 CLOTHES_COLOR_COUNTS_US[447] = {
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 5, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    6, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 5, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 7, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 7,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
};

const u8 CLOTHES_COLOR_COUNTS_EU[447] = {
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 5, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
};

const u8 HATS_COLOR_COUNTS_US[219] = {
    8, 8, 8, 8, 8, 8, 1, 8, 6, 8, 1, 1, 6, 6, 1, 1, 1, 8, 1, 1, 1, 1, 1, 1,
    1, 8, 8, 1, 1, 1, 1, 1, 8, 1, 8, 1, 1, 1, 8, 1, 8, 8, 8, 6, 5, 6, 4, 6,
    8, 8, 8, 8, 8, 1, 8, 8, 8, 8, 7, 8, 8, 7, 7, 8, 5, 8, 7, 6, 8, 8, 8, 8,
    1, 8, 8, 8, 6, 1, 8, 8, 8, 8, 8, 7, 6, 8, 7, 8, 8, 8, 8, 8, 8, 8, 8, 7,
    7, 8, 8, 8, 6, 7, 8, 8, 8, 8, 8, 7, 7, 8, 5, 8, 6, 1, 1, 6, 8, 8, 8, 7,
    8, 6, 8, 8, 8, 7, 7, 6, 8, 7, 1, 5, 5, 8, 1, 8, 5, 4, 8, 4, 7, 6, 8, 1,
    1, 1, 1, 8, 8, 8, 8, 8, 8, 1, 1, 8, 1, 8, 4, 6, 6, 1, 7, 7, 6, 1, 1, 8,
    1, 1, 1, 8, 8, 6, 6, 8, 7, 5, 7, 1, 5, 1, 1, 8, 1, 7, 7, 1, 8, 8, 1, 1,
    8, 1, 1, 1, 6, 1, 1, 1, 1, 1, 1, 6, 1, 1, 7, 6, 6, 6, 6, 1, 6, 5, 5, 1,
    7, 1, 8,
};

const u8 HATS_COLOR_COUNTS_EU[219] = {
    8, 8, 8, 8, 7, 8, 1, 8, 6, 8, 1, 1, 6, 6, 1, 1, 1, 1, 8, 1, 1, 1, 1, 1,
    1, 1, 8, 8, 1, 1, 1, 8, 1, 8, 1, 1, 1, 8, 1, 8, 8, 8, 6, 5, 6, 4, 6, 8,
    6, 8, 8, 8, 8, 1, 8, 8, 8, 8, 7, 8, 8, 7, 7, 8, 5, 8, 7, 6, 8, 8, 8, 8,
    1, 8, 8, 8, 6, 1, 8, 8, 8, 8, 8, 7, 6, 8, 7, 8, 8, 8, 8, 8, 8, 8, 8, 7,
    7, 8, 8, 8, 6, 7, 8, 8, 8, 8, 8, 7, 7, 8, 5, 8, 6, 1, 1, 6, 8, 8, 8, 7,
    8, 6, 8, 8, 8, 7, 7, 6, 8, 7, 1, 5, 5, 8, 1, 8, 5, 4, 8, 4, 6, 6, 8, 1,
    1, 1, 1, 8, 8, 8, 8, 8, 8, 1, 1, 8, 1, 8, 4, 6, 6, 1, 7, 7, 6, 1, 1, 8,
    1, 1, 1, 8, 8, 6, 6, 5, 7, 5, 7, 1, 5, 1, 1, 8, 1, 7, 7, 1, 8, 8, 1, 1,
    8, 1, 1, 1, 6, 1, 1, 1, 1, 1, 1, 6, 1, 1, 7, 6, 6, 6, 6, 1, 6, 5, 5, 1,
    7, 1, 8,
};

CategoryInfo categories[10]; // FOOD, CLOTHES, HATS, INTERIORS, GOODS, TREASURES, SPECIAL_FOOD
int selectedColor = -1;
int savedItemCursor = 0;

int getValidColorCount(const CategoryInfo* cat, int originalIdx) {
    if (!cat->hasColors) return 1;
    if (cat->colorCounts != NULL && originalIdx >= 0 && originalIdx < cat->maxItems) {
        int count = (int)cat->colorCounts[originalIdx];
        if (count >= 1 && count <= cat->bytesPerItem) {
            return count;
        }
    }
    return cat->bytesPerItem;
}

void initCategory(CategoryInfo& cat, const char** names, int count, int offset, int maxItems,
                  u32 saveBase, int bytesPerItem, bool hasColors, bool needsFiltering,
                  const u8* colorCounts = NULL) {
    cat.names = names;
    cat.nameCount = count;
    cat.nameOffset = offset;
    cat.saveBase = saveBase;
    cat.bytesPerItem = bytesPerItem;
    cat.hasColors = hasColors;
    cat.hasSpecialLogic = needsFiltering;
    cat.maxItems = maxItems;
    cat.colorCounts = colorCounts;
    cat.filteredIndices.clear();
    
    for (int i = 0; i < maxItems; i++) {
        if (offset + i >= count) break;
        if (needsFiltering) {
            const char* name = names[offset + i];
            if (strstr(name, "Unknown_Item") != NULL) continue;
            if (strstr(name, "Unknown_")     != NULL) continue;
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

const char* getRegionName(SaveRegion reg) {
    switch (reg) {
        case REGION_US: return "US";
        case REGION_EU: return "EU";
        case REGION_JP: return "JP";
        case REGION_KR: return "KR";
        default:        return "US";
    }
}

u32 getMoneyOffset(SaveRegion reg) {
    switch (reg) {
        case REGION_JP: return 0x14BCA8;
        case REGION_KR: return 0x1EFF68;
        default:        return 0x1E4BB8;
    }
}

u32 getIslandNameOffset(SaveRegion reg) {
    switch (reg) {
        case REGION_JP: return 0x14BCBC;
        case REGION_KR: return 0x1EFF7C;
        default:        return 0x1E4BCC;
    }
}

void updateRegionOffsets() {
    switch (currentRegion) {
        case REGION_US:
            categories[1].colorCounts = CLOTHES_COLOR_COUNTS_US;
            categories[2].saveBase = 0xFF8;
            categories[2].colorCounts = HATS_COLOR_COUNTS_US;
            categories[5].names = TREASURES_NAMES_US;
            categories[6].saveBase = 0x19E2;
            break;
        case REGION_EU:
            categories[1].colorCounts = CLOTHES_COLOR_COUNTS_EU;
            categories[2].saveBase = 0xFF8;
            categories[2].colorCounts = HATS_COLOR_COUNTS_EU;
            categories[5].names = TREASURES_NAMES_EU;
            categories[6].saveBase = 0x1A14;
            break;
        case REGION_JP:
            categories[1].colorCounts = CLOTHES_COLOR_COUNTS_US;
            categories[2].saveBase = 0xFF8;
            categories[2].colorCounts = HATS_COLOR_COUNTS_US;
            categories[5].names = TREASURES_NAMES_US;
            categories[6].saveBase = 0x19B0;
            break;
        case REGION_KR:
            categories[1].colorCounts = CLOTHES_COLOR_COUNTS_US;
            categories[2].saveBase = 0xFF8;
            categories[2].colorCounts = HATS_COLOR_COUNTS_US;
            categories[5].names = TREASURES_NAMES_US;
            categories[6].saveBase = 0x1A46;
            break;
        default:
            categories[1].colorCounts = CLOTHES_COLOR_COUNTS_US;
            categories[2].saveBase = 0xFF8;
            categories[2].colorCounts = HATS_COLOR_COUNTS_US;
            categories[5].names = TREASURES_NAMES_US;
            categories[6].saveBase = 0x19E2;
            break;
    }
}

// ── RAII wrappers & Auto-Backup ──────────────────────────────────────

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

static bool g_backupCreated = false;
static char g_backupStatus[64] = "Standby (Before 1st Edit)";

class SafeFile {
private:
    Handle mHandle = 0;
    FILE* mStdFile = NULL;

    void ensureBackupBeforeWrite() {
        if (g_backupCreated) return;
        g_backupCreated = true;

        mkdir("sdmc:/3ds", 0777);
        mkdir("sdmc:/3ds/tlse", 0777);
        mkdir("sdmc:/3ds/tlse/backups", 0777);

        const char* backupPath = "sdmc:/3ds/tlse/backups/savedataArc.bak";
        FILE* out = fopen(backupPath, "wb");
        if (!out) {
            backupPath = "savedataArc.bak";
            out = fopen(backupPath, "wb");
        }
        if (!out) {
            snprintf(g_backupStatus, sizeof(g_backupStatus), "Backup Failed (SD Read-Only)");
            return;
        }

        u8 chunk[4096];
        u64 offset = 0;
        while (true) {
            u32 bytesRead = 0;
            if (R_FAILED(read(offset, chunk, sizeof(chunk), &bytesRead)) || bytesRead == 0) {
                break;
            }
            fwrite(chunk, 1, bytesRead, out);
            offset += bytesRead;
            if (bytesRead < sizeof(chunk)) break;
        }
        fclose(out);
        snprintf(g_backupStatus, sizeof(g_backupStatus), "Saved (savedataArc.bak)");
    }

public:
    SafeFile(FS_Archive archive, FS_Path path, u32 flags, u32 attributes) {
        if (R_FAILED(FSUSER_OpenFile(&mHandle, archive, path, flags, attributes))) {
            mHandle = 0;
        }
    }

    explicit SafeFile(const char* sdFilePath) {
        mStdFile = fopen(sdFilePath, "r+b");
    }
    
    ~SafeFile() {
        if (mHandle) {
            FSFILE_Close(mHandle);
        }
        if (mStdFile) {
            fflush(mStdFile);
            fclose(mStdFile);
        }
    }

    bool isOpen() const {
        return (mHandle != 0) || (mStdFile != NULL);
    }

    u64 getSize() {
        if (mHandle) {
            u64 sz = 0;
            if (!R_FAILED(FSFILE_GetSize(mHandle, &sz))) return sz;
            return 0;
        }
        if (mStdFile) {
            long cur = ftell(mStdFile);
            fseek(mStdFile, 0, SEEK_END);
            long end = ftell(mStdFile);
            fseek(mStdFile, cur, SEEK_SET);
            return (end > 0) ? (u64)end : 0;
        }
        return 0;
    }
    
    Result write(u64 offset, const void* data, u32 size, u32* bytesWritten) {
        if (!isOpen()) return -1;
        ensureBackupBeforeWrite();
        if (mHandle) {
            return FSFILE_Write(mHandle, bytesWritten, offset, data, size, FS_WRITE_FLUSH);
        }
        if (mStdFile) {
            if (fseek(mStdFile, (long)offset, SEEK_SET) != 0) return -1;
            size_t w = fwrite(data, 1, size, mStdFile);
            fflush(mStdFile);
            if (bytesWritten) *bytesWritten = (u32)w;
            return (w == size) ? 0 : -1;
        }
        return -1;
    }
    
    Result read(u64 offset, void* data, u32 size, u32* bytesRead) {
        if (mHandle) {
            return FSFILE_Read(mHandle, bytesRead, offset, data, size);
        }
        if (mStdFile) {
            if (fseek(mStdFile, (long)offset, SEEK_SET) != 0) return -1;
            size_t r = fread(data, 1, size, mStdFile);
            if (bytesRead) *bytesRead = (u32)r;
            return (r > 0) ? 0 : -1;
        }
        return -1;
    }

    Handle get() const { return mHandle; }

    // Delete copy constructors to prevent double-closing of handle
    SafeFile(const SafeFile&) = delete;
    SafeFile& operator=(const SafeFile&) = delete;
};

// ── Special Items Logic ──────────────────────────────────────────────
// Verified SpotPass / StreetPass slot indices (within standard Clothes/Hats blocks)
// excluding all unmapped Unknown_Item gaps.
static const u16 SPOTPASS_CLOTHES_SLOTS_US[] = {
    2, 20, 27, 39, 43, 45, 54, 69, 75, 76, 83, 91, 138, 141, 146, 153, 155,
    158, 161, 167, 174, 198, 200, 216, 217, 224, 240, 241, 244, 262, 266, 280,
    297, 301, 306, 308, 314, 316, 329, 331, 332, 346, 348, 349, 360, 427, 428,
    431, 437
};
static const u16 SPOTPASS_CLOTHES_SLOTS_EU[] = {
    6, 19, 26, 27, 33, 38, 42, 44, 53, 68, 74, 75, 81, 82, 88, 95, 134, 137,
    140, 154, 157, 174, 196, 198, 214, 215, 222, 238, 239, 241, 259, 263, 278,
    280, 285, 295, 299, 304, 306, 313, 315, 321, 328, 329, 331, 345, 347, 348,
    359, 398, 445
};
static const u16 SPOTPASS_HATS_SLOTS_US[] = {
    5, 49, 63, 70, 90, 92, 95, 96, 108, 112, 121, 137, 142, 176, 178, 206
};
static const u16 SPOTPASS_HATS_SLOTS_EU[] = {
    0, 5, 18, 48, 55, 63, 70, 71, 79, 90, 92, 95, 96, 97, 108, 112, 137, 140, 176
};

void unlockSpecialItems(SafeFile* file, CategoryInfo* cat, u8 val) {
    if (!cat->hasSpecialLogic) return;

    const u16* slots = NULL;
    int slotCount = 0;
    if (cat == &categories[1]) {
        if (currentRegion == REGION_EU) {
            slots = SPOTPASS_CLOTHES_SLOTS_EU;
            slotCount = (int)(sizeof(SPOTPASS_CLOTHES_SLOTS_EU) / sizeof(u16));
        } else {
            slots = SPOTPASS_CLOTHES_SLOTS_US;
            slotCount = (int)(sizeof(SPOTPASS_CLOTHES_SLOTS_US) / sizeof(u16));
        }
    } else if (cat == &categories[2]) {
        if (currentRegion == REGION_EU) {
            slots = SPOTPASS_HATS_SLOTS_EU;
            slotCount = (int)(sizeof(SPOTPASS_HATS_SLOTS_EU) / sizeof(u16));
        } else {
            slots = SPOTPASS_HATS_SLOTS_US;
            slotCount = (int)(sizeof(SPOTPASS_HATS_SLOTS_US) / sizeof(u16));
        }
    }
    if (slots == NULL) return;

    u32 bytesWritten = 0;
    for (int idx = 0; idx < slotCount; idx++) {
        int slot = (int)slots[idx];
        if (slot < 0 || slot >= cat->maxItems) continue;
        int validColors = getValidColorCount(cat, slot);
        for (int c = 0; c < validColors; c++) {
            file->write(cat->saveBase + (slot * cat->bytesPerItem) + c, &val, 1, &bytesWritten);
        }
    }
}

void unlockSpecialFoods(SafeFile* file, u8 val) {
    u32 base = categories[6].saveBase;
    if (base == 0) {
        base = (currentRegion == REGION_EU) ? 0x1A14 : 0x19E2;
    }
    u32 bytesWritten = 0;
    // Slot 0 is at base (0x19E2 US / 0x1A14 EU); byte at base + 1 is skipped (0xFD sentinel);
    // Slots 1..47 are contiguous at (base + 2) .. (base + 48).
    file->write(base, &val, 1, &bytesWritten);
    for (u32 i = 0; i < 47; i++) {
        file->write(base + 2 + i, &val, 1, &bytesWritten);
    }
}

void readIslandMetadata(SafeFile* file, SaveRegion reg, u32* outMoney, char* outIslandNameUTF8) {
    if (!file || !file->isOpen()) return;
    u32 bytesRead = 0;
    file->read(getMoneyOffset(reg), outMoney, sizeof(u32), &bytesRead);

    u16 islandNameUTF16[11] = {0};
    file->read(getIslandNameOffset(reg), islandNameUTF16, 20, &bytesRead);
    memset(outIslandNameUTF8, 0, 32);
    utf16_to_utf8((uint8_t*)outIslandNameUTF8, islandNameUTF16, 31);
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

// ── Sprite Atlas Indices (must match gfx/sprites.t3s order) ────────────

enum SpriteIndex {
    SPR_BANNER_LOGO = 0,
    SPR_TOP_BG,
    SPR_BOTTOM_BG,
    SPR_ICON_MONEY,
    SPR_ICON_FOOD,
    SPR_ICON_CLOTHES,
    SPR_ICON_HATS,
    SPR_ICON_INTERIORS,
    SPR_ICON_GOODS,
    SPR_ICON_TREASURES,
    SPR_ICON_SPECIAL,
    SPR_BADGE_US,
    SPR_BADGE_EU,
    SPR_BADGE_JP,
    SPR_BADGE_KR,
    SPR_BADGE_CARTRIDGE,
    SPR_BADGE_SD,
    SPR_BADGE_BACKUP_OK,
    SPR_CURSOR_HAND,
    SPR_ROW_NORMAL,
    SPR_ROW_SELECTED,
    SPR_BTN_STEPPER,
    SPR_COLOR_SWATCH_RING,
    SPR_MODAL_ERROR,
    SPR_COUNT
};

static inline void drawSprite(C2D_SpriteSheet sheet, size_t idx, float x, float y,
                              float scaleX = 1.0f, float scaleY = 1.0f,
                              const C2D_ImageTint* tint = nullptr) {
    if (sheet != nullptr && idx < C2D_SpriteSheetCount(sheet)) {
        C2D_DrawImageAt(C2D_SpriteSheetGetImage(sheet, idx), x, y, 0.5f, tint, scaleX, scaleY);
    }
}

// ── Main ─────────────────────────────────────────────────────────────

int main(int argc, char** argv) {
    initServices();

    C3D_RenderTarget* topTarget = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    C3D_RenderTarget* bottomTarget = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    C2D_TextBuf staticBuf = C2D_TextBufNew(4096);
    C2D_TextBuf bottomBuf = C2D_TextBufNew(4096);
    C2D_SpriteSheet spriteSheet = C2D_SpriteSheetLoad("romfs:/gfx/sprites.t3x");

    g_backupCreated = false;
    snprintf(g_backupStatus, sizeof(g_backupStatus), "Standby (Before 1st Edit)");

    struct TitleCandidate {
        FS_MediaType media;
        u32 lowId;
        SaveRegion region;
        const char* sourceLabel;
    };
    // Probe Game Card first across all 4 regional Title IDs, then SD Installed titles (#17, #23, #34)
    static const TitleCandidate CANDIDATES[] = {
        {MEDIATYPE_GAME_CARD, 0x0008C300, REGION_US, "Cartridge (US)"},
        {MEDIATYPE_GAME_CARD, 0x0008C400, REGION_EU, "Cartridge (EU)"},
        {MEDIATYPE_GAME_CARD, 0x00035100, REGION_JP, "Cartridge (JP)"},
        {MEDIATYPE_GAME_CARD, 0x0008C500, REGION_KR, "Cartridge (KR)"},
        {MEDIATYPE_SD,        0x0008C300, REGION_US, "SD Digital (US)"},
        {MEDIATYPE_SD,        0x0008C400, REGION_EU, "SD Digital (EU)"},
        {MEDIATYPE_SD,        0x00035100, REGION_JP, "SD Digital (JP)"},
        {MEDIATYPE_SD,        0x0008C500, REGION_KR, "SD Digital (KR)"},
    };

    std::unique_ptr<SafeArchive> archive = nullptr;
    std::unique_ptr<SafeFile> file = nullptr;
    bool fileOpenSuccess = false;
    char saveSourceDesc[64] = "Not Found";

    for (size_t i = 0; i < sizeof(CANDIDATES) / sizeof(TitleCandidate); i++) {
        u32 pathData[3] = {(u32)CANDIDATES[i].media, CANDIDATES[i].lowId, 0x00040000};
        FS_Path arcPath = {PATH_BINARY, 12, (const void*)pathData};
        std::unique_ptr<SafeArchive> candidateArc(new SafeArchive(ARCHIVE_USER_SAVEDATA, arcPath));
        if (candidateArc->get() != 0) {
            FS_Path filePath = fsMakePath(PATH_ASCII, "/savedataArc.txt");
            std::unique_ptr<SafeFile> candidateFile(new SafeFile(candidateArc->get(), filePath, FS_OPEN_READ | FS_OPEN_WRITE, 0));
            if (candidateFile->isOpen()) {
                archive = std::move(candidateArc);
                file = std::move(candidateFile);
                currentRegion = CANDIDATES[i].region;
                snprintf(saveSourceDesc, sizeof(saveSourceDesc), "%s", CANDIDATES[i].sourceLabel);
                fileOpenSuccess = true;
                break;
            }
        }
    }

    // Fallback: Check SD card paths for Checkpoint-exported savedataArc.txt (#18)
    if (!fileOpenSuccess) {
        static const char* SD_FALLBACK_PATHS[] = {
            "sdmc:/3ds/tlse/savedataArc.txt",
            "sdmc:/3ds/TomodachiLife_3DS_SaveEditor/savedataArc.txt",
            "sdmc:/savedataArc.txt"
        };
        for (size_t i = 0; i < sizeof(SD_FALLBACK_PATHS) / sizeof(const char*); i++) {
            std::unique_ptr<SafeFile> sdFile(new SafeFile(SD_FALLBACK_PATHS[i]));
            if (sdFile->isOpen()) {
                u64 fsize = sdFile->getSize();
                if (fsize == 0x14BD68) currentRegion = REGION_JP;
                else if (fsize == 0x1F0048) currentRegion = REGION_KR;
                file = std::move(sdFile);
                snprintf(saveSourceDesc, sizeof(saveSourceDesc), "SD File (savedataArc.txt)");
                fileOpenSuccess = true;
                break;
            }
        }
    }

    char displayString[2048] = "Tomodachi Life Save Editor\n\n";
    char bottomString[2048] = "";
    u32 currentMoney = 0;
    char islandNameUTF8[32] = {0};

    if (fileOpenSuccess) {
        readIslandMetadata(file.get(), currentRegion, &currentMoney, islandNameUTF8);
    }

    // ── Initialize categories ────────────────────────────────────────
    // Food: index 0 is "Nothing", save at 0x17F0+ID
    initCategory(categories[0], FOOD_NAMES, FOOD_COUNT, 0, FOOD_COUNT,
                 0x17F0, 1, false, false);
    // Clothes: standard block at 0x30+(ID*8), 447 items (0x30..0xE27)
    // Offset 984 aligns item 0 ("regular clothes") with save slot 0 (0x30)
    initCategory(categories[1], CLOTHES_NAMES, CLOTHES_COUNT, 984, 447,
                 0x30, 8, true, true, CLOTHES_COLOR_COUNTS_US);
    // Hats: standard block at 0xFF8+(ID*8), 219 items (0xFF8..0x16CF)
    // Offset 2600 aligns item 0 ("bear hat") with save slot 0 (0xFF8)
    initCategory(categories[2], CLOTHES_NAMES, CLOTHES_COUNT, 2600, 219,
                 0xFF8, 8, true, true, HATS_COLOR_COUNTS_US);
    // Interiors: save at 0x1778+ID, 102 items (filters out Unknown_108 at slot 98)
    initCategory(categories[3], INTERIORS_NAMES, INTERIORS_COUNT, 0, INTERIORS_COUNT,
                 0x1778, 1, false, true);
    // Goods: save at 0x18F0+ID, 18 items (US)
    initCategory(categories[4], GOODS_NAMES, GOODS_COUNT, 0, GOODS_COUNT,
                 0x18F0, 1, false, false);
    // Treasures: save at 0x1902+ID, 152 items (0x1902..0x1999)
    initCategory(categories[5], TREASURES_NAMES_US, TREASURES_COUNT, 0, TREASURES_COUNT,
                 0x1902, 1, false, true);
    // Special Foods: save at 0x19E2 (US) / 0x1A14 (EU), 48 items (skipping base + 1)
    initCategory(categories[6], NULL, 0, 0, 48,
                 0x19E2, 1, false, false);
                 
    // Update offsets and names based on the current region
    updateRegionOffsets();

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
            if (fileOpenSuccess && archive != nullptr && archive->get() != 0) {
                archive->commit(); 
            }
            break;
        }

        // ═══════════════════ INPUT LOGIC ═══════════════════

        if (currentState == STATE_MAIN_MENU) {
            if (kDown & KEY_DUP)   cursorIndex = (cursorIndex > 0) ? cursorIndex - 1 : MAIN_MENU_ITEMS - 1;
            if (kDown & KEY_DDOWN) cursorIndex = (cursorIndex < MAIN_MENU_ITEMS - 1) ? cursorIndex + 1 : 0;
            if (kDown & KEY_A && fileOpenSuccess) {
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
                    // Quick Action: Unlock All Special Items (SpotPass/StreetPass Clothes, Hats, and Special Foods)
                    unlockSpecialItems(file.get(), &categories[1], 99);
                    unlockSpecialItems(file.get(), &categories[2], 99);
                    unlockSpecialFoods(file.get(), 99);
                }
            }
            
            // Region Toggle logic
            if (kDown & KEY_L) {
                currentRegion = (currentRegion == REGION_US) ? REGION_EU : REGION_US;
                updateRegionOffsets();
                if (fileOpenSuccess) readIslandMetadata(file.get(), currentRegion, &currentMoney, islandNameUTF8);
            } else if (kDown & KEY_R) {
                currentRegion = (currentRegion == REGION_EU) ? REGION_US : REGION_EU;
                updateRegionOffsets();
                if (fileOpenSuccess) readIslandMetadata(file.get(), currentRegion, &currentMoney, islandNameUTF8);
            }
        }
        else if (currentState == STATE_MONEY_EDIT) {
            if (kDown & KEY_B) {
                currentState = STATE_MAIN_MENU;
                cursorIndex = 0;
            }
            if (fileOpenSuccess) {
                const u32 MAX_MONEY = 99999999; // $999,999.99 (Tomodachi Life maximum)
                bool valueChanged = false;
                if (kDown & KEY_A) {
                    currentMoney = (currentMoney + 10000 <= MAX_MONEY) ? (currentMoney + 10000) : MAX_MONEY;
                    valueChanged = true;
                }
                else if (kDown & KEY_RIGHT) {
                    currentMoney = (currentMoney + 1000000 <= MAX_MONEY) ? (currentMoney + 1000000) : MAX_MONEY;
                    valueChanged = true;
                }
                else if (kDown & KEY_LEFT) {
                    currentMoney = (currentMoney >= 1000000) ? (currentMoney - 1000000) : 0;
                    valueChanged = true;
                }
                else if (kDown & KEY_X) {
                    currentMoney = MAX_MONEY;
                    valueChanged = true;
                }
                else if (kDown & KEY_Y) {
                    currentMoney = 0;
                    valueChanged = true;
                }

                if (valueChanged) {
                    u32 bytesWritten = 0;
                    file->write(getMoneyOffset(currentRegion), &currentMoney, sizeof(u32), &bytesWritten);
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
                        int validColors = getValidColorCount(cat, originalIdx);
                        for (int c = 0; c < validColors; c++) {
                            file->write(cat->saveBase + (originalIdx * cat->bytesPerItem) + c, &val, 1, &bytesWritten);
                        }
                    } else {
                        file->write(cat->saveBase + originalIdx, &val, 1, &bytesWritten);
                    }
                }
            }
            if (kDown & KEY_A && maxItems > 0 && fileOpenSuccess) {
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
            CategoryInfo* cat = getCategoryForState(previousCategory);
            int originalIdx = cat->filteredIndices[savedItemCursor];
            int maxColorIdx = getValidColorCount(cat, originalIdx);

            if (colorCursor > maxColorIdx) colorCursor = maxColorIdx;
            if (kDown & KEY_DUP)   colorCursor = (colorCursor > 0) ? colorCursor - 1 : maxColorIdx;
            if (kDown & KEY_DDOWN) colorCursor = (colorCursor < maxColorIdx) ? colorCursor + 1 : 0;
            
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
                        int validColors = getValidColorCount(cat, originalIdx);
                        if (selectedColor == -1) {
                            // Write only valid color variants for this item
                            for (int i = 0; i < validColors; i++) {
                                file->write(cat->saveBase + (originalIdx * cat->bytesPerItem) + i, &val, 1, &bytesWritten);
                            }
                        } else if (selectedColor >= 0 && selectedColor < validColors) {
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
            const char* regionName = getRegionName(currentRegion);
            if (!fileOpenSuccess) {
                snprintf(displayString, sizeof(displayString),
                    "Tomodachi Life Save Editor\n\n"
                    "ERROR: Save Data Not Found!\n\n"
                    "Checked:\n"
                    "- Cartridge / SD Title (US/EU/JP/KR)\n"
                    "- sdmc:/3ds/tlse/savedataArc.txt\n\n"
                    "Insert cartridge or place Checkpoint\n"
                    "savedataArc.txt in /3ds/tlse/");
                snprintf(bottomString, sizeof(bottomString),
                    "Save file not loaded.\n"
                    "Press START to exit");
            } else {
                snprintf(displayString, sizeof(displayString), 
                    "Tomodachi Life Save Editor (Region: %s)\n"
                    "Source: %s\n"
                    "Island: %s | Money: $%lu.%02lu\n\n"
                    "%s Edit Money\n"
                    "%s Edit Food\n"
                    "%s Edit Clothes\n"
                    "%s Edit Hats\n"
                    "%s Edit Interiors\n"
                    "%s Edit Goods\n"
                    "%s Edit Treasures\n"
                    "%s Unlock All Special Items\n",
                    regionName, saveSourceDesc, islandNameUTF8, dollars, cents,
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
                        "Press A to unlock SpotPass/StreetPass\n"
                        "Clothes, Hats & Special Foods (x99)\n"
                        "Backup: %s\n"
                        "L/R: Region | START: Save & Exit",
                        g_backupStatus);
                } else {
                    snprintf(bottomString, sizeof(bottomString),
                        "Press A to select | L/R: Switch Region\n"
                        "Backup: %s\n"
                        "Press START to save & exit",
                        g_backupStatus);
                }
            }
        }
        else if (currentState == STATE_MONEY_EDIT) {
            u32 dollars = currentMoney / 100;
            u32 cents = currentMoney % 100;
            if (fileOpenSuccess) {
                snprintf(displayString, sizeof(displayString), 
                    "Money Editor\n\n"
                    "Island Name: %s\n"
                    "> Current Money: $%lu.%02lu <\n"
                    "(Max Limit: $999,999.99)\n",
                    islandNameUTF8, dollars, cents);
                snprintf(bottomString, sizeof(bottomString),
                    "A: +$100.00 | L/R D-Pad: -/+$10,000\n"
                    "X: Max ($999,999.99) | Y: Reset ($0)\n"
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
            snprintf(listStr, sizeof(listStr), "%s List (%d items)\n", title, maxItems);
            
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
                snprintf(temp, sizeof(temp), "%s [%03d] %.40s\n", (i == cursorIndex) ? "->" : "  ", i + 1, itemName);
                strncat(listStr, temp, sizeof(listStr) - strlen(listStr) - 1);
            }
            snprintf(displayString, sizeof(displayString), "%s", listStr);
            snprintf(bottomString, sizeof(bottomString),
                "A: Set Quantity | X: Unlock All (x99)\n"
                "Up/Down: Scroll | B: Back");
        }
        else if (currentState == STATE_COLOR_SELECT) {
            CategoryInfo* cat = getCategoryForState(previousCategory);
            int originalIdx = cat->filteredIndices[savedItemCursor];
            int validColors = getValidColorCount(cat, originalIdx);
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
            snprintf(colorStr, sizeof(colorStr), "Item: %.38s\nSelect Color (%d available):\n\n", itemName, validColors);
            for (int i = 0; i <= validColors; i++) {
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

            u8 currentOwned = 0;
            if (fileOpenSuccess) {
                u32 bytesRead = 0;
                u64 slotOffset = cat->hasColors
                    ? (cat->saveBase + (originalIdx * cat->bytesPerItem) + ((selectedColor >= 0) ? selectedColor : 0))
                    : (cat->saveBase + originalIdx);
                file->read(slotOffset, &currentOwned, 1, &bytesRead);
            }
            
            snprintf(displayString, sizeof(displayString), 
                "Set Item Quantity:\n%.40s\n\nCurrent Owned: %u\nNew Quantity:  %d\n\n(Replaces current inventory count)", 
                itemName, (unsigned)currentOwned, quantityToGive);
            snprintf(bottomString, sizeof(bottomString), 
                "Up/Down: +/- 1 | Left/Right: +/- 10\n"
                "A: Set Quantity | B: Cancel");
        }

        C2D_Text infoText;
        C2D_TextParse(&infoText, staticBuf, displayString);
        C2D_TextOptimize(&infoText);
        
        C2D_Text bottomText;
        C2D_TextParse(&bottomText, bottomBuf, bottomString);
        C2D_TextOptimize(&bottomText);
        
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

        const u32 colSkyBlue  = C2D_Color32(110, 198, 255, 255);
        const u32 colCream    = C2D_Color32(255, 253, 245, 232);
        const u32 colSand     = C2D_Color32(215, 204, 200, 255);
        const u32 colTextDark = C2D_Color32(62, 39, 35, 255);

        // ── Top Screen (400x240) ─────────────────────────────────────
        C2D_TargetClear(topTarget, colSkyBlue);
        C2D_SceneBegin(topTarget);
        drawSprite(spriteSheet, SPR_TOP_BG, 0.0f, 0.0f);

        if (!fileOpenSuccess) {
            drawSprite(spriteSheet, SPR_MODAL_ERROR, 40.0f, 36.0f);
            C2D_DrawText(&infoText, C2D_WithColor, 54.0f, 72.0f, 0.5f, 0.48f, 0.48f, colTextDark);
        } else {
            // Warm Notebook Cream card container with Soft Sand border
            C2D_DrawRectSolid(7.0f, 5.0f, 0.2f, 386.0f, 230.0f, colSand);
            C2D_DrawRectSolid(8.0f, 6.0f, 0.3f, 384.0f, 228.0f, colCream);

            if (currentState == STATE_MAIN_MENU) {
                // Region, Media Source, and Backup badges at top-right
                size_t regBadge = SPR_BADGE_US;
                if (currentRegion == REGION_EU) regBadge = SPR_BADGE_EU;
                else if (currentRegion == REGION_JP) regBadge = SPR_BADGE_JP;
                else if (currentRegion == REGION_KR) regBadge = SPR_BADGE_KR;
                drawSprite(spriteSheet, regBadge, 346.0f, 10.0f);

                bool isCart = (strncmp(saveSourceDesc, "Cartridge", 9) == 0);
                drawSprite(spriteSheet, isCart ? SPR_BADGE_CARTRIDGE : SPR_BADGE_SD, 322.0f, 9.0f);
                if (g_backupCreated) {
                    drawSprite(spriteSheet, SPR_BADGE_BACKUP_OK, 298.0f, 9.0f);
                }

                // Active category shop icon preview & hand cursor
                drawSprite(spriteSheet, SPR_ICON_MONEY + (size_t)cursorIndex, 344.0f, 186.0f, 1.25f, 1.25f);
                drawSprite(spriteSheet, SPR_CURSOR_HAND, 318.0f, 194.0f, 0.9f, 0.9f);
            } else if (currentState == STATE_MONEY_EDIT) {
                drawSprite(spriteSheet, SPR_ICON_MONEY, 352.0f, 10.0f, 1.0f, 1.0f);
            } else if (currentState >= STATE_CATEGORY_FOOD && currentState <= STATE_CATEGORY_TREASURES) {
                size_t catIcon = SPR_ICON_FOOD + (size_t)(currentState - STATE_CATEGORY_FOOD);
                drawSprite(spriteSheet, catIcon, 352.0f, 10.0f, 1.0f, 1.0f);
            } else if (currentState == STATE_COLOR_SELECT) {
                static const u32 swatchColors[8] = {
                    C2D_Color32(239, 83, 80, 255),   // Color 1
                    C2D_Color32(66, 165, 245, 255),  // Color 2
                    C2D_Color32(102, 187, 106, 255), // Color 3
                    C2D_Color32(255, 213, 79, 255),  // Color 4
                    C2D_Color32(171, 71, 188, 255),  // Color 5
                    C2D_Color32(255, 167, 38, 255),  // Color 6
                    C2D_Color32(141, 110, 99, 255),  // Color 7
                    C2D_Color32(96, 125, 139, 255)   // Color 8
                };
                CategoryInfo* cat = getCategoryForState(previousCategory);
                int originalIdx = cat->filteredIndices[savedItemCursor];
                int validColors = getValidColorCount(cat, originalIdx);
                for (int c = 0; c < validColors && c < 8; c++) {
                    C2D_ImageTint tint;
                    C2D_PlainImageTint(&tint, swatchColors[c], 0.85f);
                    drawSprite(spriteSheet, SPR_COLOR_SWATCH_RING, 150.0f, 70.0f + (c * 15.0f), 0.9f, 0.9f, &tint);
                }
            }

            C2D_DrawText(&infoText, C2D_WithColor, 14.0f, 10.0f, 0.6f, 0.5f, 0.5f, colTextDark);
        }

        // ── Bottom Screen (320x240) ──────────────────────────────────
        C2D_TargetClear(bottomTarget, colCream);
        C2D_SceneBegin(bottomTarget);
        drawSprite(spriteSheet, SPR_BOTTOM_BG, 0.0f, 0.0f);
        drawSprite(spriteSheet, SPR_BANNER_LOGO, 32.0f, 34.0f);

        // Cream control card in lower half
        C2D_DrawRectSolid(15.0f, 104.0f, 0.2f, 290.0f, 100.0f, colSand);
        C2D_DrawRectSolid(16.0f, 105.0f, 0.3f, 288.0f, 98.0f, colCream);

        float textW = 0, textH = 0;
        C2D_TextGetDimensions(&bottomText, 0.5f, 0.5f, &textW, &textH);
        float textX = (320.0f - textW) / 2.0f;
        float textY = 105.0f + (98.0f - textH) / 2.0f;
        C2D_DrawText(&bottomText, C2D_WithColor, textX, textY, 0.6f, 0.5f, 0.5f, colTextDark);

        C3D_FrameEnd(0);
    }

    // Free resources
    if (spriteSheet != nullptr) {
        C2D_SpriteSheetFree(spriteSheet);
    }
    C2D_TextBufDelete(staticBuf);
    C2D_TextBufDelete(bottomBuf);
    exitServices();

    return 0;
}

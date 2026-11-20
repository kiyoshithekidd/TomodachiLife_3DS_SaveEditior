#include <3ds.h>
#include <citro2d.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <memory>
#include <vector>
#include <sys/stat.h>

#include "save_io.h"
#include "food_names.h"
#include "clothes_dataset.h"
#include "interiors_dataset.h"
#include "treasures_dataset.h"
#include "goods_dataset.h"
#include "mii_data.h"
#include "relationships_data.h"
#include "island_data.h"

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

void formatFoodName(u16 id, char* out, size_t outSz) {
    if (id == 0xFFFF) {
        snprintf(out, outSz, "None");
        return;
    }
    if ((int)id < FOOD_COUNT) {
        strncpy(out, FOOD_NAMES[id], outSz - 1);
        out[outSz - 1] = '\0';
        capitalizeString(out);
    } else {
        snprintf(out, outSz, "Special Food #%u", (unsigned)id);
    }
}

void formatClothesName(u16 id, char* out, size_t outSz) {
    if (id == 0xFFFF) {
        snprintf(out, outSz, "None");
        return;
    }
    int slot = (int)(id / 8);
    int col = (int)(id % 8) + 1;
    if (slot >= 0 && slot < 447 && (990 + slot) < CLOTHES_COUNT) {
        char base[40];
        strncpy(base, CLOTHES_NAMES[990 + slot], sizeof(base) - 1);
        base[sizeof(base) - 1] = '\0';
        capitalizeString(base);
        snprintf(out, outSz, "%.22s (C%d)", base, col);
    } else {
        snprintf(out, outSz, "Clothes #%u", (unsigned)id);
    }
}

void formatHatName(u16 id, char* out, size_t outSz) {
    if (id == 0xFFFF) {
        snprintf(out, outSz, "None");
        return;
    }
    int slot = (int)(id / 8);
    int col = (int)(id % 8) + 1;
    if (slot >= 0 && slot < 219 && (2606 + slot) < CLOTHES_COUNT) {
        char base[40];
        strncpy(base, CLOTHES_NAMES[2606 + slot], sizeof(base) - 1);
        base[sizeof(base) - 1] = '\0';
        capitalizeString(base);
        snprintf(out, outSz, "%.22s (C%d)", base, col);
    } else {
        snprintf(out, outSz, "Hat #%u", (unsigned)id);
    }
}

void formatInteriorName(u16 id, char* out, size_t outSz) {
    if (id == 0xFFFF) {
        snprintf(out, outSz, "None");
        return;
    }
    if ((int)id < INTERIORS_COUNT) {
        strncpy(out, INTERIORS_NAMES[id], outSz - 1);
        out[outSz - 1] = '\0';
        capitalizeString(out);
    } else {
        snprintf(out, outSz, "Interior #%u", (unsigned)id);
    }
}

int exportColorBitToIndex(u8 bit) {
    for (int i = 0; i < 8; i++) {
        if (bit == (u8)(1u << i)) return i;
    }
    return 0;
}

bool editStringWithKeyboard(char* outBuf, int maxChars, const char* hintText) {
    SwkbdState swkbd;
    swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 2, maxChars);
    swkbdSetInitialText(&swkbd, outBuf);
    swkbdSetHintText(&swkbd, hintText);
    swkbdSetButton(&swkbd, SWKBD_BUTTON_LEFT, "Cancel", false);
    swkbdSetButton(&swkbd, SWKBD_BUTTON_RIGHT, "Save", true);
    char temp[128] = {0};
    SwkbdButton btn = swkbdInputText(&swkbd, temp, sizeof(temp));
    if (btn == SWKBD_BUTTON_CONFIRM) {
        strncpy(outBuf, temp, (size_t)maxChars);
        outBuf[maxChars] = '\0';
        return true;
    }
    return false;
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
    STATE_QUANTITY_SELECT,
    // Mii Editor Suite (#11)
    STATE_MII_SELECT,
    STATE_MII_SUBMENU,
    STATE_MII_STATUS_EDIT,
    STATE_MII_PROFILE_EDIT,
    STATE_MII_VOICE_PERS,
    STATE_MII_GIFTS_FOODS,
    // Relationship Editor (#44)
    STATE_REL_MII_A,
    STATE_REL_MII_B,
    STATE_REL_EDIT,
    // Island Config (#45) & Island Info (#46)
    STATE_ISLAND_CONFIG,
    STATE_ISLAND_INFO,
    // Active Interactions (#47)
    STATE_INTERACTIONS_MII,
    STATE_INTERACTIONS_EDIT,
    // Quick Extras & Save Repair (#48)
    STATE_EXTRAS_MENU,
    // Travelers & Concert Hall (#49, #56)
    STATE_TRAVELERS_CONCERT_MENU,
    STATE_TRAVELERS_LIST,
    STATE_TRAVELER_EDIT,
    STATE_CONCERT_SONGS,
    STATE_CONCERT_EDIT
};

// ── Category Info ────────────────────────────────────────────────────

SaveRegion currentRegion = REGION_US;

struct CategoryInfo {
    const char** names;
    int nameCount;
    int nameOffset;
    u32 saveBase;
    int bytesPerItem;
    bool hasColors;
    bool hasSpecialLogic;
    int maxItems;
    const u8* colorCounts;
    std::vector<int> filteredIndices;
};

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
    8, 8, 8, 8, 8, 8, 0, 8, 6, 8, 0, 0, 6, 6, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0,
    0, 8, 8, 0, 0, 0, 0, 0, 8, 0, 8, 0, 0, 0, 8, 0, 8, 8, 8, 6, 5, 6, 4, 6,
    8, 8, 8, 8, 8, 0, 8, 8, 8, 8, 7, 8, 8, 7, 7, 8, 5, 8, 7, 6, 8, 8, 8, 8,
    0, 8, 8, 8, 6, 1, 8, 8, 8, 8, 8, 7, 6, 8, 7, 8, 8, 8, 8, 8, 8, 8, 8, 7,
    7, 8, 8, 8, 6, 7, 8, 8, 8, 8, 8, 7, 7, 8, 5, 8, 6, 0, 0, 6, 8, 8, 8, 7,
    8, 6, 8, 8, 8, 7, 7, 6, 8, 7, 0, 5, 5, 8, 0, 8, 5, 4, 8, 4, 7, 6, 8, 0,
    0, 0, 0, 8, 8, 8, 8, 8, 8, 0, 0, 8, 0, 8, 4, 6, 6, 0, 7, 7, 6, 0, 0, 8,
    1, 0, 0, 8, 8, 6, 6, 8, 7, 5, 7, 0, 5, 0, 0, 8, 0, 7, 7, 0, 8, 8, 0, 0,
    8, 0, 0, 0, 6, 0, 0, 0, 0, 0, 0, 6, 0, 0, 7, 6, 6, 6, 6, 0, 6, 5, 5, 0,
    7, 0, 8,
};

const u8 HATS_COLOR_COUNTS_EU[219] = {
    8, 8, 8, 8, 7, 8, 0, 8, 6, 8, 0, 0, 6, 6, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0,
    0, 0, 8, 8, 0, 0, 0, 8, 0, 8, 0, 0, 0, 8, 0, 8, 8, 8, 6, 5, 6, 4, 6, 8,
    6, 8, 8, 8, 8, 0, 8, 8, 8, 8, 7, 8, 8, 7, 7, 8, 5, 8, 7, 6, 8, 8, 8, 8,
    0, 8, 8, 8, 6, 1, 8, 8, 8, 8, 8, 7, 6, 8, 7, 8, 8, 8, 8, 8, 8, 8, 8, 7,
    7, 8, 8, 8, 6, 7, 8, 8, 8, 8, 8, 7, 7, 8, 5, 8, 6, 0, 0, 6, 8, 8, 8, 7,
    8, 6, 8, 8, 8, 7, 7, 6, 8, 7, 0, 5, 5, 8, 0, 8, 5, 4, 8, 4, 6, 6, 8, 0,
    0, 0, 0, 8, 8, 8, 8, 8, 8, 0, 0, 8, 0, 8, 4, 6, 6, 0, 7, 7, 6, 0, 0, 8,
    1, 0, 0, 8, 8, 6, 6, 5, 7, 5, 7, 0, 5, 0, 0, 8, 0, 7, 7, 0, 8, 8, 0, 0,
    8, 0, 0, 0, 6, 0, 0, 0, 0, 0, 0, 6, 0, 0, 7, 6, 6, 6, 6, 0, 6, 5, 5, 0,
    7, 0, 8,
};

CategoryInfo categories[10];
int selectedColor = -1;
int savedItemCursor = 0;

int getValidColorCount(const CategoryInfo* cat, int originalIdx) {
    if (!cat->hasColors) return 1;
    if (cat->colorCounts != NULL && originalIdx >= 0 && originalIdx < cat->maxItems) {
        int count = (int)cat->colorCounts[originalIdx];
        if (count >= 0 && count <= cat->bytesPerItem) return count;
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
        if (colorCounts != NULL && colorCounts[i] == 0) continue;
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

void rebuildCategoryFilter(CategoryInfo& cat) {
    cat.filteredIndices.clear();
    for (int i = 0; i < cat.maxItems; i++) {
        if (cat.nameOffset + i >= cat.nameCount) break;
        if (cat.colorCounts != NULL && cat.colorCounts[i] == 0) continue;
        if (cat.hasSpecialLogic && cat.names != NULL) {
            const char* name = cat.names[cat.nameOffset + i];
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
    rebuildCategoryFilter(categories[2]);
}

// ── Special Items Logic ──────────────────────────────────────────────

static const u16 SPOTPASS_CLOTHES_SLOTS_US[] = {
    2, 19, 20, 27, 39, 43, 45, 54, 69, 75, 76, 83, 91, 138, 141, 146, 153, 155,
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

static inline void formatItemQty(char* out, size_t outSize, u8 rawQty) {
    if (rawQty == 0xFF) snprintf(out, outSize, "N/A");
    else if (rawQty == 0xFD) snprintf(out, outSize, "Locked");
    else if (rawQty == 0xFE) snprintf(out, outSize, "x0");
    else if (rawQty > 99) snprintf(out, outSize, "x99+");
    else snprintf(out, outSize, "x%u", (unsigned)rawQty);
}

void unlockCategoryAll(SafeFile* file, SafeArchive* archive, CategoryInfo* cat, u8 val) {
    if (!file || !file->isOpen() || !cat || cat->maxItems <= 0) return;
    if (val > 99) val = 99;

    u32 totalBytes = (u32)(cat->maxItems * cat->bytesPerItem);
    std::vector<u8> buf(totalBytes, 0xFD);
    u32 bytesRead = 0;
    if (R_FAILED(file->read(cat->saveBase, buf.data(), totalBytes, &bytesRead)) || bytesRead == 0) {
        return;
    }

    for (size_t i = 0; i < cat->filteredIndices.size(); i++) {
        int originalIdx = cat->filteredIndices[i];
        if (originalIdx < 0 || originalIdx >= cat->maxItems) continue;
        if (cat->hasColors) {
            int validColors = getValidColorCount(cat, originalIdx);
            for (int c = 0; c < validColors; c++) {
                u32 off = (u32)(originalIdx * cat->bytesPerItem + c);
                if (off < totalBytes && buf[off] != 0xFF) buf[off] = val;
            }
        } else {
            u32 off = (u32)originalIdx;
            if (off < totalBytes && buf[off] != 0xFF) buf[off] = val;
        }
    }

    u32 bytesWritten = 0;
    file->write(cat->saveBase, buf.data(), totalBytes, &bytesWritten);
    if (archive && archive->get() != 0) archive->commit();
}

void unlockSpecialItems(SafeFile* file, SafeArchive* archive, CategoryInfo* cat, u8 val) {
    if (!file || !file->isOpen() || !cat || !cat->hasSpecialLogic) return;
    if (val > 99) val = 99;

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

    u32 totalBytes = (u32)(cat->maxItems * cat->bytesPerItem);
    std::vector<u8> buf(totalBytes, 0xFD);
    u32 bytesRead = 0;
    if (R_FAILED(file->read(cat->saveBase, buf.data(), totalBytes, &bytesRead)) || bytesRead == 0) {
        return;
    }

    for (int idx = 0; idx < slotCount; idx++) {
        int slot = (int)slots[idx];
        if (slot < 0 || slot >= cat->maxItems) continue;
        int validColors = getValidColorCount(cat, slot);
        for (int c = 0; c < validColors; c++) {
            u32 off = (u32)(slot * cat->bytesPerItem + c);
            if (off < totalBytes && buf[off] != 0xFF) buf[off] = val;
        }
    }

    u32 bytesWritten = 0;
    file->write(cat->saveBase, buf.data(), totalBytes, &bytesWritten);
    if (archive && archive->get() != 0) archive->commit();
}

void unlockSpecialFoods(SafeFile* file, SafeArchive* archive, u8 val) {
    if (!file || !file->isOpen()) return;
    u32 base = categories[6].saveBase;
    if (base == 0) base = (currentRegion == REGION_EU) ? 0x1A14 : 0x19E2;

    const u32 totalBytes = 49;
    u8 buf[totalBytes];
    u32 bytesRead = 0;
    if (R_FAILED(file->read(base, buf, totalBytes, &bytesRead)) || bytesRead == 0) return;
    if (buf[0] != 0xFF) buf[0] = val;
    for (u32 i = 2; i < totalBytes; i++) {
        if (buf[i] != 0xFF) buf[i] = val;
    }

    u32 bytesWritten = 0;
    file->write(base, buf, totalBytes, &bytesWritten);
    if (archive && archive->get() != 0) archive->commit();
}

bool restoreBackup(SafeFile* file, SafeArchive* archive) {
    if (!file || !file->isOpen()) return false;

    FILE* in = fopen("sdmc:/3ds/tlse/backups/savedataArc.bak", "rb");
    if (!in) in = fopen("sdmc:/3ds/tlse/savedataArc.bak", "rb");
    if (!in) in = fopen("savedataArc.bak", "rb");
    if (!in) {
        snprintf(g_backupStatus, sizeof(g_backupStatus), "No .bak File Found");
        return false;
    }

    g_backupCreated = true;
    u8 chunk[4096];
    u64 offset = 0;
    while (true) {
        size_t r = fread(chunk, 1, sizeof(chunk), in);
        if (r == 0) break;
        u32 bytesWritten = 0;
        file->write(offset, chunk, (u32)r, &bytesWritten);
        offset += r;
        if (r < sizeof(chunk)) break;
    }
    fclose(in);

    if (archive && archive->get() != 0) archive->commit();
    snprintf(g_backupStatus, sizeof(g_backupStatus), "Restored from .bak!");
    return true;
}

void readIslandMetadata(SafeFile* file, SaveRegion reg, u32* outMoney, char* outIslandNameUTF8) {
    if (!file || !file->isOpen()) return;
    u32 bytesRead = 0;
    file->read(getMoneyOffset(reg), outMoney, sizeof(u32), &bytesRead);

    u16 islandNameUTF16[11] = {0};
    file->read(getIslandNameOffset(reg), islandNameUTF16, 20, &bytesRead);
    memset(outIslandNameUTF8, 0, 32);
    utf16le_to_utf8_buf(outIslandNameUTF8, 32, islandNameUTF16, 10);
}

// ── Services ─────────────────────────────────────────────────────────

u32 old_time_limit;

void initServices() {
    hidInit();
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();
    APT_GetAppCpuTimeLimit(&old_time_limit);
    APT_SetAppCpuTimeLimit(30);
    romfsInit();
    cfguInit();
    amInit();
    acInit();
    gfxSetScreenFormat(GFX_BOTTOM, GSP_BGR8_OES);
    gfxSetDoubleBuffering(GFX_BOTTOM, true);
}

void exitServices() {
    acExit();
    amExit();
    cfguExit();
    romfsExit();
    if (old_time_limit != UINT32_MAX) APT_SetAppCpuTimeLimit(old_time_limit);
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
    SPR_ICON_MII,
    SPR_ICON_RELATIONSHIPS,
    SPR_ICON_ISLAND,
    SPR_ICON_INTERACTIONS,
    SPR_ICON_EXTRAS,
    SPR_ICON_CONCERT,
    SPR_BADGE_HEART_PINK,
    SPR_BADGE_FRIEND_GREEN,
    SPR_BADGE_CONFLICT_STORM,
    SPR_HEADER_BAR,
    SPR_FOOTER_CARD,
    SPR_COUNT
};

static inline void drawSprite(C2D_SpriteSheet sheet, size_t idx, float x, float y,
                              float scaleX = 1.0f, float scaleY = 1.0f,
                              const C2D_ImageTint* tint = nullptr) {
    if (sheet != nullptr && idx < C2D_SpriteSheetCount(sheet)) {
        C2D_DrawImageAt(C2D_SpriteSheetGetImage(sheet, idx), x, y, 0.5f, tint, scaleX, scaleY);
    }
}

static inline void drawTextShadowed(C2D_TextBuf buf, const char* str, float x, float y,
                                    float scaleX, float scaleY, u32 color, u32 shadowColor) {
    if (!str || str[0] == '\0') return;
    C2D_Text txt;
    C2D_TextParse(&txt, buf, str);
    C2D_TextOptimize(&txt);
    C2D_DrawText(&txt, C2D_WithColor, x + 1.0f, y + 1.0f, 0.58f, scaleX, scaleY, shadowColor);
    C2D_DrawText(&txt, C2D_WithColor, x, y, 0.62f, scaleX, scaleY, color);
}

// ── Structured Row Buffer (#43 UI Overhaul) ───────────────────────────

struct UIRow {
    char label[64];
    char value[32];
    size_t iconIdx; // SPR_COUNT means none
};

struct TouchButton {
    float x, y, w, h, scale;
    u32 keyMask;
    const char* label;
};

static inline int getBottomButtons(MenuState state, bool fileOpen, TouchButton outBtns[5]) {
    if (!fileOpen) {
        outBtns[0] = {128.0f, 186.0f, 64.0f, 36.0f, 1.0f, KEY_START, "Exit"};
        return 1;
    }
    if (state == STATE_MONEY_EDIT || state == STATE_QUANTITY_SELECT) {
        outBtns[0] = {12.0f,  186.0f, 64.0f, 36.0f, 1.0f, KEY_DDOWN, "-1"};
        outBtns[1] = {89.0f,  186.0f, 64.0f, 36.0f, 1.0f, KEY_DUP,   "+1"};
        outBtns[2] = {167.0f, 186.0f, 64.0f, 36.0f, 1.0f, KEY_A,     "Apply"};
        outBtns[3] = {244.0f, 186.0f, 64.0f, 36.0f, 1.0f, KEY_B,     "Cancel"};
        return 4;
    }
    if (state == STATE_MII_STATUS_EDIT || state == STATE_MII_PROFILE_EDIT ||
        state == STATE_MII_VOICE_PERS  || state == STATE_MII_GIFTS_FOODS  ||
        state == STATE_REL_EDIT        || state == STATE_ISLAND_CONFIG    ||
        state == STATE_ISLAND_INFO     || state == STATE_INTERACTIONS_EDIT ||
        state == STATE_TRAVELER_EDIT   || state == STATE_CONCERT_EDIT) {
        const float w = 64.0f * 0.85f;
        const float h = 36.0f * 0.85f;
        outBtns[0] = {10.0f,  189.0f, w, h, 0.85f, KEY_DUP,    "Prev"};
        outBtns[1] = {71.0f,  189.0f, w, h, 0.85f, KEY_DDOWN,  "Next"};
        outBtns[2] = {133.0f, 189.0f, w, h, 0.85f, KEY_DLEFT,  "-"};
        outBtns[3] = {194.0f, 189.0f, w, h, 0.85f, KEY_DRIGHT, "+"};
        outBtns[4] = {256.0f, 189.0f, w, h, 0.85f, KEY_B,      "Save"};
        return 5;
    }
    outBtns[0] = {12.0f,  186.0f, 64.0f, 36.0f, 1.0f, KEY_DUP,   "Up"};
    outBtns[1] = {89.0f,  186.0f, 64.0f, 36.0f, 1.0f, KEY_DDOWN, "Down"};
    outBtns[2] = {167.0f, 186.0f, 64.0f, 36.0f, 1.0f, KEY_A,     "Open"};
    outBtns[3] = {244.0f, 186.0f, 64.0f, 36.0f, 1.0f,
                  (state == STATE_MAIN_MENU) ? (u32)KEY_START : (u32)KEY_B,
                  (state == STATE_MAIN_MENU) ? "Exit" : "Back"};
    return 4;
}

// ── Busy / Loading Screen Helper (#55) ───────────────────────────────

static C3D_RenderTarget* g_busyTopTarget = nullptr;
static C3D_RenderTarget* g_busyBotTarget = nullptr;
static C2D_TextBuf g_busyTopBuf = nullptr;
static C2D_TextBuf g_busyBotBuf = nullptr;
static C2D_SpriteSheet g_busySheet = nullptr;

static void renderBusyFrame(const char* title, const char* subtitle) {
    if (!g_busyTopTarget || !g_busyBotTarget || !g_busyTopBuf || !g_busyBotBuf) return;
    const u32 colSkyBlue  = C2D_Color32(168, 224, 248, 255);
    const u32 colCream    = C2D_Color32(252, 248, 232, 255);
    const u32 colTextDark = C2D_Color32(48, 36, 28, 255);
    const u32 colShadowLt = C2D_Color32(255, 255, 255, 190);
    const u32 colAccent   = C2D_Color32(180, 72, 42, 255);

    C2D_TextBufClear(g_busyTopBuf);
    C2D_TextBufClear(g_busyBotBuf);
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

    C2D_TargetClear(g_busyTopTarget, colSkyBlue);
    C2D_SceneBegin(g_busyTopTarget);
    drawSprite(g_busySheet, SPR_TOP_BG, 0.0f, 0.0f);
    drawSprite(g_busySheet, SPR_HEADER_BAR, 8.0f, 4.0f);
    drawSprite(g_busySheet, SPR_FOOTER_CARD, 52.0f, 72.0f);
    drawTextShadowed(g_busyTopBuf, title ? title : "Working...", 16.0f, 9.0f, 0.46f, 0.46f, colTextDark, colShadowLt);
    drawTextShadowed(g_busyTopBuf, subtitle ? subtitle : "Please wait...", 70.0f, 106.0f, 0.44f, 0.44f, colAccent, colShadowLt);

    C2D_TargetClear(g_busyBotTarget, colCream);
    C2D_SceneBegin(g_busyBotTarget);
    drawSprite(g_busySheet, SPR_BOTTOM_BG, 0.0f, 0.0f);
    drawSprite(g_busySheet, SPR_BANNER_LOGO, 32.0f, 88.0f);

    C3D_FrameEnd(0);
}

static void onBeforeBackupCallback() {
    renderBusyFrame("Creating Backup...", "Backing up savedataArc.txt to SD card.\nPlease wait...");
}

// ── Main ─────────────────────────────────────────────────────────────

int main(int argc, char** argv) {
    initServices();

    C3D_RenderTarget* topTarget = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    C3D_RenderTarget* bottomTarget = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    C2D_TextBuf topBuf = C2D_TextBufNew(4096);
    C2D_TextBuf bottomBuf = C2D_TextBufNew(4096);
    C2D_SpriteSheet spriteSheet = C2D_SpriteSheetLoad("romfs:/gfx/sprites.t3x");

    g_busyTopTarget = topTarget;
    g_busyBotTarget = bottomTarget;
    g_busyTopBuf = topBuf;
    g_busyBotBuf = bottomBuf;
    g_busySheet = spriteSheet;
    g_onBeforeBackup = onBeforeBackupCallback;

    g_backupCreated = false;
    snprintf(g_backupStatus, sizeof(g_backupStatus), "Standby (Before 1st Edit)");
    renderBusyFrame("Loading Save Data...", "Scanning Cartridge & SD card...");

    struct TitleCandidate {
        FS_MediaType media;
        u32 lowId;
        SaveRegion region;
        const char* sourceLabel;
    };
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

    u32 currentMoney = 0;
    char islandNameUTF8[32] = {0};
    if (fileOpenSuccess) {
        readIslandMetadata(file.get(), currentRegion, &currentMoney, islandNameUTF8);
    }

    // ── Initialize categories ────────────────────────────────────────
    initCategory(categories[0], FOOD_NAMES, FOOD_COUNT, 0, FOOD_COUNT, 0x17F0, 1, false, false);
    initCategory(categories[1], CLOTHES_NAMES, CLOTHES_COUNT, 990, 447, 0x30, 8, true, true, CLOTHES_COLOR_COUNTS_US);
    initCategory(categories[2], CLOTHES_NAMES, CLOTHES_COUNT, 2606, 219, 0xFF8, 8, true, true, HATS_COLOR_COUNTS_US);
    initCategory(categories[3], INTERIORS_NAMES, INTERIORS_COUNT, 0, INTERIORS_COUNT, 0x1778, 1, false, true);
    initCategory(categories[4], GOODS_NAMES, GOODS_COUNT, 0, GOODS_COUNT, 0x18F0, 1, false, false);
    initCategory(categories[5], TREASURES_NAMES_US, TREASURES_COUNT, 0, TREASURES_COUNT, 0x1902, 1, false, true);
    initCategory(categories[6], NULL, 0, 0, 48, 0x19E2, 1, false, false);
    updateRegionOffsets();

    // ── Module State Data ────────────────────────────────────────────
    std::vector<MiiEntry> miiList;
    int selectedMiiListIdx = 0;
    MiiEntry activeMii;
    int relMiiAIdx = 0;
    int relMiiBIdx = 1;
    RelationshipPair activeRel;
    IslandConfigData activeConfig;
    IslandInfoData activeIslandInfo;
    MiiInteractionData activeInteraction;
    ConcertSongData activeSong;
    int activeSongIdx = 0;
    TravelerData activeTraveler;
    int activeTravelerIdx = 0;
    char statusToast[96] = "";
    bool confirmDeleteMii = false;

    MenuState currentState = STATE_MAIN_MENU;
    int cursorIndex = 0;
    int scrollOffset = 0;
    int quantityToGive = 1;
    int colorCursor = 0;
    MenuState previousCategory = STATE_MAIN_MENU;
    const int MAX_VISIBLE_ROWS = 9;
    const int MAIN_MENU_ITEMS = 14;

    auto adjustListNav = [&](u32 kDown, int totalItems) {
        if (totalItems <= 0) return;
        if (kDown & KEY_DUP) {
            cursorIndex = (cursorIndex > 0) ? cursorIndex - 1 : totalItems - 1;
        }
        if (kDown & KEY_DDOWN) {
            cursorIndex = (cursorIndex < totalItems - 1) ? cursorIndex + 1 : 0;
        }
        if (cursorIndex < scrollOffset) scrollOffset = cursorIndex;
        if (cursorIndex >= scrollOffset + MAX_VISIBLE_ROWS) {
            scrollOffset = cursorIndex - MAX_VISIBLE_ROWS + 1;
        }
    };

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();

        if (kDown & KEY_TOUCH) {
            touchPosition touch;
            hidTouchRead(&touch);
            TouchButton tBtns[5];
            int tBtnCount = getBottomButtons(currentState, fileOpenSuccess, tBtns);
            for (int i = 0; i < tBtnCount; i++) {
                if (touch.px >= (u16)tBtns[i].x && touch.px < (u16)(tBtns[i].x + tBtns[i].w) &&
                    touch.py >= (u16)tBtns[i].y && touch.py < (u16)(tBtns[i].y + tBtns[i].h)) {
                    kDown |= tBtns[i].keyMask;
                    break;
                }
            }
        }

        if (kDown & KEY_START) {
            if (fileOpenSuccess && archive != nullptr && archive->get() != 0) {
                archive->commit();
            }
            break;
        }

        // ═══════════════════ INPUT LOGIC ═══════════════════

        if (currentState == STATE_MAIN_MENU) {
            adjustListNav(kDown, MAIN_MENU_ITEMS);
            if ((kDown & KEY_A) && fileOpenSuccess) {
                statusToast[0] = '\0';
                if (cursorIndex == 0) {
                    currentState = STATE_MONEY_EDIT;
                } else if (cursorIndex >= 1 && cursorIndex <= 6) {
                    currentState = (MenuState)(STATE_CATEGORY_FOOD + (cursorIndex - 1));
                    cursorIndex = 0; scrollOffset = 0;
                } else if (cursorIndex == 7) {
                    renderBusyFrame("Unlocking Specials...", "Unlocking SpotPass & StreetPass items...");
                    unlockSpecialItems(file.get(), archive.get(), &categories[1], 99);
                    unlockSpecialItems(file.get(), archive.get(), &categories[2], 99);
                    unlockSpecialFoods(file.get(), archive.get(), 99);
                    snprintf(statusToast, sizeof(statusToast), "Unlocked SpotPass/StreetPass Items!");
                } else if (cursorIndex == 8) {
                    scanRegisteredMiis(file.get(), currentRegion, miiList);
                    currentState = STATE_MII_SELECT;
                    cursorIndex = 0; scrollOffset = 0;
                } else if (cursorIndex == 9) {
                    scanRegisteredMiis(file.get(), currentRegion, miiList);
                    currentState = STATE_REL_MII_A;
                    cursorIndex = 0; scrollOffset = 0;
                } else if (cursorIndex == 10) {
                    readIslandConfig(file.get(), currentRegion, activeConfig);
                    currentState = STATE_ISLAND_CONFIG;
                    cursorIndex = 0; scrollOffset = 0;
                } else if (cursorIndex == 11) {
                    readIslandInfo(file.get(), currentRegion, activeIslandInfo);
                    currentState = STATE_ISLAND_INFO;
                    cursorIndex = 0; scrollOffset = 0;
                } else if (cursorIndex == 12) {
                    scanRegisteredMiis(file.get(), currentRegion, miiList);
                    currentState = STATE_INTERACTIONS_MII;
                    cursorIndex = 0; scrollOffset = 0;
                } else if (cursorIndex == 13) {
                    currentState = STATE_EXTRAS_MENU;
                    cursorIndex = 0; scrollOffset = 0;
                }
            }
            if ((kDown & KEY_X) && fileOpenSuccess) {
                currentState = STATE_TRAVELERS_CONCERT_MENU;
                cursorIndex = 0; scrollOffset = 0;
            }
            if ((kDown & KEY_Y) && fileOpenSuccess) {
                if (restoreBackup(file.get(), archive.get())) {
                    readIslandMetadata(file.get(), currentRegion, &currentMoney, islandNameUTF8);
                    snprintf(statusToast, sizeof(statusToast), "Restored from savedataArc.bak!");
                }
            }
            if (kDown & KEY_L) {
                currentRegion = (SaveRegion)(((int)currentRegion + 3) % 4);
                updateRegionOffsets();
                if (fileOpenSuccess) readIslandMetadata(file.get(), currentRegion, &currentMoney, islandNameUTF8);
            } else if (kDown & KEY_R) {
                currentRegion = (SaveRegion)(((int)currentRegion + 1) % 4);
                updateRegionOffsets();
                if (fileOpenSuccess) readIslandMetadata(file.get(), currentRegion, &currentMoney, islandNameUTF8);
            }
        }
        else if (currentState == STATE_MONEY_EDIT) {
            if (kDown & KEY_B) {
                currentState = STATE_MAIN_MENU;
                cursorIndex = 0; scrollOffset = 0;
            }
            if (fileOpenSuccess) {
                const u32 MAX_MONEY = 99999999;
                bool valueChanged = false;
                if (kDown & KEY_A)      { currentMoney = (currentMoney + 10000 <= MAX_MONEY) ? (currentMoney + 10000) : MAX_MONEY; valueChanged = true; }
                else if (kDown & KEY_RIGHT) { currentMoney = (currentMoney + 1000000 <= MAX_MONEY) ? (currentMoney + 1000000) : MAX_MONEY; valueChanged = true; }
                else if (kDown & KEY_LEFT)  { currentMoney = (currentMoney >= 1000000) ? (currentMoney - 1000000) : 0; valueChanged = true; }
                else if (kDown & KEY_X)     { currentMoney = MAX_MONEY; valueChanged = true; }
                else if (kDown & KEY_Y)     { currentMoney = 0; valueChanged = true; }
                if (valueChanged) {
                    u32 bytesWritten = 0;
                    file->write(getMoneyOffset(currentRegion), &currentMoney, sizeof(u32), &bytesWritten);
                    if (archive != nullptr && archive->get() != 0) archive->commit();
                }
            }
        }
        else if (currentState >= STATE_CATEGORY_FOOD && currentState <= STATE_CATEGORY_TREASURES) {
            CategoryInfo* cat = getCategoryForState(currentState);
            int maxItems = (int)cat->filteredIndices.size();
            adjustListNav(kDown, maxItems);
            if (kDown & KEY_B) {
                int menuIdx = 1 + (int)(currentState - STATE_CATEGORY_FOOD);
                currentState = STATE_MAIN_MENU;
                cursorIndex = menuIdx; scrollOffset = 0;
            }
            if ((kDown & KEY_X) && fileOpenSuccess) {
                renderBusyFrame("Unlocking Category...", "Writing valid items (x99)...");
                unlockCategoryAll(file.get(), archive.get(), cat, 99);
                snprintf(statusToast, sizeof(statusToast), "Unlocked all %s!", getTitleForState(currentState));
            }
            if ((kDown & KEY_A) && maxItems > 0 && fileOpenSuccess) {
                savedItemCursor = cursorIndex;
                previousCategory = currentState;
                if (currentState == STATE_CATEGORY_CLOTHES || currentState == STATE_CATEGORY_HATS) {
                    currentState = STATE_COLOR_SELECT;
                    colorCursor = 0;
                } else {
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
            if (kDown & KEY_A && fileOpenSuccess) {
                CategoryInfo* cat = getCategoryForState(previousCategory);
                int originalIdx = cat->filteredIndices[savedItemCursor];
                u32 bytesWritten = 0;
                u8 val = (quantityToGive < 1) ? 1 : ((quantityToGive > 99) ? 99 : (u8)quantityToGive);
                if (cat->hasColors) {
                    int validColors = getValidColorCount(cat, originalIdx);
                    u32 slotOffset = cat->saveBase + (originalIdx * cat->bytesPerItem);
                    u8 slotBuf[8] = {0};
                    u32 bytesRead = 0;
                    if (!R_FAILED(file->read(slotOffset, slotBuf, (u32)cat->bytesPerItem, &bytesRead))) {
                        if (selectedColor == -1) {
                            for (int i = 0; i < validColors && i < 8; i++) {
                                if (slotBuf[i] != 0xFF) slotBuf[i] = val;
                            }
                        } else if (selectedColor >= 0 && selectedColor < validColors && selectedColor < 8) {
                            if (slotBuf[selectedColor] != 0xFF) slotBuf[selectedColor] = val;
                        }
                        file->write(slotOffset, slotBuf, (u32)cat->bytesPerItem, &bytesWritten);
                    }
                } else {
                    u8 curByte = 0;
                    u32 bytesRead = 0;
                    if (!R_FAILED(file->read(cat->saveBase + originalIdx, &curByte, 1, &bytesRead)) && curByte != 0xFF) {
                        file->write(cat->saveBase + originalIdx, &val, 1, &bytesWritten);
                    }
                }
                if (archive != nullptr && archive->get() != 0) archive->commit();
                currentState = previousCategory;
                cursorIndex = savedItemCursor;
            }
        }
        // ── Mii Editor Suite (#11, #52, #55) ─────────────────────────
        else if (currentState == STATE_MII_SELECT) {
            adjustListNav(kDown, (int)miiList.size());
            if (kDown & KEY_B) { currentState = STATE_MAIN_MENU; cursorIndex = 8; scrollOffset = 0; }
            if ((kDown & KEY_X) && !miiList.empty()) {
                renderBusyFrame("Updating All Miis...", "Setting Lv.99 & Max Pocket Money...");
                maxAllMiisLevelAndMoney(file.get(), archive.get(), currentRegion);
                scanRegisteredMiis(file.get(), currentRegion, miiList);
                snprintf(statusToast, sizeof(statusToast), "All Miis set to Lv.99 & Max Pocket!");
            }
            if ((kDown & KEY_L) && !miiList.empty()) {
                renderBusyFrame("Exporting .tlamii...", "Writing all 100 Mii slots to SD...");
                if (exportAllMiisTlamii(file.get(), currentRegion, "sdmc:/3ds/tlse/miis/all_miis.tlamii")) {
                    snprintf(statusToast, sizeof(statusToast), "Exported all_miis.tlamii!");
                } else {
                    snprintf(statusToast, sizeof(statusToast), "Export all_miis.tlamii failed!");
                }
            }
            if (kDown & KEY_R) {
                renderBusyFrame("Importing .tlamii...", "Restoring all 100 Mii slots from SD...");
                if (importAllMiisTlamii(file.get(), archive.get(), currentRegion, "sdmc:/3ds/tlse/miis/all_miis.tlamii")) {
                    scanRegisteredMiis(file.get(), currentRegion, miiList);
                    if (miiList.empty()) { cursorIndex = 0; scrollOffset = 0; }
                    else if (cursorIndex >= (int)miiList.size()) {
                        cursorIndex = (int)miiList.size() - 1;
                        if (scrollOffset > cursorIndex) scrollOffset = cursorIndex;
                    }
                    snprintf(statusToast, sizeof(statusToast), "Imported all_miis.tlamii (%zu Miis)!", miiList.size());
                } else {
                    snprintf(statusToast, sizeof(statusToast), "Import failed (missing/wrong region)!");
                }
            }
            if ((kDown & KEY_A) && !miiList.empty()) {
                selectedMiiListIdx = cursorIndex;
                activeMii = miiList[selectedMiiListIdx];
                confirmDeleteMii = false;
                currentState = STATE_MII_SUBMENU;
                cursorIndex = 0; scrollOffset = 0;
            }
        }
        else if (currentState == STATE_MII_SUBMENU) {
            int prevCursor = cursorIndex;
            adjustListNav(kDown, 8);
            if (cursorIndex != prevCursor) confirmDeleteMii = false;
            if (kDown & KEY_B) {
                confirmDeleteMii = false;
                scanRegisteredMiis(file.get(), currentRegion, miiList);
                currentState = STATE_MII_SELECT;
                cursorIndex = selectedMiiListIdx; scrollOffset = 0;
            }
            if (kDown & KEY_A) {
                if (cursorIndex == 0)      { currentState = STATE_MII_STATUS_EDIT; cursorIndex = 0; scrollOffset = 0; }
                else if (cursorIndex == 1) { currentState = STATE_MII_PROFILE_EDIT; cursorIndex = 0; scrollOffset = 0; }
                else if (cursorIndex == 2) { currentState = STATE_MII_VOICE_PERS; cursorIndex = 0; scrollOffset = 0; }
                else if (cursorIndex == 3) { currentState = STATE_MII_GIFTS_FOODS; cursorIndex = 0; scrollOffset = 0; }
                else if (cursorIndex == 4) {
                    char path[128];
                    snprintf(path, sizeof(path), "sdmc:/3ds/tlse/miis/mii_%03d.tlmii", activeMii.slotIndex);
                    if (exportMiiTlmii(file.get(), currentRegion, activeMii.slotIndex, path)) {
                        snprintf(statusToast, sizeof(statusToast), "Exported mii_%03d.tlmii!", activeMii.slotIndex);
                    }
                } else if (cursorIndex == 5) {
                    char path[128];
                    snprintf(path, sizeof(path), "sdmc:/3ds/tlse/miis/mii_%03d.tlmii", activeMii.slotIndex);
                    if (importMiiTlmii(file.get(), archive.get(), currentRegion, activeMii.slotIndex, path)) {
                        readMiiEntry(file.get(), currentRegion, activeMii.slotIndex, activeMii);
                        snprintf(statusToast, sizeof(statusToast), "Imported mii_%03d.tlmii!", activeMii.slotIndex);
                    }
                } else if (cursorIndex == 6) {
                    renderBusyFrame("Cloning Mii...", "Copying Mii to first empty slot...");
                    int newSlot = cloneMiiSlot(file.get(), archive.get(), currentRegion, activeMii.slotIndex);
                    if (newSlot >= 0) {
                        scanRegisteredMiis(file.get(), currentRegion, miiList);
                        snprintf(statusToast, sizeof(statusToast), "Cloned to Slot %d!", newSlot + 1);
                    } else {
                        snprintf(statusToast, sizeof(statusToast), "Island full (100/100 Miis)!");
                    }
                } else if (cursorIndex == 7) {
                    if (miiList.size() <= 1) {
                        snprintf(statusToast, sizeof(statusToast), "Cannot delete the last Mii!");
                    } else if (!confirmDeleteMii) {
                        confirmDeleteMii = true;
                        snprintf(statusToast, sizeof(statusToast), "Press [A] again to confirm Delete!");
                    } else {
                        confirmDeleteMii = false;
                        int delSlot = activeMii.slotIndex;
                        renderBusyFrame("Deleting Mii...", "Wiping slot & clearing relationships...");
                        if (deleteMiiSlot(file.get(), archive.get(), currentRegion, delSlot)) {
                            clearMiiRelationships(file.get(), archive.get(), currentRegion, delSlot);
                            scanRegisteredMiis(file.get(), currentRegion, miiList);
                            currentState = STATE_MII_SELECT;
                            cursorIndex = (selectedMiiListIdx >= (int)miiList.size()) ? (int)miiList.size() - 1 : selectedMiiListIdx;
                            if (cursorIndex < 0) cursorIndex = 0;
                            scrollOffset = 0;
                            snprintf(statusToast, sizeof(statusToast), "Deleted Mii (Slot %d)!", delSlot + 1);
                        }
                    }
                }
            }
        }
        else if (currentState == STATE_MII_STATUS_EDIT) {
            adjustListNav(kDown, 9);
            if (kDown & KEY_B) {
                writeMiiEntry(file.get(), archive.get(), currentRegion, activeMii);
                currentState = STATE_MII_SUBMENU; cursorIndex = 0; scrollOffset = 0;
            }
            int delta = (kDown & KEY_RIGHT) ? 1 : ((kDown & KEY_LEFT) ? -1 : 0);
            int bigDelta = (kDown & KEY_A) ? 10 : 0;
            if (delta != 0 || bigDelta != 0 || (kDown & KEY_X) || (kDown & KEY_Y)) {
                int step = (bigDelta != 0) ? bigDelta : delta;
                switch (cursorIndex) {
                    case 0: // Level
                        if (kDown & KEY_X) activeMii.level = 99;
                        else if (kDown & KEY_Y) activeMii.level = 1;
                        else { int v = (int)activeMii.level + step; activeMii.level = (u8)((v < 1) ? 1 : (v > 99 ? 99 : v)); }
                        break;
                    case 1: // EXP
                        if (kDown & KEY_X) activeMii.experience = 99;
                        else if (kDown & KEY_Y) activeMii.experience = 0;
                        else { int v = (int)activeMii.experience + step; activeMii.experience = (u8)((v < 0) ? 0 : (v > 99 ? 99 : v)); }
                        break;
                    case 2: // Pocket Money (in $10 steps)
                        if (kDown & KEY_X) activeMii.pocketMoney = 99999999;
                        else if (kDown & KEY_Y) activeMii.pocketMoney = 0;
                        else { long long v = (long long)activeMii.pocketMoney + (long long)step * 10000; activeMii.pocketMoney = (u32)((v < 0) ? 0 : (v > 99999999 ? 99999999 : v)); }
                        break;
                    case 3: // Pampered
                        if (kDown & KEY_X) activeMii.pampered = 99999999;
                        else if (kDown & KEY_Y) activeMii.pampered = 0;
                        else { long long v = (long long)activeMii.pampered + (long long)step * 10000; activeMii.pampered = (u32)((v < 0) ? 0 : (v > 99999999 ? 99999999 : v)); }
                        break;
                    case 4: // Splurge
                        if (kDown & KEY_X) activeMii.splurge = 99999999;
                        else if (kDown & KEY_Y) activeMii.splurge = 0;
                        else { long long v = (long long)activeMii.splurge + (long long)step * 10000; activeMii.splurge = (u32)((v < 0) ? 0 : (v > 99999999 ? 99999999 : v)); }
                        break;
                    case 5: // Stomach Fullness
                        if (kDown & KEY_X) activeMii.stomach = 100;
                        else if (kDown & KEY_Y) activeMii.stomach = 0;
                        else { int v = (int)activeMii.stomach + step; activeMii.stomach = (u8)((v < 0) ? 0 : (v > 100 ? 100 : v)); }
                        break;
                    case 6: // Hair Dye
                        if (kDown & KEY_Y) activeMii.hairColor = 1;
                        else if (delta != 0 || bigDelta != 0) {
                            if (activeMii.hairColor < 128) activeMii.hairColor = 128;
                            else {
                                int v = (int)activeMii.hairColor + delta;
                                activeMii.hairColor = (v < 128) ? 1 : (u8)((v > 191) ? 191 : v);
                            }
                        }
                        break;
                    case 7: // Apartment Room
                        { int v = (int)activeMii.apartmentRoom + delta; activeMii.apartmentRoom = (u8)((v < 0) ? 0 : (v > 99 ? 99 : v)); }
                        break;
                    case 8: // Mii Home
                        if (kDown & KEY_Y) activeMii.miiHome = 0xFF;
                        else if (delta != 0) {
                            if (activeMii.miiHome == 0xFF) activeMii.miiHome = 0;
                            else {
                                int v = (int)activeMii.miiHome + delta;
                                activeMii.miiHome = (v < 0) ? 0xFF : (u8)((v > 49) ? 49 : v);
                            }
                        }
                        break;
                }
            }
        }
        else if (currentState == STATE_MII_PROFILE_EDIT) {
            adjustListNav(kDown, 12);
            if (kDown & KEY_B) {
                writeMiiEntry(file.get(), archive.get(), currentRegion, activeMii);
                currentState = STATE_MII_SUBMENU; cursorIndex = 1; scrollOffset = 0;
            }
            if (kDown & KEY_A) {
                if (cursorIndex == 0) editStringWithKeyboard(activeMii.nickname, 10, "Mii Nickname");
                else if (cursorIndex == 1) editStringWithKeyboard(activeMii.firstName, 16, "First Name");
                else if (cursorIndex == 2) editStringWithKeyboard(activeMii.lastName, 16, "Last Name");
                else if (cursorIndex == 3) activeMii.gender = (activeMii.gender == 0) ? 1 : 0;
                else if (cursorIndex == 4) activeMii.favColor = (activeMii.favColor + 1) % 12;
                else if (cursorIndex == 5) activeMii.grownUpFlag = (activeMii.grownUpFlag == 2) ? 1 : 2;
                else if (cursorIndex == 6) activeMii.relationToYou = (activeMii.relationToYou + 1) % 12;
                else if (cursorIndex == 7) activeMii.birthMonth = (u8)((activeMii.birthMonth + 1) % 13);
                else if (cursorIndex == 8) activeMii.birthDay = (u8)((activeMii.birthDay + 1) % 32);
                else if (cursorIndex == 9) editStringWithKeyboard(activeMii.catchphrase, 16, "Mii Catchphrase");
                else if (cursorIndex == 10) activeMii.sharing = activeMii.sharing ? 0 : 1;
                else if (cursorIndex == 11) activeMii.copying = activeMii.copying ? 0 : 1;
            }
            if ((kDown & KEY_LEFT) || (kDown & KEY_RIGHT)) {
                int d = (kDown & KEY_RIGHT) ? 1 : -1;
                if (cursorIndex == 3) activeMii.gender = (activeMii.gender == 0) ? 1 : 0;
                else if (cursorIndex == 4) activeMii.favColor = (u8)((activeMii.favColor + 12 + d) % 12);
                else if (cursorIndex == 5) activeMii.grownUpFlag = (activeMii.grownUpFlag == 2) ? 1 : 2;
                else if (cursorIndex == 6) activeMii.relationToYou = (u8)((activeMii.relationToYou + 12 + d) % 12);
                else if (cursorIndex == 7) activeMii.birthMonth = (u8)((activeMii.birthMonth + 13 + d) % 13);
                else if (cursorIndex == 8) activeMii.birthDay = (u8)((activeMii.birthDay + 32 + d) % 32);
                else if (cursorIndex == 10) activeMii.sharing = activeMii.sharing ? 0 : 1;
                else if (cursorIndex == 11) activeMii.copying = activeMii.copying ? 0 : 1;
            }
        }
        else if (currentState == STATE_MII_VOICE_PERS) {
            adjustListNav(kDown, 11);
            if (kDown & KEY_B) {
                writeMiiEntry(file.get(), archive.get(), currentRegion, activeMii);
                currentState = STATE_MII_SUBMENU; cursorIndex = 2; scrollOffset = 0;
            }
            int d = (kDown & KEY_RIGHT) ? 1 : ((kDown & KEY_LEFT) ? -1 : ((kDown & KEY_A) ? 5 : 0));
            if (d != 0) {
                if (cursorIndex < 6) {
                    int v = (int)activeMii.voice[cursorIndex] + d;
                    activeMii.voice[cursorIndex] = (u8)((v < 0) ? 0 : (v > 100 ? 100 : v));
                } else {
                    int idx = cursorIndex - 6;
                    int v = (int)activeMii.personality[idx] + ((d > 0) ? 1 : -1);
                    activeMii.personality[idx] = (u8)((v < 1) ? 1 : (v > 8 ? 8 : v));
                }
            }
        }
        else if (currentState == STATE_MII_GIFTS_FOODS) {
            adjustListNav(kDown, 18); // 3 Equipped + 7 Foods + 8 Level-Up Gifts
            if (kDown & KEY_B) {
                writeMiiEntry(file.get(), archive.get(), currentRegion, activeMii);
                currentState = STATE_MII_SUBMENU; cursorIndex = 3; scrollOffset = 0;
            }
            auto stepOptionalU16 = [](u16& val, int delta, int maxId, bool allowNone) {
                if (val == 0xFFFF) {
                    if (delta > 0) val = 0;
                    else if (delta < 0) val = (u16)maxId;
                    return;
                }
                int nv = (int)val + delta;
                if (nv < 0) val = allowNone ? 0xFFFF : 0;
                else if (nv > maxId) val = allowNone ? 0xFFFF : (u16)maxId;
                else val = (u16)nv;
            };
            if (kDown & KEY_Y) {
                if (cursorIndex == 1) activeMii.equippedHat = 0xFFFF;
                else if (cursorIndex == 3) activeMii.superFavFood = 0xFFFF;
                else if (cursorIndex == 4) activeMii.allTimeFavFood = 0xFFFF;
                else if (cursorIndex == 5) activeMii.favFood1 = 0xFFFF;
                else if (cursorIndex == 6) activeMii.favFood2 = 0xFFFF;
                else if (cursorIndex == 7) activeMii.favFood3 = 0xFFFF;
                else if (cursorIndex == 8) activeMii.worstFood = 0xFFFF;
                else if (cursorIndex == 9) activeMii.worstEverFood = 0xFFFF;
                else if (cursorIndex >= 10 && cursorIndex < 18) activeMii.levelUpGifts[cursorIndex - 10] = 0xFFFF;
            }
            int d = (kDown & KEY_RIGHT) ? 1 : ((kDown & KEY_LEFT) ? -1 : ((kDown & KEY_A) ? 10 : 0));
            if (d != 0) {
                if (cursorIndex == 0) stepOptionalU16(activeMii.equippedClothes, d, 447 * 8 - 1, false);
                else if (cursorIndex == 1) stepOptionalU16(activeMii.equippedHat, d, 219 * 8 - 1, true);
                else if (cursorIndex == 2) stepOptionalU16(activeMii.equippedInterior, d, INTERIORS_COUNT - 1, false);
                else if (cursorIndex == 3) stepOptionalU16(activeMii.superFavFood, d, FOOD_COUNT - 1, true);
                else if (cursorIndex == 4) stepOptionalU16(activeMii.allTimeFavFood, d, FOOD_COUNT - 1, true);
                else if (cursorIndex == 5) stepOptionalU16(activeMii.favFood1, d, FOOD_COUNT - 1, true);
                else if (cursorIndex == 6) stepOptionalU16(activeMii.favFood2, d, FOOD_COUNT - 1, true);
                else if (cursorIndex == 7) stepOptionalU16(activeMii.favFood3, d, FOOD_COUNT - 1, true);
                else if (cursorIndex == 8) stepOptionalU16(activeMii.worstFood, d, FOOD_COUNT - 1, true);
                else if (cursorIndex == 9) stepOptionalU16(activeMii.worstEverFood, d, FOOD_COUNT - 1, true);
                else if (cursorIndex >= 10 && cursorIndex < 18) {
                    int gd = (kDown & KEY_RIGHT) ? 1 : ((kDown & KEY_LEFT) ? -1 : 1);
                    stepOptionalU16(activeMii.levelUpGifts[cursorIndex - 10], gd, 27, true);
                }
            }
        }
        // ── Relationship Editor (#44) ────────────────────────────────
        else if (currentState == STATE_REL_MII_A) {
            adjustListNav(kDown, (int)miiList.size());
            if (kDown & KEY_B) { currentState = STATE_MAIN_MENU; cursorIndex = 9; scrollOffset = 0; }
            if ((kDown & KEY_Y) && !miiList.empty()) {
                renderBusyFrame("Resolving Fights...", "Scanning & clearing relationship fights...");
                int r = resolveAllRelationshipFights(file.get(), archive.get(), currentRegion);
                snprintf(statusToast, sizeof(statusToast), "Resolved %d fights!", r);
            }
            if ((kDown & KEY_X) && !miiList.empty()) {
                renderBusyFrame("Updating Feelings...", "Maxing all acquainted feelings to 200...");
                maxAllAcquaintedFeelings(file.get(), archive.get(), currentRegion);
                snprintf(statusToast, sizeof(statusToast), "Maxed all acquainted feelings (200)!");
            }
            if ((kDown & KEY_A) && miiList.size() >= 2) {
                relMiiAIdx = cursorIndex;
                currentState = STATE_REL_MII_B;
                cursorIndex = (relMiiAIdx == 0) ? 1 : 0; scrollOffset = 0;
            }
        }
        else if (currentState == STATE_REL_MII_B) {
            adjustListNav(kDown, (int)miiList.size());
            if (kDown & KEY_B) { currentState = STATE_REL_MII_A; cursorIndex = relMiiAIdx; scrollOffset = 0; }
            if ((kDown & KEY_A) && cursorIndex != relMiiAIdx) {
                relMiiBIdx = cursorIndex;
                readRelationshipPair(file.get(), currentRegion, miiList[relMiiAIdx].slotIndex, miiList[relMiiBIdx].slotIndex, activeRel);
                currentState = STATE_REL_EDIT;
                cursorIndex = 0; scrollOffset = 0;
            }
        }
        else if (currentState == STATE_REL_EDIT) {
            adjustListNav(kDown, 3);
            if (kDown & KEY_B) {
                writeRelationshipPair(file.get(), archive.get(), currentRegion,
                                      miiList[relMiiAIdx].slotIndex, miiList[relMiiBIdx].slotIndex, activeRel);
                currentState = STATE_REL_MII_B;
                cursorIndex = relMiiBIdx; scrollOffset = 0;
            }
            int d = (kDown & KEY_RIGHT) ? 1 : ((kDown & KEY_LEFT) ? -1 : ((kDown & KEY_A) ? 10 : 0));
            if (d != 0) {
                if (cursorIndex == 0) {
                    int s = (d > 0) ? 1 : -1;
                    activeRel.relType = (u8)((activeRel.relType + 13 + s) % 13);
                } else if (cursorIndex == 1) {
                    int v = (int)activeRel.feelingAtoB + d;
                    activeRel.feelingAtoB = (u8)((v < 0) ? 0 : (v > 200 ? 200 : v));
                } else if (cursorIndex == 2) {
                    int v = (int)activeRel.feelingBtoA + d;
                    activeRel.feelingBtoA = (u8)((v < 0) ? 0 : (v > 200 ? 200 : v));
                }
            }
            if (kDown & KEY_X) { activeRel.feelingAtoB = 200; activeRel.feelingBtoA = 200; }
        }
        // ── Island Config (#45) ──────────────────────────────────────
        else if (currentState == STATE_ISLAND_CONFIG) {
            adjustListNav(kDown, 35); // 24 buildings + 10 rankings + 1 apt renov
            if (kDown & KEY_B) {
                writeIslandConfig(file.get(), archive.get(), currentRegion, activeConfig);
                currentState = STATE_MAIN_MENU; cursorIndex = 10; scrollOffset = 0;
            }
            if (kDown & KEY_X) {
                unlockAllBuildingsAndRankings(file.get(), archive.get(), currentRegion);
                readIslandConfig(file.get(), currentRegion, activeConfig);
                snprintf(statusToast, sizeof(statusToast), "Unlocked all 24 Buildings & 10 Rankings!");
            }
            if ((kDown & KEY_A) || (kDown & KEY_LEFT) || (kDown & KEY_RIGHT)) {
                if (cursorIndex < 24) {
                    u8 unlockVal = (cursorIndex == 0) ? 6 : ((cursorIndex == 1) ? 3 : 2);
                    activeConfig.buildings[cursorIndex] = (activeConfig.buildings[cursorIndex] == 0) ? unlockVal : 0;
                } else if (cursorIndex < 34) {
                    int r = cursorIndex - 24;
                    activeConfig.rankings[r] = (activeConfig.rankings[r] == 0) ? 2 : 0;
                } else {
                    activeConfig.apartmentRenov = (activeConfig.apartmentRenov + 1) % 4;
                }
            }
        }
        // ── Island Info & Settings (#46, #53) ────────────────────────
        else if (currentState == STATE_ISLAND_INFO) {
            adjustListNav(kDown, 12);
            if (kDown & KEY_B) {
                writeIslandInfo(file.get(), archive.get(), currentRegion, activeIslandInfo);
                readIslandMetadata(file.get(), currentRegion, &currentMoney, islandNameUTF8);
                currentState = STATE_MAIN_MENU; cursorIndex = 11; scrollOffset = 0;
            }
            int d = (kDown & KEY_RIGHT) ? 1 : ((kDown & KEY_LEFT) ? -1 : ((kDown & KEY_A) ? 10 : 0));
            if (cursorIndex == 0 && (kDown & KEY_A)) {
                editStringWithKeyboard(activeIslandInfo.islandName, 10, "Island Name");
            } else if (cursorIndex == 1 && (kDown & KEY_A)) {
                editStringWithKeyboard(activeIslandInfo.islandPronun, 16, "Island Pronunciation");
            } else if (d != 0) {
                auto stepU16 = [&](u16& val) {
                    int nv = (int)val + d;
                    val = (u16)((nv < 0) ? 0 : (nv > 65535 ? 65535 : nv));
                };
                if (cursorIndex == 2) stepU16(activeIslandInfo.problemsSolved);
                else if (cursorIndex == 3) stepU16(activeIslandInfo.weddings);
                else if (cursorIndex == 4) stepU16(activeIslandInfo.childrenBorn);
                else if (cursorIndex == 5) stepU16(activeIslandInfo.streetPassEncounters);
                else if (cursorIndex == 6) stepU16(activeIslandInfo.travelersReceived);
                else if (cursorIndex == 7) stepU16(activeIslandInfo.travelersSent);
                else if (cursorIndex == 8) activeIslandInfo.streetPassActive = activeIslandInfo.streetPassActive ? 0 : 1;
                else if (cursorIndex == 9) activeIslandInfo.babiesAllowed = activeIslandInfo.babiesAllowed ? 0 : 1;
                else if (cursorIndex == 10) activeIslandInfo.spotPassActive = activeIslandInfo.spotPassActive ? 0 : 1;
                else if (cursorIndex == 11) {
                    int cIdx = exportColorBitToIndex(activeIslandInfo.exportColorBit);
                    int step = (d > 0) ? 1 : -1;
                    cIdx = (cIdx + 8 + step) % 8;
                    activeIslandInfo.exportColorBit = (u8)(1u << cIdx);
                }
            }
        }
        // ── Active Interactions (#47) ────────────────────────────────
        else if (currentState == STATE_INTERACTIONS_MII) {
            adjustListNav(kDown, (int)miiList.size());
            if (kDown & KEY_B) { currentState = STATE_MAIN_MENU; cursorIndex = 12; scrollOffset = 0; }
            if ((kDown & KEY_A) && !miiList.empty()) {
                selectedMiiListIdx = cursorIndex;
                readMiiInteraction(file.get(), currentRegion, miiList[selectedMiiListIdx].slotIndex, activeInteraction);
                currentState = STATE_INTERACTIONS_EDIT;
                cursorIndex = 0; scrollOffset = 0;
            }
        }
        else if (currentState == STATE_INTERACTIONS_EDIT) {
            adjustListNav(kDown, 4);
            if (kDown & KEY_B) {
                writeMiiInteraction(file.get(), archive.get(), currentRegion, miiList[selectedMiiListIdx].slotIndex, activeInteraction);
                currentState = STATE_INTERACTIONS_MII; cursorIndex = selectedMiiListIdx; scrollOffset = 0;
            }
            if (kDown & KEY_X) {
                activeInteraction.emotion = 0;
                activeInteraction.typeId = 0;
                activeInteraction.target1 = 0xFFFF;
                activeInteraction.target2 = 0xFFFF;
            }
            int d = (kDown & KEY_RIGHT) ? 1 : ((kDown & KEY_LEFT) ? -1 : 0);
            if (d != 0 || (kDown & KEY_A)) {
                if (cursorIndex == 0) activeInteraction.emotion = (u8)((activeInteraction.emotion + 5 + (d ? d : 1)) % 5);
                else if (cursorIndex == 1) activeInteraction.typeId = (u16)((int)activeInteraction.typeId + (d ? d : 1));
                else if (cursorIndex == 2) activeInteraction.target1 = (u16)((int)activeInteraction.target1 + (d ? d : 1));
                else if (cursorIndex == 3) activeInteraction.target2 = (u16)((int)activeInteraction.target2 + (d ? d : 1));
            }
        }
        // ── Quick Extras & Save Repair (#48, #57) ────────────────────
        else if (currentState == STATE_EXTRAS_MENU) {
            adjustListNav(kDown, 8);
            if (kDown & KEY_B) { currentState = STATE_MAIN_MENU; cursorIndex = 13; scrollOffset = 0; }
            if ((kDown & KEY_A) && fileOpenSuccess) {
                renderBusyFrame("Applying Extras...", "Updating save data. Please wait...");
                if (cursorIndex == 0) {
                    int n = setAllMiisStomach(file.get(), archive.get(), currentRegion, 100);
                    snprintf(statusToast, sizeof(statusToast), "Filled %d Miis' stomachs (100%%)!", n);
                } else if (cursorIndex == 1) {
                    int n = setAllMiisStomach(file.get(), archive.get(), currentRegion, 0);
                    snprintf(statusToast, sizeof(statusToast), "Emptied %d Miis' stomachs (0%%)!", n);
                } else if (cursorIndex == 2) {
                    int n = maxAllMiisLevelAndMoney(file.get(), archive.get(), currentRegion);
                    snprintf(statusToast, sizeof(statusToast), "Maxed %d Miis (Lv.99 + Pocket)!", n);
                } else if (cursorIndex == 3) {
                    int n = resolveAllRelationshipFights(file.get(), archive.get(), currentRegion);
                    snprintf(statusToast, sizeof(statusToast), "Resolved %d fights!", n);
                } else if (cursorIndex == 4) {
                    resetAllApartmentRooms(file.get(), archive.get(), currentRegion);
                    snprintf(statusToast, sizeof(statusToast), "Reset all Apartment room assignments!");
                } else if (cursorIndex == 5) {
                    resetMiiNewsFlashes(file.get(), archive.get(), currentRegion);
                    snprintf(statusToast, sizeof(statusToast), "Cleared Mii News flashes!");
                } else if (cursorIndex == 6) {
                    int n = repairSaveInteractions(file.get(), archive.get(), currentRegion);
                    snprintf(statusToast, sizeof(statusToast), "Repaired %d malformed interactions!", n);
                } else if (cursorIndex == 7) {
                    int checked = 0;
                    int fixed = verifyAndRepairAllMiiChecksums(file.get(), archive.get(), currentRegion, &checked);
                    snprintf(statusToast, sizeof(statusToast), "Checked %d Miis: fixed %d CRCs!", checked, fixed);
                }
            }
        }
        // ── Travelers & Concert Hall (#49, #56) ──────────────────────
        else if (currentState == STATE_TRAVELERS_CONCERT_MENU) {
            adjustListNav(kDown, 2);
            if (kDown & KEY_B) { currentState = STATE_MAIN_MENU; cursorIndex = 0; scrollOffset = 0; }
            if (kDown & KEY_A) {
                currentState = (cursorIndex == 0) ? STATE_TRAVELERS_LIST : STATE_CONCERT_SONGS;
                cursorIndex = 0; scrollOffset = 0;
            }
        }
        else if (currentState == STATE_TRAVELERS_LIST) {
            adjustListNav(kDown, MAX_TRAVELERS);
            if (kDown & KEY_B) { currentState = STATE_TRAVELERS_CONCERT_MENU; cursorIndex = 0; scrollOffset = 0; }
            if ((kDown & KEY_A) && fileOpenSuccess) {
                activeTravelerIdx = cursorIndex;
                readTravelerData(file.get(), currentRegion, activeTravelerIdx, activeTraveler);
                currentState = STATE_TRAVELER_EDIT;
                cursorIndex = 0; scrollOffset = 0;
            }
            if ((kDown & KEY_X) && fileOpenSuccess) {
                clearTravelerSlot(file.get(), archive.get(), currentRegion, cursorIndex);
                snprintf(statusToast, sizeof(statusToast), "Cleared Traveler slot %d!", cursorIndex + 1);
            }
        }
        else if (currentState == STATE_TRAVELER_EDIT) {
            adjustListNav(kDown, 5); // Nickname, Message, On Island, Level, EXP
            if (kDown & KEY_B) {
                writeTravelerData(file.get(), archive.get(), currentRegion, activeTraveler);
                currentState = STATE_TRAVELERS_LIST; cursorIndex = activeTravelerIdx; scrollOffset = 0;
            }
            if (kDown & KEY_A) {
                if (cursorIndex == 0) editStringWithKeyboard(activeTraveler.nickname, 10, "Traveler Nickname");
                else if (cursorIndex == 1) editStringWithKeyboard(activeTraveler.message, 24, "Traveler Message");
                else if (cursorIndex == 2) activeTraveler.onIsland = activeTraveler.onIsland ? 0 : 1;
                else if (cursorIndex == 3) { int v = (int)activeTraveler.level + 5; activeTraveler.level = (u8)((v > 99) ? 99 : v); }
                else if (cursorIndex == 4) { int v = (int)activeTraveler.experience + 10; activeTraveler.experience = (u8)((v > 99) ? 99 : v); }
            }
            if ((kDown & KEY_LEFT) || (kDown & KEY_RIGHT)) {
                int d = (kDown & KEY_RIGHT) ? 1 : -1;
                if (cursorIndex == 2) activeTraveler.onIsland = activeTraveler.onIsland ? 0 : 1;
                else if (cursorIndex == 3) { int v = (int)activeTraveler.level + d; activeTraveler.level = (u8)((v < 1) ? 1 : (v > 99 ? 99 : v)); }
                else if (cursorIndex == 4) { int v = (int)activeTraveler.experience + d; activeTraveler.experience = (u8)((v < 0) ? 0 : (v > 99 ? 99 : v)); }
            }
        }
        else if (currentState == STATE_CONCERT_SONGS) {
            adjustListNav(kDown, 8);
            if (kDown & KEY_B) { currentState = STATE_TRAVELERS_CONCERT_MENU; cursorIndex = 1; scrollOffset = 0; }
            if ((kDown & KEY_A) && fileOpenSuccess) {
                activeSongIdx = cursorIndex;
                readConcertSong(file.get(), activeSongIdx, activeSong);
                currentState = STATE_CONCERT_EDIT;
                cursorIndex = 0; scrollOffset = 0;
            }
        }
        else if (currentState == STATE_CONCERT_EDIT) {
            adjustListNav(kDown, 15); // Title + Genre + 13 Lyric lines
            if (kDown & KEY_B) {
                writeConcertSong(file.get(), archive.get(), activeSongIdx, activeSong);
                currentState = STATE_CONCERT_SONGS; cursorIndex = activeSongIdx; scrollOffset = 0;
            }
            if (kDown & KEY_A) {
                if (cursorIndex == 0) editStringWithKeyboard(activeSong.title, 32, "Song Title");
                else if (cursorIndex == 1) activeSong.genre = (activeSong.genre + 1) % 8;
                else editStringWithKeyboard(activeSong.lines[cursorIndex - 2], 48, "Song Lyric Line");
            }
        }

        // ═══════════════════ BUILD STRUCTURED UI ROWS ════════════════

        std::vector<UIRow> rows;
        char headerTitle[80] = "";
        char bottomHint[256] = "";
        size_t headerIcon = SPR_COUNT;

        if (!fileOpenSuccess) {
            snprintf(headerTitle, sizeof(headerTitle), "Save Data Not Found");
            snprintf(bottomHint, sizeof(bottomHint),
                     "Checked Cartridge / SD (US/EU/JP/KR)\n"
                     "and sdmc:/3ds/tlse/savedataArc.txt\n"
                     "[START] Exit");
        } else if (currentState == STATE_MAIN_MENU) {
            u32 dollars = currentMoney / 100;
            u32 cents = currentMoney % 100;
            snprintf(headerTitle, sizeof(headerTitle), "%s  |  $%lu.%02lu  (%s)",
                     islandNameUTF8, dollars, cents, getRegionName(currentRegion));

            struct MainItemDef { const char* label; const char* val; size_t icon; };
            static const MainItemDef MAIN_DEFS[14] = {
                {"Edit Money",                  "Edit",   SPR_ICON_MONEY},
                {"Edit Food",                   "Items",  SPR_ICON_FOOD},
                {"Edit Clothes",                "Items",  SPR_ICON_CLOTHES},
                {"Edit Hats",                   "Items",  SPR_ICON_HATS},
                {"Edit Interiors",              "Items",  SPR_ICON_INTERIORS},
                {"Edit Goods",                  "Items",  SPR_ICON_GOODS},
                {"Edit Treasures",              "Items",  SPR_ICON_TREASURES},
                {"Unlock All Special Items",    "x99",    SPR_ICON_SPECIAL},
                {"Mii Editor (Status/Profile)", "100",    SPR_ICON_MII},
                {"Relationship Editor",         "Matrix", SPR_ICON_RELATIONSHIPS},
                {"Island Buildings & Rankings", "24+10",  SPR_ICON_ISLAND},
                {"Island Info & Settings",      "Stats",  SPR_ICON_ISLAND},
                {"Active Mii Interactions",     "Bubbles",SPR_ICON_INTERACTIONS},
                {"Quick Extras & Save Repair",  "Tools",  SPR_ICON_EXTRAS}
            };
            for (int i = 0; i < MAIN_MENU_ITEMS; i++) {
                UIRow r;
                snprintf(r.label, sizeof(r.label), "%s", MAIN_DEFS[i].label);
                snprintf(r.value, sizeof(r.value), "%s", MAIN_DEFS[i].val);
                r.iconIdx = MAIN_DEFS[i].icon;
                rows.push_back(r);
            }
            snprintf(bottomHint, sizeof(bottomHint),
                     "[A] Open  [X] Travelers/Concert  [Y] Restore .bak\n"
                     "[L/R] Region  |  Backup: %s\n"
                     "%s",
                     g_backupStatus,
                     statusToast[0] ? statusToast : "[START] Save & Exit");
        } else if (currentState == STATE_MONEY_EDIT) {
            u32 dollars = currentMoney / 100;
            u32 cents = currentMoney % 100;
            headerIcon = SPR_ICON_MONEY;
            snprintf(headerTitle, sizeof(headerTitle), "Money Editor — %s", islandNameUTF8);
            UIRow r1; snprintf(r1.label, sizeof(r1.label), "Current Balance"); snprintf(r1.value, sizeof(r1.value), "$%lu.%02lu", dollars, cents); r1.iconIdx = SPR_ICON_MONEY;
            UIRow r2; snprintf(r2.label, sizeof(r2.label), "Maximum Limit"); snprintf(r2.value, sizeof(r2.value), "$999,999.99"); r2.iconIdx = SPR_COUNT;
            rows.push_back(r1); rows.push_back(r2);
            snprintf(bottomHint, sizeof(bottomHint),
                     "[A] +$100   [Left/Right] -/+$10,000\n"
                     "[X] Max ($999,999.99)   [Y] Reset ($0)\n"
                     "[B] Back");
        } else if (currentState >= STATE_CATEGORY_FOOD && currentState <= STATE_CATEGORY_TREASURES) {
            CategoryInfo* cat = getCategoryForState(currentState);
            headerIcon = SPR_ICON_FOOD + (size_t)(currentState - STATE_CATEGORY_FOOD);
            snprintf(headerTitle, sizeof(headerTitle), "%s (%zu items)", getTitleForState(currentState), cat->filteredIndices.size());
            for (size_t i = 0; i < cat->filteredIndices.size(); i++) {
                int orig = cat->filteredIndices[i];
                UIRow r;
                if (cat->names != NULL) {
                    strncpy(r.label, cat->names[orig + cat->nameOffset], sizeof(r.label) - 1);
                    r.label[sizeof(r.label) - 1] = '\0';
                    capitalizeString(r.label);
                } else {
                    snprintf(r.label, sizeof(r.label), "Item #%d", orig + 1);
                }
                u8 qty = 0; u32 br = 0;
                u64 off = cat->hasColors ? (cat->saveBase + (u64)orig * cat->bytesPerItem) : (cat->saveBase + (u64)orig);
                file->read(off, &qty, 1, &br);
                formatItemQty(r.value, sizeof(r.value), qty);
                r.iconIdx = SPR_COUNT;
                rows.push_back(r);
            }
            snprintf(bottomHint, sizeof(bottomHint),
                     "[A] Set Quantity   [X] Unlock All (x99)\n"
                     "[Up/Down] Scroll   [B] Back\n%s", statusToast);
        } else if (currentState == STATE_COLOR_SELECT) {
            CategoryInfo* cat = getCategoryForState(previousCategory);
            int originalIdx = cat->filteredIndices[savedItemCursor];
            int validColors = getValidColorCount(cat, originalIdx);
            snprintf(headerTitle, sizeof(headerTitle), "Select Color (%d available)", validColors);
            const char* colors[] = {"All Colors", "Color 1", "Color 2", "Color 3", "Color 4", "Color 5", "Color 6", "Color 7", "Color 8"};
            for (int i = 0; i <= validColors; i++) {
                UIRow r;
                snprintf(r.label, sizeof(r.label), "%s", colors[i]);
                r.value[0] = '\0'; r.iconIdx = SPR_COUNT;
                rows.push_back(r);
            }
            snprintf(bottomHint, sizeof(bottomHint), "[A] Select Color   [B] Back");
        } else if (currentState == STATE_QUANTITY_SELECT) {
            CategoryInfo* cat = getCategoryForState(previousCategory);
            int originalIdx = cat->filteredIndices[savedItemCursor];
            u8 curQty = 0; u32 br = 0;
            u64 curOff = cat->hasColors
                ? (cat->saveBase + (u64)originalIdx * cat->bytesPerItem + (u64)(selectedColor >= 0 ? selectedColor : 0))
                : (cat->saveBase + (u64)originalIdx);
            file->read(curOff, &curQty, 1, &br);
            snprintf(headerTitle, sizeof(headerTitle), "Set Quantity");
            UIRow r0; snprintf(r0.label, sizeof(r0.label), "Current Owned"); formatItemQty(r0.value, sizeof(r0.value), curQty); r0.iconIdx = SPR_COUNT;
            UIRow r1; snprintf(r1.label, sizeof(r1.label), "New Quantity"); snprintf(r1.value, sizeof(r1.value), "x%d", quantityToGive); r1.iconIdx = SPR_COUNT;
            rows.push_back(r0);
            rows.push_back(r1);
            snprintf(bottomHint, sizeof(bottomHint), "[Up/Down] +/-1  [Left/Right] +/-10\n[A] Apply  [B] Cancel");
        } else if (currentState == STATE_MII_SELECT) {
            headerIcon = SPR_ICON_MII;
            snprintf(headerTitle, sizeof(headerTitle), "Select Mii (%zu Islanders)", miiList.size());
            for (size_t i = 0; i < miiList.size(); i++) {
                UIRow r;
                snprintf(r.label, sizeof(r.label), "%s (%s)", miiList[i].nickname, miiList[i].gender == 0 ? "M" : "F");
                snprintf(r.value, sizeof(r.value), "Lv.%u", miiList[i].level);
                r.iconIdx = SPR_COUNT;
                rows.push_back(r);
            }
            snprintf(bottomHint, sizeof(bottomHint), "[A] Edit  [X] Max Lv.99  [L/R] .tlamii\n[B] Back  %s", statusToast);
        } else if (currentState == STATE_MII_SUBMENU) {
            headerIcon = SPR_ICON_MII;
            snprintf(headerTitle, sizeof(headerTitle), "Mii: %s (Slot %d)", activeMii.nickname, activeMii.slotIndex + 1);
            const char* subs[8] = {
                "Status & Stats", "Profile & Names", "Voice & Personality",
                "Equipped, Gifts & Foods", "Export .tlmii to SD", "Import .tlmii from SD",
                "Clone Mii to Empty Slot", "Delete Mii (Wipe Slot)"
            };
            for (int i = 0; i < 8; i++) {
                UIRow r; snprintf(r.label, sizeof(r.label), "%s", subs[i]); r.value[0] = '\0'; r.iconIdx = SPR_COUNT;
                if (i == 7 && confirmDeleteMii) snprintf(r.value, sizeof(r.value), "CONFIRM?");
                rows.push_back(r);
            }
            snprintf(bottomHint, sizeof(bottomHint), "[A] Open / Confirm   [B] Back to Mii List\n%s", statusToast);
        } else if (currentState == STATE_MII_STATUS_EDIT) {
            snprintf(headerTitle, sizeof(headerTitle), "%s — Status", activeMii.nickname);
            const char* labels[9] = {"Level", "Experience", "Pocket Money", "Pampered Score", "Splurge Score", "Stomach Fullness", "Hair Dye", "Apartment Room", "Mii Home ID"};
            for (int i = 0; i < 9; i++) {
                UIRow r; snprintf(r.label, sizeof(r.label), "%s", labels[i]); r.iconIdx = SPR_COUNT;
                if (i == 0) snprintf(r.value, sizeof(r.value), "%u", activeMii.level);
                else if (i == 1) snprintf(r.value, sizeof(r.value), "%u%%", activeMii.experience);
                else if (i == 2) snprintf(r.value, sizeof(r.value), "$%lu.%02lu", activeMii.pocketMoney / 100, activeMii.pocketMoney % 100);
                else if (i == 3) snprintf(r.value, sizeof(r.value), "$%lu.%02lu", activeMii.pampered / 100, activeMii.pampered % 100);
                else if (i == 4) snprintf(r.value, sizeof(r.value), "$%lu.%02lu", activeMii.splurge / 100, activeMii.splurge % 100);
                else if (i == 5) snprintf(r.value, sizeof(r.value), "%u%%", activeMii.stomach);
                else if (i == 6) snprintf(r.value, sizeof(r.value), activeMii.hairColor < 128 ? "Natural" : "Dye #%u", activeMii.hairColor);
                else if (i == 7) snprintf(r.value, sizeof(r.value), "#%u", activeMii.apartmentRoom + 1);
                else snprintf(r.value, sizeof(r.value), activeMii.miiHome == 0xFF ? "None" : "Home #%u", activeMii.miiHome + 1);
                rows.push_back(r);
            }
            snprintf(bottomHint, sizeof(bottomHint), "[Left/Right] +/-1  [A] +10  [X] Max  [Y] Reset\n[B] Save & Back");
        } else if (currentState == STATE_MII_PROFILE_EDIT) {
            snprintf(headerTitle, sizeof(headerTitle), "%s — Profile", activeMii.nickname);
            const char* labels[12] = {"Nickname", "First Name", "Last Name", "Gender", "Favorite Color", "Adult / Kid", "Relation to You", "Birth Month", "Birth Day", "Catchphrase", "Sharing Allowed", "Copying Allowed"};
            for (int i = 0; i < 12; i++) {
                UIRow r; snprintf(r.label, sizeof(r.label), "%s", labels[i]); r.iconIdx = SPR_COUNT;
                if (i == 0) snprintf(r.value, sizeof(r.value), "%.24s", activeMii.nickname);
                else if (i == 1) snprintf(r.value, sizeof(r.value), "%.24s", activeMii.firstName);
                else if (i == 2) snprintf(r.value, sizeof(r.value), "%.24s", activeMii.lastName);
                else if (i == 3) snprintf(r.value, sizeof(r.value), "%s", activeMii.gender == 0 ? "Male" : "Female");
                else if (i == 4) snprintf(r.value, sizeof(r.value), "%s", getFavColorName(activeMii.favColor));
                else if (i == 5) snprintf(r.value, sizeof(r.value), "%s", activeMii.grownUpFlag == 2 ? "Kid" : "Adult");
                else if (i == 6) snprintf(r.value, sizeof(r.value), "%s", getRelationName(activeMii.relationToYou));
                else if (i == 7) snprintf(r.value, sizeof(r.value), "%s", getMonthName(activeMii.birthMonth));
                else if (i == 8) {
                    if (activeMii.birthDay == 0) snprintf(r.value, sizeof(r.value), "Unset");
                    else snprintf(r.value, sizeof(r.value), "%u", (unsigned)activeMii.birthDay);
                }
                else if (i == 9) snprintf(r.value, sizeof(r.value), "%.20s", activeMii.catchphrase[0] ? activeMii.catchphrase : "(None)");
                else if (i == 10) snprintf(r.value, sizeof(r.value), "%s", activeMii.sharing ? "ON" : "OFF");
                else snprintf(r.value, sizeof(r.value), "%s", activeMii.copying ? "ON" : "OFF");
                rows.push_back(r);
            }
            snprintf(bottomHint, sizeof(bottomHint), "[A] Edit Text / Toggle   [Left/Right] Cycle\n[B] Save & Back");
        } else if (currentState == STATE_MII_VOICE_PERS) {
            snprintf(headerTitle, sizeof(headerTitle), "%s — Voice & Personality", activeMii.nickname);
            const char* labels[11] = {"Voice Pitch", "Voice Speed", "Voice Quality", "Voice Tone", "Voice Accent", "Voice Intonation",
                                      "Pers: Movement", "Pers: Speech", "Pers: Expressiveness", "Pers: Attitude", "Pers: Overall"};
            for (int i = 0; i < 11; i++) {
                UIRow r; snprintf(r.label, sizeof(r.label), "%s", labels[i]); r.iconIdx = SPR_COUNT;
                snprintf(r.value, sizeof(r.value), "%u", (i < 6) ? activeMii.voice[i] : activeMii.personality[i - 6]);
                rows.push_back(r);
            }
            snprintf(bottomHint, sizeof(bottomHint), "[Left/Right] Adjust   [A] +5   [B] Save & Back");
        } else if (currentState == STATE_MII_GIFTS_FOODS) {
            snprintf(headerTitle, sizeof(headerTitle), "%s — Equipped, Foods & Gifts", activeMii.nickname);
            const char* labels[10] = {
                "Equipped Clothes", "Equipped Hat", "Equipped Interior",
                "Super All-Time Fav", "All-Time Fav", "Favorite Food #1",
                "Favorite Food #2", "Favorite Food #3", "Worst Food", "Worst Ever Food"
            };
            for (int i = 0; i < 10; i++) {
                UIRow r; snprintf(r.label, sizeof(r.label), "%s", labels[i]); r.iconIdx = SPR_COUNT;
                if (i == 0) formatClothesName(activeMii.equippedClothes, r.value, sizeof(r.value));
                else if (i == 1) formatHatName(activeMii.equippedHat, r.value, sizeof(r.value));
                else if (i == 2) formatInteriorName(activeMii.equippedInterior, r.value, sizeof(r.value));
                else if (i == 3) formatFoodName(activeMii.superFavFood, r.value, sizeof(r.value));
                else if (i == 4) formatFoodName(activeMii.allTimeFavFood, r.value, sizeof(r.value));
                else if (i == 5) formatFoodName(activeMii.favFood1, r.value, sizeof(r.value));
                else if (i == 6) formatFoodName(activeMii.favFood2, r.value, sizeof(r.value));
                else if (i == 7) formatFoodName(activeMii.favFood3, r.value, sizeof(r.value));
                else if (i == 8) formatFoodName(activeMii.worstFood, r.value, sizeof(r.value));
                else formatFoodName(activeMii.worstEverFood, r.value, sizeof(r.value));
                rows.push_back(r);
            }
            for (int g = 0; g < 8; g++) {
                UIRow rg;
                snprintf(rg.label, sizeof(rg.label), "Level-Up Gift #%d", g + 1);
                snprintf(rg.value, sizeof(rg.value), "%s", getLevelUpGiftName(activeMii.levelUpGifts[g]));
                rg.iconIdx = SPR_COUNT;
                rows.push_back(rg);
            }
            snprintf(bottomHint, sizeof(bottomHint), "[Left/Right] +/-1   [A] +10   [Y] Clear (None)\n[B] Save & Back");
        } else if (currentState == STATE_REL_MII_A || currentState == STATE_REL_MII_B) {
            headerIcon = SPR_ICON_RELATIONSHIPS;
            snprintf(headerTitle, sizeof(headerTitle),
                     (currentState == STATE_REL_MII_A) ? "Relationships: Select Mii A" : "Relationships: %s <-> Mii B",
                     (currentState == STATE_REL_MII_A) ? "" : miiList[relMiiAIdx].nickname);
            for (size_t i = 0; i < miiList.size(); i++) {
                UIRow r;
                snprintf(r.label, sizeof(r.label), "%s", miiList[i].nickname);
                r.iconIdx = SPR_COUNT;
                if (currentState == STATE_REL_MII_B) {
                    if ((int)i == relMiiAIdx) {
                        snprintf(r.value, sizeof(r.value), "(Self)");
                    } else {
                        RelationshipPair rp;
                        readRelationshipPair(file.get(), currentRegion, miiList[relMiiAIdx].slotIndex, miiList[i].slotIndex, rp);
                        snprintf(r.value, sizeof(r.value), "%s (%u/%u)", getRelationshipTypeName(rp.relType),
                                 (unsigned)rp.feelingAtoB, (unsigned)rp.feelingBtoA);
                        if (isRelationshipConflict(rp.relType)) r.iconIdx = SPR_BADGE_CONFLICT_STORM;
                        else if (isRelationshipRomantic(rp.relType)) r.iconIdx = SPR_BADGE_HEART_PINK;
                        else if (rp.relType == 1 || rp.relType == 12) r.iconIdx = SPR_BADGE_FRIEND_GREEN;
                    }
                } else {
                    snprintf(r.value, sizeof(r.value), "Lv.%u", miiList[i].level);
                }
                rows.push_back(r);
            }
            snprintf(bottomHint, sizeof(bottomHint),
                     (currentState == STATE_REL_MII_A)
                         ? "[A] Select Mii A   [Y] Resolve Fights\n[X] Max All Feelings   [B] Back\n%s"
                         : "[A] Edit Pair   [B] Back to Mii A",
                     statusToast);
        } else if (currentState == STATE_REL_EDIT) {
            headerIcon = SPR_ICON_RELATIONSHIPS;
            snprintf(headerTitle, sizeof(headerTitle), "%s <-> %s", miiList[relMiiAIdx].nickname, miiList[relMiiBIdx].nickname);
            UIRow r1; snprintf(r1.label, sizeof(r1.label), "Relationship Type (Symmetric)"); snprintf(r1.value, sizeof(r1.value), "%s", getRelationshipTypeName(activeRel.relType)); r1.iconIdx = SPR_COUNT;
            UIRow r2; snprintf(r2.label, sizeof(r2.label), "%s -> %s Feeling", miiList[relMiiAIdx].nickname, miiList[relMiiBIdx].nickname); snprintf(r2.value, sizeof(r2.value), "%u / 200", activeRel.feelingAtoB); r2.iconIdx = SPR_COUNT;
            UIRow r3; snprintf(r3.label, sizeof(r3.label), "%s -> %s Feeling", miiList[relMiiBIdx].nickname, miiList[relMiiAIdx].nickname); snprintf(r3.value, sizeof(r3.value), "%u / 200", activeRel.feelingBtoA); r3.iconIdx = SPR_COUNT;
            rows.push_back(r1); rows.push_back(r2); rows.push_back(r3);
            snprintf(bottomHint, sizeof(bottomHint), "[Left/Right] Adjust   [A] +10   [X] Max Both 200\n[B] Save & Back");
        } else if (currentState == STATE_ISLAND_CONFIG) {
            headerIcon = SPR_ICON_ISLAND;
            snprintf(headerTitle, sizeof(headerTitle), "Island Buildings (24) & Rankings (10)");
            for (int i = 0; i < 24; i++) {
                UIRow r; snprintf(r.label, sizeof(r.label), "Bldg: %s", getBuildingName(i));
                snprintf(r.value, sizeof(r.value), activeConfig.buildings[i] > 0 ? "Unlocked" : "Locked");
                r.iconIdx = SPR_COUNT; rows.push_back(r);
            }
            for (int i = 0; i < 10; i++) {
                UIRow r; snprintf(r.label, sizeof(r.label), "Rank: %s", getRankingBoardName(i));
                snprintf(r.value, sizeof(r.value), activeConfig.rankings[i] > 0 ? "Unlocked" : "Locked");
                r.iconIdx = SPR_COUNT; rows.push_back(r);
            }
            UIRow ra; snprintf(ra.label, sizeof(ra.label), "Apartment Expansion Tier");
            snprintf(ra.value, sizeof(ra.value), "Tier %u", activeConfig.apartmentRenov);
            ra.iconIdx = SPR_COUNT; rows.push_back(ra);
            snprintf(bottomHint, sizeof(bottomHint), "[A] Toggle   [X] Unlock All Buildings/Rankings\n[B] Save & Back\n%s", statusToast);
        } else if (currentState == STATE_ISLAND_INFO) {
            headerIcon = SPR_ICON_ISLAND;
            snprintf(headerTitle, sizeof(headerTitle), "Island Info & Settings");
            const char* labels[12] = {"Island Name", "Island Pronunciation", "Problems Solved", "Weddings", "Children Born", "StreetPass Encounters", "Travelers Received", "Travelers Sent", "StreetPass Active", "Babies Allowed", "SpotPass Active", "Export Outfit Color"};
            for (int i = 0; i < 12; i++) {
                UIRow r; snprintf(r.label, sizeof(r.label), "%s", labels[i]); r.iconIdx = SPR_COUNT;
                if (i == 0) snprintf(r.value, sizeof(r.value), "%s", activeIslandInfo.islandName);
                else if (i == 1) snprintf(r.value, sizeof(r.value), "%s", activeIslandInfo.islandPronun);
                else if (i == 2) snprintf(r.value, sizeof(r.value), "%u", activeIslandInfo.problemsSolved);
                else if (i == 3) snprintf(r.value, sizeof(r.value), "%u", activeIslandInfo.weddings);
                else if (i == 4) snprintf(r.value, sizeof(r.value), "%u", activeIslandInfo.childrenBorn);
                else if (i == 5) snprintf(r.value, sizeof(r.value), "%u", activeIslandInfo.streetPassEncounters);
                else if (i == 6) snprintf(r.value, sizeof(r.value), "%u", activeIslandInfo.travelersReceived);
                else if (i == 7) snprintf(r.value, sizeof(r.value), "%u", activeIslandInfo.travelersSent);
                else if (i == 8) snprintf(r.value, sizeof(r.value), "%s", activeIslandInfo.streetPassActive ? "ON" : "OFF");
                else if (i == 9) snprintf(r.value, sizeof(r.value), "%s", activeIslandInfo.babiesAllowed ? "ON" : "OFF");
                else if (i == 10) snprintf(r.value, sizeof(r.value), "%s", activeIslandInfo.spotPassActive ? "ON" : "OFF");
                else snprintf(r.value, sizeof(r.value), "Color %d", exportColorBitToIndex(activeIslandInfo.exportColorBit) + 1);
                rows.push_back(r);
            }
            snprintf(bottomHint, sizeof(bottomHint), "[A] Edit Name / +10   [Left/Right] Adjust\n[B] Save & Back");
        } else if (currentState == STATE_INTERACTIONS_MII) {
            headerIcon = SPR_ICON_INTERACTIONS;
            snprintf(headerTitle, sizeof(headerTitle), "Active Mii Interactions");
            for (size_t i = 0; i < miiList.size(); i++) {
                MiiInteractionData id;
                readMiiInteraction(file.get(), currentRegion, miiList[i].slotIndex, id);
                UIRow r; snprintf(r.label, sizeof(r.label), "%s", miiList[i].nickname);
                snprintf(r.value, sizeof(r.value), "%s", getEmotionName(id.emotion));
                r.iconIdx = SPR_COUNT; rows.push_back(r);
            }
            snprintf(bottomHint, sizeof(bottomHint), "[A] Edit Window Bubble   [B] Back");
        } else if (currentState == STATE_INTERACTIONS_EDIT) {
            headerIcon = SPR_ICON_INTERACTIONS;
            snprintf(headerTitle, sizeof(headerTitle), "%s — Window Bubble", miiList[selectedMiiListIdx].nickname);
            UIRow r0; snprintf(r0.label, sizeof(r0.label), "Bubble Emotion"); snprintf(r0.value, sizeof(r0.value), "%s", getEmotionName(activeInteraction.emotion)); r0.iconIdx = SPR_COUNT;
            UIRow r1; snprintf(r1.label, sizeof(r1.label), "Interaction Type ID"); snprintf(r1.value, sizeof(r1.value), "0x%04X", activeInteraction.typeId); r1.iconIdx = SPR_COUNT;
            UIRow r2; snprintf(r2.label, sizeof(r2.label), "Target Mii 1 Slot"); snprintf(r2.value, sizeof(r2.value), activeInteraction.target1 == 0xFFFF ? "None" : "%u", activeInteraction.target1); r2.iconIdx = SPR_COUNT;
            UIRow r3; snprintf(r3.label, sizeof(r3.label), "Target Mii 2 Slot"); snprintf(r3.value, sizeof(r3.value), activeInteraction.target2 == 0xFFFF ? "None" : "%u", activeInteraction.target2); r3.iconIdx = SPR_COUNT;
            rows.push_back(r0); rows.push_back(r1); rows.push_back(r2); rows.push_back(r3);
            snprintf(bottomHint, sizeof(bottomHint), "[Left/Right] Adjust   [X] Clear Bubble\n[B] Save & Back");
        } else if (currentState == STATE_EXTRAS_MENU) {
            headerIcon = SPR_ICON_EXTRAS;
            snprintf(headerTitle, sizeof(headerTitle), "Quick Extras & Save Repair");
            const char* labels[8] = {"Fill All Miis' Stomachs (100%)", "Empty All Miis' Stomachs (0%)", "Max All Miis (Lv.99 + Pocket)", "Resolve All Relationship Fights", "Reset Apartment Room Layout", "Reset Mii News Flashes", "Repair Malformed Interactions", "Verify & Repair Mii Checksums"};
            for (int i = 0; i < 8; i++) {
                UIRow r; snprintf(r.label, sizeof(r.label), "%s", labels[i]); snprintf(r.value, sizeof(r.value), "Run"); r.iconIdx = SPR_COUNT;
                rows.push_back(r);
            }
            snprintf(bottomHint, sizeof(bottomHint), "[A] Execute Tool   [B] Back\n%s", statusToast);
        } else if (currentState == STATE_TRAVELERS_CONCERT_MENU) {
            headerIcon = SPR_ICON_CONCERT;
            snprintf(headerTitle, sizeof(headerTitle), "Travelers & Concert Hall");
            UIRow r0; snprintf(r0.label, sizeof(r0.label), "StreetPass Travelers (50 Slots)"); snprintf(r0.value, sizeof(r0.value), "View"); r0.iconIdx = SPR_COUNT;
            UIRow r1; snprintf(r1.label, sizeof(r1.label), "Concert Hall Songs (8 Slots)"); snprintf(r1.value, sizeof(r1.value), "Edit"); r1.iconIdx = SPR_COUNT;
            rows.push_back(r0); rows.push_back(r1);
            snprintf(bottomHint, sizeof(bottomHint), "[A] Open   [B] Back");
        } else if (currentState == STATE_TRAVELERS_LIST) {
            headerIcon = SPR_ICON_CONCERT;
            snprintf(headerTitle, sizeof(headerTitle), "StreetPass Travelers (50)");
            for (int i = 0; i < MAX_TRAVELERS; i++) {
                TravelerSummary ts;
                readTravelerSummary(file.get(), currentRegion, i, ts);
                UIRow r; snprintf(r.label, sizeof(r.label), "[%02d] %s", i + 1, ts.nickname);
                snprintf(r.value, sizeof(r.value), ts.present ? "Active" : "Empty"); r.iconIdx = SPR_COUNT;
                rows.push_back(r);
            }
            snprintf(bottomHint, sizeof(bottomHint), "[A] Edit Traveler   [X] Clear   [B] Back\n%s", statusToast);
        } else if (currentState == STATE_TRAVELER_EDIT) {
            headerIcon = SPR_ICON_CONCERT;
            snprintf(headerTitle, sizeof(headerTitle), "Traveler #%d — %s", activeTravelerIdx + 1, activeTraveler.nickname);
            const char* labels[5] = {"Nickname", "Message", "On Island", "Level", "Experience"};
            for (int i = 0; i < 5; i++) {
                UIRow r; snprintf(r.label, sizeof(r.label), "%s", labels[i]); r.iconIdx = SPR_COUNT;
                if (i == 0) snprintf(r.value, sizeof(r.value), "%.24s", activeTraveler.nickname);
                else if (i == 1) snprintf(r.value, sizeof(r.value), "%.20s", activeTraveler.message[0] ? activeTraveler.message : "(None)");
                else if (i == 2) snprintf(r.value, sizeof(r.value), "%s", activeTraveler.onIsland ? "YES" : "NO");
                else if (i == 3) snprintf(r.value, sizeof(r.value), "%u", (unsigned)activeTraveler.level);
                else snprintf(r.value, sizeof(r.value), "%u%%", (unsigned)activeTraveler.experience);
                rows.push_back(r);
            }
            snprintf(bottomHint, sizeof(bottomHint), "[A] Edit / +Step   [Left/Right] Adjust\n[B] Save & Back");
        } else if (currentState == STATE_CONCERT_SONGS) {
            headerIcon = SPR_ICON_CONCERT;
            snprintf(headerTitle, sizeof(headerTitle), "Concert Hall Songs");
            for (int i = 0; i < 8; i++) {
                ConcertSongData cs;
                readConcertSong(file.get(), i, cs);
                UIRow r; snprintf(r.label, sizeof(r.label), "%s", cs.title);
                snprintf(r.value, sizeof(r.value), "%s", getConcertGenreName(cs.genre)); r.iconIdx = SPR_COUNT;
                rows.push_back(r);
            }
            snprintf(bottomHint, sizeof(bottomHint), "[A] Edit Lyrics & Title   [B] Back");
        } else if (currentState == STATE_CONCERT_EDIT) {
            headerIcon = SPR_ICON_CONCERT;
            snprintf(headerTitle, sizeof(headerTitle), "Edit Song: %s", activeSong.title);
            UIRow rt; snprintf(rt.label, sizeof(rt.label), "Song Title"); snprintf(rt.value, sizeof(rt.value), "%.22s", activeSong.title); rt.iconIdx = SPR_COUNT;
            UIRow rg; snprintf(rg.label, sizeof(rg.label), "Musical Genre"); snprintf(rg.value, sizeof(rg.value), "%s", getConcertGenreName(activeSong.genre)); rg.iconIdx = SPR_COUNT;
            rows.push_back(rt); rows.push_back(rg);
            for (int l = 0; l < 13; l++) {
                UIRow rl; snprintf(rl.label, sizeof(rl.label), "Line %02d: %.26s", l + 1, activeSong.lines[l]);
                rl.value[0] = '\0'; rl.iconIdx = SPR_COUNT;
                rows.push_back(rl);
            }
            snprintf(bottomHint, sizeof(bottomHint), "[A] Edit with Keyboard / Cycle Genre\n[B] Save & Back");
        }

        // ═══════════════════ RENDERING (#43 UI Overhaul) ════════════

        C2D_TextBufClear(topBuf);
        C2D_TextBufClear(bottomBuf);

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

        const u32 colSkyBlue  = C2D_Color32(110, 198, 255, 255);
        const u32 colCream    = C2D_Color32(255, 253, 245, 255);
        const u32 colTextDark = C2D_Color32(58, 34, 28, 255);
        const u32 colShadowLt = C2D_Color32(255, 255, 255, 190);
        const u32 colAccent   = C2D_Color32(180, 72, 42, 255);

        // ── Top Screen (400x240) ─────────────────────────────────────
        C2D_TargetClear(topTarget, colSkyBlue);
        C2D_SceneBegin(topTarget);
        drawSprite(spriteSheet, SPR_TOP_BG, 0.0f, 0.0f);

        if (!fileOpenSuccess) {
            drawSprite(spriteSheet, SPR_MODAL_ERROR, 40.0f, 36.0f);
            drawTextShadowed(topBuf, "ERROR: Save Data Not Found!\nInsert Cartridge or place savedataArc.txt\nin sdmc:/3ds/tlse/",
                             54.0f, 80.0f, 0.46f, 0.46f, colTextDark, colShadowLt);
        } else {
            // Translucent Header Bar (384x28 at 8,4) — keeps SPR_TOP_BG visible!
            drawSprite(spriteSheet, SPR_HEADER_BAR, 8.0f, 4.0f);
            drawTextShadowed(topBuf, headerTitle, 16.0f, 9.0f, 0.46f, 0.46f, colTextDark, colShadowLt);

            if (currentState == STATE_MAIN_MENU) {
                size_t regBadge = SPR_BADGE_US;
                if (currentRegion == REGION_EU) regBadge = SPR_BADGE_EU;
                else if (currentRegion == REGION_JP) regBadge = SPR_BADGE_JP;
                else if (currentRegion == REGION_KR) regBadge = SPR_BADGE_KR;
                drawSprite(spriteSheet, regBadge, 346.0f, 9.0f);
                bool isCart = (strncmp(saveSourceDesc, "Cartridge", 9) == 0);
                drawSprite(spriteSheet, isCart ? SPR_BADGE_CARTRIDGE : SPR_BADGE_SD, 322.0f, 8.0f);
                if (g_backupCreated) drawSprite(spriteSheet, SPR_BADGE_BACKUP_OK, 298.0f, 8.0f);
            } else if (headerIcon < SPR_COUNT) {
                drawSprite(spriteSheet, headerIcon, 360.0f, 2.0f, 0.9f, 0.9f);
            }

            // Individual Pill Rows (SPR_ROW_NORMAL / SPR_ROW_SELECTED)
            int totalRows = (int)rows.size();
            for (int r = 0; r < MAX_VISIBLE_ROWS && (scrollOffset + r) < totalRows; r++) {
                int idx = scrollOffset + r;
                bool isSel = (idx == cursorIndex);
                float rowY = 38.0f + (float)(r * 21);

                drawSprite(spriteSheet, isSel ? SPR_ROW_SELECTED : SPR_ROW_NORMAL, 20.0f, rowY);
                if (isSel) {
                    drawSprite(spriteSheet, SPR_CURSOR_HAND, 2.0f, rowY - 1.0f, 0.9f, 0.9f);
                }

                float labelX = 28.0f;
                if (rows[idx].iconIdx < SPR_COUNT) {
                    bool isMicroBadge = (rows[idx].iconIdx >= SPR_BADGE_HEART_PINK &&
                                         rows[idx].iconIdx <= SPR_BADGE_CONFLICT_STORM);
                    float iconScale = isMicroBadge ? 1.0f : 0.5f;
                    drawSprite(spriteSheet, rows[idx].iconIdx, 24.0f, rowY + 1.0f, iconScale, iconScale);
                    labelX = 44.0f;
                }

                drawTextShadowed(topBuf, rows[idx].label, labelX, rowY + 2.0f, 0.44f, 0.44f, colTextDark, colShadowLt);

                if (rows[idx].value[0] != '\0') {
                    C2D_Text vTxt;
                    C2D_TextParse(&vTxt, topBuf, rows[idx].value);
                    C2D_TextOptimize(&vTxt);
                    float vw = 0, vh = 0;
                    C2D_TextGetDimensions(&vTxt, 0.42f, 0.42f, &vw, &vh);
                    float vx = 372.0f - vw;
                    C2D_DrawText(&vTxt, C2D_WithColor, vx + 1.0f, rowY + 3.0f, 0.58f, 0.42f, 0.42f, colShadowLt);
                    C2D_DrawText(&vTxt, C2D_WithColor, vx, rowY + 2.0f, 0.62f, 0.42f, 0.42f, isSel ? colAccent : colTextDark);
                }
            }

            // Color swatch preview on Color Select screen
            if (currentState == STATE_COLOR_SELECT) {
                static const u32 swatchColors[8] = {
                    C2D_Color32(239, 83, 80, 255),   C2D_Color32(66, 165, 245, 255),
                    C2D_Color32(102, 187, 106, 255), C2D_Color32(255, 213, 79, 255),
                    C2D_Color32(171, 71, 188, 255),  C2D_Color32(255, 167, 38, 255),
                    C2D_Color32(141, 110, 99, 255),  C2D_Color32(96, 125, 139, 255)
                };
                for (int c = 0; c < (int)rows.size() - 1 && c < 8; c++) {
                    C2D_ImageTint tint;
                    C2D_PlainImageTint(&tint, swatchColors[c], 0.85f);
                    drawSprite(spriteSheet, SPR_COLOR_SWATCH_RING, 354.0f, 60.0f + (c * 21.0f), 0.9f, 0.9f, &tint);
                }
            }
        }

        // ── Bottom Screen (320x240) ──────────────────────────────────
        C2D_TargetClear(bottomTarget, colCream);
        C2D_SceneBegin(bottomTarget);
        drawSprite(spriteSheet, SPR_BOTTOM_BG, 0.0f, 0.0f);
        drawSprite(spriteSheet, SPR_BANNER_LOGO, 32.0f, 10.0f);
        drawSprite(spriteSheet, SPR_FOOTER_CARD, 12.0f, 80.0f);

        C2D_Text bTxt;
        C2D_TextParse(&bTxt, bottomBuf, bottomHint);
        C2D_TextOptimize(&bTxt);
        float textW = 0, textH = 0;
        C2D_TextGetDimensions(&bTxt, 0.43f, 0.43f, &textW, &textH);
        float textX = (320.0f - textW) / 2.0f;
        float textY = 80.0f + (96.0f - textH) / 2.0f;
        C2D_DrawText(&bTxt, C2D_WithColor, textX + 1.0f, textY + 1.0f, 0.58f, 0.43f, 0.43f, colShadowLt);
        C2D_DrawText(&bTxt, C2D_WithColor, textX, textY, 0.62f, 0.43f, 0.43f, colTextDark);

        TouchButton bBtns[5];
        int bBtnCount = getBottomButtons(currentState, fileOpenSuccess, bBtns);
        for (int i = 0; i < bBtnCount; i++) {
            drawSprite(spriteSheet, SPR_BTN_STEPPER, bBtns[i].x, bBtns[i].y, bBtns[i].scale, bBtns[i].scale);
            C2D_Text lblTxt;
            C2D_TextParse(&lblTxt, bottomBuf, bBtns[i].label);
            C2D_TextOptimize(&lblTxt);
            float lw = 0, lh = 0;
            C2D_TextGetDimensions(&lblTxt, 0.42f, 0.42f, &lw, &lh);
            float lx = bBtns[i].x + (bBtns[i].w - lw) / 2.0f;
            float ly = bBtns[i].y + (bBtns[i].h - lh) / 2.0f - 1.0f;
            C2D_DrawText(&lblTxt, C2D_WithColor, lx + 1.0f, ly + 1.0f, 0.58f, 0.42f, 0.42f, colShadowLt);
            C2D_DrawText(&lblTxt, C2D_WithColor, lx, ly, 0.62f, 0.42f, 0.42f, colTextDark);
        }

        C3D_FrameEnd(0);
    }

    if (spriteSheet != nullptr) C2D_SpriteSheetFree(spriteSheet);
    C2D_TextBufDelete(topBuf);
    C2D_TextBufDelete(bottomBuf);
    exitServices();
    return 0;
}

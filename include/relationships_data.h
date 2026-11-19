#pragma once

#include "save_io.h"
#include "mii_data.h"
#include <vector>

static inline u64 getRelFeelingOffset(SaveRegion reg, int miiA, int miiB) {
    u64 base = (reg == REGION_JP) ? 0x24880 : 0x299F0;
    return base + ((u64)miiA * 0x100) + (u64)miiB;
}

static inline u64 getRelTypeOffset(SaveRegion reg, int miiA, int miiB) {
    u64 base = (reg == REGION_JP) ? 0x248E4 : 0x29A54;
    return base + ((u64)miiA * 0x100) + (u64)miiB;
}

static inline const char* getRelationshipTypeName(u8 type) {
    static const char* NAMES[13] = {
        "Unknown",
        "Friend",
        "Lover",
        "Ex-Sweetheart",
        "Spouse",
        "Spouse (Alt)",
        "Ex-Spouse",
        "Parent / Child",
        "Sibling",
        "Friend (Fight)",
        "Lover (Fight)",
        "Spouse (Fight)",
        "Best Friend"
    };
    return (type <= 12) ? NAMES[type] : "Unknown";
}

static inline bool isRelationshipConflict(u8 type) {
    return (type == 9 || type == 10 || type == 11);
}

static inline bool isRelationshipRomantic(u8 type) {
    return (type == 2 || type == 4 || type == 5 || type == 10 || type == 11);
}

struct RelationshipPair {
    u8 feelingAtoB;  // 0..200
    u8 feelingBtoA;  // 0..200
    u8 relType;      // 0..12 (symmetrical)
};

static inline void readRelationshipPair(SafeFile* file, SaveRegion reg, int miiA, int miiB, RelationshipPair& out) {
    memset(&out, 0, sizeof(out));
    if (!file || !file->isOpen() || miiA < 0 || miiA >= MAX_MIIS || miiB < 0 || miiB >= MAX_MIIS) return;
    u32 br = 0;
    file->read(getRelFeelingOffset(reg, miiA, miiB), &out.feelingAtoB, 1, &br);
    file->read(getRelFeelingOffset(reg, miiB, miiA), &out.feelingBtoA, 1, &br);
    file->read(getRelTypeOffset(reg, miiA, miiB), &out.relType, 1, &br);
}

static inline bool isExclusiveRole(u8 relType, int* outCategory) {
    // Category 1 = Best Friend (12)
    // Category 2 = Lover (2, 10)
    // Category 3 = Spouse (4, 5, 11)
    if (relType == 12) { if (outCategory) *outCategory = 1; return true; }
    if (relType == 2 || relType == 10) { if (outCategory) *outCategory = 2; return true; }
    if (relType == 4 || relType == 5 || relType == 11) { if (outCategory) *outCategory = 3; return true; }
    return false;
}

static inline void writeRelationshipPair(SafeFile* file, SafeArchive* archive, SaveRegion reg,
                                         int miiA, int miiB, const RelationshipPair& in) {
    if (!file || !file->isOpen() || miiA < 0 || miiA >= MAX_MIIS || miiB < 0 || miiB >= MAX_MIIS || miiA == miiB) return;

    u8 fAB = (in.feelingAtoB > 200) ? 200 : in.feelingAtoB;
    u8 fBA = (in.feelingBtoA > 200) ? 200 : in.feelingBtoA;
    u8 rType = (in.relType > 12) ? 0 : in.relType;
    u32 bw = 0;

    // 1. Conflict-Free Uniqueness Check for Best Friend / Lover / Spouse
    int roleCat = 0;
    if (isExclusiveRole(rType, &roleCat)) {
        int endpoints[2] = { miiA, miiB };
        for (int e = 0; e < 2; e++) {
            int src = endpoints[e];
            int other = endpoints[1 - e];
            for (int k = 0; k < MAX_MIIS; k++) {
                if (k == src || k == other) continue;
                u8 existing = 0;
                u32 br = 0;
                file->read(getRelTypeOffset(reg, src, k), &existing, 1, &br);
                int existingCat = 0;
                if (isExclusiveRole(existing, &existingCat) && existingCat == roleCat) {
                    // Downgrade prior exclusive partner cleanly to Friend (1) symmetrically
                    u8 friendVal = 1;
                    file->write(getRelTypeOffset(reg, src, k), &friendVal, 1, &bw);
                    file->write(getRelTypeOffset(reg, k, src), &friendVal, 1, &bw);
                }
            }
        }
    }

    // 2. Write asymmetrical feelings
    file->write(getRelFeelingOffset(reg, miiA, miiB), &fAB, 1, &bw);
    file->write(getRelFeelingOffset(reg, miiB, miiA), &fBA, 1, &bw);

    // 3. Write symmetrical relationship type
    file->write(getRelTypeOffset(reg, miiA, miiB), &rType, 1, &bw);
    file->write(getRelTypeOffset(reg, miiB, miiA), &rType, 1, &bw);

    // 4. Sync Mii Home ID if Married (Spouse = 4/5/11)
    if (rType == 4 || rType == 5 || rType == 11) {
        u8 homeA = 0xFF, homeB = 0xFF;
        u32 br = 0;
        file->read(getMiiFieldOffset(reg, miiA, 0x22A9, 0x21A9), &homeA, 1, &br);
        file->read(getMiiFieldOffset(reg, miiB, 0x22A9, 0x21A9), &homeB, 1, &br);
        u8 sharedHome = (homeA < 50) ? homeA : ((homeB < 50) ? homeB : (u8)(miiA / 2));
        if (sharedHome >= 50) sharedHome = 0;
        file->write(getMiiFieldOffset(reg, miiA, 0x22A9, 0x21A9), &sharedHome, 1, &bw);
        file->write(getMiiFieldOffset(reg, miiB, 0x22A9, 0x21A9), &sharedHome, 1, &bw);
    }

    if (archive && archive->get() != 0) archive->commit();
}

static inline int resolveAllRelationshipFights(SafeFile* file, SafeArchive* archive, SaveRegion reg) {
    if (!file || !file->isOpen()) return 0;
    int resolved = 0;
    u8 row[MAX_MIIS];
    for (int i = 0; i < MAX_MIIS; i++) {
        u32 br = 0;
        if (R_FAILED(file->read(getRelTypeOffset(reg, i, 0), row, MAX_MIIS, &br)) || br != MAX_MIIS) continue;
        bool changed = false;
        for (int j = 0; j < MAX_MIIS; j++) {
            if (row[j] == 9)      { row[j] = 1; changed = true; resolved++; }
            else if (row[j] == 10){ row[j] = 2; changed = true; resolved++; }
            else if (row[j] == 11){ row[j] = 4; changed = true; resolved++; }
        }
        if (changed) {
            u32 bw = 0;
            file->write(getRelTypeOffset(reg, i, 0), row, MAX_MIIS, &bw);
        }
    }
    if (archive && archive->get() != 0) archive->commit();
    return resolved / 2;
}

static inline void maxAllAcquaintedFeelings(SafeFile* file, SafeArchive* archive, SaveRegion reg) {
    if (!file || !file->isOpen()) return;
    u8 feelings[MAX_MIIS];
    u8 types[MAX_MIIS];
    for (int i = 0; i < MAX_MIIS; i++) {
        u32 br = 0;
        file->read(getRelFeelingOffset(reg, i, 0), feelings, MAX_MIIS, &br);
        file->read(getRelTypeOffset(reg, i, 0), types, MAX_MIIS, &br);
        bool changed = false;
        for (int j = 0; j < MAX_MIIS; j++) {
            if (i != j && types[j] != 0 && feelings[j] != 200) {
                feelings[j] = 200;
                changed = true;
            }
        }
        if (changed) {
            u32 bw = 0;
            file->write(getRelFeelingOffset(reg, i, 0), feelings, MAX_MIIS, &bw);
        }
    }
    if (archive && archive->get() != 0) archive->commit();
}

static inline void acquaintAllRegisteredMiis(SafeFile* file, SafeArchive* archive, SaveRegion reg,
                                             const std::vector<MiiEntry>& miis) {
    if (!file || !file->isOpen()) return;
    for (size_t a = 0; a < miis.size(); a++) {
        for (size_t b = a + 1; b < miis.size(); b++) {
            int i = miis[a].slotIndex;
            int j = miis[b].slotIndex;
            u8 t = 0;
            u32 br = 0;
            file->read(getRelTypeOffset(reg, i, j), &t, 1, &br);
            if (t == 0) {
                u32 bw = 0;
                u8 friendType = 1;
                u8 feeling = 120;
                file->write(getRelTypeOffset(reg, i, j), &friendType, 1, &bw);
                file->write(getRelTypeOffset(reg, j, i), &friendType, 1, &bw);
                file->write(getRelFeelingOffset(reg, i, j), &feeling, 1, &bw);
                file->write(getRelFeelingOffset(reg, j, i), &feeling, 1, &bw);
            }
        }
    }
    if (archive && archive->get() != 0) archive->commit();
}

static inline void clearMiiRelationships(SafeFile* file, SafeArchive* archive, SaveRegion reg, int slot) {
    if (!file || !file->isOpen() || slot < 0 || slot >= MAX_MIIS) return;
    u8 zero = 0;
    u32 bw = 0;
    u8 zeros[MAX_MIIS] = {0};
    file->write(getRelFeelingOffset(reg, slot, 0), zeros, MAX_MIIS, &bw);
    file->write(getRelTypeOffset(reg, slot, 0), zeros, MAX_MIIS, &bw);
    for (int k = 0; k < MAX_MIIS; k++) {
        file->write(getRelFeelingOffset(reg, k, slot), &zero, 1, &bw);
        file->write(getRelTypeOffset(reg, k, slot), &zero, 1, &bw);
    }
    if (archive && archive->get() != 0) archive->commit();
}

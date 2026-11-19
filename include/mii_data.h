#pragma once

#include "save_io.h"
#include <vector>
#include <stdio.h>

static const int MAX_MIIS = 100;

static inline u32 getMiiStride(SaveRegion reg) {
    return (reg == REGION_JP) ? 0x590 : 0x660;
}

static inline u64 getMiiFieldOffset(SaveRegion reg, int slot, u64 usBase, u64 jpBase) {
    u64 base = (reg == REGION_JP) ? jpBase : usBase;
    return base + ((u64)slot * (u64)getMiiStride(reg));
}

struct MiiEntry {
    int slotIndex;
    bool isRegistered;
    // Profile
    char nickname[32];
    char firstName[48];
    char lastName[48];
    u8 gender;          // 0 = Male, 1 = Female (0x1C88 bit 0)
    u8 birthMonth;      // 0 = Unset, 1..12 (0x1C88 bits 4..1)
    u8 birthDay;        // 0 = Unset, 1..31 (0x1C88 bits 7..5 | 0x1C89 bits 1..0)
    u8 favColor;        // 0..11 (0x1C89 bits 5..2)
    u8 relationToYou;   // 0..11
    u8 grownUpFlag;     // 1 = Adult (can marry), 2 = Kid
    // Status
    u8 level;           // 1..99
    u8 experience;      // 0..99
    u32 pampered;       // 0..99999999
    u32 pocketMoney;    // 0..99999999
    u32 splurge;        // 0..99999999
    u8 hairColor;       // 1 = Natural, 128..191 = Spray
    u8 stomach;         // 0..100
    u8 apartmentRoom;   // 0..99
    u8 miiHome;         // 0..49 or 0xFF
    // Equipped & Gifts
    u16 equippedClothes;   // 0x1D42 US/EU/KR, 0x1D12 JP
    u16 equippedHat;       // 0x1D44 US/EU/KR, 0x1D14 JP (0xFFFF = None)
    u16 equippedInterior;  // 0x1F20 US/EU/KR, 0x1E20 JP
    u16 levelUpGifts[8];   // 0x2258..0x2266 US/EU/KR, 0x2158..0x2166 JP (0..27 or 0xFFFF)
    // Voice & Personality
    u8 voice[6];        // 0..100
    u8 personality[5];  // 1..8
    // Food Likes / Dislikes (0x2298..0x22A4 US/EU/KR, 0x2198..0x21A4 JP)
    u16 superFavFood;   // 0x2298
    u16 worstEverFood;  // 0x229A
    u16 allTimeFavFood; // 0x229C
    u16 worstFood;      // 0x229E
    u16 favFood1;       // 0x22A0
    u16 favFood2;       // 0x22A2
    u16 favFood3;       // 0x22A4
};

static inline const char* getFavColorName(u8 c) {
    static const char* NAMES[12] = {
        "Red", "Orange", "Yellow", "Lime", "Green", "Blue",
        "Cyan", "Pink", "Purple", "Brown", "White", "Black"
    };
    return (c < 12) ? NAMES[c] : "Unknown";
}

static inline const char* getMonthName(u8 m) {
    static const char* NAMES[13] = {
        "Unset", "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    return (m <= 12) ? NAMES[m] : "Unset";
}

static inline const char* getRelationName(u8 r) {
    static const char* NAMES[12] = {
        "None", "Spouse", "Parent", "Child", "Sibling", "Friend",
        "Relative", "Self", "Other", "Grandparent", "Grandchild", "Cousin"
    };
    return (r < 12) ? NAMES[r] : "Other";
}

static inline const char* getLevelUpGiftName(u16 id) {
    if (id == 0xFFFF) return "None";
    static const char* GIFTS[28] = {
        "Wii U", "3DS", "Study Kit", "Cell Phone", "Mirror", "Maracas",
        "Acoustic Guitar", "Treadmill", "Yoga Mat", "Punching Bag",
        "Frying Pan", "Sewing Machine", "Camera", "Laptop", "Dog Voucher",
        "Cat Voucher", "Swing", "Slide", "Sandcastle Kit", "Bubble Blower",
        "Hula Hoop", "Soccer Ball", "Baseball", "Tennis Racket",
        "Golf Club", "Skateboard", "Comic Book", "Novel"
    };
    return (id < 28) ? GIFTS[id] : "Gift";
}

static inline void readMiiEntry(SafeFile* file, SaveRegion reg, int slot, MiiEntry& m) {
    memset(&m, 0, sizeof(m));
    m.slotIndex = slot;
    if (!file || !file->isOpen() || slot < 0 || slot >= MAX_MIIS) return;

    u32 br = 0;
    u8 regFlag = 0;
    file->read(getMiiFieldOffset(reg, slot, 0x22C8, 0x21C8), &regFlag, 1, &br);
    m.isRegistered = (regFlag == 1);

    u16 nick16[11] = {0};
    u16 first16[17] = {0};
    u16 last16[17] = {0};
    file->read(getMiiFieldOffset(reg, slot, 0x1C8A, 0x1C5A), nick16, 20, &br);
    file->read(getMiiFieldOffset(reg, slot, 0x1CD0, 0x1CA0), first16, 32, &br);
    file->read(getMiiFieldOffset(reg, slot, 0x1CF0, 0x1CC0), last16, 32, &br);
    utf16le_to_utf8_buf(m.nickname, sizeof(m.nickname), nick16, 10);
    utf16le_to_utf8_buf(m.firstName, sizeof(m.firstName), first16, 16);
    utf16le_to_utf8_buf(m.lastName, sizeof(m.lastName), last16, 16);
    if (m.nickname[0] == '\0') snprintf(m.nickname, sizeof(m.nickname), "Mii #%d", slot + 1);

    // CFSD bitfields at 0x1C88 & 0x1C89 (US/EU/KR) / 0x1C58 & 0x1C59 (JP)
    u8 b88 = 0, b89 = 0;
    file->read(getMiiFieldOffset(reg, slot, 0x1C88, 0x1C58), &b88, 1, &br);
    file->read(getMiiFieldOffset(reg, slot, 0x1C89, 0x1C59), &b89, 1, &br);
    m.gender     = b88 & 0x01;
    m.birthMonth = (b88 >> 1) & 0x0F;
    m.birthDay   = ((b88 >> 5) & 0x07) | ((b89 & 0x03) << 3);
    m.favColor   = (b89 >> 2) & 0x0F;

    file->read(getMiiFieldOffset(reg, slot, 0x1F2D, 0x1E2D), &m.relationToYou, 1, &br);
    file->read(getMiiFieldOffset(reg, slot, 0x22AE, 0x21AE), &m.grownUpFlag, 1, &br);

    file->read(getMiiFieldOffset(reg, slot, 0x1F22, 0x1E22), &m.experience, 1, &br);
    file->read(getMiiFieldOffset(reg, slot, 0x1F23, 0x1E23), &m.level, 1, &br);
    file->read(getMiiFieldOffset(reg, slot, 0x1F24, 0x1E24), &m.pampered, 4, &br);
    file->read(getMiiFieldOffset(reg, slot, 0x1F28, 0x1E28), &m.pocketMoney, 4, &br);
    file->read(getMiiFieldOffset(reg, slot, 0x22B0, 0x21B0), &m.splurge, 4, &br);
    file->read(getMiiFieldOffset(reg, slot, 0x1D13, 0x1CE3), &m.hairColor, 1, &br);
    file->read(getMiiFieldOffset(reg, slot, 0x2293, 0x2193), &m.stomach, 1, &br);
    file->read(getMiiFieldOffset(reg, slot, 0x22A8, 0x21A8), &m.apartmentRoom, 1, &br);
    file->read(getMiiFieldOffset(reg, slot, 0x22A9, 0x21A9), &m.miiHome, 1, &br);

    file->read(getMiiFieldOffset(reg, slot, 0x1D42, 0x1D12), &m.equippedClothes, 2, &br);
    file->read(getMiiFieldOffset(reg, slot, 0x1D44, 0x1D14), &m.equippedHat, 2, &br);
    file->read(getMiiFieldOffset(reg, slot, 0x1F20, 0x1E20), &m.equippedInterior, 2, &br);
    for (int g = 0; g < 8; g++) {
        file->read(getMiiFieldOffset(reg, slot, 0x2258 + g * 2, 0x2158 + g * 2), &m.levelUpGifts[g], 2, &br);
    }

    file->read(getMiiFieldOffset(reg, slot, 0x1D4A, 0x1D1A), m.voice, 6, &br);
    file->read(getMiiFieldOffset(reg, slot, 0x1D50, 0x1D20), m.personality, 5, &br);

    file->read(getMiiFieldOffset(reg, slot, 0x2298, 0x2198), &m.superFavFood, 2, &br);
    file->read(getMiiFieldOffset(reg, slot, 0x229A, 0x219A), &m.worstEverFood, 2, &br);
    file->read(getMiiFieldOffset(reg, slot, 0x229C, 0x219C), &m.allTimeFavFood, 2, &br);
    file->read(getMiiFieldOffset(reg, slot, 0x229E, 0x219E), &m.worstFood, 2, &br);
    file->read(getMiiFieldOffset(reg, slot, 0x22A0, 0x21A0), &m.favFood1, 2, &br);
    file->read(getMiiFieldOffset(reg, slot, 0x22A2, 0x21A2), &m.favFood2, 2, &br);
    file->read(getMiiFieldOffset(reg, slot, 0x22A4, 0x21A4), &m.favFood3, 2, &br);
}

static inline void scanRegisteredMiis(SafeFile* file, SaveRegion reg, std::vector<MiiEntry>& outList) {
    outList.clear();
    if (!file || !file->isOpen()) return;
    for (int i = 0; i < MAX_MIIS; i++) {
        u8 regFlag = 0;
        u32 br = 0;
        file->read(getMiiFieldOffset(reg, i, 0x22C8, 0x21C8), &regFlag, 1, &br);
        if (regFlag == 1) {
            MiiEntry entry;
            readMiiEntry(file, reg, i, entry);
            outList.push_back(entry);
        }
    }
}

static inline void writeMiiEntry(SafeFile* file, SafeArchive* archive, SaveRegion reg, const MiiEntry& in) {
    if (!file || !file->isOpen() || in.slotIndex < 0 || in.slotIndex >= MAX_MIIS) return;
    int slot = in.slotIndex;
    u32 bw = 0;

    u8 gender = (in.gender > 1) ? 1 : in.gender;
    u8 bMonth = (in.birthMonth > 12) ? 12 : in.birthMonth;
    u8 bDay   = (in.birthDay > 31) ? 31 : in.birthDay;
    u8 favCol = (in.favColor > 11) ? 11 : in.favColor;
    u8 relYou = (in.relationToYou > 11) ? 0 : in.relationToYou;
    u8 grown  = (in.grownUpFlag == 2) ? 2 : 1;
    u8 xp     = (in.experience > 99) ? 99 : in.experience;
    u8 lv     = (in.level < 1) ? 1 : ((in.level > 99) ? 99 : in.level);
    u32 pamp  = (in.pampered > 99999999) ? 99999999 : in.pampered;
    u32 pock  = (in.pocketMoney > 99999999) ? 99999999 : in.pocketMoney;
    u32 splr  = (in.splurge > 99999999) ? 99999999 : in.splurge;
    u8 stom   = (in.stomach > 100) ? 100 : in.stomach;
    u8 room   = (in.apartmentRoom > 99) ? 99 : in.apartmentRoom;

    u16 nick16[10] = {0};
    u16 first16[16] = {0};
    u16 last16[16] = {0};
    utf8_to_utf16le_buf(nick16, 10, in.nickname);
    utf8_to_utf16le_buf(first16, 16, in.firstName);
    utf8_to_utf16le_buf(last16, 16, in.lastName);

    file->write(getMiiFieldOffset(reg, slot, 0x1C8A, 0x1C5A), nick16, 20, &bw);
    file->write(getMiiFieldOffset(reg, slot, 0x1CD0, 0x1CA0), first16, 32, &bw);
    file->write(getMiiFieldOffset(reg, slot, 0x1CF0, 0x1CC0), last16, 32, &bw);

    // Read-modify-write CFSD bitfields (preserving bits 7..6 of 0x1C89)
    u8 b88 = 0, b89 = 0;
    u32 br = 0;
    file->read(getMiiFieldOffset(reg, slot, 0x1C88, 0x1C58), &b88, 1, &br);
    file->read(getMiiFieldOffset(reg, slot, 0x1C89, 0x1C59), &b89, 1, &br);
    b88 = (u8)((gender & 0x01) | ((bMonth & 0x0F) << 1) | ((bDay & 0x07) << 5));
    b89 = (u8)((b89 & 0xC0) | ((bDay >> 3) & 0x03) | ((favCol & 0x0F) << 2));

    file->write(getMiiFieldOffset(reg, slot, 0x1C88, 0x1C58), &b88, 1, &bw);
    file->write(getMiiFieldOffset(reg, slot, 0x1C89, 0x1C59), &b89, 1, &bw);
    file->write(getMiiFieldOffset(reg, slot, 0x1F2D, 0x1E2D), &relYou, 1, &bw);
    file->write(getMiiFieldOffset(reg, slot, 0x22AE, 0x21AE), &grown, 1, &bw);

    file->write(getMiiFieldOffset(reg, slot, 0x1F22, 0x1E22), &xp, 1, &bw);
    file->write(getMiiFieldOffset(reg, slot, 0x1F23, 0x1E23), &lv, 1, &bw);
    file->write(getMiiFieldOffset(reg, slot, 0x1F24, 0x1E24), &pamp, 4, &bw);
    file->write(getMiiFieldOffset(reg, slot, 0x1F28, 0x1E28), &pock, 4, &bw);
    file->write(getMiiFieldOffset(reg, slot, 0x22B0, 0x21B0), &splr, 4, &bw);
    file->write(getMiiFieldOffset(reg, slot, 0x1D13, 0x1CE3), &in.hairColor, 1, &bw);
    file->write(getMiiFieldOffset(reg, slot, 0x2293, 0x2193), &stom, 1, &bw);
    file->write(getMiiFieldOffset(reg, slot, 0x22A8, 0x21A8), &room, 1, &bw);
    file->write(getMiiFieldOffset(reg, slot, 0x22A9, 0x21A9), &in.miiHome, 1, &bw);

    file->write(getMiiFieldOffset(reg, slot, 0x1D42, 0x1D12), &in.equippedClothes, 2, &bw);
    file->write(getMiiFieldOffset(reg, slot, 0x1D44, 0x1D14), &in.equippedHat, 2, &bw);
    file->write(getMiiFieldOffset(reg, slot, 0x1F20, 0x1E20), &in.equippedInterior, 2, &bw);
    for (int g = 0; g < 8; g++) {
        file->write(getMiiFieldOffset(reg, slot, 0x2258 + g * 2, 0x2158 + g * 2), &in.levelUpGifts[g], 2, &bw);
    }

    u8 voiceClamped[6];
    for (int v = 0; v < 6; v++) voiceClamped[v] = (in.voice[v] > 100) ? 100 : in.voice[v];
    u8 persClamped[5];
    for (int p = 0; p < 5; p++) persClamped[p] = (in.personality[p] < 1) ? 1 : ((in.personality[p] > 8) ? 8 : in.personality[p]);
    file->write(getMiiFieldOffset(reg, slot, 0x1D4A, 0x1D1A), voiceClamped, 6, &bw);
    file->write(getMiiFieldOffset(reg, slot, 0x1D50, 0x1D20), persClamped, 5, &bw);

    file->write(getMiiFieldOffset(reg, slot, 0x2298, 0x2198), &in.superFavFood, 2, &bw);
    file->write(getMiiFieldOffset(reg, slot, 0x229A, 0x219A), &in.worstEverFood, 2, &bw);
    file->write(getMiiFieldOffset(reg, slot, 0x229C, 0x219C), &in.allTimeFavFood, 2, &bw);
    file->write(getMiiFieldOffset(reg, slot, 0x229E, 0x219E), &in.worstFood, 2, &bw);
    file->write(getMiiFieldOffset(reg, slot, 0x22A0, 0x21A0), &in.favFood1, 2, &bw);
    file->write(getMiiFieldOffset(reg, slot, 0x22A2, 0x21A2), &in.favFood2, 2, &bw);
    file->write(getMiiFieldOffset(reg, slot, 0x22A4, 0x21A4), &in.favFood3, 2, &bw);

    if (archive && archive->get() != 0) archive->commit();
}

static inline u64 getMiiTableBase(SaveRegion reg) {
    return (reg == REGION_JP) ? 0x1C40 : 0x1C70;
}

static inline bool exportMiiTlmii(SafeFile* file, SaveRegion reg, int slot, const char* path) {
    if (!file || !file->isOpen() || slot < 0 || slot >= MAX_MIIS) return false;
    u32 stride = getMiiStride(reg);
    std::vector<u8> buf(stride, 0);
    u32 br = 0;
    if (R_FAILED(file->read(getMiiTableBase(reg) + (u64)slot * stride, buf.data(), stride, &br)) || br != stride) return false;
#ifndef HOST_TEST
    mkdir("sdmc:/3ds", 0777);
    mkdir("sdmc:/3ds/tlse", 0777);
    mkdir("sdmc:/3ds/tlse/miis", 0777);
#endif
    FILE* out = fopen(path, "wb");
    if (!out) return false;
    fwrite(buf.data(), 1, stride, out);
    fclose(out);
    return true;
}

static inline bool importMiiTlmii(SafeFile* file, SafeArchive* archive, SaveRegion reg, int slot, const char* path) {
    if (!file || !file->isOpen() || slot < 0 || slot >= MAX_MIIS) return false;
    u32 stride = getMiiStride(reg);
    FILE* in = fopen(path, "rb");
    if (!in) return false;
    std::vector<u8> buf(stride, 0);
    size_t r = fread(buf.data(), 1, stride, in);
    fclose(in);
    if (r != stride) return false;
    u32 bw = 0;
    file->write(getMiiTableBase(reg) + (u64)slot * stride, buf.data(), stride, &bw);
    if (archive && archive->get() != 0) archive->commit();
    return true;
}

static inline bool exportAllMiisTlamii(SafeFile* file, SaveRegion reg, const char* path) {
    if (!file || !file->isOpen() || !path) return false;
    u32 stride = getMiiStride(reg);
    size_t totalBytes = (size_t)MAX_MIIS * (size_t)stride;
    std::vector<u8> buf(totalBytes, 0);
    u32 br = 0;
    if (R_FAILED(file->read(getMiiTableBase(reg), buf.data(), (u32)totalBytes, &br)) || br != totalBytes) return false;
#ifndef HOST_TEST
    mkdir("sdmc:/3ds", 0777);
    mkdir("sdmc:/3ds/tlse", 0777);
    mkdir("sdmc:/3ds/tlse/miis", 0777);
#endif
    FILE* out = fopen(path, "wb");
    if (!out) return false;
    size_t written = fwrite(buf.data(), 1, totalBytes, out);
    fclose(out);
    return (written == totalBytes);
}

static inline bool importAllMiisTlamii(SafeFile* file, SafeArchive* archive, SaveRegion reg, const char* path) {
    if (!file || !file->isOpen() || !path) return false;
    u32 stride = getMiiStride(reg);
    size_t totalBytes = (size_t)MAX_MIIS * (size_t)stride;
    FILE* in = fopen(path, "rb");
    if (!in) return false;
    fseek(in, 0, SEEK_END);
    long fileSize = ftell(in);
    fseek(in, 0, SEEK_SET);
    if (fileSize != (long)totalBytes) {
        fclose(in);
        return false;
    }
    std::vector<u8> buf(totalBytes, 0);
    size_t r = fread(buf.data(), 1, totalBytes, in);
    fclose(in);
    if (r != totalBytes) return false;
    u32 bw = 0;
    file->write(getMiiTableBase(reg), buf.data(), (u32)totalBytes, &bw);
    if (archive && archive->get() != 0) archive->commit();
    return (bw == totalBytes);
}

static inline int findFirstEmptyMiiSlot(SafeFile* file, SaveRegion reg) {
    if (!file || !file->isOpen()) return -1;
    for (int i = 0; i < MAX_MIIS; i++) {
        u8 regFlag = 0;
        u32 br = 0;
        file->read(getMiiFieldOffset(reg, i, 0x22C8, 0x21C8), &regFlag, 1, &br);
        if (regFlag != 1) return i;
    }
    return -1;
}

static inline int cloneMiiSlot(SafeFile* file, SafeArchive* archive, SaveRegion reg, int srcSlot) {
    if (!file || !file->isOpen() || srcSlot < 0 || srcSlot >= MAX_MIIS) return -1;
    u8 srcReg = 0;
    u32 br = 0;
    file->read(getMiiFieldOffset(reg, srcSlot, 0x22C8, 0x21C8), &srcReg, 1, &br);
    if (srcReg != 1) return -1;

    int dstSlot = findFirstEmptyMiiSlot(file, reg);
    if (dstSlot < 0) return -1;

    u32 stride = getMiiStride(reg);
    std::vector<u8> buf(stride, 0);
    u64 base = getMiiTableBase(reg);
    if (R_FAILED(file->read(base + (u64)srcSlot * stride, buf.data(), stride, &br)) || br != stride) return -1;

    u32 bw = 0;
    file->write(base + (u64)dstSlot * stride, buf.data(), stride, &bw);

    // Assign a free apartment room (0..99) so the clone does not collide with another Mii
    bool usedRooms[100] = {false};
    for (int i = 0; i < MAX_MIIS; i++) {
        if (i == dstSlot) continue;
        u8 rf = 0;
        file->read(getMiiFieldOffset(reg, i, 0x22C8, 0x21C8), &rf, 1, &br);
        if (rf == 1) {
            u8 rm = 0;
            file->read(getMiiFieldOffset(reg, i, 0x22A8, 0x21A8), &rm, 1, &br);
            if (rm < 100) usedRooms[rm] = true;
        }
    }
    u8 freeRoom = (u8)dstSlot;
    for (int r = 0; r < 100; r++) {
        if (!usedRooms[r]) { freeRoom = (u8)r; break; }
    }
    file->write(getMiiFieldOffset(reg, dstSlot, 0x22A8, 0x21A8), &freeRoom, 1, &bw);
    u8 regOne = 1;
    file->write(getMiiFieldOffset(reg, dstSlot, 0x22C8, 0x21C8), &regOne, 1, &bw);

    if (archive && archive->get() != 0) archive->commit();
    return dstSlot;
}

static inline bool deleteMiiSlot(SafeFile* file, SafeArchive* archive, SaveRegion reg, int slot) {
    if (!file || !file->isOpen() || slot < 0 || slot >= MAX_MIIS) return false;
    u8 regFlag = 0;
    u32 br = 0;
    file->read(getMiiFieldOffset(reg, slot, 0x22C8, 0x21C8), &regFlag, 1, &br);
    if (regFlag != 1) return false;

    u32 stride = getMiiStride(reg);
    std::vector<u8> zeros(stride, 0);
    u32 bw = 0;
    file->write(getMiiTableBase(reg) + (u64)slot * stride, zeros.data(), stride, &bw);
    if (archive && archive->get() != 0) archive->commit();
    return (bw == stride);
}

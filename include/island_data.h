#pragma once

#include "save_io.h"
#include "mii_data.h"
#include "relationships_data.h"
#include <vector>

// ── #45: Island Buildings (24) & Ranking Boards (10) ─────────────────

static const int NUM_ISLAND_BUILDINGS = 24;
static const int NUM_RANKING_BOARDS   = 10;

static inline const char* getBuildingName(int idx) {
    static const char* NAMES[24] = {
        "Mii Apartments", "Town Hall", "Mii News", "Ranking Board",
        "Clothing Shop", "Hat Shop", "Food Mart", "Interior Shop",
        "Pawn Shop", "Import Wear", "Fountain", "Mii Homes",
        "Port", "Campground", "Concert Hall", "Compatibility Tester",
        "Photo Studio", "Beach", "Observation Tower", "Park",
        "Cafe", "Amusement Park", "Travel Agency", "Hat/SpotPass Network"
    };
    return (idx >= 0 && idx < 24) ? NAMES[idx] : "Unknown";
}

static inline const char* getRankingBoardName(int idx) {
    static const char* NAMES[10] = {
        "Vitality", "Popularity", "Friendship", "Romance", "Boy Appeal",
        "Girl Appeal", "Pampered", "Splurge", "Travelers", "Island"
    };
    return (idx >= 0 && idx < 10) ? NAMES[idx] : "Unknown";
}

static inline u64 getBuildingsBaseOffset(SaveRegion reg) {
    switch (reg) {
        case REGION_JP: return 0x14BCF0;
        case REGION_KR: return 0x1EFFD0;
        default:        return 0x1E4C20;
    }
}

static inline u64 getRankingsBaseOffset(SaveRegion reg) {
    switch (reg) {
        case REGION_JP: return 0x14BD0E;
        case REGION_KR: return 0x1EFFEE;
        default:        return 0x1E4C3E;
    }
}

static inline u64 getApartmentRenovOffset(SaveRegion reg) {
    switch (reg) {
        case REGION_JP: return 0x14BD49;
        case REGION_KR: return 0x1F0029;
        default:        return 0x1E4C79;
    }
}

struct IslandConfigData {
    u8 buildings[24];
    u8 rankings[10];
    u8 apartmentRenov;
};

static inline void readIslandConfig(SafeFile* file, SaveRegion reg, IslandConfigData& out) {
    memset(&out, 0, sizeof(out));
    if (!file || !file->isOpen()) return;
    u32 br = 0;
    file->read(getBuildingsBaseOffset(reg), out.buildings, 24, &br);
    file->read(getRankingsBaseOffset(reg), out.rankings, 10, &br);
    file->read(getApartmentRenovOffset(reg), &out.apartmentRenov, 1, &br);
}

static inline void writeIslandConfig(SafeFile* file, SafeArchive* archive, SaveRegion reg, const IslandConfigData& in) {
    if (!file || !file->isOpen()) return;
    u32 bw = 0;
    file->write(getBuildingsBaseOffset(reg), in.buildings, 24, &bw);
    file->write(getRankingsBaseOffset(reg), in.rankings, 10, &bw);
    file->write(getApartmentRenovOffset(reg), &in.apartmentRenov, 1, &bw);
    if (archive && archive->get() != 0) archive->commit();
}

static inline void unlockAllBuildingsAndRankings(SafeFile* file, SafeArchive* archive, SaveRegion reg) {
    IslandConfigData cfg;
    readIslandConfig(file, reg, cfg);
    for (int i = 0; i < 24; i++) {
        if (i == 0) cfg.buildings[i] = (cfg.buildings[i] < 6) ? 6 : cfg.buildings[i]; // Mii Apartments
        else if (i == 1) cfg.buildings[i] = (cfg.buildings[i] < 3) ? 3 : cfg.buildings[i]; // Town Hall
        else cfg.buildings[i] = (cfg.buildings[i] == 0) ? 2 : cfg.buildings[i];
    }
    for (int i = 0; i < 10; i++) {
        if (cfg.rankings[i] == 0) cfg.rankings[i] = 2;
    }
    writeIslandConfig(file, archive, reg, cfg);
}

// ── #46: Island Info, Statistics & Settings ──────────────────────────

struct IslandInfoData {
    char islandName[32];
    char islandPronun[48];
    u16 streetPassEncounters;
    u16 travelersReceived;
    u16 weddings;
    u16 childrenBorn;
    u16 travelersSent;
    u16 problemsSolved;
    u8 exportColorBit;   // 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80
    u8 streetPassActive; // 0/1
    u8 babiesAllowed;    // 0/1
    u8 spotPassActive;   // 0/1
};

static inline void readIslandInfo(SafeFile* file, SaveRegion reg, IslandInfoData& out) {
    memset(&out, 0, sizeof(out));
    if (!file || !file->isOpen()) return;
    u32 br = 0;
    u64 base = (reg == REGION_JP) ? 0x14BCAC : ((reg == REGION_KR) ? 0x1EFF6C : 0x1E4BBC);

    file->read(base + 0x00, &out.streetPassEncounters, 2, &br);
    file->read(base + 0x02, &out.travelersReceived, 2, &br);
    file->read(base + 0x04, &out.weddings, 2, &br);
    file->read(base + 0x06, &out.childrenBorn, 2, &br);
    file->read(base + 0x08, &out.travelersSent, 2, &br);
    file->read(base + 0x0A, &out.problemsSolved, 2, &br);

    u16 name16[11] = {0};
    file->read(base + 0x10, name16, 20, &br);
    utf16le_to_utf8_buf(out.islandName, sizeof(out.islandName), name16, 10);

    if (reg != REGION_JP) {
        u16 pron16[17] = {0};
        file->read(base + 0x3A, pron16, 32, &br);
        utf16le_to_utf8_buf(out.islandPronun, sizeof(out.islandPronun), pron16, 16);
    }

    u64 colorOff = (reg == REGION_JP) ? 0x14BCD8 : ((reg == REGION_KR) ? 0x1EFF9A : 0x1E4BEA);
    u64 spOff    = (reg == REGION_JP) ? 0x14BCD0 : ((reg == REGION_KR) ? 0x1EFF92 : 0x1E4BE2);
    u64 babyOff  = (reg == REGION_JP) ? 0x14BD40 : ((reg == REGION_KR) ? 0x1F0020 : 0x1E4C70);
    u64 spotOff  = (reg == REGION_JP) ? 0x14BD41 : ((reg == REGION_KR) ? 0x1F0021 : 0x1E4C71);

    file->read(colorOff, &out.exportColorBit, 1, &br);
    file->read(spOff, &out.streetPassActive, 1, &br);
    file->read(babyOff, &out.babiesAllowed, 1, &br);
    file->read(spotOff, &out.spotPassActive, 1, &br);
}

static inline void writeIslandInfo(SafeFile* file, SafeArchive* archive, SaveRegion reg, const IslandInfoData& in) {
    if (!file || !file->isOpen()) return;
    u32 bw = 0;
    u64 base = (reg == REGION_JP) ? 0x14BCAC : ((reg == REGION_KR) ? 0x1EFF6C : 0x1E4BBC);

    file->write(base + 0x00, &in.streetPassEncounters, 2, &bw);
    file->write(base + 0x02, &in.travelersReceived, 2, &bw);
    file->write(base + 0x04, &in.weddings, 2, &bw);
    file->write(base + 0x06, &in.childrenBorn, 2, &bw);
    file->write(base + 0x08, &in.travelersSent, 2, &bw);
    file->write(base + 0x0A, &in.problemsSolved, 2, &bw);

    u16 name16[10] = {0};
    utf8_to_utf16le_buf(name16, 10, in.islandName);
    file->write(base + 0x10, name16, 20, &bw);

    if (reg != REGION_JP) {
        u16 pron16[16] = {0};
        utf8_to_utf16le_buf(pron16, 16, in.islandPronun);
        file->write(base + 0x3A, pron16, 32, &bw);
    }

    u64 colorOff = (reg == REGION_JP) ? 0x14BCD8 : ((reg == REGION_KR) ? 0x1EFF9A : 0x1E4BEA);
    u64 spOff    = (reg == REGION_JP) ? 0x14BCD0 : ((reg == REGION_KR) ? 0x1EFF92 : 0x1E4BE2);
    u64 babyOff  = (reg == REGION_JP) ? 0x14BD40 : ((reg == REGION_KR) ? 0x1F0020 : 0x1E4C70);
    u64 spotOff  = (reg == REGION_JP) ? 0x14BD41 : ((reg == REGION_KR) ? 0x1F0021 : 0x1E4C71);

    file->write(colorOff, &in.exportColorBit, 1, &bw);
    file->write(spOff, &in.streetPassActive, 1, &bw);
    file->write(babyOff, &in.babiesAllowed, 1, &bw);
    file->write(spotOff, &in.spotPassActive, 1, &bw);

    if (archive && archive->get() != 0) archive->commit();
}

// ── #47: Active Mii Interactions / Window Bubbles ────────────────────

struct MiiInteractionData {
    u8 emotion;      // 0=None, 1=Orange(Problem), 2=Pink(Love), 3=Blue(Sad/Fight), 4=Green(Play)
    u32 endTime;
    u16 typeId;      // 0x0000=None, 0x100..0x18F
    u16 target1;
    u16 target2;
};

static inline const char* getEmotionName(u8 e) {
    static const char* NAMES[5] = { "None (Normal)", "Orange (Problem)", "Pink (Love)", "Blue (Sad/Fight)", "Green (Play)" };
    return (e <= 4) ? NAMES[e] : "Unknown";
}

static inline void readMiiInteraction(SafeFile* file, SaveRegion reg, int slot, MiiInteractionData& out) {
    memset(&out, 0, sizeof(out));
    if (!file || !file->isOpen() || slot < 0 || slot >= MAX_MIIS) return;
    u32 br = 0;
    file->read(getMiiFieldOffset(reg, slot, 0x1F2C, 0x1E2C), &out.emotion, 1, &br);
    u64 base = ((reg == REGION_JP) ? 0x24948 : 0x29AB8) + ((u64)slot * 0x100);
    file->read(base + 0x00, &out.endTime, 4, &br);
    file->read(base + 0x08, &out.typeId, 2, &br);
    file->read(base + 0x0C, &out.target1, 2, &br);
    file->read(base + 0x0E, &out.target2, 2, &br);
}

static inline void writeMiiInteraction(SafeFile* file, SafeArchive* archive, SaveRegion reg, int slot, const MiiInteractionData& in) {
    if (!file || !file->isOpen() || slot < 0 || slot >= MAX_MIIS) return;
    u32 bw = 0;
    u8 emo = (in.emotion > 4) ? 0 : in.emotion;
    file->write(getMiiFieldOffset(reg, slot, 0x1F2C, 0x1E2C), &emo, 1, &bw);
    u64 base = ((reg == REGION_JP) ? 0x24948 : 0x29AB8) + ((u64)slot * 0x100);
    file->write(base + 0x00, &in.endTime, 4, &bw);
    file->write(base + 0x08, &in.typeId, 2, &bw);
    file->write(base + 0x0C, &in.target1, 2, &bw);
    file->write(base + 0x0E, &in.target2, 2, &bw);
    if (archive && archive->get() != 0) archive->commit();
}

// ── #48: Quick Extras & Save Repair Suite ────────────────────────────

static inline int setAllMiisStomach(SafeFile* file, SafeArchive* archive, SaveRegion reg, u8 fullness) {
    if (!file || !file->isOpen()) return 0;
    u8 val = (fullness > 100) ? 100 : fullness;
    int count = 0;
    for (int i = 0; i < MAX_MIIS; i++) {
        u8 regFlag = 0;
        u32 br = 0;
        file->read(getMiiFieldOffset(reg, i, 0x22C8, 0x21C8), &regFlag, 1, &br);
        if (regFlag == 1) {
            u32 bw = 0;
            file->write(getMiiFieldOffset(reg, i, 0x2293, 0x2193), &val, 1, &bw);
            count++;
        }
    }
    if (archive && archive->get() != 0) archive->commit();
    return count;
}

static inline int maxAllMiisLevelAndMoney(SafeFile* file, SafeArchive* archive, SaveRegion reg) {
    if (!file || !file->isOpen()) return 0;
    int count = 0;
    u8 lv = 99;
    u32 pocket = 99999999;
    for (int i = 0; i < MAX_MIIS; i++) {
        u8 regFlag = 0;
        u32 br = 0;
        file->read(getMiiFieldOffset(reg, i, 0x22C8, 0x21C8), &regFlag, 1, &br);
        if (regFlag == 1) {
            u32 bw = 0;
            file->write(getMiiFieldOffset(reg, i, 0x1F23, 0x1E23), &lv, 1, &bw);
            file->write(getMiiFieldOffset(reg, i, 0x1F28, 0x1E28), &pocket, 4, &bw);
            count++;
        }
    }
    if (archive && archive->get() != 0) archive->commit();
    return count;
}

static inline void resetAllApartmentRooms(SafeFile* file, SafeArchive* archive, SaveRegion reg) {
    if (!file || !file->isOpen()) return;
    for (int i = 0; i < MAX_MIIS; i++) {
        u8 room = (u8)i;
        u32 bw = 0;
        file->write(getMiiFieldOffset(reg, i, 0x22A8, 0x21A8), &room, 1, &bw);
    }
    if (archive && archive->get() != 0) archive->commit();
}

static inline void resetMiiNewsFlashes(SafeFile* file, SafeArchive* archive, SaveRegion reg) {
    if (!file || !file->isOpen()) return;
    u64 off = (reg == REGION_JP) ? 0x14BD18 : ((reg == REGION_KR) ? 0x1EFFF8 : 0x1E4C48);
    u8 zeros[5] = {0};
    u32 bw = 0;
    file->write(off, zeros, 5, &bw);
    if (archive && archive->get() != 0) archive->commit();
}

static inline int repairSaveInteractions(SafeFile* file, SafeArchive* archive, SaveRegion reg) {
    if (!file || !file->isOpen()) return 0;
    int cleared = 0;
    u8 zeros[16] = {0};
    for (int i = 0; i < MAX_MIIS; i++) {
        u64 base = ((reg == REGION_JP) ? 0x24948 : 0x29AB8) + ((u64)i * 0x100);
        u16 typeId = 0;
        u32 br = 0;
        file->read(base + 0x08, &typeId, 2, &br);
        if (typeId != 0 && (typeId < 0x100 || typeId > 0x18F)) {
            u32 bw = 0;
            u8 emo = 0;
            file->write(getMiiFieldOffset(reg, i, 0x1F2C, 0x1E2C), &emo, 1, &bw);
            file->write(base, zeros, 16, &bw);
            cleared++;
        }
    }
    if (archive && archive->get() != 0) archive->commit();
    return cleared;
}

// ── #49: StreetPass Travelers (50) & Concert Hall Songs ──────────────

static const int MAX_TRAVELERS = 50;

struct TravelerSummary {
    int slot;
    char nickname[32];
    bool present;
};

static inline u64 getTravelerBase(SaveRegion reg, int slot) {
    u64 base = (reg == REGION_JP) ? 0x2C740 : 0x318B0;
    u64 stride = (reg == REGION_JP) ? 0xB58 : 0xC28;
    return base + ((u64)slot * stride);
}

static inline void readTravelerSummary(SafeFile* file, SaveRegion reg, int slot, TravelerSummary& out) {
    memset(&out, 0, sizeof(out));
    out.slot = slot;
    if (!file || !file->isOpen() || slot < 0 || slot >= MAX_TRAVELERS) return;
    u16 nick16[11] = {0};
    u32 br = 0;
    file->read(getTravelerBase(reg, slot), nick16, 20, &br);
    utf16le_to_utf8_buf(out.nickname, sizeof(out.nickname), nick16, 10);
    out.present = (out.nickname[0] != '\0');
    if (!out.present) snprintf(out.nickname, sizeof(out.nickname), "(Empty Slot %d)", slot + 1);
}

static inline void clearTravelerSlot(SafeFile* file, SafeArchive* archive, SaveRegion reg, int slot) {
    if (!file || !file->isOpen() || slot < 0 || slot >= MAX_TRAVELERS) return;
    u64 stride = (reg == REGION_JP) ? 0xB58 : 0xC28;
    std::vector<u8> zeros(stride, 0);
    u32 bw = 0;
    file->write(getTravelerBase(reg, slot), zeros.data(), (u32)stride, &bw);
    if (archive && archive->get() != 0) archive->commit();
}

struct ConcertSongData {
    char title[64];
    u8 genre; // 0..7
    char lines[13][96];
};

static inline const char* getConcertGenreName(u8 g) {
    static const char* GENRES[8] = {
        "Opera", "Metal", "Rap", "Pop", "Rock", "Techno", "Musical", "Ballad"
    };
    return (g < 8) ? GENRES[g] : "Pop";
}

static inline void readConcertSong(SafeFile* file, int songIdx, ConcertSongData& out) {
    memset(&out, 0, sizeof(out));
    if (!file || !file->isOpen() || songIdx < 0 || songIdx >= 8) return;
    u64 songBase = 0x17B6E4 + ((u64)songIdx * 0x5A0);
    u32 br = 0;
    u16 title16[33] = {0};
    file->read(songBase + 0x548, title16, 64, &br);
    utf16le_to_utf8_buf(out.title, sizeof(out.title), title16, 32);
    if (out.title[0] == '\0') snprintf(out.title, sizeof(out.title), "Song #%d", songIdx + 1);
    file->read(songBase + 0x58D, &out.genre, 1, &br);
    for (int l = 0; l < 13; l++) {
        u16 line16[49] = {0};
        file->read(songBase + 0x06 + (u64)l * 0x68, line16, 96, &br);
        utf16le_to_utf8_buf(out.lines[l], sizeof(out.lines[l]), line16, 48);
    }
}

static inline void writeConcertSong(SafeFile* file, SafeArchive* archive, int songIdx, const ConcertSongData& in) {
    if (!file || !file->isOpen() || songIdx < 0 || songIdx >= 8) return;
    u64 songBase = 0x17B6E4 + ((u64)songIdx * 0x5A0);
    u32 bw = 0;
    u16 title16[32] = {0};
    utf8_to_utf16le_buf(title16, 32, in.title);
    file->write(songBase + 0x548, title16, 64, &bw);
    u8 genre = (in.genre > 7) ? 0 : in.genre;
    file->write(songBase + 0x58D, &genre, 1, &bw);
    for (int l = 0; l < 13; l++) {
        u16 line16[48] = {0};
        utf8_to_utf16le_buf(line16, 48, in.lines[l]);
        file->write(songBase + 0x06 + (u64)l * 0x68, line16, 96, &bw);
    }
    if (archive && archive->get() != 0) archive->commit();
}

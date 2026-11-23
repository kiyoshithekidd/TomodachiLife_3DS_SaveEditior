#pragma once

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/stat.h>
#include <time.h>

#ifdef HOST_TEST
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t Result;
typedef uint32_t Handle;
typedef uint64_t FS_Archive;
typedef struct { int dummy; } FS_Path;
#define R_FAILED(res) ((res) < 0)
#else
#include <3ds.h>
#endif

enum SaveRegion {
    REGION_US = 0,
    REGION_EU = 1,
    REGION_JP = 2,
    REGION_KR = 3
};

static inline void utf16le_to_utf8_buf(char* outUtf8, size_t maxUtf8Bytes, const u16* inUtf16, size_t maxChars) {
    if (!outUtf8 || maxUtf8Bytes == 0) return;
    size_t pos = 0;
    for (size_t i = 0; i < maxChars && inUtf16[i] != 0; i++) {
        u16 ch = inUtf16[i];
        if (ch < 0x80) {
            if (pos + 1 >= maxUtf8Bytes) break;
            outUtf8[pos++] = (char)ch;
        } else if (ch < 0x800) {
            if (pos + 2 >= maxUtf8Bytes) break;
            outUtf8[pos++] = (char)(0xC0 | (ch >> 6));
            outUtf8[pos++] = (char)(0x80 | (ch & 0x3F));
        } else {
            if (pos + 3 >= maxUtf8Bytes) break;
            outUtf8[pos++] = (char)(0xE0 | (ch >> 12));
            outUtf8[pos++] = (char)(0x80 | ((ch >> 6) & 0x3F));
            outUtf8[pos++] = (char)(0x80 | (ch & 0x3F));
        }
    }
    outUtf8[pos] = '\0';
}

static inline void utf8_to_utf16le_buf(u16* outUtf16, size_t maxChars, const char* inUtf8) {
    if (!outUtf16 || maxChars == 0) return;
    memset(outUtf16, 0, maxChars * sizeof(u16));
    if (!inUtf8) return;
    size_t outIdx = 0;
    const unsigned char* s = (const unsigned char*)inUtf8;
    while (*s && outIdx < maxChars) {
        u32 cp = 0;
        if (*s < 0x80) {
            cp = *s++;
        } else if ((*s & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80) {
            cp = ((s[0] & 0x1F) << 6) | (s[1] & 0x3F);
            s += 2;
        } else if ((*s & 0xF0) == 0xE0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80) {
            cp = ((s[0] & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
            s += 3;
        } else {
            s++;
            continue;
        }
        if (cp <= 0xFFFF) {
            outUtf16[outIdx++] = (u16)cp;
        }
    }
}

class SafeArchive {
private:
    FS_Archive mArchive = 0;
public:
#ifndef HOST_TEST
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
    Result commit() {
        if (!mArchive) return -1;
        return FSUSER_ControlArchive(mArchive, ARCHIVE_ACTION_COMMIT_SAVE_DATA, NULL, 0, NULL, 0);
    }
#else
    SafeArchive() {}
    Result commit() { return 0; }
#endif
    FS_Archive get() const { return mArchive; }
    SafeArchive(const SafeArchive&) = delete;
    SafeArchive& operator=(const SafeArchive&) = delete;
};

static bool g_backupCreated = false;
static char g_backupStatus[64] = "Standby (Before 1st Edit)";
typedef void (*BackupProgressFn)();
static BackupProgressFn g_onBeforeBackup = nullptr;

class SafeFile {
private:
    Handle mHandle = 0;
    FILE* mStdFile = NULL;

    void ensureBackupBeforeWrite() {
        if (g_backupCreated) return;
        g_backupCreated = true;
        if (g_onBeforeBackup) g_onBeforeBackup();

#ifndef HOST_TEST
        mkdir("sdmc:/3ds", 0777);
        mkdir("sdmc:/3ds/tlse", 0777);
        mkdir("sdmc:/3ds/tlse/backups", 0777);
        const char* backupDir = "sdmc:/3ds/tlse/backups";
#else
        const char* backupDir = "/tmp";
#endif
        char latestPath[256];
        snprintf(latestPath, sizeof(latestPath), "%s/savedataArc.bak", backupDir);
        FILE* outLatest = fopen(latestPath, "wb");
        if (!outLatest) {
            outLatest = fopen("savedataArc.bak", "wb");
        }

        // Preserve an immutable initial backup on the very first edit
        char initialPath[256];
        snprintf(initialPath, sizeof(initialPath), "%s/savedataArc_initial.bak", backupDir);
        FILE* outInitial = nullptr;
        FILE* checkInit = fopen(initialPath, "rb");
        if (checkInit) {
            fclose(checkInit);
        } else {
            outInitial = fopen(initialPath, "wb");
        }

        // Also keep a timestamped per-session backup so history is never lost
        FILE* outDated = nullptr;
        time_t now = time(nullptr);
        struct tm* tmNow = localtime(&now);
        if (tmNow) {
            char datedPath[256];
            snprintf(datedPath, sizeof(datedPath), "%s/savedataArc_%04d%02d%02d_%02d%02d%02d.bak",
                     backupDir,
                     tmNow->tm_year + 1900, tmNow->tm_mon + 1, tmNow->tm_mday,
                     tmNow->tm_hour, tmNow->tm_min, tmNow->tm_sec);
            outDated = fopen(datedPath, "wb");
        }

        if (!outLatest && !outInitial && !outDated) {
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
            if (outLatest)  fwrite(chunk, 1, bytesRead, outLatest);
            if (outInitial) fwrite(chunk, 1, bytesRead, outInitial);
            if (outDated)   fwrite(chunk, 1, bytesRead, outDated);
            offset += bytesRead;
            if (bytesRead < sizeof(chunk)) break;
        }
        if (outLatest)  fclose(outLatest);
        if (outInitial) fclose(outInitial);
        if (outDated)   fclose(outDated);

        if (outDated || outInitial) {
            snprintf(g_backupStatus, sizeof(g_backupStatus), "Saved (.bak + History)");
        } else {
            snprintf(g_backupStatus, sizeof(g_backupStatus), "Saved (savedataArc.bak)");
        }
    }

public:
#ifndef HOST_TEST
    SafeFile(FS_Archive archive, FS_Path path, u32 flags, u32 attributes) {
        if (R_FAILED(FSUSER_OpenFile(&mHandle, archive, path, flags, attributes))) {
            mHandle = 0;
        }
    }
#endif
    explicit SafeFile(const char* sdFilePath) {
        mStdFile = fopen(sdFilePath, "r+b");
    }

    ~SafeFile() {
#ifndef HOST_TEST
        if (mHandle) {
            FSFILE_Close(mHandle);
        }
#endif
        if (mStdFile) {
            fflush(mStdFile);
            fclose(mStdFile);
        }
    }

    bool isOpen() const {
        return (mHandle != 0) || (mStdFile != NULL);
    }

    u64 getSize() {
#ifndef HOST_TEST
        if (mHandle) {
            u64 sz = 0;
            if (!R_FAILED(FSFILE_GetSize(mHandle, &sz))) return sz;
            return 0;
        }
#endif
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
#ifndef HOST_TEST
        if (mHandle) {
            return FSFILE_Write(mHandle, bytesWritten, offset, data, size, FS_WRITE_FLUSH);
        }
#endif
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
#ifndef HOST_TEST
        if (mHandle) {
            return FSFILE_Read(mHandle, bytesRead, offset, data, size);
        }
#endif
        if (mStdFile) {
            if (fseek(mStdFile, (long)offset, SEEK_SET) != 0) return -1;
            size_t r = fread(data, 1, size, mStdFile);
            if (bytesRead) *bytesRead = (u32)r;
            return (r > 0) ? 0 : -1;
        }
        return -1;
    }

    Handle get() const { return mHandle; }
    SafeFile(const SafeFile&) = delete;
    SafeFile& operator=(const SafeFile&) = delete;
};

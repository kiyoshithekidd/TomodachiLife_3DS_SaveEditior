#include <3ds.h>
#include <citro2d.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <memory>

class SafeArchive {
private:
    Handle mHandle = 0;
public:
    SafeArchive(FS_ArchiveID id, FS_Path path) {
        if (R_FAILED(FSUSER_OpenArchive(&mHandle, id, path))) {
            mHandle = 0;
        }
    }

    ~SafeArchive() {
        if (mHandle) {
            FSUSER_CloseArchive(mHandle);
        }
    }

    // Must be called to ensure save data writes aren't lost
    Result commit() {
        if (!mHandle) return -1;
        return FSUSER_ControlArchive(mHandle, ARCHIVE_ACTION_COMMIT_SAVE_DATA, NULL, 0, NULL, 0);
    }
    
    Handle get() const { return mHandle; }
    
    // Delete copy constructors to prevent double-closing of handle
    SafeArchive(const SafeArchive&) = delete;
    SafeArchive& operator=(const SafeArchive&) = delete;
};

class SafeFile {
private:
    Handle mHandle = 0;
public:
    SafeFile(Handle archive, FS_Path path, u32 flags, u32 attributes) {
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

int main(int argc, char** argv) {
    initServices();

    C3D_RenderTarget* topTarget = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    C2D_TextBuf staticBuf = C2D_TextBufNew(4096);

    // North American Tomodachi Life Title ID: 000400000008C300
    // Low ID: 0008C300, High ID: 00040000
    u32 pathDataSD[3] = {MEDIATYPE_SD, 0x0008C300, 0x00040000};
    FS_Path arcPathSD = {PATH_BINARY, 12, (const void*)pathDataSD};
    std::unique_ptr<SafeArchive> archive = std::make_unique<SafeArchive>(ARCHIVE_USER_SAVEDATA, arcPathSD);

    if (archive->get() == 0) {
        // Try game card if not found on SD
        u32 pathDataCart[3] = {MEDIATYPE_GAME_CARD, 0x0008C300, 0x00040000};
        FS_Path arcPathCart = {PATH_BINARY, 12, (const void*)pathDataCart};
        archive = std::make_unique<SafeArchive>(ARCHIVE_USER_SAVEDATA, arcPathCart);
    }

    char displayString[512] = "Tomodachi Life Save Editor\n\n";

    if (archive->get() != 0) {
        FS_Path filePath = fsMakePath(PATH_ASCII, "/savedata.arc");
        std::unique_ptr<SafeFile> file = std::make_unique<SafeFile>(archive->get(), filePath, FS_OPEN_READ, 0);

        if (file->get() != 0) {
            u32 currentMoney = 0;
            u32 bytesRead = 0;
            
            // Read Money (offset 0x1E4BB8)
            file->read(0x1E4BB8, &currentMoney, sizeof(u32), &bytesRead);
            
            // Read Island Name (offset 0x1E4BCC, 20 bytes for 10 UTF-16 chars)
            u16 islandNameUTF16[11] = {0}; 
            file->read(0x1E4BCC, islandNameUTF16, 20, &bytesRead);

            // Convert UTF-16 string to UTF-8 for citro2d rendering
            char islandNameUTF8[32] = {0};
            utf16_to_utf8((uint8_t*)islandNameUTF8, islandNameUTF16, 31);
            
            snprintf(displayString + strlen(displayString), sizeof(displayString) - strlen(displayString), 
                     "Island Name: %s\nCurrent Money: %lu", islandNameUTF8, currentMoney);
        } else {
            strncat(displayString, "Error: Could not open /savedata.arc file.", sizeof(displayString) - strlen(displayString) - 1);
        }
    } else {
        strncat(displayString, "Error: Could not mount NA Tomodachi Life save data.", sizeof(displayString) - strlen(displayString) - 1);
    }

    strncat(displayString, "\n\nPress START to exit.", sizeof(displayString) - strlen(displayString) - 1);

    C2D_Text infoText;
    C2D_TextParse(&infoText, staticBuf, displayString);
    C2D_TextOptimize(&infoText);

    while (aptMainLoop()) {
        // 1. Scan inputs
        hidScanInput(); 
        u32 keysDown = hidKeysDown();
        
        if (keysDown & KEY_START) break; // Standard exit condition
        
        // 2. Frame Rendering Begin
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        
        // Render Top Screen
        C2D_TargetClear(topTarget, C2D_Color32(0, 0, 0, 255));
        C2D_SceneBegin(topTarget);
        
        // Draw Text
        C2D_DrawText(&infoText, C2D_WithColor, 10.0f, 10.0f, 0.5f, 0.5f, 0.5f, C2D_Color32(255, 255, 255, 255));
        
        // 3. Frame Rendering End
        C3D_FrameEnd(0);
    }

    // Free resources
    C2D_TextBufDelete(staticBuf);
    exitServices();
    
    return 0;
}

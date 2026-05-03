// OTA_Update.h — OTA implementation (include at top of .ino file)
// Add to your .ino: #include "OTA_Update.h"

#ifndef OTA_UPDATE_H
#define OTA_UPDATE_H

#include <HTTPClient.h>
#include <Update.h>

// Forward declare WatchUI to avoid circular dependency
class WatchUI;

// OTA updater class - implementations are at the bottom of this file
// (they need WatchUI to be fully defined first)
class OTAUpdater {
public:
    static void performUpdate(WatchUI* ui, const char* url);
    
private:
    static void updateProgress(WatchUI* ui, int progress);
    static void updateStatus(WatchUI* ui, const char* message, uint32_t color);
};

// Note: Method implementations are at the end of this file after WatchUI is defined

#endif // OTA_UPDATE_H

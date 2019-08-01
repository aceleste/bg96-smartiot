#ifndef __LOG_MANAGER_H__
#define __LOG_MANAGER_H__
#include "BG96Interface.h"
#include "FSInterface.h"
#include "mbed.h"
#include "Thread.h"
#include <string>
#if !defined(GEOFENCE_EVENTS_FILENAME)
#define GEOFENCE_EVENTS_FILENAME "geofenceevents.log"
#endif
#if !defined(ERRORS_FILENAME)
#define ERRORS_FILENAME "errors.log"
#endif
#if !defined(LOCATION_HISTORY_FILENAME)
#define LOCATION_HISTORY_FILENAME "location.log"
#endif
#if !defined(DEVICE_TO_SYSTEM_MSG_FILENAME)
#define DEVICE_TO_SYSTEM_MSG_FILENAME "dts.log"
#endif

class BTLLogManager
{
public:
    BTLLogManager(BG96Interface *bg96);
    ~BTLLogManager(){};
    bool logGeofenceEvents(void *param);
    bool logAnError(std::string error);
    bool logNewLocation(GNSSLoc &loc);
    bool logSystemStartEvent();
    bool logLocationError();
    bool logConnectionError();
    bool appendDeviceToSystemMessage(std::string &dts_string);
    bool startDeviceToSystemDumpSession(FILE_HANDLE &fh);
    void stopDeviceSystemDumpSession(FILE_HANDLE &fh);
    bool getNextDeviceToSystemMessage(FILE_HANDLE &fh, std::string &dts_message);
    bool flushDeviceToSystemFile(FILE_HANDLE &fh);
private:
    bool append(std::string filename, void *data, size_t length, bool initialize, bool powerOff);
    Mutex _log_m_mutex;
    BG96Interface *_bg96;
    size_t _dts_file_offset;
    FILE_HANDLE _dts_file_handle;
    FILE_HANDLE _error_file_handle;
    FILE_HANDLE _location_events_file_handle;
};

#endif //__LOG_MANAGER_H__

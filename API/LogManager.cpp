#include "LogManager.h"
#include "BG96Interface.h"
#include "AppManager.h"
#include <string>

char dts[BG96_MQTT_CLIENT_MAX_PUBLISH_MSG_SIZE];

LogManager::LogManager(BG96Interface *bg96)
{
    _bg96 = bg96;
}

bool LogManager::append(std::string filename, void *data, size_t length, bool initialize, bool powerOff)
{
    if (initialize) _bg96->initializeBG96();
    _bg96->disallowPowerOff();
    FILE_HANDLE fh;
    if (!_bg96->fs_open(filename.c_str(), CREATE_RW, fh)) return false;
    if (!_bg96->fs_eof(fh)) {
        _bg96->fs_close(fh);
        return false;
    }
    if (!_bg96->fs_write(fh, length, data)) {
        _bg96->fs_close(fh);
        return false;
    }
    _bg96->fs_close(fh);
    _bg96->allowPowerOff();
    if (powerOff) _bg96->powerDown();
    return true;
}

bool LogManager::appendDeviceToSystemMessage(std::string &dts_string)
{
    bool rc;
    char eol = '\n';
    strcpy(dts, dts_string.c_str());
    std::string filename = DEVICE_TO_SYSTEM_MSG_FILENAME;
    _log_m_mutex.lock();
    if (append(filename, (void *) dts, strlen(dts)+1, true, false)) {
        rc = append(filename, (void *) &eol, 1, false, true);
    } else {
        rc = false;
    }
    _log_m_mutex.unlock();
    return rc;
}

bool LogManager::startDeviceToSystemDumpSession(FILE_HANDLE &fh)
{
    bool rc = false;
    _log_m_mutex.lock();
    if (_bg96->fs_open(DEVICE_TO_SYSTEM_MSG_FILENAME, EXISTONLY_RO, fh)) {
        rc = _bg96->fs_rewind(fh);
    } else {
        rc = false;
    }
    return rc;
}

bool LogManager::getNextDeviceToSystemMessage(FILE_HANDLE &fh, std::string &dts_string)
{
    bool rc;
    char buffer[1548] = {0};
    char eol = '\n';
    for (int i = 0; i < BG96_MQTT_CLIENT_MAX_PUBLISH_MSG_SIZE; i++) {
        rc = _bg96->fs_read(fh,1,&buffer[i]);
        if (!rc) return false; //reading past the eof will return an error;
        if (buffer[i] == '\n') break;
    }
    dts_string = buffer;
    return true;
}

bool LogManager::flushDeviceToSystemFile(FILE_HANDLE &fh)
{
    bool rc = _bg96->fs_rewind(fh);
    if (rc) {
        rc = _bg96->fs_truncate(fh, 0);
    }
    return rc;
}

void LogManager::stopDeviceSystemDumpSession(FILE_HANDLE &fh)
{
    _bg96->fs_close(fh);
    _log_m_mutex.unlock();
}

bool LogManager::logAnError(std::string error)
{
    bool rc;
    char eol = '\n';
    std::string filename = ERRORS_FILENAME;
    _log_m_mutex.lock();
    if (append(filename, (void *)(error.c_str()), error.length()+1, true, false)) {
        rc = append(filename, (void *) &eol, 1, false, true);
    } else {
        rc = false;
    }
    _log_m_mutex.unlock();
    return rc;
}

bool LogManager::logNewLocation(GNSSLoc &loc)
{
    bool rc;
    char eol = '\n';
    char location_line[80];
    std::string filename(LOCATION_HISTORY_FILENAME);
    _log_m_mutex.lock();
    time_t loctime = loc.getGNSSTime();
    sprintf(location_line, "%s: %3.6f, %3.6f", ctime(&loctime), loc.getGNSSLatitude(), loc.getGNSSLongitude());
    if (append(filename, (void *)location_line, strlen(location_line)+1, true, false)) {
        rc = append(filename, (void *) &eol, (size_t)1, false, true);
    } else {
        rc = false;
    }
    _log_m_mutex.unlock();
    return rc;    
}

bool LogManager::logLocationError()
{
    time_t now = time(NULL);
    std::string timestr = ctime(&now);
    std::string error = " GNSS Location error.";
    return logAnError(timestr+error);
}

bool LogManager::logSystemStartEvent()
{
    time_t now = time(NULL);
    std::string timestr = ctime(&now);
    std::string error = " SYSTEM STARTED.";
    std::string fevent = timestr+error;
    bool rc;
    char eol = '\n';
    std::string filename(EVENTS_FILENAME);
    _log_m_mutex.lock();
    if (append(filename, (void *)fevent.c_str(), fevent.length+1, true, false)) {
        rc = append(filename, (void *) &eol, (size_t)1, false, true);
    } else {
        rc = false;
    }
    _log_m_mutex.unlock();
    return rc;  
}

bool LogManager::logConnectionError()
{
    time_t now = time(NULL);
    std::string timestr = ctime(&now);
    std::string error = " Connection error.";
    return logAnError(timestr+error);  
}

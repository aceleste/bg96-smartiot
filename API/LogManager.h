/**
 * @file LogManager.h
 * @author Alain CELESTE (alain.celeste@polaris-innovation.com)
 * @brief 
 * @version 0.1
 * @date 2019-08-13
 * 
 * @copyright Copyright (c) 2019 Polaris Innovation
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#define __LOG_MANAGER_H__
#include "BG96Interface.h"
#include "FSInterface.h"
#include "mbed.h"
#include "Thread.h"
#include <string>

#if !defined(ERRORS_FILENAME)
#define ERRORS_FILENAME "errors.log"
#endif
#if !defined(EVENTS_FILENAME)
#define EVENTS_FILENAME "events.log"
#endif
#if !defined(LOCATION_HISTORY_FILENAME)
#define LOCATION_HISTORY_FILENAME "location.log"
#endif
#if !defined(DEVICE_TO_SYSTEM_MSG_FILENAME)
#define DEVICE_TO_SYSTEM_MSG_FILENAME "dts.log"
#endif

class LogManager
{
public:
    LogManager(BG96Interface *bg96, Mutex *bg96mutex);
    ~LogManager(){};
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
//    bool startFileDumpSession(FILE_HANDLE &fh);
//    bool getNextLineFromFile(FILE_HANDLE &fh);
//    bool stopFileDumpSession(FILE_HANDLE &fh);
private:
    bool append(std::string filename, void *data, size_t length, bool initialize, bool powerOff);
    Mutex           * _log_m_mutex;
    BG96Interface   * _bg96;
    size_t          _dts_file_offset;
    FILE_HANDLE     _dts_file_handle;
    FILE_HANDLE     _error_file_handle;
    FILE_HANDLE     _location_events_file_handle;
    FILE_HANDLE     _events_file_handle;
};

#endif //__LOG_MANAGER_H__

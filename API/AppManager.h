/**
 * @file AppManager.h
 * @author Alain CELESTE (alain.celeste@polaris-innovation.com)
 * @API to be used by the user application to call services. 
 * @version 0.1
 * @date 2019-08-12
 * 
 * @copyright Copyright (c) 2019 Polaris Innovation 
 * 
 */
#include "mbed.h"
#include <string>
#include "GNSSLoc.h"
#include "LocationManager.h"
#include "ConnectionManager.h"
#include "LogManager.h"

#define DEFAULT_CONNECTION_TIMEOUT 100

// typedef struct {
//     ConnectionManager   *conn_m;
//     LocationManager     *loc_m;
//     LogManager          *log_m;
// } TaskParameter;

/**
 * @brief The AppManager class provides the main interface to the following services:
 *  - Connection services (sending and receiving MQTT messages from Azure IoT Hub)
 *  - Location GNSS service (getting GNSS location from the BG96 modem)
 *  - Log service (logging in files on the UFS flash memory on the BG96 modem)
 */
class AppManager
{
public: 
                        AppManager(ConnectionManager *conn_m,
                               LocationManager *loc_m,
                               LogManager *log_m);
                        ~AppManager();
    LogManager *        getLogManager();
    ConnectionManager * getConnectionManager();
    LocationManager *   getLocationManager();
    bool                getLocation(GNSSLoc &location);
    void                processLocation(GNSSLoc *current_location, void (*callback)(GNSSLoc *, AppManager *));
    bool                getSystemToDeviceMessage(std::string &message, int timeout);
    void                processSystemToDeviceMessage(std::string &system_message, void (*callback)(std::string &, AppManager *));
    bool                queueDeviceToSystemMessage(std::string &device_message);
    bool                sendDeviceToSystemMessageQueue();
    bool                sendDeviceToSystemMessage(std::string &device_message);
    bool                logConnectionError();
    bool                logLocationError();
    bool                logError(std::string error);
    bool                logLocation(GNSSLoc &location);

    void                setQueueMessageStatus(bool status);
    void                setConnectionStatus(bool status);
    void                setLogStatus(bool status);
    void				setLocationStatus(bool status);
//    bool            dumpErrorFile();
//    bool            dumpLocationFile();
//    bool            dumpFile(std::string filename);


private:


    Thread              *_thread;
    Mutex               _app_mutex;
    std::string         _system_message;
    ConnectionManager   *_conn_m;
    LocationManager     *_loc_m;
    LogManager          *_log_m;
    bool                _loc_result;
    bool                _queue_result;
    bool                _connect_result;
    bool                _log_result;
};

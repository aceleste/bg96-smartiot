#include "mbed.h"
#include <string>
#include "GNSSLoc.h"
#include "LocationManager.h"
#include "ConnectionManager.h"
#include "LogManager.h"

#define DEFAULT_CONNECTION_TIMEOUT 100

typedef struct {
    ConnectionManager   *conn_m;
    LocationManager     *loc_m;
    LogManager          *log_m;
} TaskParameter;

class AppManager
{
public: 
                    AppManager(ConnectionManager *conn_m,
                               LocationManager *loc_m,
                               LogManager *log_m);
                    ~AppManager();
    bool            getLocation(GNSSLoc &location);
    void            processLocation(GNSSLoc &current_location, void (*callback)(GNSSLoc &, TaskParameter &));
    bool            getSystemToDeviceMessage(std::string &system_message);
    void            processSystemToDeviceMessage(std::string &system_message, void (*callback)(std::string &, TaskParameter &));
    bool            queueDeviceToSystemMessage(std::string &device_message);
    bool            sendDeviceToSystemMessageQueue();

private:

    Thread              _thread;
    Mutex               _app_mutex;
    std::string         _system_message;
    ConnectionManager   *_conn_m;
    LocationManager     *_loc_m;
    LogManager          *_log_m;
};
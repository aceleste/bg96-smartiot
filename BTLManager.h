#include "mbed.h"
#include <string>
#include "GNSSLoc.h"
#include "LocationManager.h"
#include "TemperatureManager.h"
#include "ConnectionManager.h"
#include "BTLLogManager.h"

#define MAX_NUM_OF_VERTICES 10

typedef struct {
    ConnectionManager   *conn_m;
    TemperatureManager  *temp_m;
    LocationManager     *loc_m;
    BTLLogManager       *log_m;
    bool                heating;
} BTLTaskParameter;

typedef struct {
    double id;
    bool notifyonleave;
    bool notifyonenter;
    bool heating;
} GeofenceParams;

typedef struct {
    std::string deviceID;
    double      container_temperature;
    double      ambient_temperature;
    double      heater_temperature;
    double      current_latitude;
    double      current_longitude;
    std::string current_utc_time;
    int         geofenceNum; 
    int         geofenceEntryDeparture;
    double      rssi;
    bool        enRoute;
} DeviceToSystemParameter;

typedef enum {GEOFENCE_SHAPE_CIRCLE=1, GEOFENCE_SHAPE_POLY} GEOFENCE_SHAPE;

typedef enum {SYSTEM_MSG_TYPE_STATUS_REQUEST=1, SYSTEM_MSG_TYPE_CONFIG} SYSTEM_MSG_TYPE;

typedef enum {GEOFENCE_TASK, ROUTE_TASK} TASK_NATURE;
class BTLManager
{
public: 
                    BTLManager(ConnectionManager *conn_m,
                               TemperatureManager *temp_m,
                               LocationManager *loc_m,
                               BTLLogManager *log_m);
                    ~BTLManager();
    void            processLocation(GNSSLoc &current_location);
    bool            processSystemToDeviceMessage(std::string &system_message);
protected:
    void            terminateTask(void);
    void            createTask(double temperatureInterval, TASK_NATURE nature, bool heating);
    double          jsonParseSystemToDevice(const char * parameter, int msg_index);
    void            queueGeofenceLeaveNotification(GeofenceParams &geofence);
    void            queueGeofenceEnterNotification(GeofenceParams &geofence);
private:

    LowPowerTicker      _timeout;
    Thread              _thread;
    Mutex               _btl_mutex;
    GeofenceParams      _lastgeofence;
    bool                _task_running;
    std::string         _system_message;
    bool                _status_request;
    int                 _status_request_id;
    ConnectionManager   *_conn_m;
    TemperatureManager  *_temp_m;
    LocationManager     *_loc_m;
    BTLLogManager       *_log_m;
};
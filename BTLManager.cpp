#include "BTLManager.h"
#include "BTLLogManager.h"
#include "MbedJSONValue.h"
#include "MQTTClient_Settings.h"
#include "TinyGPSplus.h"
#include "math.h"

static BTLTaskParameter param;
static DeviceToSystemParameter dts_param;
static bool temperature_timeout_triggered;
TinyGPSPlus tgps;

void jsonSerializeDeviceToSystem(std::string &s, DeviceToSystemParameter &dts);
int inCircle(double xcentre, double ycentre, double xedge, double yedge, double testx, double testy);
int pnpoly(int nvert, double *vertLong, double *vertLat, double testLong, double testLat);

BTLManager::BTLManager(ConnectionManager *conn_m,
                       TemperatureManager *temp_m,
                       LocationManager *loc_m,
                       BTLLogManager *log_m)
{
    _conn_m = conn_m;
    _temp_m = temp_m;
    _loc_m = loc_m;
    _log_m = log_m;
    _lastgeofence.id = 0;
    _lastgeofence.notifyonenter = false;
    _lastgeofence.notifyonleave = false;
    log_m->logSystemStartEvent();
}

BTLManager::~BTLManager()
{
//    log_m->logSystemStopEvent();
}

void BTLManager::queueGeofenceEnterNotification(GeofenceParams &geofence)
{
    DeviceToSystemParameter dts_param;
    double temperatures[MAX_PROBES];
    dts_param.deviceID = DEVICE_ID;
    _temp_m->updateTemperatures(temperatures);
    dts_param.container_temperature = temperatures[CONTAINER_TEMPERATURE_IDX];
    if (geofence.heating) {
        dts_param.heater_temperature = temperatures[HEATER_TEMPERATURE_IDX];
    } else {
        dts_param.heater_temperature = -777.7;
    }
    dts_param.ambient_temperature = temperatures[AMBIANT_TEMPERATURE_IDX];
    _loc_m->getCurrentLatitude(dts_param.current_latitude);
    _loc_m->getCurrentLongitude(dts_param.current_longitude);
    _loc_m->getCurrentUTCTime(dts_param.current_utc_time);
    dts_param.geofenceEntryDeparture = 1.0;     
    std::string notification;
    jsonSerializeDeviceToSystem(notification, dts_param);
    _log_m->appendDeviceToSystemMessage(notification);   
}

void BTLManager::queueGeofenceLeaveNotification(GeofenceParams &geofence)
{
    DeviceToSystemParameter dts_param;
    double temperatures[MAX_PROBES];
    dts_param.deviceID = DEVICE_ID;
    _temp_m->updateTemperatures(temperatures);
    dts_param.container_temperature = temperatures[CONTAINER_TEMPERATURE_IDX];
    if (geofence.heating) {
        dts_param.heater_temperature = temperatures[HEATER_TEMPERATURE_IDX];
    } else {
        dts_param.heater_temperature = -777.7;
    }
    dts_param.ambient_temperature = temperatures[AMBIANT_TEMPERATURE_IDX];
    _loc_m->getCurrentLatitude(dts_param.current_latitude);
    _loc_m->getCurrentLongitude(dts_param.current_longitude);
    _loc_m->getCurrentUTCTime(dts_param.current_utc_time);
    dts_param.geofenceEntryDeparture = -1.0;     
    std::string notification;
    jsonSerializeDeviceToSystem(notification, dts_param);
    _log_m->appendDeviceToSystemMessage(notification);
}

/**
* Returns true if current_location inside geofence, or if we received a status request message
*
**/
void BTLManager::processLocation(GNSSLoc &current_location)
{
    double ngeofences = jsonParseSystemToDevice("config",3);
    char route[10]={0};
    bool inGeofence;
    for (int i = 0; i<ngeofences; i++) {
        sprintf(route,"route%d",i+1);
        double shape = jsonParseSystemToDevice(route, 5);
        if (shape == GEOFENCE_SHAPE_CIRCLE) 
        {
            double geo_lat_c    = jsonParseSystemToDevice(route, 6);
            double geo_long_c   = jsonParseSystemToDevice(route, 7);
            double geo_lat_e    = jsonParseSystemToDevice(route, 8);
            double geo_long_e   = jsonParseSystemToDevice(route, 9);
        
            inGeofence = inCircle(geo_lat_c, geo_long_c, geo_lat_e, geo_long_e, current_location.getGNSSLatitude(), current_location.getGNSSLongitude());
        } else if (shape == GEOFENCE_SHAPE_POLY) {
            int vertices = jsonParseSystemToDevice(route, 6);  //number of polygon vertices
            double geo_lat[MAX_NUM_OF_VERTICES];
            double geo_long[MAX_NUM_OF_VERTICES];
            int msg_index = 7;
                for (int verts=0; verts < vertices; verts++ )
                {
                    geo_lat[verts] = jsonParseSystemToDevice(route,msg_index);
                    geo_long[verts] = jsonParseSystemToDevice(route,msg_index+1);
                    msg_index = msg_index + 2;   
                }
            
            inGeofence = pnpoly(vertices, geo_long, geo_lat,current_location.getGNSSLatitude(), current_location.getGNSSLongitude());
        } 
        if (inGeofence) {
            GeofenceParams newgeofence;
            newgeofence.id = i;
            newgeofence.notifyonenter = jsonParseSystemToDevice(route, 3) == 1 ? false : true;
            newgeofence.notifyonenter = jsonParseSystemToDevice(route, 4) == 1 ? false : true; 
            newgeofence.heating = jsonParseSystemToDevice(route, 1) == 1 ? false : true;
            if (newgeofence.id != _lastgeofence.id) {
                if (_task_running) terminateTask();
                if (_lastgeofence.notifyonleave) {
                    queueGeofenceLeaveNotification(_lastgeofence);
                }
                if (newgeofence.notifyonenter) {
                    queueGeofenceEnterNotification(newgeofence);
                }
                createTask(jsonParseSystemToDevice("config",2), GEOFENCE_TASK, newgeofence.heating);
                _lastgeofence = newgeofence;
            } else {
                if (!_task_running) createTask(jsonParseSystemToDevice("config",2), GEOFENCE_TASK, newgeofence.heating);
            }
        } else {
            if (_lastgeofence.id != 0) {
                if (_task_running) terminateTask();
                if (_lastgeofence.notifyonleave) queueGeofenceLeaveNotification(_lastgeofence);
                if (!_task_running) createTask(jsonParseSystemToDevice("config",2), ROUTE_TASK, false);
                _lastgeofence.id = 0;
                _lastgeofence.notifyonenter = false;
                _lastgeofence.notifyonleave = false;
            } else {
                if (!_task_running) createTask(jsonParseSystemToDevice("config",2), ROUTE_TASK, false);
            }
        }
    }
    //For each geofences in _system_message
    //If circle
        //Verify if current_location is in Circle.
    //elsif polygone
        //Verify if current_location is in polygone
    //endif
    // if in a geofence 
        //if the geofence is a new geofence (we changed geofence)
            //if there is a task running
                //terminate the task
            //fi
            //if notification requested (lastgeofence.notifyonleave == true || newgeofence.notifyonenter == true)
                //then queue a device status
            //fi
            //start geofence task
            //lastgeofence = newgeofence
        //else
            //if task is not running
                //start task
            //fi
        //fi
    //else
        //if we were a geofence prior to that
            //if there is a task running
                //terminate the task
            //fi
            //if notification requested (lastgeofence.notifyonleave == true)
                //queue a device status
            //fi
            //start a route task
            //lastgeofence = NULL;
        //else
            //if route task not running
                //start a route task
            //fi
        //fi
    //fi
}

void BTLManager::terminateTask(void)
{
    _thread.terminate();
    _timeout.detach();
    _task_running = false;
}

void btl_task_timeout(void)
{
    temperature_timeout_triggered = true;   
}

void btl_geofence_task(BTLTaskParameter *param);
void btl_route_task(BTLTaskParameter *param);

void BTLManager::createTask(double temperatureInterval, TASK_NATURE nature, bool heating)
{
    temperature_timeout_triggered = false;
    _timeout.attach(btl_task_timeout, temperatureInterval);

    param.conn_m = _conn_m;
    param.temp_m = _temp_m;
    param.loc_m = _loc_m;
    param.log_m = _log_m;
    param.heating = heating;
    if (nature == GEOFENCE_TASK) {
        _thread.start(callback(btl_geofence_task, &param));
    } else {
        _thread.start(callback(btl_route_task, &param));
    }
    _task_running = true;
}

bool BTLManager::processSystemToDeviceMessage(std::string &system_message)
{
    bool rc;
    double type = jsonParseSystemToDevice("header", 0);
    _btl_mutex.lock();
    if (type == SYSTEM_MSG_TYPE_STATUS_REQUEST) {
        // Factor request msg
        // Queue request in event queue;
        rc = true;
    } else if (type == SYSTEM_MSG_TYPE_CONFIG) {
        _system_message = system_message;
        rc = true;
    } else {
        rc = false;
    }
    _btl_mutex.unlock();
    return rc;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
//      JSON parse
////////////////////////////////////////////////////////////////////////////////////////////////////////
double BTLManager::jsonParseSystemToDevice(const char * parameter, int msg_index)
{
// system_message = "{\"header\":[1.0,12.0],\"config\":[1.0,1.0,240.0,5.0],\"route1\":[1.0,0.0,1.0,1.0,1.0,1.0,54.5983852,-1.5708491,54.5969549,-1.5663735],\"route2\":[2.0,0.0,1.0,1.0,1.0,1.0,54.6542957,-1.4459836,54.6495902,-1.4430425],\"route3\":[3.0,0.0,1.0,1.0,1.0,1.0,54.7051416,-1.5638412,54.7101814,-1.5615844],\"route4\":[4.0,0.0,1.0,1.0,1.0,1.0,54.6298560,-1.3059736,54.6267899,-1.3075833],\"route5\":[5.0,1.0,1.0,1.0,1.0,2.0,5.0,54.6710093,-1.4587418,54.6730758,-1.4461951,54.6672642,-1.4436423,54.6678548,-1.4562232,54.6710093,-1.4587418]}";
// Journey message received from system parsed into values. Message description:
//"{\"header"\:[type, id],\"config\":[device,temperatureRequired,temperatureInterval,numberOfGeos],\"route1\":[geoFenceNum,heating,temperatureRequired,pingOnArrival,pingOnDeparture,shape,outerCircle,latCentre,longCentre,latEdge,LongEdge,latEdge2,longEdge2],.........,\"routeX\":[geoFenceNum,heating,temperatureRequired,pingOnArrival,pingOnDeparture,shape,numOfVertices,lat1,long1,lat2,long2,..............,latX,longY]}

    MbedJSONValue journey;
    _btl_mutex.lock();
    parse(journey, _system_message.c_str());
    _btl_mutex.unlock();
    double msg; 
    msg = journey[parameter][msg_index].get<double>(); 
    return msg;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
//      JSON serialize
////////////////////////////////////////////////////////////////////////////////////////////////////////
//ToDo: pass arguments, currently some dummy values
//geoFenceNum: the geofence entered or left, value: 1(entered), 2(left)
void jsonSerializeDeviceToSystem(std::string &s, DeviceToSystemParameter &dts)
{
    MbedJSONValue statusReport;
    //fill the object
    statusReport["timestamp"]= dts.current_utc_time.c_str();
    statusReport["device"] = dts.deviceID;
    statusReport["latitude"]= dts.current_latitude;
    statusReport["longitude"] = dts.current_longitude;
    statusReport["geoFenceNum"] = dts.geofenceNum;
    statusReport["geoFenceEnteryDeparture"] = dts.geofenceEntryDeparture;
    statusReport["liquidTemp"] = dts.container_temperature;
    statusReport["AmbientTemp"] = dts.ambient_temperature;
    statusReport["heater"] = dts.heater_temperature;
    statusReport["batteryVoltage"] = 3.67;
    statusReport["network"] = "Tele2";
    statusReport["signalStrength"] = dts.rssi;
    statusReport["enRoute"] = dts.enRoute ? 0.00 : 1.00;
    
    //serialize it into a JSON string
    s = statusReport.serialize();
}

void btl_geofence_task(BTLTaskParameter *param)
{
    double temperatures[MAX_PROBES];
    std::string dts_string;
    while(true) {
        while(!temperature_timeout_triggered) sleep();
        temperature_timeout_triggered = false;
        dts_param.deviceID = DEVICE_ID;
        param->temp_m->updateTemperatures(temperatures);
        dts_param.container_temperature = temperatures[CONTAINER_TEMPERATURE_IDX];
        if (param->heating == true) dts_param.heater_temperature = temperatures[HEATER_TEMPERATURE_IDX];
        dts_param.ambient_temperature = temperatures[AMBIANT_TEMPERATURE_IDX];
        param->loc_m->getCurrentLatitude(dts_param.current_latitude);
        param->loc_m->getCurrentLongitude(dts_param.current_longitude);
        param->loc_m->getCurrentUTCTime(dts_param.current_utc_time);
        param->conn_m->getRSSI(dts_param.rssi);
        dts_param.geofenceEntryDeparture = 0; 
        dts_param.enRoute = false;
        param->temp_m->displayTemperature(dts_param.container_temperature);
        jsonSerializeDeviceToSystem(dts_string, dts_param);
        param->log_m->appendDeviceToSystemMessage(dts_string);
    }
}

void btl_route_task(BTLTaskParameter *param)
{
    double temperatures[MAX_PROBES];
    std::string dts_string;
    while(true) {
        while(!temperature_timeout_triggered) sleep();
        temperature_timeout_triggered = false;
        dts_param.deviceID = DEVICE_ID;
        param->temp_m->updateTemperatures(temperatures);
        dts_param.container_temperature = temperatures[CONTAINER_TEMPERATURE_IDX];
        if (param->heating == true) {
            dts_param.heater_temperature = temperatures[HEATER_TEMPERATURE_IDX];
        } else {
            dts_param.heater_temperature = -777.7;
        }
        dts_param.ambient_temperature = temperatures[AMBIANT_TEMPERATURE_IDX];
        param->loc_m->getCurrentLatitude(dts_param.current_latitude);
        param->loc_m->getCurrentLongitude(dts_param.current_longitude);
        param->loc_m->getCurrentUTCTime(dts_param.current_utc_time);
        dts_param.geofenceEntryDeparture = 0; 
        dts_param.enRoute = true;
        param->conn_m->getRSSI(dts_param.rssi);
        param->temp_m->displayTemperature(dts_param.container_temperature);
        jsonSerializeDeviceToSystem(dts_string, dts_param);
        param->log_m->appendDeviceToSystemMessage(dts_string);
    }
}





int inCircle(double xcentre, double ycentre, double xedge, double yedge, double testx, double testy)
{
    //Returns 1 if test point is inside circle, zero otherwise
    //The circle is defined by centre and a point on the circumference 
    double distance;
    int test;
    double radius;
    
    radius = tgps.distanceBetween(xcentre, ycentre, xedge, yedge);
    distance = tgps.distanceBetween(xcentre, ycentre, testx, testy);
    
     if(distance < radius){
            test = 1;
            }
        else{
            test = 0;
            }
  return (test);
}

int pnpoly(int nvert, double *vertLong, double *vertLat, double testLong, double testLat)
{
  //Returns 1 if test point is inside polygon, zero otherwise
  //last vertix should be the same as the first
  int i, j, c = 0;
  for (i = 0, j = nvert-1; i < nvert; j = i++) {
    if ( ((vertLat[i]>testLat) != (vertLat[j]>testLat)) &&
     (testLong < (vertLong[j]-vertLong[i]) * (testLat-vertLat[i]) / (vertLat[j]-vertLat[i]) + vertLong[i]) )
       c = !c;
  }
  return c;
}

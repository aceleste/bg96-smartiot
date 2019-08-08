#include "AppManager.h"
#include "LogManager.h"
#include "MQTTClient_Settings.h"

static TaskParameter param;


AppManager::AppManager(ConnectionManager *conn_m,
                       LocationManager *loc_m,
                       LogManager *log_m)
{
    _conn_m = conn_m;
    _loc_m = loc_m;
    _log_m = log_m;
    param.conn_m = conn_m;
    param.loc_m = loc_m;
    param.log_m = log_m;
    log_m->logSystemStartEvent();
}

AppManager::~AppManager()
{
//    log_m->logSystemStopEvent();
}

/**
* Returns true if current_location inside geofence, or if we received a status request message
*
**/
void AppManager::processLocation(GNSSLoc *current_location, void (*callback)(GNSSLoc *, TaskParameter &))
{
    callback(current_location, param);
}


void AppManager::processSystemToDeviceMessage(std::string &system_message, void (*callback)(std::string &, TaskParameter &))
{
    _app_mutex.lock();
    _system_message = system_message;
    _app_mutex.unlock();
    callback(system_message, param);
}

bool AppManager::queueDeviceToSystemMessage(std::string &device_message)
{
    return _log_m->appendDeviceToSystemMessage(device_message);
}

bool AppManager::sendDeviceToSystemMessageQueue()
{
    return _conn_m->sendAllMessages(_log_m, DEFAULT_CONNECTION_TIMEOUT);
}

// ////////////////////////////////////////////////////////////////////////////////////////////////////////
// //      JSON parse
// ////////////////////////////////////////////////////////////////////////////////////////////////////////
// double AppManager::jsonParseSystemToDevice(const char * parameter, int msg_index)
// {
// // system_message = "{\"header\":[1.0,12.0],\"config\":[1.0,1.0,240.0,5.0],\"route1\":[1.0,0.0,1.0,1.0,1.0,1.0,54.5983852,-1.5708491,54.5969549,-1.5663735],\"route2\":[2.0,0.0,1.0,1.0,1.0,1.0,54.6542957,-1.4459836,54.6495902,-1.4430425],\"route3\":[3.0,0.0,1.0,1.0,1.0,1.0,54.7051416,-1.5638412,54.7101814,-1.5615844],\"route4\":[4.0,0.0,1.0,1.0,1.0,1.0,54.6298560,-1.3059736,54.6267899,-1.3075833],\"route5\":[5.0,1.0,1.0,1.0,1.0,2.0,5.0,54.6710093,-1.4587418,54.6730758,-1.4461951,54.6672642,-1.4436423,54.6678548,-1.4562232,54.6710093,-1.4587418]}";
// // Journey message received from system parsed into values. Message description:
// //"{\"header"\:[type, id],\"config\":[device,temperatureRequired,temperatureInterval,numberOfGeos],\"route1\":[geoFenceNum,heating,temperatureRequired,pingOnArrival,pingOnDeparture,shape,outerCircle,latCentre,longCentre,latEdge,LongEdge,latEdge2,longEdge2],.........,\"routeX\":[geoFenceNum,heating,temperatureRequired,pingOnArrival,pingOnDeparture,shape,numOfVertices,lat1,long1,lat2,long2,..............,latX,longY]}

//     MbedJSONValue journey;
//     _app_mutex.lock();
//     parse(journey, _system_message.c_str());
//     _app_mutex.unlock();
//     double msg; 
//     msg = journey[parameter][msg_index].get<double>(); 
//     return msg;
// }

// ////////////////////////////////////////////////////////////////////////////////////////////////////////
// //      JSON serialize
// ////////////////////////////////////////////////////////////////////////////////////////////////////////
// //ToDo: pass arguments, currently some dummy values
// //geoFenceNum: the geofence entered or left, value: 1(entered), 2(left)
// void jsonSerializeDeviceToSystem(std::string &s, DeviceToSystemParameter &dts)
// {
//     MbedJSONValue statusReport;
//     //fill the object
//     statusReport["timestamp"]= dts.current_utc_time.c_str();
//     statusReport["device"] = dts.deviceID;
//     statusReport["latitude"]= dts.current_latitude;
//     statusReport["longitude"] = dts.current_longitude;
//     statusReport["geoFenceNum"] = dts.geofenceNum;
//     statusReport["geoFenceEnteryDeparture"] = dts.geofenceEntryDeparture;
//     statusReport["liquidTemp"] = dts.container_temperature;
//     statusReport["AmbientTemp"] = dts.ambient_temperature;
//     statusReport["heater"] = dts.heater_temperature;
//     statusReport["batteryVoltage"] = 3.67;
//     statusReport["network"] = "Tele2";
//     statusReport["signalStrength"] = dts.rssi;
//     statusReport["enRoute"] = dts.enRoute ? 0.00 : 1.00;
    
//     //serialize it into a JSON string
//     s = statusReport.serialize();
// }

// void app_geofence_task(TaskParameter *param)
// {
//     double temperatures[MAX_PROBES];
//     std::string dts_string;
//     while(true) {
//         while(!temperature_timeout_triggered) sleep();
//         temperature_timeout_triggered = false;
//         dts_param.deviceID = DEVICE_ID;
//         param->temp_m->updateTemperatures(temperatures);
//         dts_param.container_temperature = temperatures[CONTAINER_TEMPERATURE_IDX];
//         if (param->heating == true) dts_param.heater_temperature = temperatures[HEATER_TEMPERATURE_IDX];
//         dts_param.ambient_temperature = temperatures[AMBIANT_TEMPERATURE_IDX];
//         param->loc_m->getCurrentLatitude(dts_param.current_latitude);
//         param->loc_m->getCurrentLongitude(dts_param.current_longitude);
//         param->loc_m->getCurrentUTCTime(dts_param.current_utc_time);
//         param->conn_m->getRSSI(dts_param.rssi);
//         dts_param.geofenceEntryDeparture = 0; 
//         dts_param.enRoute = false;
//         param->temp_m->displayTemperature(dts_param.container_temperature);
//         jsonSerializeDeviceToSystem(dts_string, dts_param);
//         param->log_m->appendDeviceToSystemMessage(dts_string);
//     }
// }

// void app_route_task(TaskParameter *param)
// {
//     double temperatures[MAX_PROBES];
//     std::string dts_string;
//     while(true) {
//         while(!temperature_timeout_triggered) sleep();
//         temperature_timeout_triggered = false;
//         dts_param.deviceID = DEVICE_ID;
//         param->temp_m->updateTemperatures(temperatures);
//         dts_param.container_temperature = temperatures[CONTAINER_TEMPERATURE_IDX];
//         if (param->heating == true) {
//             dts_param.heater_temperature = temperatures[HEATER_TEMPERATURE_IDX];
//         } else {
//             dts_param.heater_temperature = -777.7;
//         }
//         dts_param.ambient_temperature = temperatures[AMBIANT_TEMPERATURE_IDX];
//         param->loc_m->getCurrentLatitude(dts_param.current_latitude);
//         param->loc_m->getCurrentLongitude(dts_param.current_longitude);
//         param->loc_m->getCurrentUTCTime(dts_param.current_utc_time);
//         dts_param.geofenceEntryDeparture = 0; 
//         dts_param.enRoute = true;
//         param->conn_m->getRSSI(dts_param.rssi);
//         param->temp_m->displayTemperature(dts_param.container_temperature);
//         jsonSerializeDeviceToSystem(dts_string, dts_param);
//         param->log_m->appendDeviceToSystemMessage(dts_string);
//     }
// }





// int inCircle(double xcentre, double ycentre, double xedge, double yedge, double testx, double testy)
// {
//     //Returns 1 if test point is inside circle, zero otherwise
//     //The circle is defined by centre and a point on the circumference 
//     double distance;
//     int test;
//     double radius;
    
//     radius = tgps.distanceBetween(xcentre, ycentre, xedge, yedge);
//     distance = tgps.distanceBetween(xcentre, ycentre, testx, testy);
    
//      if(distance < radius){
//             test = 1;
//             }
//         else{
//             test = 0;
//             }
//   return (test);
// }

// int pnpoly(int nvert, double *vertLong, double *vertLat, double testLong, double testLat)
// {
//   //Returns 1 if test point is inside polygon, zero otherwise
//   //last vertix should be the same as the first
//   int i, j, c = 0;
//   for (i = 0, j = nvert-1; i < nvert; j = i++) {
//     if ( ((vertLat[i]>testLat) != (vertLat[j]>testLat)) &&
//      (testLong < (vertLong[j]-vertLong[i]) * (testLat-vertLat[i]) / (vertLat[j]-vertLat[i]) + vertLong[i]) )
//        c = !c;
//   }
//   return c;
// }

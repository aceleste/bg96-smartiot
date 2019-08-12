/*
 * This example application demonstrates using the API for GNSS location and IoT Hub messaging services
 * It will wait until a first cloud to device message with "OK" as data is sent, then it will start
 * getting the GNSS location and queuing this time-stamped information with the following format:
 * {"type": "STATUS", "utctime": "Fri  2/08/2019 16:08:02 UTC+2", "gnss": {"altitude": "645.4", "latitude": "-21.245970", "longitude": "55.516857"}}
 *
 * Periodically, the connection to IoT Hub is triggered and the queue is dumped to the server (several status messages are sent at once.)
 *
 * The server can send configuration json messages to the devices of the form:
 * {"type": "CONFIG", <"gnss_period": 10-3600> || <"connect_period": 360-86400>}
 * For example: {"type": "CONFIG", "gnss_period": 200} or {"type": "CONFIG", "connect_period": 3600} or
 * {"type": "CONFIG", "gnss_period": 360, "connect_period": 3600}
 * which allows remote control on the periods of gnss tracking and server connection.
 *
 */
#include "mbed.h"
#include "app_main.h"
#include <string>
#include "API/AppManager.h"
#include "API/ConnectionManager.h"
#include "API/LocationManager.h"
#include "API/LogManager.h"
#include "LowPowerTicker.h"
#include "MbedJSONValue.h"

#define CONNECT_PERIOD_IN_SECONDS 120
#define GNSS_PERIOD_IN_SECONDS 60

bool gnss_timeout;
time_t now;

std::string system_message;
std::string device_to_system_message;

time_t latest_connect_time;
time_t target_gnss_timeout;

LowPowerTicker halfminuteticker;
static Mutex bg96mutex;
static Mutex appmutex;
static BG96Interface bg96;
static ConnectionManager conn_m(&bg96, &bg96mutex);
static LocationManager loc_m(&bg96, &bg96mutex);
static LogManager log_m(&bg96, &bg96mutex);
static AppManager app_m(&conn_m,
                 &loc_m,
                 &log_m);
static bool initialized;
static int gnss_period_in_sec;
static int connect_period_in_sec;

void locationProcess(GNSSLoc *location, TaskParameter &param)
{
	if (location == NULL) return;
	MbedJSONValue message;
//	std::string altitude;
//	std::string latitude;
//	std::string longitude;
	std::string serialized_message;
	time_t loc_time = location->getGNSSTime();
	char utc_time[24];
	strftime(utc_time, 24, "%a, %b %d, %Y %H:%M:%S", localtime(&loc_time));
	message["type"]="STATUS";
	char value[20];
	sprintf(value, "%.1f", location->getGNSSAltitude());
	message["gnss"]["altitude"] = value;
	sprintf(value, "%.3f", location->getGNSSLatitude());
	message["gnss"]["latitude"] = value;
	sprintf(value, "%.3f", location->getGNSSLongitude());
	message["gnss"]["longitude"] = value;
	message["utctime"]= utc_time;
	serialized_message = message.serialize();
	param.conn_m->sendDeviceToSystemMessage(serialized_message, MAX_ACCEPTABLE_CONNECT_DELAY);
}

void recoverQuotes(std::string &message) {
    for (auto it=message.begin(); it!=message.end();++it) {
        if (*it == '#') {
            it++;
            if (*it == '#') {
                *it = ' ';
            } else {
                it--;
                *it = '"';
            }
        }
    }
}

void checkConfig(std::string &message, TaskParameter &param)
{
	char json_buf[1458];
	int period;
	MbedJSONValue json_message;
    if (message.front() != '{') {
        return; // we only expect json data
    } else {
        recoverQuotes(message);
    }
    strcpy(json_buf, message.c_str());
    std::string err = parse(json_message, json_buf);
	if (err.empty()) {
		std::string type = json_message["Type"].get<std::string>();
		if (type.compare("CONFIG")==0){
			if (json_message.hasMember((char*)"GNSS_PERIOD")) {
				period = json_message["GNSS_PERIOD"].get<int>();
				if (period >10 && period < 3600) {
                    appmutex.lock();
					gnss_period_in_sec = period;
                    appmutex.unlock();
					printf("APP: The GPS tracking period is set to %d seconds\r\n", period);
				} else {
					printf("APP: Out of range value sent for the GPS tracking period.\r\n");
				}
			}
			if (json_message.hasMember((char*)"CONNECT_PERIOD")) {
				period = json_message["CONNECT_PERIOD"].get<int>();
				if (period > 360 && period < 86400) {
                    appmutex.lock();
					connect_period_in_sec = period;
                    appmutex.unlock();
					printf("APP: The IoT hub connect period is set to %d seconds\r\n", period);
				} else {
					printf("APP: Out of range value sent for the IoT hub connect period.\r\n");
				}

			}

		}
	}
}

void checkTimeouts()
{
	now += 30;
    if (now >= target_gnss_timeout) gnss_timeout = true;
}

void main_task(){
    GNSSLoc current_location;
    gnss_period_in_sec = GNSS_PERIOD_IN_SECONDS;
    connect_period_in_sec = CONNECT_PERIOD_IN_SECONDS;
    while(1) {
        while(!gnss_timeout) {
            sleep();
        }
        halfminuteticker.detach();
		if (loc_m.tryGetGNSSLocation(current_location, 3)) {
			log_m.logNewLocation(current_location);
            wait(0.2);
			app_m.processLocation(&current_location, locationProcess);
			if (time(NULL) - latest_connect_time > connect_period_in_sec) {
				if (conn_m.getSystemToDeviceMessage(system_message, MAX_ACCEPTABLE_CONNECT_DELAY)) {
					latest_connect_time = time(NULL);
					if (conn_m.checkSystemToDeviceMessage(system_message)) app_m.processSystemToDeviceMessage(system_message, checkConfig);
				}
			}
		} else {
			log_m.logLocationError();
			if (time(NULL) - latest_connect_time > connect_period_in_sec) {
				if (conn_m.getSystemToDeviceMessage(system_message, MAX_ACCEPTABLE_CONNECT_DELAY)){
					latest_connect_time = time(NULL);
					app_m.processSystemToDeviceMessage(system_message, checkConfig);
				} else {
					log_m.logConnectionError();
				}
			}
		}
		now = time(NULL);
        target_gnss_timeout = now + gnss_period_in_sec;
        gnss_timeout = false;
        halfminuteticker.attach(&checkTimeouts, 30);
    }
}

void checkAppInitialize(std::string &message, TaskParameter &param)
{
	if (message.compare("OK")==0) {
		initialized = true;
		printf("APP: received OK. Application starts tracking.\r\n");
	}

}

void app_run(void) {
    initialized = false;
    /*printf("First test json parser\r\n");
    std::string json_string = "{\"my_array\": [\"demo_string\", 10], \"my_boolean\": true}";
    char json_buf[80];
    strcpy(json_buf, json_string.c_str());
    MbedJSONValue json_object;
    std::string err = parse(json_object, json_buf);
    if (err.empty()) {
    	printf("Parsing is successful.\r\n");
    	wait(3);
    } else {
    	exit(-1);
    }*/

    bg96.doDebug(MBED_CONF_BG96_LIBRARY_BG96_DEBUG_SETTING);
    while(!initialized) { 
        if (conn_m.getSystemToDeviceMessage(system_message, MAX_ACCEPTABLE_CONNECT_DELAY)) {
            latest_connect_time = time(NULL);
            app_m.processSystemToDeviceMessage(system_message, checkAppInitialize);
        } else {
            log_m.logConnectionError();
        }
        if (!initialized) wait(30);
    }
	now = time(NULL);
    latest_connect_time = now;
    target_gnss_timeout = now + GNSS_PERIOD_IN_SECONDS;
    halfminuteticker.attach(&checkTimeouts, 30);
    main_task();
}

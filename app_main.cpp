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
#include "MbedJSONValue/MbedJSONValue.h"
#include "jsmn/jsmn.h"

#define CONNECT_PERIOD_IN_SECONDS 120
#define GNSS_PERIOD_IN_SECONDS 60

#if !defined(MAX_ACCEPTABLE_CONNECT_DELAY)
#define MAX_ACCEPTABLE_CONNECT_DELAY 120
#endif 

#if !defined(MBED_CONF_BG96_LIBRARY_BG96_DEBUG_SETTING)
#define MBED_CONF_BG96_LIBRARY_BG96_DEBUG_SETTING false
#endif

bool gnss_timeout;
time_t now;

std::string system_message;
std::string device_to_system_message;

time_t latest_connect_time;
time_t target_gnss_timeout;

LowPowerTicker halfminuteticker;
static Mutex bg96mutex;
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

static int jsoneq(const char *json, jsmntok_t *tok, const char *s) {
  if (tok->type == JSMN_STRING && (int)strlen(s) == tok->end - tok->start &&
      strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
    return 0;
  }
  return -1;
}

void locationProcess(GNSSLoc *location, AppManager *app_manager)
{
	if (app_manager == NULL) return;
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
	app_manager->sendDeviceToSystemMessage(serialized_message);
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

void checkConfig(std::string &message, AppManager *app_manager)
{
	char json_buf[1459] = {0};
	char valuebuf[16] = {0};
	int i;
  	int r;
	int value;
	char *pEnd;
  	jsmn_parser p;
  	jsmntok_t t[16]; /* We expect no more than 10 tokens */
	jsmn_init(&p);

	if (message.front() != '{') {
        return; // we only expect json data
    } else {
        recoverQuotes(message);
    }
	printf("Received the system to device message %s\r\n", message.c_str());
    strcpy(json_buf, message.c_str());
	r = jsmn_parse(&p, json_buf, strlen(json_buf), t, sizeof(t) / sizeof(t[0]));
	if (r < 0) {
    	printf("Failed to parse JSON: %d\n", r);
    	return;
  	}
	/* Assume the top-level element is an object */
  	if (r < 1 || t[0].type != JSMN_OBJECT) {
    	printf("Error: Message badly formed. Correct JSON object expected.\n");
    	return;
  	}
	
	for (i = 1; i < r; i++) {
		if (jsoneq(json_buf, &t[i], "Type") == 0) { // Key is Type
			char type[8];
			memcpy(type, json_buf + t[i+1].start, strlen("CONFIG")+1);
			if (strcmp(type, "CONFIG") == 0) {
				printf("Received a message of type CONFIG\r\n");
			}
		} else if (jsoneq(json_buf, &t[i], "GNSS_PERIOD") == 0) { // Key is GNSS_PERIOD
			memcpy(valuebuf, json_buf+t[i+1].start, t[i+1].end - t[i+1].start);
			value = strtol(valuebuf, &pEnd, 10);
			if (value > 10 && value <3600) {
				printf("APP: Now setting GNSS sampling period to %d\r\n", value);
				gnss_period_in_sec = value;
			} else {
				printf("APP: Provided GNSS sampling period %d is out of range.\r\n", value);
			}
		} else if (jsoneq(json_buf, &t[i], "CONNECT_PERIOD") == 0) { // Key is CONNECT_PERIOD
			memcpy(valuebuf, json_buf+t[i+1].start, t[i+1].end - t[i+1].start);
			value = strtol(valuebuf, &pEnd, 10);
			if (value > 360 && value < 86400) {
				printf("APP: Now setting the connect period to %d\r\n", value);
				connect_period_in_sec = value;
			} else {
				printf("APP: Out of range value %d sent for the IoT hub connect period.\r\n", value);
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
		if (app_m.getLocation(current_location)) {
			app_m.logLocation(current_location);
            wait(0.2);
			app_m.processLocation(&current_location, locationProcess);
			if (time(NULL) - latest_connect_time > connect_period_in_sec) {
				if (app_m.getSystemToDeviceMessage(system_message, MAX_ACCEPTABLE_CONNECT_DELAY)) {
					latest_connect_time = time(NULL);
					app_m.processSystemToDeviceMessage(system_message, checkConfig);
				}
			}
		} else {
			app_m.logLocationError();
			if (time(NULL) - latest_connect_time > connect_period_in_sec) {
				if (app_m.getSystemToDeviceMessage(system_message, MAX_ACCEPTABLE_CONNECT_DELAY)){
					latest_connect_time = time(NULL);
					app_m.processSystemToDeviceMessage(system_message, checkConfig);
				} else {
					app_m.logConnectionError();
				}
			}
		}
		wait(1);
		now = time(NULL);
        target_gnss_timeout = now + gnss_period_in_sec;
        gnss_timeout = false;
        halfminuteticker.attach(&checkTimeouts, 30);
    }
}

void checkAppInitialize(std::string &message, AppManager *app_m)
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
        if (app_m.getSystemToDeviceMessage(system_message, MAX_ACCEPTABLE_CONNECT_DELAY)) {
            latest_connect_time = time(NULL);
            app_m.processSystemToDeviceMessage(system_message, checkAppInitialize);
        } else {
            app_m.logConnectionError();
        }
        if (!initialized) wait(30);
    }
	now = time(NULL);
    latest_connect_time = now;
    target_gnss_timeout = now + GNSS_PERIOD_IN_SECONDS;
    halfminuteticker.attach(&checkTimeouts, 30);
    main_task();
}

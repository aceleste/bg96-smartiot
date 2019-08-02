#include "mbed.h"
#include "app_main.h"
#include <string>
#include "API/AppManager.h"
#include "API/ConnectionManager.h"
#include "API/LocationManager.h"
#include "API/LogManager.h"
#include "API/TemperatureManager.h"
#include "LowPowerTicker.h"

#define CONNECT_PERIOD_IN_SECONDS 900
#define GNSS_PERIOD_IN_SECONDS 300

bool gnss_timeout;

std::string system_message;
std::string device_to_system_message;

time_t latest_connect_time;
time_t target_gnss_timeout;

LowPowerTicker halfminuteticker;
static BG96Interface bg96;
static ConnectionManager conn_m(&bg96);
static LocationManager loc_m(&bg96);
static LogManager log_m(&bg96);
static AppManager app_m(&conn_m,
                 null,
                 &loc_m,
                 &log_m);


void main_task(){
    GNSSLoc current_location;
    latest_connect_time = time(NULL);
    while(1) {
        while(!gnss_timeout) {
            sleep();
        }
        if (loc_m.tryGetGNSSLocation(current_location, 3)) {
            log_m.logNewLocation(current_location);
            app_m.processLocation(current_location);
            if (time(NULL) - latest_connect_time > CONNECT_PERIOD_IN_SECONDS) {
                if (conn_m.sendAllMessages(&log_m, MAX_ACCEPTABLE_CONNECT_DELAY)) {
                    latest_connect_time = time(NULL);
                    if (conn_m.checkSystemToDeviceMessage(system_message)) app_m.processSystemToDeviceMessage(system_message);
                }
            }
        } else {
            log_m.logLocationError();
            if (time(NULL) - latest_connect_time > CONNECT_PERIOD_IN_SECONDS) {
                if (conn_m.getSystemToDeviceMessage(system_message, MAX_ACCEPTABLE_CONNECT_DELAY)){
                    latest_connect_time = time(NULL);
                    app_m.processSystemToDeviceMessage(system_message);
                } else {
                    log_m.logConnectionError();
                };
            }
        };
        target_gnss_timeout = time(NULL) + GNSS_PERIOD_IN_SECONDS;
        gnss_timeout = false;
    }
}


void checkTimeouts()
{
    time_t now = time(NULL);
    if (now >= target_gnss_timeout) gnss_timeout = true;
}

void app_run(void) {
    bool initialized = false;
    bg96.doDebug(MBED_CONF_BG96_LIBRARY_BG96_DEBUG_SETTING);
    while(!initialized) { 
        if (conn_m.getSystemToDeviceMessage(system_message, MAX_ACCEPTABLE_CONNECT_DELAY)) {
            latest_connect_time = time(NULL);
            app_m.processSystemToDeviceMessage(system_message);
            initialized = true;
        } else {
            log_m.logConnectionError();
        }
        wait(30);
    }
    target_gnss_timeout = time(NULL) + GNSS_PERIOD_IN_SECONDS;
    halfminuteticker.attach(&checkTimeouts, 30);
    main_task();
}

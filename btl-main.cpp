#include "mbed.h"
#include "btl_main.h"
#include <string>
#include "BTLManager.h"
#include "ConnectionManager.h"
#include "LocationManager.h"
#include "BTLLogManager.h"
#include "TemperatureManager.h"
#include "LowPowerTicker.h"

#define CONNECT_PERIOD_IN_SECONDS 900
#define GNSS_PERIOD_IN_SECONDS 300
#define TEMPERATURE_PERIOD_IN_SECONDS 60

bool gnss_timeout;
bool temperature_timeout;
double temperatures[MAX_PROBES]; //temperatures[0] -> Container; temperatures[1] -> Heater

std::string system_message;
std::string device_to_system_message;

time_t latest_connect_time;
time_t target_gnss_timeout;
time_t target_temperature_timeout;

LowPowerTicker halfminuteticker;
LowPowerTicker temperatureticker;
static BG96Interface bg96;
static ConnectionManager conn_m(&bg96);
static LocationManager loc_m(&bg96);
static BTLLogManager log_m(&bg96);
static TemperatureManager temp_m;
static BTLManager btl_m(&conn_m,
                 &temp_m,
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
            btl_m.processLocation(current_location);
            if (time(NULL) - latest_connect_time > CONNECT_PERIOD_IN_SECONDS) {
                if (conn_m.sendAllMessages(&log_m, MAX_ACCEPTABLE_CONNECT_DELAY)) {
                    latest_connect_time = time(NULL);
                    if (conn_m.checkSystemToDeviceMessage(system_message)) btl_m.processSystemToDeviceMessage(system_message);
                }
            }
        } else {
            log_m.logLocationError();
            if (time(NULL) - latest_connect_time > CONNECT_PERIOD_IN_SECONDS) {
                if (conn_m.getSystemToDeviceMessage(system_message, MAX_ACCEPTABLE_CONNECT_DELAY)){
                    latest_connect_time = time(NULL);
                    btl_m.processSystemToDeviceMessage(system_message);
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

void btl_run(void) {
    bool initialized = false;
    while(!initialized) { 
        if (conn_m.getSystemToDeviceMessage(system_message, MAX_ACCEPTABLE_CONNECT_DELAY)) {
            latest_connect_time = time(NULL);
            btl_m.processSystemToDeviceMessage(system_message);
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
#include "mbed.h"
#include <string>
#include "BTLManager.h"
#include "ConnectionManager.h"
#include "LocationManager.h"
#include "LogManager.h"
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
BG96Interface bg96;
BTLManager btl_m;
ConnectionManager conn_m(&bg96);
LocationManager loc_m(&bg96);
LogManager log_m(&bg96);
TemperatureManager temp_m;

void main_task(){
    GNSSLoc current_location;
    while(1) {
        while(!gnss_timeout) {
            sleep();
        }
        if (loc_m.tryGetGNSSLocation(current_location, 3)) {
            log_m.logNewLocation(current_location);
            if (btl_m.processLocation(current_location) == true) {
                btl_m.updateDeviceToSystemMessage(device_to_system_message);
                if (conn_m.sendDeviceToSystemMessage(device_to_system_message, MAX_ACCEPTABLE_CONNECT_DELAY)) {
                    latest_connect_time = time(NULL);
                    if (conn_m.checkSystemToDeviceMessage(system_message)) btl_m.processSystemToDeviceMessage(system_message);
                }
            };
        } else {
            log_m.logLocationError();
            if (time(NULL) - latest_connect_time > CONNECT_PERIOD_IN_SECONDS) {
                if (conn_m.getSystemToDeviceMessage(system_message)){
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

void temperature_task(){
    while(1) {
        while(!temperature_timeout) {
            sleep();
        }
        temp_m.updateTemperatures(temperatures);
        if (btl_m.processTemperatures(temperatures)) {
            btl_m.updateDeviceToSystemMessage(device_to_system_message);
            if (conn_m.sendDeviceToSystemMessage(device_to_system_message, MAX_ACCEPTABLE_CONNECT_DELAY)) latest_connect_time = time(NULL);
        }
        target_temperature_timeout = time(NULL) + TEMPERATURE_PERIOD_IN_SECONDS;
        temperature_timeout = false;
    }
}

void checkTimeouts()
{
    time_t now = time(NULL);
    if (now >= target_gnss_timeout) gnss_timeout = true;
    if (now >= target_temperature_timeout) temperature_timeout = true;
}

void btl_run() {
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
    target_temperature_timeout = time(NULL) + TEMPERATURE_PERIOD_IN_SECONDS;
    halfminuteticker.attach(&checkTimeouts, 30);
    main_task();
}
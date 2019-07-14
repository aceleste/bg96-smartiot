#ifndef __LOCATION_MANAGER_H__
#define __LOCATION_MANAGER_H__
#include "mbed.h"
#include <string>
#include "GNSSLoc.h"
#include "BG96Interface.h"

class LocationManager
{
public:
    LocationManager(BG96Interface *bg96);
    ~LocationManager();
    bool tryGetGNSSLocation(GNSSLoc &current_location, int tries);
    void getCurrentLatitude(double &latitude);
    void getCurrentLongitude(double &longitude);
    void getCurrentUTCTime(std::string &utc_time);
private:
    bool getGNSSLocation(GNSSLoc &current_location);
    Timer _timeout;
    BG96Interface *_bg96;
    GNSSLoc _current_loc;
};

#endif //__LOCATION_MANAGER_H__
#include "LocationManager.h"
#include "GNSSLoc.h"
#include "BG96Interface.h"
#include <string.h>

LocationManager::LocationManager(BG96Interface *bg96)
{
    _bg96 = bg96;
}

LocationManager::~LocationManager()
{
}

bool LocationManager::tryGetGNSSLocation(GNSSLoc &current_location, int tries)
{
    bool done;
    _bg96->initializeGNSS();
    _bg96->disallowPowerOff();
    for (int i = 0; i < tries; i++) {
        if ((done = getGNSSLocation(current_location)) == true) break;
    }
    _current_loc = current_location;
    _bg96->allowPowerOff();
    _bg96->powerDown();
    return done;
}

bool LocationManager::getGNSSLocation(GNSSLoc &current_location)
{
    return _bg96->getGNSSLocation(current_location);
}

void LocationManager::getCurrentLatitude(double &latitude)
{
   latitude = _current_loc.getGNSSLatitude();
}

void LocationManager::getCurrentLongitude(double &longitude)
{
    longitude = _current_loc.getGNSSLongitude();
}

void LocationManager::getCurrentUTCTime(std::string &utc_time)
{
    time_t loctime = _current_loc.getGNSSTime();
    utc_time = ctime(&loctime);
}




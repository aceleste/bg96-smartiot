#include "LocationManager.h"
#include "GNSSLoc.h"
#include "BG96Interface.h"
#include <string.h>

LocationManager::LocationManager(BG96Interface *bg96, Mutex * bg96mutex)
{
    _bg96 = bg96;
    _loc_m_mutex = bg96mutex;
}

LocationManager::~LocationManager()
{
}

bool LocationManager::tryGetGNSSLocation(GNSSLoc &current_location, int tries)
{
    bool done;
    _loc_m_mutex->lock();
    _bg96->initializeGNSS();
    _bg96->disallowPowerOff();
    for (int i = 0; i < tries; i++) {
        if ((done = getGNSSLocation(current_location)) == true) break;
    }
    _current_loc = current_location;
    _bg96->allowPowerOff();
    _bg96->powerDown();
    wait(1);
    _loc_m_mutex->unlock();
    return done;
}

bool LocationManager::getGNSSLocation(GNSSLoc &current_location)
{
    bool rc;
    _loc_m_mutex->lock();
    rc = _bg96->getGNSSLocation(current_location);
    _loc_m_mutex->unlock();
    return rc;
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




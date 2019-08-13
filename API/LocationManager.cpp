/**
 * @file LocationManager.cpp
 * @author Alain CELESTE (alain.celeste@polaris-innovation.com)
 * @brief 
 * @version 0.1
 * @date 2019-08-13
 * 
 * @copyright Copyright (c) 2019 Polaris Innovation 
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

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




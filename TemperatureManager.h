#ifndef __TEMPERATURE_MANAGER_H__
#define __TEMPERATURE_MANAGER_H__
#include "mbed.h"
#include <string>
#if !defined(DEVBOARD)
#include "DS1820.h"
#include "epd1in54.h"
#endif


#define MAX_PROBES 16
#define AMBIANT_TEMPERATURE_IDX     0
#define CONTAINER_TEMPERATURE_IDX   1
#define HEATER_TEMPERATURE_IDX      2

class TemperatureManager
{
public:
    TemperatureManager();
    ~TemperatureManager();

    void updateTemperatures(double (&temperatures)[MAX_PROBES]);
    void displayTemperature(double temperature);
private:
    double getTemp(int device);
    int Display(double t);
#if !defined(DEVBOARD)
    Epd _epd;
    DS1820* _probe[MAX_PROBES];
#endif
};

#endif
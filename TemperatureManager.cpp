#include "mbed.h"
#include "TemperatureManager.h"
#if !defined(DEVBOARD)
#include "DS1820.h"

#include "Utilities/Fonts/fonts.h"

//Temp sensors

#define DATA_PIN        PB_2

//E-ink Display
PinName rst; PinName dc; PinName busy; PinName mosi; PinName miso; PinName sclk; PinName cs;
unsigned char frame_black[EPD_HEIGHT*EPD_WIDTH/8];
char cValt[32];

TemperatureManager::TemperatureManager():
    _epd(PB_5, PB_4, PB_3, PA_8, PC_4, PC_7, PB_10);
{
    DigitalOut Epd_EN(PA_8);            //Epd SPI enable (active low)
    int num_devices = 0;
    while(DS1820::unassignedProbe(DATA_PIN)) {
        _probe[num_devices] = new DS1820(DATA_PIN);
        if (_probe[num_devices] == NULL) {
            printf("TemperatureManager: Error NOEM.\r\n");
            break;
        }
        num_devices++;
        if (num_devices == MAX_PROBES)
            break;
    }
}
#else
TemperatureManager::TemperatureManager()
{

}
#endif

#if !defined(DEVBOARD)
TemperatureManager::~TemperatureManager()
{
    for (int i = 0; i< MAX_PROBES; i++) {
        if (_probe[i]!= NULL) delete(_probe[i]);
    }
}
#else
TemperatureManager::~TemperatureManager()
{

}
#endif

#if !defined(DEVBOARD)
double TemperatureManager::getTemp(int device)
{
    double temp;
    _probe[0]->convertTemperature(true, DS1820::all_devices);         //Start temperature conversion, wait until ready
    temp = _probe[device]->temperature();
    return temp;
}
#else
double TemperatureManager::getTemp(int device)
{   
    double temp = 25.5;
    return temp;
}
#endif

void TemperatureManager::updateTemperatures(double (&temperatures)[MAX_PROBES])
{
    for (int i = 0; i < MAX_PROBES; i++) {
        temperatures[i] = getTemp(i);
    }
    Display(temperatures[0]);
}

int TemperatureManager::Display(double t)
{   
#if !defined(DEVBOARD)
    sprintf(cValt,"%.2f", t);    
    memset(frame_black, 0xFF, sizeof(unsigned char)*EPD_HEIGHT*EPD_WIDTH/8);
    if (_epd.Init(lut_full_update) != 0) {
        return -1;
    }
    //Write strings to the buffer 
    _epd.DrawStringAt(frame_black, 0, 40, cValt, &Font72, COLORED);
    _epd.DrawStringAt(frame_black, 160, 120, "C", &Font24, COLORED);
   
    // Display the frame_buffer 
    _epd.SetFrameMemory(frame_black, 0, 0, _epd.width, _epd.height);
    _epd.DisplayFrame();
    _epd.Sleep();
    return 0;
#else 
    printf("Temperature is: %.2f\r\n",t);
    return 0;
#endif
}

void TemperatureManager::displayTemperature(double temperature)
{
    Display(temperature);
}
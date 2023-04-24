#ifndef FuelMonitor_H
#define FuelMonitor_H

#include <Arduino.h>

#define FUEL_SAMPLE_SIZE 100

class FuelMonitor {

  private:
    const char* name;
    
    byte fuelLevel = 0;
    byte fuelLevelArray[FUEL_SAMPLE_SIZE];

    int minValueAllowed;
    int maxValueAllowed;    
    int pin;    
    int SampleCount = 0;
    int sampleIndex = 0;
    int fuelAnalog = 0;

  public:
    FuelMonitor (const char *name, int pin, int minValue, int maxValue);
    void begin();
    void loop();
    byte getFuelLevel();
    byte getLast();
    bool initialized();
};

#endif
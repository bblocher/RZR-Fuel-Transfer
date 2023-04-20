#ifndef FuelMonitor_H
#define FuelMonitor_H

#include <Arduino.h>

#define FUEL_SAMPLE_SIZE 100
#define DEBUG_FUEL

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
    int minValueSeen = 0xFFFF;
    int maxValueSeen = 0;

  public:
    FuelMonitor (const char *name, int pin, int minValue, int maxValue);
    void begin();
    void loop();
    byte getFuelLevel();
    bool initialized();
    int getMinValue();
    int getMaxValue();
};

#endif
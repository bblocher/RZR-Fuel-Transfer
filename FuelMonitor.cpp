#include "FuelMonitor.h"

FuelMonitor::FuelMonitor(const char *name, int pin, int minValue, int maxValue) {
    this->name = name;
    this->pin = pin;
    this->minValueAllowed = minValue;
    this->maxValueAllowed = maxValue;
}

void FuelMonitor::begin() {
  // Configuring pin for fuel pump output
  pinMode(pin, OUTPUT);
}

void FuelMonitor::loop() {
  // Read analog input
  int fuelAnalog = analogRead(pin);

  if (fuelAnalog < minValueSeen) {
    minValueSeen = fuelAnalog;
  }
  if (fuelAnalog > maxValueSeen) {
    maxValueSeen = fuelAnalog;
  }

  // Standardize the analog input
  if (fuelAnalog < minValueAllowed) {
    fuelAnalog = minValueAllowed;
  } else if (fuelAnalog > maxValueAllowed) {
    fuelAnalog = maxValueAllowed;
  }
  
  // Add the new sample to the array
  fuelLevelArray[sampleIndex++] = map(fuelAnalog, minValueAllowed, maxValueAllowed, 100, 0);
  if (sampleIndex >= FUEL_SAMPLE_SIZE) {
    sampleIndex = 0;
  }
  
  // Increment the sample count
  SampleCount++;
  if (SampleCount > FUEL_SAMPLE_SIZE) {
    SampleCount = FUEL_SAMPLE_SIZE;
  }

  // Calculate the average
  long sum = 0;
  int samples = min(SampleCount, FUEL_SAMPLE_SIZE);

  for (int i = 0; i < samples; i++) {
    sum += fuelLevelArray[i];
  }
  int avg = sum / samples;

  // Sanity check the average
  if (avg < 0 || avg > UINT8_MAX) {
        sampleIndex = 0;
        SampleCount = 0;
        avg = 0;
  }

  fuelLevel = avg;

  #ifdef DEBUG_FUEL
    Serial.print(name);
    Serial.print(" Fuel Level:");
    Serial.print(fuelAnalog);
    Serial.print(" - ");
    Serial.println(fuelLevel);
  #endif
}

byte FuelMonitor::getFuelLevel() {
    return fuelLevel;
}

bool FuelMonitor::initialized() {
    return SampleCount >= FUEL_SAMPLE_SIZE;
}

int FuelMonitor::getMinValue() {
    return minValueSeen;
}

int FuelMonitor::getMaxValue() {
    return maxValueSeen;
}
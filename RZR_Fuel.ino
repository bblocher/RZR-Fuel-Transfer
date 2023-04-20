#include <Arduino.h>
#include <SPI.h>
#include "AM_BleNINA.h"
#include "FuelMonitor.h"

#define DEVICE_NAME "RZR_FUEL"

// Define the debug flags
// #define DEBUG_AUX
// #define DEBUG_BLE

#ifdef DEBUG_AUX || DEBUG_BLE
  #define DEBUG
#endif

// Define the constants for the primary fuel level sensor
#define PRI_FUEL_LEVEL_PIN A0
#define PRI_FUEL_LEVEL_MIN 125
#define PRI_FUEL_LEVEL_MAX 450

// Define the constants for the auxillary fuel level sensor
#define AUX_FUEL_LEVEL_PIN A1
#define AUX_FUEL_LEVEL_MIN 125
#define AUX_FUEL_LEVEL_MAX 450

// Define the constants for fuel level transfer
#define FUEL_TRANSFER_MAX 85
#define FUEL_TRANSFER_THRESHOLD 10

#define PUMP_PIN 8

bool bleStatus = false;
bool pumpOn = false;
bool manualPumpOn = false;

FuelMonitor primaryFuelLevel("Primary", PRI_FUEL_LEVEL_PIN, PRI_FUEL_LEVEL_MIN, PRI_FUEL_LEVEL_MAX);
FuelMonitor auxilaryFuelLevel("Auxilary", AUX_FUEL_LEVEL_PIN, AUX_FUEL_LEVEL_MIN, AUX_FUEL_LEVEL_MAX);

void doWork();
void doSync();
void processIncomingMessages(char *variable, char *value);
void processOutgoingMessages();
void deviceConnected();
void deviceDisconnected();
bool shouldTransferFuel(bool transferring);

AMController amController(&doWork,&doSync,&processIncomingMessages,&processOutgoingMessages,&deviceConnected,&deviceDisconnected);

void setup()
{  
  #ifdef DEBUG
  Serial.begin(115200);
  Serial.println("RZR Fuel Controller");
  Serial.println("Initializing...");
  #endif
  
  // Initialize the BLE module
  bleStatus = amController.begin(DEVICE_NAME);

  // Initialize the fuel level sensors
  primaryFuelLevel.begin();
  auxilaryFuelLevel.begin();

  #ifdef DEBUG
  Serial.println("Setup Complete");
  #endif
}

void loop()
{
  // Read the fuel levels
  primaryFuelLevel.loop();
  auxilaryFuelLevel.loop();

  // Process the BLE module
  amController.loop(100);

  // Check if we should transfer the fuel
  pumpOn = manualPumpOn || shouldTransferFuel(pumpOn);
  digitalWrite(PUMP_PIN, pumpOn);
}

bool shouldTransferFuel(bool transferring)
{
  // Check we've had enough samples to determine the average fuel level
  if (!primaryFuelLevel.initialized() || !auxilaryFuelLevel.initialized())
    return false;

    // Get the fuel levels
  byte priFuelLevel = primaryFuelLevel.getFuelLevel();
  byte auxFuelLevel = auxilaryFuelLevel.getFuelLevel();
    
  // Check if the primary fuel level is too high to transfer
  if (priFuelLevel >= FUEL_TRANSFER_MAX)
    return false;

  // Check if the aux fuel level is too low to transfer
  if (auxFuelLevel == 0)
    return false;

  // If not transferring, and primary tank has less more than aux tank by the threshold, start transferring (ignore if the primary fuel level is too low)
  if (!transferring && priFuelLevel > FUEL_TRANSFER_THRESHOLD && priFuelLevel + FUEL_TRANSFER_THRESHOLD > auxFuelLevel)
    return false;

  // If transferring, and the primary tank is fuller than the aux tank by the threshold, stop transferring
  if (transferring && priFuelLevel - FUEL_TRANSFER_THRESHOLD > auxFuelLevel)
    return false;

  // If we've made it this far, we should transfer fuel
  return true;
}

void sendPrimaryFuelLevel() {
  amController.writeMessage("priFuelLevel", primaryFuelLevel.getFuelLevel());
}

void sendAuxFuelLevel() {
  amController.writeMessage("auxFuelLevel", auxilaryFuelLevel.getFuelLevel());
}

void sendPumpOnState() {
  amController.writeMessage("pumpOn", pumpOn);
}

void sendManualPumpOnState() {
  amController.writeMessage("manualPumpOn", manualPumpOn);
}

void sendStatus() {
  amController.writeMessage("initStatus", primaryFuelLevel.initialized() && auxilaryFuelLevel.initialized());
  amController.writeMessage("bleStatus", bleStatus);
}

/**
*
*
* This function is called periodically and it is equivalent to the standard loop() function
*
*/
void doWork() {
  amController.logLn("Doing work...");
}

/**
*
*
* This function is called when the ios device connects and needs to initialize the position of switches and knobs
*
*/
void doSync() {
  sendPrimaryFuelLevel();
  sendAuxFuelLevel();
  sendPumpOnState();
}

/**
*
*
* This function is called when a new message is received from the iOS device
*
*/
void processIncomingMessages(char *variable, char *value) {
  if (strcmp(variable,"manualPumpOn")==0) {
    manualPumpOn = atoi(value) == 1;
  }
}

/**
*
*
* This function is called periodically and messages can be sent to the iOS device
*
*/
void processOutgoingMessages() {
  sendPrimaryFuelLevel();
  sendAuxFuelLevel();
  sendPumpOnState();
  sendManualPumpOnState();
  sendStatus();

  // Debug
  amController.writeMessage("priMin", primaryFuelLevel.getMinValue());
  amController.writeMessage("priMax", primaryFuelLevel.getMaxValue());
  amController.writeMessage("auxMin", auxilaryFuelLevel.getMinValue());
  amController.writeMessage("auxMax", auxilaryFuelLevel.getMaxValue());
}

void deviceConnected() {
  Serial.println("deviceConnected");
}

void deviceDisconnected() {
  Serial.println("deviceDisconnected");
}
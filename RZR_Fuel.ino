#include <Arduino.h>
#include <mcp_can.h>
#include <SPI.h>
#include "AM_BleNINA.h"

#define DEVICE_NAME "RZR_FUEL"

// Define the debug flags
// #define DEBUG_AUX
// #define DEBUG_CAN
// #define DEBUG_BLE

#ifdef DEBUG_AUX || DEBUG_CAN || DEBUG_BLE
  #define DEBUG
#endif

// Define MCP2515 (CAN BUS) PINS
#define CAN0_INT 2
#define CAN0_CS 10

// Define the constants for the auxillary fuel level sensor
#define AUX_FUEL_LEVEL_PIN A0
#define AUX_FUEL_LEVEL_MIN 125
#define AUX_FUEL_LEVEL_MAX 450
#define FUEL_SAMPLE_SIZE 100

// Define the constants for fuel level transfer
#define FUEL_TRANSFER_MAX 75
#define FUEL_TRANSFER_THRESHOLD 10

#define PUMP_PIN 8

bool canStatus = false;
bool bleStatus = false;
bool pumpOn = false;
bool manualPumpOn = false;
byte priFuelLevel = 0;
byte auxFuelLevel = 0;
byte auxFuelLevelArray[FUEL_SAMPLE_SIZE];
int auxSampleCount = 0;
int auxSampleIndex = 0;
int minValue = 1024;
int maxValue = 0;

void doWork();
void doSync();
void processIncomingMessages(char *variable, char *value);
void processOutgoingMessages();
void deviceConnected();
void deviceDisconnected();
void readPrimaryFuelLevel();
void readAuxFuelLevel();
bool shouldTransferFuel(bool transferring);

MCP_CAN CAN0(CAN0_CS);
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

  // Initialize MCP2515 running at 8MHz with a baudrate of 256kb/s and the masks and filters disabled.
  canStatus = CAN0.begin(MCP_ANY, CAN_250KBPS, MCP_8MHZ) == CAN_OK;

  #ifdef DEBUG_CAN
    if (canStatus)
      Serial.println("MCP2515 Initialized Successfully!");
    else
      Serial.println("Error Initializing MCP2515...");
  #endif
  
  // Set operation mode to normal so the MCP2515 sends acks to received data.
  CAN0.setMode(MCP_LISTENONLY);

  // Configuring pin for CAN BUS interupt input
  pinMode(CAN0_INT, INPUT);

  // Configuring pin for fuel pump output
  pinMode(PUMP_PIN, OUTPUT);

  #ifdef DEBUG
  Serial.println("Setup Complete");
  #endif
}

void loop()
{
  // Read the fuel levels
  readPrimaryFuelLevel();
  readAuxFuelLevel();

  amController.loop(100);

  // Check if we should transfer the fuel
  pumpOn = manualPumpOn;// || shouldTransferFuel(pumpOn);
  digitalWrite(PUMP_PIN, pumpOn);
}

void readPrimaryFuelLevel()
{
  long unsigned int rxId;
  unsigned char len = 0;
  unsigned char rxBuf[8];
  char msgString[128];

  // If CAN0_INT pin is low, read receive buffer
  if(!digitalRead(CAN0_INT))
  {
    // Read data: len = data length, buf = data byte(s)
    //CAN0.readMsgBuf(&rxId, &len, rxBuf);
    
    if((rxId & 0x80000000) == 0x80000000)     // Determine if ID is standard (11 bits) or extended (29 bits)
      sprintf(msgString, "Extended ID: 0x%.8lX  DLC: %1d  Data:", (rxId & 0x1FFFFFFF), len);
    else
      sprintf(msgString, "Standard ID: 0x%.3lX       DLC: %1d  Data:", rxId, len);
  
    #ifdef DEBUG_CAN
       Serial.print(msgString);
    #endif
  
    // Determine if message is a remote request frame.
    if((rxId & 0x40000000) == 0x40000000){
      sprintf(msgString, " REMOTE REQUEST FRAME");
      #ifdef DEBUG_CAN
        Serial.print(msgString);
      #endif
    } else {

      switch (rxId & 0x1FFFFFFF)
      {
        case 0x18FEFC17:
          priFuelLevel = map(rxBuf[1],0,255,0,100);
          break;
        
        default:
          break;
      }

      #ifdef DEBUG_CAN
        for(byte i = 0; i<len; i++){
          sprintf(msgString, " 0x%.2X", rxBuf[i]);
          Serial.print(msgString);
        }
      #endif
    }
    
    #ifdef DEBUG_CAN
      Serial.println();
    #endif
  }
}

void readAuxFuelLevel()
{
  // Read analog input
  int auxFuelAnalog = analogRead(AUX_FUEL_LEVEL_PIN);

  if (auxFuelAnalog < minValue) {
    minValue = auxFuelAnalog;
  }
  if (auxFuelAnalog > maxValue) {
    maxValue = auxFuelAnalog;
  }

  // Standardize the analog input
  if (auxFuelAnalog < AUX_FUEL_LEVEL_MIN) {
    auxFuelAnalog = AUX_FUEL_LEVEL_MIN;
  } else if (auxFuelAnalog > AUX_FUEL_LEVEL_MAX) {
    auxFuelAnalog = AUX_FUEL_LEVEL_MAX;
  }
  
  // Add the new sample to the array
  auxFuelLevelArray[auxSampleIndex++] = map(auxFuelAnalog, AUX_FUEL_LEVEL_MIN, AUX_FUEL_LEVEL_MAX, 100, 0);
  if (auxSampleIndex >= FUEL_SAMPLE_SIZE) {
    auxSampleIndex = 0;
  }
  
  // Increment the sample count
  auxSampleCount++;
  if (auxSampleCount > FUEL_SAMPLE_SIZE) {
    auxSampleCount = FUEL_SAMPLE_SIZE;
  }

  // Calculate the average
  long sum = 0;
  int samples = min(auxSampleCount, FUEL_SAMPLE_SIZE);

  for (int i = 0; i < samples; i++) {
    sum += auxFuelLevelArray[i];
  }
  int avg = sum / samples;

  // Sanity check the average
  if (avg < 0 || avg > UINT8_MAX) {
        auxSampleIndex = 0;
        auxSampleCount = 0;
        avg = 0;
  }

  auxFuelLevel = avg;

  #ifdef DEBUG_AUX
    Serial.print("Aux Fuel Level:");
    Serial.print(auxFuelAnalog);
    Serial.print(" - ");
    Serial.println(auxFuelLevel);
  #endif
}

bool shouldTransferFuel(bool transferring)
{
  // Check we've had enough samples to determine the average fuel level
  if (auxSampleCount < FUEL_SAMPLE_SIZE)
    return false;
  
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
  amController.writeMessage("priFuelLevel", priFuelLevel);
}

void sendAuxFuelLevel() {
  amController.writeMessage("auxFuelLevel", auxFuelLevel);
}

void sendPumpOnState() {
  amController.writeMessage("pumpOn", pumpOn);
}

void sendManualPumpOnState() {
  amController.writeMessage("manualPumpOn", manualPumpOn);
}

void sendStatus() {
  amController.writeMessage("initStatus", (int) map(auxSampleCount, 0, FUEL_SAMPLE_SIZE, 0, 100));
  amController.writeMessage("canStatus", canStatus);
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

  amController.writeMessage("min", minValue);
  amController.writeMessage("max", maxValue);
}

void deviceConnected() {
  Serial.println("deviceConnected");
}

void deviceDisconnected() {
  Serial.println("deviceDisconnected");
}
/*********
  Rui Santos
  Complete instructions at https://RandomNerdTutorials.com/esp32-ble-server-client/
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*********/

#include "BLEDevice.h"
#include <Wire.h>

#define RXD2 16
#define TXD2 17

#define RXD0 3
#define TXD0 1

String toSend = "#";
 
//BLE Server name (the other ESP32 name running the server sketch)
#define shefleraServerName "Sheflera_ESP32"
#define dracenaServerName "Dracena_ESP32"

long int conectionCount = 0;
/* UUID's of the service, characteristic that we want to read*/
static BLEUUID dracenaServiceUUID ("6f7f4996-cc94-11ec-9d64-0242ac120002");
static BLEUUID shefleraServiceUUID("f3e2d44a-c0a1-11ec-9d64-0242ac120002");
//Characteristics
static BLEUUID soilCharacteristicUUID("ca73b3ba-39f6-4ab3-91ae-186dc9577d99");
static BLEUUID tempCharacteristicUUID("91bad492-b950-4226-aa2b-4ede9fa42f59");
static BLEUUID statCharacteristicUUID("beb5483e-36e1-4688-b7f5-ea07361b26a8");
static BLEUUID humCharacteristicUUID("944710ea-d46b-11ec-9d64-0242ac120002");

int scanTime = 5; //In seconds
BLEScan* pBLEScan;

//Flags stating if should begin connecting and if the connection is up
static boolean doConnect = false;
static boolean connected = false;
static boolean shefleraConnected = false;
static boolean dracenaConnected = false;
static boolean shefleraFound = false;
static boolean dracenaFound = false;

//Address of the peripheral device. Address will be found during scanning...
static BLEAddress *dracenaServerAddress;
static BLEAddress *shefleraServerAddress;
 
//Characteristicd that we want to read
BLERemoteCharacteristic* soilCharacteristic;
BLERemoteCharacteristic* tempCharacteristic;
BLERemoteCharacteristic* statCharacteristic;
BLERemoteCharacteristic* humCharacteristic;

static BLEAdvertisedDevice* myDevice;
static BLEClient* pClient;

//Activate notify
const uint8_t notificationOn[] = {0x1, 0x0};
const uint8_t notificationOff[] = {0x0, 0x0};

//Variables to store temperature and humidity
float soilChar;
float tempChar;
float humChar;

float dracenaSoilChar;
float dracenaTempChar;
float dracenaHumChar;

//Flags to check whether new temperature and humidity readings are available
boolean newSoil = false;
boolean newTemp = false;
boolean newHum = false;

float startTime=0;
float endTime=0;

//Connect to the BLE Server that has the name, Service, and Characteristics
bool connectToServer(BLEAddress pAddress,BLEUUID pServiceUUID) {
  // Connect to the remove BLE Server.
  Serial.println("Connecting...");
  pClient->connect(pAddress);
  Serial.println(" - Connected to server");
 
  // Obtain a reference to the service we are after in the remote BLE server.
  BLERemoteService* pRemoteService = pClient->getService(pServiceUUID);
  if (pRemoteService == nullptr) {
    Serial.print("Failed to find our service UUID: ");
    Serial.println(pServiceUUID.toString().c_str());
    return (false);
  }
 
  // Obtain a reference to the characteristics in the service of the remote BLE server.
  soilCharacteristic = pRemoteService->getCharacteristic(soilCharacteristicUUID);
  tempCharacteristic = pRemoteService->getCharacteristic(tempCharacteristicUUID);
  humCharacteristic = pRemoteService->getCharacteristic(humCharacteristicUUID);
  statCharacteristic = pRemoteService->getCharacteristic(statCharacteristicUUID);
  
  if (soilCharacteristic == nullptr || tempCharacteristic == nullptr || statCharacteristic == nullptr) {
    Serial.println("Failed to find our characteristic UUID");
    pClient->disconnect();
    return false;
  }
  Serial.println(" - Found our characteristics");

  if(statCharacteristic->canRead())
  {
    std::string value = statCharacteristic->readValue();
    Serial.print("The characteristic value was: ");
    Serial.println(value.c_str());
  }

  if(statCharacteristic->canNotify())
  {
    statCharacteristic->registerForNotify(statNotifyCallback);

  }
  
  //Assign callback functions for the Characteristics
  soilCharacteristic->registerForNotify(soilNotifyCallback);
  tempCharacteristic->registerForNotify(tempNotifyCallback);
  humCharacteristic->registerForNotify(humNotifyCallback);
  return true;
}

//Callback function that gets called, when another device's advertisement has been received
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    String s1 = advertisedDevice.toString().c_str();
    //Serial.println("Advertised Device:"+ s1 + "\n");
    if (s1.indexOf(shefleraServiceUUID.toString().c_str()) > 0 && !dracenaFound) { //Check if the name of the advertiser matches
      //Scan can be stopped, we found what we are looking for
      advertisedDevice.getScan()->stop();
      shefleraServerAddress = new BLEAddress(advertisedDevice.getAddress()); //Address of advertiser is the one we need
      doConnect = true; //Set indicator, stating that we are ready to connect
      shefleraFound = true;
      Serial.println("Sheflera device found. Connecting!");
    }
    else if (s1.indexOf(dracenaServiceUUID.toString().c_str()) > 0 && !shefleraFound) { //Check if the name of the advertiser matches
       //Scan can be stopped, we found what we are looking for
      advertisedDevice.getScan()->stop();
      dracenaServerAddress = new BLEAddress(advertisedDevice.getAddress()); //Address of advertiser is the one we need
      doConnect = true; //Set indicator, stating that we are ready to connect
      dracenaFound = true;
      
      Serial.println("Dracena device found. Connecting!");
    }
  }
};
 
//When the BLE Server sends a new temperature reading with the notify property
static void soilNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristicSoil, 
                                        uint8_t* pSoil, size_t length, bool isNotify) {
  //store temperature value
  if(shefleraConnected){
    soilChar = atof((char*)pSoil);
    Serial.println(soilChar);
  }
  else if(dracenaConnected){
    dracenaSoilChar = atof((char*)pSoil);
    //Serial.println(dracenaSoilChar);
  }
  newSoil = true;
  
}

static void statNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic,
                            uint8_t* pStat, size_t length, bool isNotify)
{
  Serial.print("Notify callback for characteristic ");
  Serial.print(statCharacteristic->getUUID().toString().c_str());
  Serial.print(" of data length ");
  Serial.println(length);
  Serial.print("data: ");
  Serial.println((char*)pStat);
}

//When the BLE Server sends a new humidity reading with the notify property
static void tempNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristicData, 
                                    uint8_t* pData, size_t length, bool isNotify) {

  //store humidity value
  if(shefleraConnected){
    
    tempChar = atof((char*)pData);
    Serial.println(tempChar);
  }
  else if(dracenaConnected){
    dracenaTempChar = atof((char*)pData);
    //Serial.println(dracenaTempChar);
  }
  newTemp = true;
  
}

static void humNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristicData, 
                                    uint8_t* pHum, size_t length, bool isNotify) {

  //store humidity value
  if(shefleraConnected){
    
    humChar = atof((char*)pHum);
    Serial.println(humChar);
  }
  else if(dracenaConnected){
    dracenaHumChar = atof((char*)pHum);
    //Serial.println(dracenaTempChar);
  }
  newHum = true;
  
}

void pintData()
  { 
    toSend = "#";
    toSend.concat(soilChar);
    toSend.concat("*");
    if (Serial2.available()){
      Serial2.println(toSend); 
      delay(100);
    }
    Serial.println(toSend);

    toSend = "$";
    toSend.concat(tempChar);
    toSend.concat("*");
    if (Serial2.available()){
      Serial2.println(toSend); 
      delay(100);
    }
    Serial.println(toSend);

    toSend = "@";
    toSend.concat(humChar);
    toSend.concat("*");
    if (Serial2.available()){
      Serial2.println(toSend); 
      delay(100);
    } 
    Serial.println(toSend);
}

void setup() {
  //Start serial communication
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);
  while (!Serial2) {
    ; // wait for serial port to connect. Needed for Native USB only
  }
  Serial.println("Starting Arduino BLE Client application...");
  BLEDevice::init("ESP32-BaseClient");
  pClient = BLEDevice::createClient();
  pBLEScan = BLEDevice::getScan();
  
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(500);
  pBLEScan->setWindow(500);  // less or equal setInterval value
}

void loop() {
  /*Serial.println("Is coonected:" + (String)connected);
  Serial.println("Dracena Found:"+ (String)dracenaFound);
  Serial.println("Sheflera Found:"+ (String)shefleraFound);
  Serial.println("Dracena connected:"+ (String)dracenaConnected);
  Serial.println("Sheflera connected:"+ (String)shefleraConnected);
  Serial.println("New soil info:"+ (String)newSoil);
  Serial.println("New temp info:"+ (String)newTemp);
  */
  if(!connected){
    BLEScanResults foundDevices = pBLEScan->start(scanTime, false);
    Serial.print("Devices found: ");
    Serial.println(foundDevices.getCount());
    Serial.println("Scan done!");
    startTime = millis();
  }
  Serial.println(conectionCount);
  if(shefleraFound && !dracenaConnected){
    if(connectToServer(*shefleraServerAddress,shefleraServiceUUID)) {
      delay(500);
      conectionCount++;
      Serial.println("We are now connected to the sheflera BLE Server.");
      //Activate the Notify property of each Characteristic
      soilCharacteristic->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue((uint8_t*)notificationOn, 2, true);
      tempCharacteristic->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue((uint8_t*)notificationOn, 2, true);
      humCharacteristic->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue((uint8_t*)notificationOn, 2, true);
      //try{}catch(...){}
      connected = true;
      shefleraConnected = true;
      doConnect = false;
      shefleraFound = false;
      startTime = millis();
     } 
     else {
        Serial.println("We have failed to connect to the server; Restart your device to scan for nearby BLE server again.");
     }
  }
  if(dracenaFound && !shefleraConnected){
    if(connectToServer(*dracenaServerAddress,dracenaServiceUUID)) {
      delay(500);
      conectionCount++;
      Serial.println("We are now connected to the dracena BLE Server.");
      //Activate the Notify property of each Characteristic
      soilCharacteristic->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue((uint8_t*)notificationOn, 2, true);
      tempCharacteristic->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue((uint8_t*)notificationOn, 2, true);
      humCharacteristic->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue((uint8_t*)notificationOn, 2, true);
      connected = true;
      dracenaConnected = true;
      doConnect = false;
      dracenaFound = false;
      startTime = millis();
     } 
     else {
        Serial.println("We have failed to connect to the server; Restart your device to scan for nearby BLE server again.");
     }
  }
  //if new temperature readings are available, print in the OLED
  if (newSoil && newTemp && newHum && shefleraConnected){
    pintData();
    String newValue = "Time since boot: " + String(millis()/2000);
    Serial.println("Setting new characteristic value to \"" + newValue + "\"");
    statCharacteristic->writeValue(newValue.c_str(), newValue.length());
    pClient->disconnect();
    connected = false;
    shefleraConnected = false;
    newSoil = false;
    newTemp = false;
    newHum = false;
  }
  else if(newSoil && newTemp && newHum && dracenaConnected){
    pintData();
    String newValue = "Time since boot: " + String(millis()/2000);
    Serial.println("Setting new characteristic value to \"" + newValue + "\"");
    statCharacteristic->writeValue(newValue.c_str(), newValue.length());
    pClient->disconnect();
    connected = false;
    dracenaConnected = false;
    newSoil = false;
    newTemp = false;
    newHum = false;
  }
  else if((!newSoil || !newTemp || !newHum) && connected && endTime>15000){
    pClient->disconnect();
    connected = false;
    dracenaConnected = false;
    shefleraConnected = false;
    Serial.println("Timeout");
  }
  delay(1000); // Delay a second between loops.
  pBLEScan->clearResults();
  endTime = millis()-startTime;
}

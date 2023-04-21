/*********
  Rui Santos
  Complete instructions at https://RandomNerdTutorials.com/esp32-ble-server-client/
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*********/

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Wire.h>
#include <BME280I2C.h>

#define s_TO_MIN_FACTOR 60
#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  15        /* Time ESP32 will go to sleep (in seconds) */

//BME280 Sensor--------------------------------------------------------------------------------------------------------------------------------------------------------------------
BME280I2C bme;    // Default : forced mode, standby time = 1000 ms
                  // Oversampling = pressure ×1, temperature ×1, humidity ×1, filter off,

double pressure,temperature,humidity;

void getBME280Data(){
   float temp=0, hum=0, pres=0;

   BME280::TempUnit tempUnit(BME280::TempUnit_Celsius);
   BME280::PresUnit presUnit(BME280::PresUnit_Pa);

   bme.read(pres, temp, hum, tempUnit, presUnit);
   
   pressure=101;
   temperature=25;
   humidity=78;
}

//Soil sensor----------------------------------------------------------------------------------------------------------------------------------------------------------------------
double light = 0.01;
double soil;

const int AirValue = 3050;   //you need to replace this value with Value_1
const int WaterValue = 1410;  //you need to replace this value with Value_2
double intervals = AirValue - WaterValue;
double soilMoistureValue = 0;
double percentage = 0;

double getSoilMoisture(){
  soilMoistureValue = analogRead(34);  //put Sensor insert into soil
  percentage = 100 - (((soilMoistureValue - WaterValue)/intervals)* 100);
  return percentage;
}

//BLE server name------------------------------------------------------------------------------------------------------------------------------------------------------------------
#define ServerName "Sheflera_ESP32"
float tempo;
float humid;

// Timer variables
unsigned long lastTime = 0;
unsigned long timerDelay = 2000;

bool deviceConnected = false;

bool sendData = true;
bool dataSent = false;
// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID "f3e2d44a-c0a1-11ec-9d64-0242ac120002"
#define STAT_CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

BLECharacteristic soilCharacteristics("ca73b3ba-39f6-4ab3-91ae-186dc9577d99",BLECharacteristic::PROPERTY_NOTIFY);
BLECharacteristic tempCharacteristics("91bad492-b950-4226-aa2b-4ede9fa42f59",BLECharacteristic::PROPERTY_NOTIFY);
BLECharacteristic humCharacteristics("944710ea-d46b-11ec-9d64-0242ac120002",BLECharacteristic::PROPERTY_NOTIFY);

BLEDescriptor soilDescriptor(BLEUUID((uint16_t)0x2902));
BLEDescriptor tempDescriptor(BLEUUID((uint16_t)0x2902));
BLEDescriptor humDescriptor(BLEUUID((uint16_t)0x2902));

BLEServer *pServer;
BLEService *shefleraService;
BLECharacteristic *statCharacteristic;

float startTime=0;
float endTime=0;

//Setup callbacks onConnect and onDisconnect
class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    sendData = true;
    startTime = millis();
  };
  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
  }
};

class MyCharacteristicCallbacks: public BLECharacteristicCallbacks {
    void onRead(BLECharacteristic *pCharacteristic) {
      Serial.println("Characteristics was red!");
    }
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string value = pCharacteristic->getValue();
      Serial.print("The new characteristic value is: ");
      Serial.println(value.c_str());
      sendData = false;
      Serial.println("Going to sleep for "+ String(TIME_TO_SLEEP) + "seconds!");
      esp_deep_sleep_start();
    }
};

void setup() {
  // Start serial communication 
  Serial.begin(115200);
  // Create the BLE Device
  BLEDevice::init(ServerName);

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  shefleraService = pServer->createService(SERVICE_UUID);

  statCharacteristic = shefleraService->createCharacteristic(
                                         STAT_CHARACTERISTIC_UUID,
                                         BLECharacteristic::PROPERTY_READ |
                                         BLECharacteristic::PROPERTY_WRITE
                                       );
                                       
  statCharacteristic->setValue("Hello, World!");
  statCharacteristic->setCallbacks(new MyCharacteristicCallbacks());
  //Moisture
  shefleraService->addCharacteristic(&soilCharacteristics);
  soilDescriptor.setValue("Soil moisture");
  soilCharacteristics.addDescriptor(&soilDescriptor);
  //Temp                   
  shefleraService->addCharacteristic(&tempCharacteristics);
  tempDescriptor.setValue("BME temp");
  tempCharacteristics.addDescriptor(&tempDescriptor);
  //Hum
  shefleraService->addCharacteristic(&humCharacteristics);
  humDescriptor.setValue("BME temp");
  humCharacteristics.addDescriptor(new BLE2902());
  
  
  
  // Start the service
  shefleraService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();   //67773700
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  BLEDevice::startAdvertising();
  //pServer->getAdvertising()->start();
  Serial.println("Waiting a client connection to notify...");

  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR /** s_TO_MIN_FACTOR*/);
  Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) +
  " Seconds");
}

void loop() {
  if (deviceConnected) {
    if (sendData) {
      vTaskDelay(1000);
      delay(1000);
      endTime = millis() - startTime;
      if(endTime>10000){
        ESP.restart();
      }
      soil = getSoilMoisture();//bme.readHumidity();
      getBME280Data();
      light += 0.01;
      
      //Notify humidity reading from BME
      static char soilTemp[6];
      dtostrf(soil, 4, 2, soilTemp);
      //Set humidity Characteristic value and notify connected client
      soilCharacteristics.setValue(soilTemp);
      soilCharacteristics.notify();   
      Serial.print(soil);
      Serial.println(" %");
      
     static char tempTemp[6];
      dtostrf(temperature, 4, 2, tempTemp);
      //Set humidity Characteristic value and notify connected client
      tempCharacteristics.setValue(tempTemp);
      tempCharacteristics.notify(); 
      Serial.print(temperature);
      Serial.println(" °C");

      static char humTemp[6];
      dtostrf(humidity, 4, 2, humTemp);
      //Set humidity Characteristic value and notify connected client
      humCharacteristics.setValue(humTemp);
      humCharacteristics.notify(); 
      Serial.print(humidity);
      Serial.println(" °C");
        
    }
  }
}

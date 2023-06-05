#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// Provide the token generation process info.
#include "addons/TokenHelper.h"
// Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

//Sleep defines
#define s_TO_MIN_FACTOR 60
#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  15        /* Time ESP32 will go to sleep (in seconds) */

//Sensor define
#define VIN 3.3 // V power voltage
#define R 10000 //ohm resistance value

// Insert your network credentials
#define WIFI_SSID "Ventspils"//"B535_51BA"
#define WIFI_PASSWORD ""//"tm4Fb4TdTn3"

// Insert Firebase project API Key
#define API_KEY "AIzaSyBs8eEZz9tepq3S9SMjZNBKMp-xP9s856s"

// Insert Authorized Email and Corresponding Password
#define USER_EMAIL "smirnovaaleksa39@gmail.com"
#define USER_PASSWORD "qwerty12345"

// Insert RTDB URLefine the RTDB URL
#define DATABASE_URL "https://esp32-plant-monitoring-default-rtdb.europe-west1.firebasedatabase.app/"

// Define Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Variable to save USER UID
String uid;

// Variables to save database paths
String databasePath;
String timePath;
String tempPath;
String humPath;
String presPath;
String SoilPath;
String lightPath;

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
String myDate;

//BME280 sensor
Adafruit_BME280 bme; // I2C
float temperature;
float humidity;
float pressure;

//light sensor
int light;
double lux;

//Soil sensor
double soil;
const int AirValue = 3060;   //you need to replace this value with Value_1
const int WaterValue = 1410;  //you need to replace this value with Value_2
double interval = AirValue - WaterValue;
double soilMoistureValue = 0;
double percentage = 0;

// Timer variables 
unsigned long wifiPrevMillis = 0;
unsigned long timerDelay = 15000;

// Initialize BME280
void initBME(){
  if (!bme.begin(0x76)) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    //while (1);
  }
}

double getSoilMoisture(){
  soilMoistureValue = analogRead(13);  //put Sensor insert into soil
  Serial.println(soilMoistureValue);
  percentage = 100 - (((soilMoistureValue - WaterValue)/interval)* 100);
  return percentage;
}

int sensorRawToLux(int raw){
  // Conversion rule
  float Vout = float(raw) * (VIN / float(4095));// Conversion analog to voltage
  float RLDR = (R * (VIN - Vout))/Vout; // Conversion voltage to resistance
  int result=500/(RLDR/1000); // Conversion resitance to lumen
  return result;
}

//get time
String getTime(){
  // Variables to save date and time
  int posT;
  String formattedDate;
  String dayStamp;
  String timeStamp;
  String dateTime;

  while(!timeClient.update()) {
    timeClient.forceUpdate();
  }
  formattedDate = timeClient.getFormattedDate();
  posT = formattedDate.indexOf("T");
  dayStamp = formattedDate.substring(0, posT);
  timeStamp = formattedDate.substring(posT+1, formattedDate.length()-1);
  Serial.println(dayStamp);
  Serial.println(timeStamp);
  return dayStamp+" "+timeStamp;
}

// Initialize WiFi
void initWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(100);
    if (millis() - wifiPrevMillis > timerDelay){
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      wifiPrevMillis = millis();
      Serial.println();
      Serial.println("Trying Again..");
    }
    
  }
  Serial.println(WiFi.localIP());
  Serial.println();
}

void setup(){
  Serial.begin(115200);

  config.signer.lastReqMillis = 0;
  config.signer.tokens.expires = 0;

  timeClient.setTimeOffset(10800);

  soil = getSoilMoisture();
  //Serial.println(soil);

  initBME();
  initWiFi();

  // Assign the api key (required)
  config.api_key = API_KEY;

  // Assign the user sign in credentials
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  // Assign the RTDB URL (required)
  config.database_url = DATABASE_URL;

  //To make the token to expire immediately.
  //config.signer.lastReqMillis = 0;
  //config.signer.tokens.expires = 0;

  Firebase.reconnectWiFi(true);
  fbdo.setResponseSize(4096);

  // Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h

  // Assign the maximum retry of token generation
  config.max_token_generation_retry = 5;

  // Initialize the library with the Firebase authen and config
  Firebase.begin(&config, &auth);

  // Getting the user UID might take a few seconds
  Serial.println("Getting User UID");
  wifiPrevMillis = millis();
  while ((auth.token.uid) == "") {
    Serial.print('.');
    delay(1000);
    if (millis() - wifiPrevMillis > timerDelay){
      ESP.restart();
    }
  }
  // Print user UID
  uid = auth.token.uid.c_str();
  Serial.print("User UID: ");
  Serial.println(uid);

  // Update database path
  databasePath = uid + "/ESP32_plant1/";

  // Update database path for sensor readings
  timePath = databasePath + "/time"; // --> <user_uid>/ESP32_plantx/time
  tempPath = databasePath + "/temperature"; // --> <user_uid>/ESP32_plantx/temperature
  humPath = databasePath + "/humidity"; // --> <user_uid>/ESP32_plantx/humidity
  presPath = databasePath + "/pressure"; // --> <user_uid>/ESP32_plantx/pressure
  SoilPath= databasePath + "/soil"; // --> <user_uid>/ESP32_plantx/soil
  lightPath = databasePath + "/light"; // --> <user_uid>/ESP32_plantx/light

  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR /** s_TO_MIN_FACTOR*/);
  Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) +" Seconds");
}

void loop(){
  // Send new readings to database
  if (Firebase.ready()){
    //get time
    myDate = getTime();
    Serial.println(myDate);
    // Get latest sensor readings
    temperature = bme.readTemperature();
    humidity = bme.readHumidity();
    pressure = bme.readPressure()/1000.0F;

    light = analogRead(34);
    lux=sensorRawToLux(light);

    // Send readings to database:
    Serial.println(Firebase.RTDB.setString(&fbdo,timePath,myDate));
    Serial.println(Firebase.RTDB.setFloat(&fbdo, tempPath, temperature));
    Serial.println(Firebase.RTDB.setFloat(&fbdo, humPath, humidity));
    Serial.println(Firebase.RTDB.setFloat(&fbdo, presPath, pressure));
    Serial.println(Firebase.RTDB.setFloat(&fbdo, SoilPath, soil));
    Serial.println(Firebase.RTDB.setFloat(&fbdo, lightPath, lux));
    
    esp_deep_sleep_start();
  }
}
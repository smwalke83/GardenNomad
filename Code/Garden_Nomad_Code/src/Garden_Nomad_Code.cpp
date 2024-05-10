/* 
 * Project Garden Nomad - Smart Garden Monitor
 * Author: Scott Walker
 * Date: 4/26/2024
 */

#include "Particle.h"
#include "IoTClassroom_CNM.h"
#include "Adafruit_I2CDevice.h"
#include "Adafruit_I2CRegister.h"
#include "Adafruit_VEML7700.h"
#include "Adafruit_GFX.h"
#include "Adafruit_SSD1306.h"
#include "Adafruit_BME280.h"
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT/Adafruit_MQTT_SPARK.h"
#include "Adafruit_MQTT/Adafruit_MQTT.h"
#include "credentials.h"
const int OLED_RESET = -1;
const int OLEDADDRESS = 0x3C;
const int PHPIN = A1;
const int MOISTUREPIN = A2;
const int SAMPLEINTERVAL = 20;
const int ARRAYLENGTH = 40;
const int MODEBUTTONPIN = D4;
const int SAMPLEBUTTONPIN = D5;
const int FEEDRESETPIN = D6;
const int WATERRESETPIN = D7;
const int NEXTBUTTONPIN = D10;
const int WATERLEDPIN = D3;
const int FEEDLEDPIN = D2;
const int SLEEPINTERVAL = 300000;
const int BMEADDRESS = 0x76;
int PHArray[ARRAYLENGTH];
int PHArrayIndex;
int modeValue;
int sampleType;
int waterAlertPCT;
int setupModeValue;
int feedDays;
unsigned long feedAlertMillis;
unsigned long samplingTime;
unsigned long publishTime;
unsigned long displayTime;
unsigned long feedTime;
const float OFFSET = 15.7692;
const float SLOPE = -5.7692;
float PHValue;
float voltage;
float lastPH;
bool sleepTimerOn;
bool waterAlert;
bool feedAlert;
Adafruit_SSD1306 display(OLED_RESET);
Adafruit_VEML7700 lightSensor;
Adafruit_BME280 bme;
TCPClient TheClient;
Adafruit_MQTT_SPARK mqtt(&TheClient, SERVER, SERVERPORT, USERNAME, PASSWORD);
Adafruit_MQTT_Publish tempPub = Adafruit_MQTT_Publish(&mqtt, "TempFeed");
Adafruit_MQTT_Publish luxPub = Adafruit_MQTT_Publish(&mqtt, "LuxFeed");
Adafruit_MQTT_Publish humPub = Adafruit_MQTT_Publish(&mqtt, "HumFeed");
Adafruit_MQTT_Publish moisturePub = Adafruit_MQTT_Publish(&mqtt, "MoistureFeed");
Adafruit_MQTT_Publish soilPHPub = Adafruit_MQTT_Publish(&mqtt, "SoilPH");
Adafruit_MQTT_Publish waterPHPub = Adafruit_MQTT_Publish(&mqtt, "WaterPH");
Adafruit_MQTT_Publish runoffPub = Adafruit_MQTT_Publish(&mqtt, "RunoffPH");
IoTTimer sleepTimer;
Button modeButton(MODEBUTTONPIN);
Button sampleButton(SAMPLEBUTTONPIN);
Button feedAlertReset(FEEDRESETPIN);
Button waterAlertReset(WATERRESETPIN);
Button nextButton(NEXTBUTTONPIN);
double averageArray(int *arr, int number);
void passiveCollection();
void manualSample();
void setupMode();
void sleepULP();
void MQTT_connect();
bool MQTT_ping();

SYSTEM_MODE(AUTOMATIC);

void setup() {

Serial.begin(9600);
waitFor(Serial.isConnected, 10000);
pinMode(PHPIN, INPUT);
pinMode(MOISTUREPIN, INPUT);
pinMode(FEEDLEDPIN, OUTPUT);
pinMode(WATERLEDPIN, OUTPUT);
display.begin(SSD1306_SWITCHCAPVCC, OLEDADDRESS);
display.clearDisplay();
display.display();
bme.begin(BMEADDRESS);
if (!lightSensor.begin()){
  Serial.println("Sensor not found");
  while (1);
}
lightSensor.setGain(VEML7700_GAIN_1_8);
lightSensor.setIntegrationTime(VEML7700_IT_25MS);
PHArrayIndex = 0;
samplingTime = millis();
publishTime = millis();
displayTime = millis();
feedTime = millis();
modeValue = 0;
sampleType = 0;
setupModeValue = 0;
lastPH = 0;
feedDays = 7;
waterAlertPCT = 50;
sleepTimerOn = false;
waterAlert = false;

}

void loop() {

MQTT_connect();
MQTT_ping();   
if(sleepTimerOn == false){
    sleepTimer.startTimer(SLEEPINTERVAL);
    sleepTimerOn = true;
}
if(sleepTimer.isTimerReady() && sleepTimerOn == true){
    sleepTimerOn = false;
    sleepULP();
}
if(feedAlert == true){
    digitalWrite(FEEDLEDPIN, HIGH);
}
if(feedAlert == false){
    digitalWrite(FEEDLEDPIN, LOW);
}
if(waterAlert == true){
    digitalWrite(WATERLEDPIN, HIGH);
}
if(waterAlertReset.isClicked() && waterAlert == false){
    digitalWrite(WATERLEDPIN, LOW);
    sleepTimer.startTimer(SLEEPINTERVAL);
}
if(modeButton.isClicked()){
    modeValue++;
    sleepTimer.startTimer(SLEEPINTERVAL);
}
switch(modeValue%3){
    case 0:
        passiveCollection();
        break;
    case 1:
        manualSample();
        break;
    case 2:
        setupMode();
        break;
}

}
bool MQTT_ping() {
  static unsigned int last;
  bool pingStatus;

  if ((millis()-last)>120000) {
      Serial.printf("Pinging MQTT \n");
      pingStatus = mqtt.ping();
      if(!pingStatus) {
        Serial.printf("Disconnecting \n");
        mqtt.disconnect();
      }
      last = millis();
  }
  return pingStatus;
}
void MQTT_connect() {
  int8_t ret;
 
  // Return if already connected.
  if (mqtt.connected()) {
    return;
  }
 
  Serial.print("Connecting to MQTT... ");
 
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
       Serial.printf("Error Code %s\n",mqtt.connectErrorString(ret));
       Serial.printf("Retrying MQTT connection in 5 seconds...\n");
       mqtt.disconnect();
       delay(5000);  // wait 5 seconds and try again
  }
  Serial.printf("MQTT Connected!\n");
}
double averageArray(int *arr, int number){
  int i;
  int max, min;
  double avg;
  long amount = 0;
  if(number <= 0){
    Serial.printf("Error: Divide by Zero\n");
    return 0;
  }
  if(number < 5){
    for(i =0; i < number; i++){
      amount += arr[i];
    }
    avg = amount/number;
    return avg;
  }
  else{
    if(arr[0] < arr[1]){
      min = arr[0];
      max = arr[1];
    }
    else{
      min = arr[1];
      max = arr[0];
    }
    for(i = 2; i < number; i++){
      if(arr[i] < min){
        amount += min;
        min = arr[i];
      }
      else{
        if(arr[i] > max){
          amount += max;
          max = arr[i];
        }
        else{
          amount += arr[i];
        }
      }
    }
    avg = (double)amount/(number - 2);
  }
  return avg;
}

void passiveCollection(){
    int luxValue;
    int moisture;
    float tempC;
    float tempF;
    float humidRH;
    float moisturePCT;
    luxValue = lightSensor.readLux();
    tempC = bme.readTemperature();
    humidRH = bme.readHumidity();
    tempF = (9.0/5.0)*tempC + 32;
    moisture = analogRead(MOISTUREPIN);
    moisturePCT = (-(100.0/2000.0)*moisture) + 155.0;
    if(moisturePCT < waterAlertPCT){
        waterAlert = true;
    }
    if(moisturePCT > waterAlertPCT){
        waterAlert = false;
    }
    if(millis() - publishTime > 5000){
        tempPub.publish(tempF);
        luxPub.publish(luxValue);
        humPub.publish(humidRH);
        moisturePub.publish(moisturePCT);
        publishTime = millis();
    }
    if(millis() - displayTime > 500){
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(WHITE);
        display.setCursor(0, 10);
        display.printf("Lux: %i\nMoisture: %0.0f%%\nTemp: %0.2fF\nHum: %0.2f\nLast PH: %0.2f\n", luxValue, moisturePCT, tempF, humidRH, lastPH);
        display.display();
        displayTime = millis();
    }
    feedAlertMillis = feedDays * 24 * 60 * 60 * 1000;
    if(millis() - feedTime > feedAlertMillis){
        feedAlert = true;
    }
    if(feedAlertReset.isClicked()){
        feedAlert = false;
        feedTime = millis();
        sleepTimer.startTimer(SLEEPINTERVAL);
    }
}

void manualSample(){
    float PHSample;
    float soilSample;
    float waterSample;
    float runoffSample;
    if(millis() - samplingTime > SAMPLEINTERVAL){
        PHArray[PHArrayIndex++] = analogRead(PHPIN);
        if(PHArrayIndex == ARRAYLENGTH){
            PHArrayIndex = 0;
        }
        voltage = averageArray(PHArray, ARRAYLENGTH) * 3.3/4096;
        PHValue = SLOPE * voltage + OFFSET;
        samplingTime = millis();
    }
    if(millis() - displayTime > 500){
        display.clearDisplay();
        display.setTextSize(2);
        display.setTextColor(WHITE);
        display.setCursor(0, 10);
        if(sampleType%3 == 0){
            display.printf("Soil PH:\n%0.2f\n", PHValue);
        }
        if(sampleType%3 == 1){
            display.printf("Water PH:\n%0.2f\n", PHValue);
        }
        if(sampleType%3 == 2){
            display.printf("Runoff PH:\n%0.2f\n", PHValue);
        }
        display.display();
        displayTime = millis();
    }
    if(nextButton.isClicked()){
        sampleType++;
        sleepTimer.startTimer(SLEEPINTERVAL);
        display.clearDisplay();
        display.setTextSize(2);
        display.setTextColor(WHITE);
        display.setCursor(0, 10);
        if(sampleType%3 == 0){
            display.printf("Soil PH:\n%0.2f\n", PHValue);
        }
        if(sampleType%3 == 1){
            display.printf("Water PH:\n%0.2f\n", PHValue);
        }
        if(sampleType%3 == 2){
            display.printf("Runoff PH:\n%0.2f\n", PHValue);
        }
        display.display();
        displayTime = millis();
    }
    if(sampleButton.isClicked()){
        if(sampleType%3 == 0){
            soilSample = PHValue;
            lastPH = PHValue;
            soilPHPub.publish(soilSample);
        }
        if(sampleType%3 == 1){
            waterSample = PHValue;
            lastPH = PHValue;
            waterPHPub.publish(waterSample);
        }
        if(sampleType%3 == 2){
            runoffSample = PHValue;
            lastPH = PHValue;
            runoffPub.publish(runoffSample);
        }
        sleepTimer.startTimer(SLEEPINTERVAL);
    }
}
void setupMode(){
    if(nextButton.isClicked()){
        setupModeValue++;
        sleepTimer.startTimer(SLEEPINTERVAL);
    }
    if(setupModeValue%2 == 0){
        if(millis() - displayTime > 500){
            display.clearDisplay();
            display.setTextSize(2);
            display.setTextColor(WHITE);
            display.setCursor(0, 10);
            display.printf("Water Threshold:\n%i%%\n", waterAlertPCT);
            display.display();
            displayTime = millis();
        }
        if(sampleButton.isClicked()){
            waterAlertPCT = waterAlertPCT + 5;
            if(waterAlertPCT > 100){
                waterAlertPCT = 0;
            }
            display.clearDisplay();
            display.setTextSize(2);
            display.setTextColor(WHITE);
            display.setCursor(0, 10);
            display.printf("Water Threshold:\n%i%%\n", waterAlertPCT);
            display.display();
            displayTime = millis();
        }
    }
    if(setupModeValue%2 == 1){
        if(millis() - displayTime > 500){
            display.clearDisplay();
            display.setTextSize(2);
            display.setTextColor(WHITE);
            display.setCursor(0, 10);
            display.printf("Feeding Interval:\n%i days\n", feedDays);
            display.display();
            displayTime = millis();
        }
        if(sampleButton.isClicked()){
            feedDays++;
            if(feedDays > 14){
                feedDays = 1;
            }
            display.clearDisplay();
            display.setTextSize(2);
            display.setTextColor(WHITE);
            display.setCursor(0, 10);
            display.printf("Feeding Interval:\n%i days\n", feedDays);
            display.display();
            displayTime = millis();
        }
    }
}
void sleepULP(){
    SystemSleepConfiguration config;
    config.mode(SystemSleepMode::ULTRA_LOW_POWER).gpio(D4, RISING).gpio(D5, RISING).gpio(D6, RISING).gpio(D7, RISING).gpio(D10, RISING).duration(3600000);
    SystemSleepResult result = System.sleep(config);
    if(result.wakeupReason() == SystemSleepWakeupReason::BY_GPIO){
        Serial.printf("Awakened by GPIO %i\n", result.wakeupPin());
    }
    if(result.wakeupReason() == SystemSleepWakeupReason::BY_RTC){
        Serial.printf("Awakened by RTC\n");
    }
}

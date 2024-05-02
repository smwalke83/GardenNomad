/* 
 * Project Garden Nomad - Smart Garden Monitor
 * Author: Scott Walker
 * Date: 4/26/2024
 */

#include "Particle.h"
#include "IoTClassroom_CNM.h"
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
const int WATERLEDPIN = D2;
const int FEEDLEDPIN = D3;
const int SLEEPINTERVAL = 300000;
const int BMEADDRESS = 0x76;
int PHArray[ARRAYLENGTH];
int PHArrayIndex;
int modeValue;
int sampleType;
int waterAlertPCT;
int feedAlertMillis;
int setupModeValue;
int feedDays;
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
Adafruit_VEML7700 lightSensor = Adafruit_VEML7700();
Adafruit_BME280 bme;
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
feedDays = 7;
sleepTimerOn = false;
waterAlert = false;

}

void loop() {

if(sleepTimerOn == false){
    sleepTimer.startTimer(SLEEPINTERVAL);
    sleepTimerOn = true;
}
if(sleepTimer.isTimerReady()){
    sleepTimerOn = false;
    // sleep function, wake up after 1 hour or button press
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
if(waterAlert == false){
    digitalWrite(WATERLEDPIN, LOW);
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
    moisturePCT = (-(100/1900)*moisture) + 157.895;
    if(moisturePCT < waterAlertPCT){
        waterAlert = true;
    }
    if(moisturePCT > waterAlertPCT){
        waterAlert = false;
    }
    if(millis() - publishTime > 120000){
        // publish data to dashboard
        publishTime = millis();
    }
    if(millis() - displayTime > 1000){
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(WHITE);
        display.setCursor(0, 10);
        display.printf("");             // display lux value, moisture, tempF, humidity, last PH reading in readable formats
        display.display();
        displayTime = millis();
        // find a way to switch display modes smoothly (variable other than millis?)
    }
    feedAlertMillis = feedDays * 25 * 60 * 60 * 1000;
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
        display.printf("PH Value:\n%0.2f\n", PHValue);
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
            // publish soilSample to dashboard
        }
        if(sampleType%3 == 1){
            waterSample = PHValue;
            lastPH = PHValue;
            // publish waterSample to dashboard
        }
        if(sampleType%3 == 2){
            runoffSample = PHValue;
            lastPH = PHValue;
            // publish runoffSample to dashboard
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
            display.printf("Water Threshold:\n%0.0f%%\n", waterAlertPCT);
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
            display.printf("Water Threshold:\n%0.0f%%\n", waterAlertPCT);
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
            display.printf("Feeding Interval:\n%i%%\n", feedDays);
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
            display.printf("Feeding Interval:\n%i%%\n", feedDays);
            display.display();
            displayTime = millis();
        }
    }
}

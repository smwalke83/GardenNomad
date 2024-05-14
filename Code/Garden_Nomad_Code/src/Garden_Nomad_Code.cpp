/* 
 * Project Garden Nomad - Smart Garden Monitor
 * Author: Scott Walker
 * Date: 4/26/2024
 */

#include "Particle.h"                                                   // Lines 7-18 Necessary Header Files
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
const int OLED_RESET = -1;                                              // Lines 19-20 Constants for OLED setup
const int OLEDADDRESS = 0x3C;
const int PHPIN = A1;                                                   // Lines 21-29 Defining Pins
const int MOISTUREPIN = A2;
const int MODEBUTTONPIN = D4;
const int SAMPLEBUTTONPIN = D5;
const int FEEDRESETPIN = D6;
const int WATERRESETPIN = D7;
const int NEXTBUTTONPIN = D10;
const int WATERLEDPIN = D3;
const int FEEDLEDPIN = D2;
const int SAMPLEINTERVAL = 20;                                          // Lines 30-31 Constants for PH Sensor
const int ARRAYLENGTH = 40;
const int SLEEPINTERVAL = 300000;                                       // Inactive millis before system sleep
const int BMEADDRESS = 0x76;                                            // Constnat for BME setup
int PHArray[ARRAYLENGTH];                                               // Lines 34-35 Used for PH calculation
int PHArrayIndex;
int modeValue;                                                          // Used to determine what mode the device is in
int displayValue;                                                       // Changes what is displayed in passive mode
int sampleType;                                                         // Changes what sample is being taken in manual sample mode
int waterAlertPCT;                                                      // Threshold for the water alert
int setupModeValue;                                                     // Changes between options in setup menu
int feedDays;                                                           // Threshold for feed alert in days
unsigned long feedAlertMillis;                                          // Threshold for feed alert in millis
unsigned long samplingTime;                                             // Millis for PH sampling timer
unsigned long publishTime;                                              // Millis for data publishing timer
unsigned long displayTime;                                              // Millis for display refresh
unsigned long feedTime;                                                 // Millis for feed alert timer
const float OFFSET = 15.7692;                                           // Lines 47-48 constants for voltage to PH conversion
const float SLOPE = -5.7692;
float PHValue;                                                          // Lines 49-51 PH variables
float voltage;
float lastPH;
bool sleepTimerOn;                                                      // Bools to setup sleep timer, water alert, feed alert
bool waterAlert;
bool feedAlert;
Adafruit_SSD1306 display(OLED_RESET);                                   // Lines 55-58 Defining objects OLED, light sensor, BME, MQTT client
Adafruit_VEML7700 lightSensor;
Adafruit_BME280 bme;
TCPClient TheClient;                                                                 
Adafruit_MQTT_SPARK mqtt(&TheClient, SERVER, SERVERPORT, USERNAME, PASSWORD);       // Lines 59-66 Setup MQTT and feeds to publish to
Adafruit_MQTT_Publish tempPub = Adafruit_MQTT_Publish(&mqtt, "TempFeed");
Adafruit_MQTT_Publish luxPub = Adafruit_MQTT_Publish(&mqtt, "LuxFeed");
Adafruit_MQTT_Publish humPub = Adafruit_MQTT_Publish(&mqtt, "HumFeed");
Adafruit_MQTT_Publish moisturePub = Adafruit_MQTT_Publish(&mqtt, "MoistureFeed");
Adafruit_MQTT_Publish soilPHPub = Adafruit_MQTT_Publish(&mqtt, "SoilPH");
Adafruit_MQTT_Publish waterPHPub = Adafruit_MQTT_Publish(&mqtt, "WaterPH");
Adafruit_MQTT_Publish runoffPub = Adafruit_MQTT_Publish(&mqtt, "RunoffPH");
IoTTimer sleepTimer;                                                    // Lines 67-72 Creating objects - timer and buttons
Button modeButton(MODEBUTTONPIN);
Button sampleButton(SAMPLEBUTTONPIN);
Button feedAlertReset(FEEDRESETPIN);
Button waterAlertReset(WATERRESETPIN);
Button nextButton(NEXTBUTTONPIN);
double averageArray(int *arr, int number);                              // Lines 73-79 Functions for data collection, the various modes, and MQTT communications
void passiveCollection();
void manualSample();
void setupMode();
void sleepULP();
void MQTT_connect();
bool MQTT_ping();

SYSTEM_MODE(AUTOMATIC);

void setup() {

Serial.begin(9600);                                                     // Lines 85-86 Setup Serial monitor
waitFor(Serial.isConnected, 10000);
pinMode(PHPIN, INPUT);                                                  // Lines 87-90 Pinmodes for PH, moisture, LEDs
pinMode(MOISTUREPIN, INPUT);
pinMode(FEEDLEDPIN, OUTPUT);
pinMode(WATERLEDPIN, OUTPUT);
display.begin(SSD1306_SWITCHCAPVCC, OLEDADDRESS);                       // Lines 91-93 Setup OLED display
display.clearDisplay();
display.display();
bme.begin(BMEADDRESS);                                                  // Setup BME
if (!lightSensor.begin()){                                              // Lines 95-100 Setup Light Sensor
  Serial.println("Sensor not found");
  while (1);
}
lightSensor.setGain(VEML7700_GAIN_1_8);
lightSensor.setIntegrationTime(VEML7700_IT_25MS);
PHArrayIndex = 0;                                                       // Lines 101-114 Predefining some variables
samplingTime = millis();
publishTime = millis();
displayTime = millis();
feedTime = millis();
modeValue = 0;
displayValue = 0;
sampleType = 0;
setupModeValue = 0;
lastPH = 0;
feedDays = 7;
waterAlertPCT = 50;
sleepTimerOn = false;
waterAlert = false;

}

void loop() {

MQTT_connect();                                                         // Lines 120-121 Connect to and Ping MQTT Server
MQTT_ping();   
if(sleepTimerOn == false){                                              // Lines 122-125 Starting the sleep timer if the device just woke up
    sleepTimer.startTimer(SLEEPINTERVAL);
    sleepTimerOn = true;
}
if(sleepTimer.isTimerReady() && sleepTimerOn == true){                  // Lines 126-132 Put the device to sleep if the timer is ready. Resets some values so it always wakes up in the same state.
    sleepTimerOn = false;
    modeValue = 0;
    displayValue = 0;
    setupModeValue = 0;
    sleepULP();
}
if(feedAlert == true){                                                  // Lines 133-135 If the feed alert is active, light the LED
    digitalWrite(FEEDLEDPIN, HIGH);
}
if(feedAlert == false){                                                 // Lines 136-138 If the feed alert is inactive, turn off LED
    digitalWrite(FEEDLEDPIN, LOW);
}
if(waterAlert == true){                                                 // Lines 139-141 If the water alert is active, light the LED
    digitalWrite(WATERLEDPIN, HIGH);
}
if(waterAlertReset.isClicked() && waterAlert == false){                 // Lines 142-145 If the water alert is inactive and the reset button is clicked, turn off LED
    digitalWrite(WATERLEDPIN, LOW);
    sleepTimer.startTimer(SLEEPINTERVAL);
}
if(modeButton.isClicked()){                                             // Lines 146-149 If the mode button is clicked, iterate mode value, reset sleep timer
    modeValue++;
    sleepTimer.startTimer(SLEEPINTERVAL);
}
switch(modeValue%3){                                                    // Lines 150-160 Switch case - run different function depending on mode value
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
bool MQTT_ping() {                                                      // Lines 163-177 Pings the MQTT server periodically, obtained in clas, not written by me
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
void MQTT_connect() {                                                   // Lines 178-195 Connects to the MQTT server if not already connected, obtained in class, not written by me
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
double averageArray(int *arr, int number){                              // Lines 196-239 Function to collect voltage data from the PH sensor, then fill and average an array, found online and edited by me
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

void passiveCollection(){                                               // Lines 241-345 Function for passive data collection mode
    int luxValue;                                                           // 242-247 Defining variables
    int moisture;
    float tempC;
    float tempF;
    float humidRH;
    float moisturePCT;
    luxValue = lightSensor.readLux();                                       // 248-253 Obtaining data and converting to relevant units
    tempC = bme.readTemperature();
    humidRH = bme.readHumidity();
    tempF = (9.0/5.0)*tempC + 32;
    moisture = analogRead(MOISTUREPIN);
    moisturePCT = (-(100.0/2000.0)*moisture) + 155.0;
    if(moisturePCT < waterAlertPCT){                                        // 254-259 Turns water alert on or off based on soil moisture
        waterAlert = true;
    }
    if(moisturePCT > waterAlertPCT){
        waterAlert = false;
    }
    if(millis() - publishTime > 5000){                                      // 260-266 Publishes data to cloud every five seconds (if awake and in this mode)
        tempPub.publish(tempF);
        luxPub.publish(luxValue);
        humPub.publish(humidRH);
        moisturePub.publish(moisturePCT);
        waterPHPub.publish(lastPH);
        publishTime = millis();
    }
    if(nextButton.isClicked()){                                             // 267-269 Iterates display value and resets sleep timer when Next button is clicked
        displayValue++;
        sleepTimer.startTimer(SLEEPINTERVAL);
    }
    if(millis() - displayTime > 500){                                       // 270-325 Refreshes display every 500 millis, displays different values based on display value
        if(displayValue%6 == 0){
            display.clearDisplay();
            display.setTextSize(1);
            display.setTextColor(WHITE);
            display.setCursor(0, 10);
            display.printf("Lux: %i\nMoisture: %0.0f%%\nTemp: %0.2fF\nHum: %0.2f\nLast PH: %0.2f\n", luxValue, moisturePCT, tempF, humidRH, lastPH);
            display.display();
            displayTime = millis();
        }
        if(displayValue%6 == 1){
            display.clearDisplay();
            display.setTextSize(2);
            display.setTextColor(WHITE);
            display.setCursor(0, 10);
            display.printf("Lux:\n%i\n", luxValue);
            display.display();
            displayTime = millis();
        }
        if(displayValue%6 == 2){
            display.clearDisplay();
            display.setTextSize(2);
            display.setTextColor(WHITE);
            display.setCursor(0, 10);
            display.printf("Moisture:\n%0.0f%%\n", moisturePCT);
            display.display();
            displayTime = millis();
        }
        if(displayValue%6 == 3){
            display.clearDisplay();
            display.setTextSize(2);
            display.setTextColor(WHITE);
            display.setCursor(0, 10);
            display.printf("Temp:\n%0.2fF\n", tempF);
            display.display();
            displayTime = millis();
        }
        if(displayValue%6 == 4){
            display.clearDisplay();
            display.setTextSize(2);
            display.setTextColor(WHITE);
            display.setCursor(0, 10);
            display.printf("Hum:\n%0.2f\n", humidRH);
            display.display();
            displayTime = millis();
        }
        if(displayValue%6 == 5){
            display.clearDisplay();
            display.setTextSize(2);
            display.setTextColor(WHITE);
            display.setCursor(0, 10);
            display.printf("Last PH:\n%0.2f\n", lastPH);
            display.display();
            displayTime = millis();
        }
    }
    feedAlertMillis = feedDays * 24 * 60 * 60 * 1000;                       // Converts feed alert days into millis
    if(millis() - feedTime > feedAlertMillis){                              // Lines 327-329 turns on feed alert if enough time has passed
        feedAlert = true;
    }
    if(feedAlertReset.isClicked() & feedAlert == true){                     // Lines 330-334 turns off feed alert if button is pressed, restarts feed timer and sleep timer 
        feedAlert = false;
        feedTime = millis();
        sleepTimer.startTimer(SLEEPINTERVAL);
    }
}

void manualSample(){                                                    // Lines 338-405 Function for manual data collection mode                                            
    float soilSample;                                                       // 339-341 Defining Variables
    float waterSample;
    float runoffSample;
    if(millis() - samplingTime > SAMPLEINTERVAL){                           // 342-350 Populating an array with PH voltage values, averaging them, converting average to PH
        PHArray[PHArrayIndex++] = analogRead(PHPIN);
        if(PHArrayIndex == ARRAYLENGTH){
            PHArrayIndex = 0;
        }
        voltage = averageArray(PHArray, ARRAYLENGTH) * 3.3/4096;
        PHValue = SLOPE * voltage + OFFSET;
        samplingTime = millis();
    }
    if(millis() - displayTime > 500){                                       // 351-367 Refreshing the display with relevant data every 500 millis
        display.clearDisplay();
        display.setTextSize(2);
        display.setTextColor(WHITE);
        display.setCursor(0, 10);
        if(sampleType%3 == 0){
            display.printf("Soil PH:\n%0.2f\n", PHValue);                       // Changes sample type displayed depending on which one you're sampling
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
    if(nextButton.isClicked()){                                             // 368-386 Changes active sample type and displays it on press of Next button, also resetting sleep timer
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
    if(sampleButton.isClicked()){                                           // Saves the value of the active sample and sends it to the cloud when Sample button is pressed, also resetting sleep timer
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
void setupMode(){                                                       // Lines 406-459 Function for setup mode
    if(nextButton.isClicked()){                                             // 407-410 Next button cycles through setup modes, resets sleep timer
        setupModeValue++;
        sleepTimer.startTimer(SLEEPINTERVAL);
    }
    if(setupModeValue%2 == 0){                                              // 411-434 Displays what setup mode you're in, changes the water alert threshold if sample button is pressed
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
    if(setupModeValue%2 == 1){                                              // 435-458 Displays what setup mode you're in, changes the feed interval when sample button is pressed
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
void sleepULP(){                                                        // Lines 460-470 Function for sleep mode, wakes up on button press or after duration
    SystemSleepConfiguration config;
    config.mode(SystemSleepMode::ULTRA_LOW_POWER).gpio(D4, RISING).gpio(D5, RISING).gpio(D6, RISING).gpio(D7, RISING).gpio(D10, RISING).duration(720000);
    SystemSleepResult result = System.sleep(config);
    if(result.wakeupReason() == SystemSleepWakeupReason::BY_GPIO){
        Serial.printf("Awakened by GPIO %i\n", result.wakeupPin());
    }
    if(result.wakeupReason() == SystemSleepWakeupReason::BY_RTC){
        Serial.printf("Awakened by RTC\n");
    }
}

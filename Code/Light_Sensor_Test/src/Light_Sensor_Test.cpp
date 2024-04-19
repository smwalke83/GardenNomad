/* 
 * Project Light Sensor
 * Author: Scott Walker
 * Date: 4/19/2024
 */

#include "Particle.h"
#include "Adafruit_VEML7700.h"
#include "Adafruit_GFX.h"
#include "Adafruit_SSD1306.h"   
Adafruit_VEML7700 lightSensor = Adafruit_VEML7700();
int lastTime;
const int OLED_RESET = -1;
const int OLEDADDRESS = 0x3C;
Adafruit_SSD1306 display(OLED_RESET);

SYSTEM_MODE(AUTOMATIC);

void setup() {

Serial.begin(9600);
waitFor(Serial.isConnected, 10000);
display.begin(SSD1306_SWITCHCAPVCC, OLEDADDRESS);
display.clearDisplay();
display.display();
if(!lightSensor.begin()){
  Serial.printf("Sensor not found.\n");
}
Serial.printf("Sensor found.\n");
lastTime = millis();
lightSensor.setGain(VEML7700_GAIN_1_8);
lightSensor.setIntegrationTime(VEML7700_IT_25MS);

}

void loop() {

if(millis() - lastTime > 1000){
  Serial.printf("Lux value: %0.2f\nWhite Light: %0.2f\nRaw value: %i\n", lightSensor.readLux(), lightSensor.readWhite(), lightSensor.readALS());
  lastTime = millis();
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.printf("Lux:%0.0f\nRaw:%i", lightSensor.readLux(), lightSensor.readALS());
  display.display();
}

}

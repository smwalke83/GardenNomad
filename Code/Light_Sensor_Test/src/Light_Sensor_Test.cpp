/* 
 * Project Light Sensor
 * Author: Scott Walker
 * Date: 4/19/2024
 */

#include "Particle.h"
#include "Adafruit_GFX.h"
#include "Adafruit_SSD1306.h"   
#include "Adafruit_VEML7700.h"
int lastTime;
uint16_t luxValue;
const int OLED_RESET = -1;
const int OLEDADDRESS = 0x3C;
Adafruit_SSD1306 display(OLED_RESET);
Adafruit_VEML7700 veml;

SYSTEM_MODE(AUTOMATIC);

void setup() {

Serial.begin(9600);
waitFor(Serial.isConnected, 10000);
display.begin(SSD1306_SWITCHCAPVCC, OLEDADDRESS);
display.clearDisplay();
display.display();
lastTime = millis();
if (!veml.begin()){
  Serial.println("Sensor not found");
  while (1);
}
Serial.println("Sensor found");
//Wire.begin();
//Wire.beginTransmission(0x10);
//Wire.write(0x00);
//Wire.write(0x1300);
//Wire.endTransmission(true);

}

void loop() {

if(millis() - lastTime > 1000){
  //Wire.begin();
  //Wire.beginTransmission(0x10);
  //Wire.write(0x05);
  //Wire.endTransmission(false);
  //Wire.requestFrom(0x10, 1, true);
  //luxValue = Wire.read();
  Serial.printf("Lux value: %0.0f\n", veml.readLux());
  lastTime = millis();
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.printf("Lux value %0.0f\n", veml.readLux());
  display.display();
}

}

/* 
 * Project PH Meter Test
 * Author: Scott Walker
 * Date: 4/19/2024
 */

#include "Particle.h"
const int PHPIN = A1;
const float OFFSET = 15.7692;
const float SLOPE = -5.7692;
const int SAMPLEINTERVAL = 20;
const int PRINTINTERVAL = 800;
const int ARRAYLENGTH = 40;
int PHArray[ARRAYLENGTH];
int PHArrayIndex;
unsigned long samplingTime;
unsigned long printTime;
float PHValue;
float voltage;
double averageArray(int *arr, int number);

SYSTEM_MODE(AUTOMATIC);

void setup() {

Serial.begin(9600);
waitFor(Serial.isConnected, 10000);
pinMode(PHPIN, INPUT);
PHArrayIndex = 0;
samplingTime = millis();
printTime = millis();
Serial.printf("PH Meter Test:\n");

}

void loop() {

if(millis() - samplingTime > SAMPLEINTERVAL){
  PHArray[PHArrayIndex++] = analogRead(PHPIN);
  if(PHArrayIndex == ARRAYLENGTH){
    PHArrayIndex = 0;
  }
  voltage = averageArray(PHArray, ARRAYLENGTH) * 3.3/4096;
  PHValue = SLOPE * voltage + OFFSET;
  samplingTime = millis();
}
if(millis() - printTime > PRINTINTERVAL){
  Serial.printf("Voltage: %0.2f\nPH: %0.2f\n", voltage, PHValue);
  printTime = millis();
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


/* 
 * Project MQTT Test
 * Author: Scott Walker
 * Date: 5/10/2024
 */

#include "Particle.h"
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT/Adafruit_MQTT_SPARK.h"
#include "Adafruit_MQTT/Adafruit_MQTT.h"
#include "credentials.h"
int value;
int value2;
int lastTime;
TCPClient TheClient;
Adafruit_MQTT_SPARK mqtt(&TheClient, SERVER, SERVERPORT, USERNAME, PASSWORD);
Adafruit_MQTT_Publish testPub = Adafruit_MQTT_Publish(&mqtt, "testfeed");
Adafruit_MQTT_Publish testPub2 = Adafruit_MQTT_Publish(&mqtt, "testfeed2");
void MQTT_connect();
bool MQTT_ping();

SYSTEM_MODE(AUTOMATIC);

void setup() {

Serial.begin(9600);
waitFor(Serial.isConnected, 10000);
lastTime = 0;
value = 0;
value2 = 50;

}

void loop() {

MQTT_connect();
MQTT_ping();
if(millis() - lastTime > 5000){
  value++;
  if(value > 50){
    value = 0;
  }
  value2--;
  if(value2 < 0){
    value2 = 50;
  }
  testPub.publish(value);
  testPub2.publish(value2);
  lastTime = millis();
  Serial.printf("Publishing %i to Feed 1.\nPublishing %i to Feed 2.\n", value, value2);
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
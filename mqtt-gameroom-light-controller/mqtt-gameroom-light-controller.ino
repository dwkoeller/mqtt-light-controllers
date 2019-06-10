//This can be used to output the date the code was compiled
const char compile_date[] = __DATE__ " " __TIME__;

/************ WIFI, OTA and MQTT INFORMATION (CHANGE THESE FOR YOUR SETUP) ******************/
//#define WIFI_SSID "" //enter your WIFI SSID
//#define WIFI_PASSWORD "" //enter your WIFI Password
//#define MQTT_SERVER "" // Enter your MQTT server address or IP.
//#define MQTT_USER "" //enter your MQTT username
//#define MQTT_PASSWORD "" //enter your password
#define MQTT_DEVICE "mqtt-gameroom-light-controller" // Enter your MQTT device
#define MQTT_SSL_PORT 8883 // Enter your MQTT server port.
#define MQTT_SOCKET_TIMEOUT 120
#define FW_UPDATE_INTERVAL_SEC 24*3600
#define STATUS_UPDATE_INTERVAL_SEC 120
#define WATCHDOG_UPDATE_INTERVAL_SEC 1
#define WATCHDOG_RESET_INTERVAL_SEC 120
#define UPDATE_SERVER "http://192.168.100.15/firmware/"
#define FIRMWARE_VERSION "-1.20"

/****************************** MQTT TOPICS (change these topics as you wish)  ***************************************/

#define MQTT_VERSION_PUB "mqtt/gameroom-light/version"
#define MQTT_COMPILE_PUB "mqtt/gameroom-light/compile"
#define MQTT_SWITCH_TOPIC_1 "mqtt/gameroom-light/light/switch"
#define MQTT_SWITCH_REPLY_TOPIC_1 "mqtt/gameroom-light/light/status"
#define MQTT_HEARTBEAT_SUB "heartbeat/#"
#define MQTT_HEARTBEAT_TOPIC "heartbeat"
#define MQTT_HEARTBEAT_PUB "mqtt/gameroom-light/heartbeat"

#define RELAY_ON 1
#define RELAY_OFF 0
#define LIGHT_ON "ON"
#define LIGHT_OFF "OFF"

#define RELAY_1    5 //  D1  
#define WATCHDOG_PIN   14 //  D5   

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Ticker.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include "credentials.h" // Place credentials for wifi and mqtt in this file
#include "certificates.h" // Place certificates for mqtt in this file

Ticker ticker_fw, ticker_status;

bool readyForFwUpdate = false;

int relayStatus = 0;

WiFiClientSecure espClient;
PubSubClient client(espClient);

#include "common.h"

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(MQTT_SERVER, MQTT_SSL_PORT); //CHANGE PORT HERE IF NEEDED
  client.setCallback(callback);
  
  digitalWrite(RELAY_1, RELAY_OFF);
  pinMode(RELAY_1, OUTPUT);   

  ticker_status.attach_ms(STATUS_UPDATE_INTERVAL_SEC * 1000, statusTicker);

  checkForUpdates();

}

void loop() {

  client.loop();

  if(readyForFwUpdate) {
    readyForFwUpdate = false;
    checkForUpdates();
  }

  if (!client.connected()) {
      reconnect();
      client.subscribe(MQTT_SWITCH_TOPIC_1);

  }

}

void callback(char* p_topic, byte* p_payload, unsigned int p_length) {

  //convert topic to string to make it easier to work with
  String payload;
  for (uint8_t i = 0; i < p_length; i++) {
    payload.concat((char)p_payload[i]);
  }   
  if (String(MQTT_HEARTBEAT_TOPIC).equals(p_topic)) {
    resetWatchdog();
    client.publish(MQTT_HEARTBEAT_PUB, "Heartbeat Received"); 
    return;
  }      
  if (payload.equals(String(LIGHT_ON))) {
    digitalWrite(RELAY_1, RELAY_ON);
    client.publish(MQTT_SWITCH_REPLY_TOPIC_1, LIGHT_ON);
    Serial.println(String(MQTT_SWITCH_REPLY_TOPIC_1) + LIGHT_ON);
    relayStatus = 1;
  }
  else if (payload.equals(String(LIGHT_OFF))) {
    digitalWrite(RELAY_1, RELAY_OFF);
    client.publish(MQTT_SWITCH_REPLY_TOPIC_1, LIGHT_OFF);
    Serial.println(String(MQTT_SWITCH_REPLY_TOPIC_1) + LIGHT_OFF);
    relayStatus = 0;
  }
}

void statusTicker() {
  for (int i=1; i<=8; i++) {
    String status;
    if (relayStatus > 0) {
      status = "ON";
    }
    else {
      status = "OFF";
    }
    client.publish(MQTT_SWITCH_REPLY_TOPIC_1, status.c_str());
  }
}

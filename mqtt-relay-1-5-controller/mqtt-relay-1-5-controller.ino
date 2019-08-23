//This can be used to output the date the code was compiled
const char compile_date[] = __DATE__ " " __TIME__;

/************ WIFI, OTA and MQTT INFORMATION (CHANGE THESE FOR YOUR SETUP) ******************/
//#define WIFI_SSID "" //enter your WIFI SSID
//#define WIFI_PASSWORD "" //enter your WIFI Password
//#define MQTT_SERVER "" // Enter your MQTT server address or IP.
//#define MQTT_USER "" //enter your MQTT username
//#define MQTT_PASSWORD "" //enter your password
#define MQTT_DEVICE "mqtt-relay-1-5-controller" // Enter your MQTT device
#define MQTT_SSL_PORT 8883 // Enter your MQTT server port.
#define MQTT_SOCKET_TIMEOUT 120
#define FW_UPDATE_INTERVAL_SEC 24*3600
#define STATUS_UPDATE_INTERVAL_SEC 120
#define WATCHDOG_UPDATE_INTERVAL_SEC 1
#define WATCHDOG_RESET_INTERVAL_SEC 120
#define FLASH_INTERVAL_MS 1500
#define UPDATE_SERVER "http://192.168.100.15/firmware/"
#define FIRMWARE_VERSION "-1.30"

/****************************** MQTT TOPICS (change these topics as you wish)  ***************************************/

#define MQTT_VERSION_PUB "mqtt/relay-1-5/version"
#define MQTT_COMPILE_PUB "mqtt/relay-1-5/compile"
#define MQTT_SWITCH_TOPIC_1 "mqtt/relay-1-5/light/switch"
#define MQTT_SWITCH_REPLY_TOPIC_1 "mqtt/relay-1-5/light/status"
#define MQTT_HEARTBEAT_SUB "heartbeat/#"
#define MQTT_HEARTBEAT_TOPIC "heartbeat"
#define MQTT_HEARTBEAT_PUB "mqtt/relay-1-5/heartbeat"

#define RELAY_ON 1
#define RELAY_OFF 0
#define LIGHT_ON "ON"
#define LIGHT_OFF "OFF"
#define LIGHT_FLASH "FLASH"

#define RELAY_1    5  //  D1
#define WATCHDOG_PIN   14 //  D5   

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Ticker.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include "credentials.h" // Place credentials for wifi and mqtt in this file
#include "certificates.h" // Place certificates for mqtt in this file

Ticker ticker_fw, ticker_status, ticker_relay;

bool readyForFwUpdate = false;
bool relayFlashState = false;
int relayStatus = 0;

WiFiClientSecure espClient;
PubSubClient client(espClient);

#include "common.h"

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(MQTT_SERVER, MQTT_SSL_PORT); //CHANGE PORT HERE IF NEEDED
  client.setCallback(callback);
  
// Initialize Pins so relays are inactive at reset

   
  digitalWrite(RELAY_1, RELAY_OFF);
  digitalWrite(WATCHDOG_PIN, LOW);  
  
// Set pins as outputs

  pinMode(RELAY_1, OUTPUT);   
  pinMode(WATCHDOG_PIN, OUTPUT);

  ticker_status.attach_ms(STATUS_UPDATE_INTERVAL_SEC * 1000, statusTicker);

  checkForUpdates();
  resetWatchdog();

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
  else if (payload.equals(String(LIGHT_FLASH))) {
    Serial.println(String(MQTT_SWITCH_REPLY_TOPIC_1) + "FLASH");
    ticker_relay.attach_ms(FLASH_INTERVAL_MS, relayTicker);
    relayStatus = -1;
  }
  else if (payload.equals(String(LIGHT_OFF))) {
    digitalWrite(RELAY_1, RELAY_OFF);
    client.publish(MQTT_SWITCH_REPLY_TOPIC_1, LIGHT_OFF);
    Serial.println(String(MQTT_SWITCH_REPLY_TOPIC_1) + LIGHT_OFF);
    ticker_relay.detach();
    relayStatus = 0;
  }
}

void statusTicker() {
  String status;
  if (relayStatus > 0) {
    status = "ON";
  }
  else if (relayStatus < 0) {
    status = "FLASH";
  }
  else {
    status = "OFF";
  }
  client.publish(MQTT_SWITCH_REPLY_TOPIC_1, status.c_str());
}

void relayTicker() {
  if (relayFlashState) {
    relayFlashState = false;
    digitalWrite(RELAY_1, RELAY_ON);
  }
  else {
    relayFlashState = true;
    digitalWrite(RELAY_1, RELAY_OFF);
  }
}

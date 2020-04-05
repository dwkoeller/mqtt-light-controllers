//This can be used to output the date the code was compiled
const char compile_date[] = __DATE__ " " __TIME__;

/************ WIFI, OTA and MQTT INFORMATION (CHANGE THESE FOR YOUR SETUP) ******************/
//#define WIFI_SSID "" //enter your WIFI SSID
//#define WIFI_PASSWORD "" //enter your WIFI Password
//#define MQTT_SERVER "" // Enter your MQTT server address or IP.
//#define MQTT_USER "" //enter your MQTT username
//#define MQTT_PASSWORD "" //enter your password
#define MQTT_DEVICE "mqtt-relay-1-4-controller" // Enter your MQTT device
#define MQTT_DEVICE_NAME "Relay 1-4 Controller"
#define MQTT_SSL_PORT 8883 // Enter your MQTT server port.
#define MQTT_SOCKET_TIMEOUT 120
#define FW_UPDATE_INTERVAL_SEC 24*3600
#define STATUS_UPDATE_INTERVAL_SEC 120
#define FLASH_INTERVAL_MS 1500
#define UPDATE_SERVER "http://192.168.100.15/firmware/"
#define FIRMWARE_VERSION "-1.51"

/****************************** MQTT TOPICS (change these topics as you wish)  ***************************************/

#define MQTT_HEARTBEAT_SUB "heartbeat/#"
#define MQTT_HEARTBEAT_TOPIC "heartbeat"
#define MQTT_DISCOVERY_LIGHT_PREFIX  "homeassistant/light/"
#define MQTT_DISCOVERY_SENSOR_PREFIX  "homeassistant/sensor/"
#define HA_TELEMETRY                         "ha"

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
bool registered = false;

String lightStateTopic = String(MQTT_DISCOVERY_LIGHT_PREFIX) + MQTT_DEVICE + "/state";
String ligthCommandTopic = String(MQTT_DISCOVERY_LIGHT_PREFIX) + MQTT_DEVICE + "/command";

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
      client.subscribe(ligthCommandTopic.c_str());

  }
  if (! registered) {
    registerTelemetry();
    updateTelemetry("Unknown");
    createLight(MQTT_DEVICE, MQTT_DEVICE_NAME);
    registered = true;
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
    updateTelemetry(payload);
    return;
  }
  if (payload.equals(String(LIGHT_ON))) {
    digitalWrite(RELAY_1, RELAY_ON);
    updateLight(MQTT_DEVICE, LIGHT_ON);
    relayStatus = 1;
    Serial.println("ON");
  }
  else if (payload.equals(String(LIGHT_FLASH))) {
    ticker_relay.attach_ms(FLASH_INTERVAL_MS, relayTicker);
    relayStatus = -1;
    Serial.println("FLASH");
  }
  else if (payload.equals(String(LIGHT_OFF))) {
    digitalWrite(RELAY_1, RELAY_OFF);
    updateLight(MQTT_DEVICE, LIGHT_OFF);
    ticker_relay.detach();
    relayStatus = 0;
    Serial.println("OFF");
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
//  client.publish(MQTT_SWITCH_REPLY_TOPIC_1, status.c_str());
  updateLight(MQTT_DEVICE, status.c_str());
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

void createLight(String light, String light_name) {
  String topic = String(MQTT_DISCOVERY_LIGHT_PREFIX) + light + "/config";
  String message = String("{\"name\": \"") + light_name +
                   String("\", \"retain\": \"true") +
                   String("\", \"unique_id\": \"") + light + getUUID() +
                   String("\", \"optimistic\": \"false") +
                   String("\", \"command_topic\": \"") + String(MQTT_DISCOVERY_LIGHT_PREFIX) + light +
                   String("/command\", \"state_topic\": \"") + String(MQTT_DISCOVERY_LIGHT_PREFIX) + light +
                   String("/state\"}");
  Serial.print(F("MQTT - "));
  Serial.print(topic);
  Serial.print(F(" : "));
  Serial.println(message.c_str());

  client.publish(topic.c_str(), message.c_str(), true);

}

void updateLight(String light, String state) {
  String topic = String(MQTT_DISCOVERY_LIGHT_PREFIX) + light + "/state";

  Serial.print(F("MQTT - "));
  Serial.print(topic);
  Serial.print(F(" : "));
  Serial.println(state);
  client.publish(topic.c_str(), state.c_str(), true);

}

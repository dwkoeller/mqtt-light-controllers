#include <PubSubClient.h>
#include <DHT.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <Ticker.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include "credentials.h" // Place credentials for wifi and mqtt in this file

//This can be used to output the date the code was compiled
const char compile_date[] = __DATE__ " " __TIME__;

/************ WIFI, OTA and MQTT INFORMATION (CHANGE THESE FOR YOUR SETUP) ******************/
//#define WIFI_SSID "" //enter your WIFI SSID
//#define WIFI_PASSWORD "" //enter your WIFI Password
//#define MQTT_SERVER "" // Enter your MQTT server address or IP.
//#define MQTT_USER "" //enter your MQTT username
//#define MQTT_PASSWORD "" //enter your password
#define MQTT_DEVICE "mqtt-relay-1-2-controller" // Enter your MQTT device
#define MQTT_PORT 1883 // Enter your MQTT server port.
#define MQTT_SOCKET_TIMEOUT 120
#define FW_UPDATE_INTERVAL_SEC 24*3600
#define STATUS_UPDATE_INTERVAL_SEC 120
#define WATCHDOG_UPDATE_INTERVAL_SEC 1
#define WATCHDOG_RESET_INTERVAL_SEC 120
#define FLASH_INTERVAL_MS 1500
#define UPDATE_SERVER "http://192.168.100.15/firmware/"
#define FIRMWARE_VERSION "-1.10"

/****************************** MQTT TOPICS (change these topics as you wish)  ***************************************/

#define MQTT_VERSION_PUB "mqtt/relay-1-2/version"
#define MQTT_COMPILE_PUB "mqtt/relay-1-2/compile"
#define MQTT_SWITCH_TOPIC_1 "mqtt/relay-1-2/light/switch"
#define MQTT_SWITCH_REPLY_TOPIC_1 "mqtt/relay-1-2/light/status"
#define MQTT_HEARTBEAT_SUB "heartbeat/#"
#define MQTT_HEARTBEAT_TOPIC "heartbeat"
#define MQTT_HEARTBEAT_PUB "mqtt/relay-1-2/heartbeat"

Ticker ticker_fw, ticker_status, ticker_relay;

bool readyForFwUpdate = false;
bool relayFlashState = false;
int relayStatus = 0;

// Init WiFi
WiFiClient espClient;

// Init MQTT
PubSubClient client(espClient);

#define RELAY_ON 1
#define RELAY_OFF 0
#define LIGHT_ON "ON"
#define LIGHT_OFF "OFF"
#define LIGHT_FLASH "FLASH"

#define RELAY_1    5  //  D1
#define WATCHDOG   14 //  D5   

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(MQTT_SERVER, MQTT_PORT); //CHANGE PORT HERE IF NEEDED
  client.setCallback(callback);
  
// Initialize Pins so relays are inactive at reset

   
  digitalWrite(RELAY_1, RELAY_OFF);
  digitalWrite(WATCHDOG, LOW);  
  
// Set pins as outputs

  pinMode(RELAY_1, OUTPUT);   
  pinMode(WATCHDOG, OUTPUT);

  ticker_status.attach_ms(STATUS_UPDATE_INTERVAL_SEC * 1000, statusTicker);

  checkForUpdates();
  resetWatchdog();

}

void setup_wifi() {
  int count = 0;
  my_delay(50);

  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);
  
  WiFi.mode(WIFI_STA);
  WiFi.hostname(MQTT_DEVICE);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    my_delay(250);
    Serial.print(".");
    count++;
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  
}

void loop() {

  client.loop();

  if(readyForFwUpdate) {
    readyForFwUpdate = false;
    checkForUpdates();
  }

  if (!client.connected()) {
      reconnect();
  }

}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {

    // Attempt to connect
  if (client.connect(MQTT_DEVICE, MQTT_USER, MQTT_PASSWORD)) {
      client.subscribe(MQTT_HEARTBEAT_SUB);
      client.subscribe(MQTT_SWITCH_TOPIC_1);
    }
    else {
      // Wait 5 seconds before retrying
      my_delay(5000);
    }
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

// FW update ticker
void fwTicker() {
  readyForFwUpdate = true;
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

String WiFi_macAddressOf(IPAddress aIp) {
  if (aIp == WiFi.localIP())
    return WiFi.macAddress();

  if (aIp == WiFi.softAPIP())
    return WiFi.softAPmacAddress();

  return String("00-00-00-00-00-00");
}

void checkForUpdates() {

  String clientMAC = WiFi_macAddressOf(espClient.localIP());

  Serial.print("MAC: ");
  Serial.println(clientMAC);
  clientMAC.replace(":", "-");
  String filename = clientMAC.substring(9);
  String firmware_URL = String(UPDATE_SERVER) + filename + String(FIRMWARE_VERSION);
  String current_firmware_version_URL = String(UPDATE_SERVER) + filename + String("-current_version");

  HTTPClient http;

  http.begin(current_firmware_version_URL);
  int httpCode = http.GET();
  
  if ( httpCode == 200 ) {

    String newFirmwareVersion = http.getString();
    newFirmwareVersion.trim();
    
    Serial.print( "Current firmware version: " );
    Serial.println( FIRMWARE_VERSION );
    Serial.print( "Available firmware version: " );
    Serial.println( newFirmwareVersion );
    
    if(newFirmwareVersion.substring(1).toFloat() > String(FIRMWARE_VERSION).substring(1).toFloat()) {
      Serial.println( "Preparing to update" );
      String new_firmware_URL = String(UPDATE_SERVER) + filename + newFirmwareVersion + ".bin";
      Serial.println(new_firmware_URL);
      t_httpUpdate_return ret = ESPhttpUpdate.update( new_firmware_URL );

      switch(ret) {
        case HTTP_UPDATE_FAILED:
          Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
          break;

        case HTTP_UPDATE_NO_UPDATES:
          Serial.println("HTTP_UPDATE_NO_UPDATES");
         break;
      }
    }
    else {
      Serial.println("Already on latest firmware");  
    }
  }
  else {
    Serial.print("GET RC: ");
    Serial.println(httpCode);
  }
}

void my_delay(unsigned long ms) {
  uint32_t start = micros();

  while (ms > 0) {
    yield();
    while ( ms > 0 && (micros() - start) >= 1000) {
      ms--;
      start += 1000;
    }
  }
}

void resetWatchdog() {
  digitalWrite(WATCHDOG, HIGH);
  my_delay(20);
  digitalWrite(WATCHDOG, LOW);
}

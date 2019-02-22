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
#define MQTT_DEVICE "mqtt-relay-8-1-controller" // Enter your MQTT device
#define MQTT_PORT 1883 // Enter your MQTT server port.
#define MQTT_SOCKET_TIMEOUT 120
#define FW_UPDATE_INTERVAL_SEC 24*3600
#define STATUS_UPDATE_INTERVAL_SEC 120
#define WATCHDOG_UPDATE_INTERVAL_SEC 1
#define WATCHDOG_RESET_INTERVAL_SEC 120
#define FLASH_INTERVAL_MS 1500
#define UPDATE_SERVER "http://192.168.100.15/firmware/"
#define FIRMWARE_VERSION "-1.40"

/****************************** MQTT TOPICS (change these topics as you wish)  ***************************************/

#define MQTT_VERSION_PUB "mqtt/relay-8-1/light/version"
#define MQTT_COMPILE_PUB "mqtt/relay-8-1/light/compile"
#define MQTT_HEARTBEAT_PUB "mqtt/relay-8-1/heartbeat"
#define MQTT_HEARTBEAT_SUB "heartbeat/#"
#define MQTT_HEARTBEAT_TOPIC "heartbeat"
#define MQTT_SWITCH_TOPIC_1 "mqtt/relay-8-1/light/switch-1"
#define MQTT_SWITCH_TOPIC_2 "mqtt/relay-8-1/light/switch-2"
#define MQTT_SWITCH_TOPIC_3 "mqtt/relay-8-1/light/switch-3"
#define MQTT_SWITCH_TOPIC_4 "mqtt/relay-8-1/light/switch-4"
#define MQTT_SWITCH_TOPIC_5 "mqtt/relay-8-1/light/switch-5"
#define MQTT_SWITCH_TOPIC_6 "mqtt/relay-8-1/light/switch-6"
#define MQTT_SWITCH_TOPIC_7 "mqtt/relay-8-1/light/switch-7"
#define MQTT_SWITCH_TOPIC_8 "mqtt/relay-8-1/light/switch-8"
#define MQTT_SWITCH_REPLY_TOPIC_1 "mqtt/relay-8-1/light/status-1"
#define MQTT_SWITCH_REPLY_TOPIC_2 "mqtt/relay-8-1/light/status-2"
#define MQTT_SWITCH_REPLY_TOPIC_3 "mqtt/relay-8-1/light/status-3"
#define MQTT_SWITCH_REPLY_TOPIC_4 "mqtt/relay-8-1/light/status-4"
#define MQTT_SWITCH_REPLY_TOPIC_5 "mqtt/relay-8-1/light/status-5"
#define MQTT_SWITCH_REPLY_TOPIC_6 "mqtt/relay-8-1/light/status-6"
#define MQTT_SWITCH_REPLY_TOPIC_7 "mqtt/relay-8-1/light/status-7"
#define MQTT_SWITCH_REPLY_TOPIC_8 "mqtt/relay-8-1/light/status-8"
#define MQTT_SWITCH_REPLY_TOPIC_PREFIX "mqtt/relay-8-1/light/status-"


enum string_code {
    eSwitch1,
    eSwitch2,
    eSwitch3,
    eSwitch4,
    eSwitch5,
    eSwitch6,
    eSwitch7,
    eSwitch8
};

string_code hashit (String inString) {
    if (inString == MQTT_SWITCH_TOPIC_1) return eSwitch1;
    if (inString == MQTT_SWITCH_TOPIC_2) return eSwitch2;
    if (inString == MQTT_SWITCH_TOPIC_3) return eSwitch3;
    if (inString == MQTT_SWITCH_TOPIC_4) return eSwitch4;
    if (inString == MQTT_SWITCH_TOPIC_5) return eSwitch5;
    if (inString == MQTT_SWITCH_TOPIC_6) return eSwitch6;
    if (inString == MQTT_SWITCH_TOPIC_7) return eSwitch7;
    if (inString == MQTT_SWITCH_TOPIC_8) return eSwitch8;
}

Ticker ticker_fw, ticker_status;
Ticker ticker_relay1, ticker_relay2, ticker_relay3, ticker_relay4, ticker_relay5, ticker_relay6, ticker_relay7, ticker_relay8;

bool readyForFwUpdate = false;

bool relayFlashState1 = false;
bool relayFlashState2 = false;
bool relayFlashState3 = false;
bool relayFlashState4 = false;
bool relayFlashState5 = false;
bool relayFlashState6 = false;
bool relayFlashState7 = false; 
bool relayFlashState8 = false;
int relayStatus[8];

// Init WiFi
WiFiClient espClient;

// Init MQTT
PubSubClient client(espClient);

#define RELAY_ON  0
#define RELAY_OFF 1

#define LIGHT_ON "ON"
#define LIGHT_OFF "OFF"
#define LIGHT_FLASH "FLASH"

#define RELAY_1    16 //  D0  
#define RELAY_2    5  //  D1  
#define RELAY_3    4  //  D2  
#define RELAY_4    0  //  D3  
#define RELAY_5    2  //  D4  
#define RELAY_6    14 //  D5  
#define RELAY_7    12 //  D6  
#define RELAY_8    13 //  D7
#define WATCHDOG   15 //  D8

void setup() {
  for (int i=0; i<8; i++) {
    relayStatus[i] = 0;
  }
  
  Serial.begin(115200);
  setup_wifi();
  client.setServer(MQTT_SERVER, MQTT_PORT); //CHANGE PORT HERE IF NEEDED
  client.setCallback(callback);
  
// Initialize Pins so relays are inactive at reset
   
  digitalWrite(RELAY_1, RELAY_OFF);
  digitalWrite(RELAY_2, RELAY_OFF);
  digitalWrite(RELAY_3, RELAY_OFF);
  digitalWrite(RELAY_4, RELAY_OFF);  
  digitalWrite(RELAY_5, RELAY_OFF);
  digitalWrite(RELAY_6, RELAY_OFF);
  digitalWrite(RELAY_7, RELAY_OFF);
  digitalWrite(RELAY_8, RELAY_OFF);  
  digitalWrite(WATCHDOG, LOW);  
  
// Set pins as outputs

  pinMode(RELAY_1, OUTPUT);   
  pinMode(RELAY_2, OUTPUT);  
  pinMode(RELAY_3, OUTPUT);  
  pinMode(RELAY_4, OUTPUT);    
  pinMode(RELAY_5, OUTPUT);   
  pinMode(RELAY_6, OUTPUT);  
  pinMode(RELAY_7, OUTPUT);  
  pinMode(RELAY_8, OUTPUT);
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
      
      client.subscribe(MQTT_SWITCH_TOPIC_1);
      client.subscribe(MQTT_SWITCH_TOPIC_2);
      client.subscribe(MQTT_SWITCH_TOPIC_3);
      client.subscribe(MQTT_SWITCH_TOPIC_4);
      client.subscribe(MQTT_SWITCH_TOPIC_5);
      client.subscribe(MQTT_SWITCH_TOPIC_6);
      client.subscribe(MQTT_SWITCH_TOPIC_7);
      client.subscribe(MQTT_SWITCH_TOPIC_8);
      client.subscribe(MQTT_HEARTBEAT_SUB);

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
  String strTopic;
  for (uint8_t i = 0; i < p_length; i++) {
    payload.concat((char)p_payload[i]);
  }   
  strTopic = String((char*)p_topic);
    if (strTopic == MQTT_HEARTBEAT_TOPIC) {
    resetWatchdog();
    client.publish(MQTT_HEARTBEAT_PUB, "Heartbeat Received");
    return;
  } 
  switch (hashit(p_topic)) {
    case eSwitch1:
      if (payload.equals(String(LIGHT_ON))) {
        digitalWrite(RELAY_1, RELAY_ON);
        client.publish(MQTT_SWITCH_REPLY_TOPIC_1, LIGHT_ON);
        Serial.println(String(MQTT_SWITCH_REPLY_TOPIC_1) + "ON");
        relayStatus[0] = 1;
      }
      else if (payload.equals(String(LIGHT_FLASH))) {
        Serial.println(String(MQTT_SWITCH_REPLY_TOPIC_1) + "FLASH");
        ticker_relay1.attach_ms(FLASH_INTERVAL_MS, relay1Ticker);
        relayStatus[0] = -1;
      }
      else if (payload.equals(String(LIGHT_OFF))) {
        digitalWrite(RELAY_1, RELAY_OFF);
        client.publish(MQTT_SWITCH_REPLY_TOPIC_1, LIGHT_OFF);
        Serial.println(String(MQTT_SWITCH_REPLY_TOPIC_1) + "OFF");
        ticker_relay1.detach();
        relayStatus[0] = 0;
      }
      break;
    case eSwitch2:
      if (payload.equals(String(LIGHT_ON))) {
        digitalWrite(RELAY_2, RELAY_ON);
        client.publish(MQTT_SWITCH_REPLY_TOPIC_2, LIGHT_ON);
        Serial.println(String(MQTT_SWITCH_REPLY_TOPIC_2) + "1");
        relayStatus[1] = 1;
      }
      else if (payload.equals(String(LIGHT_FLASH))) {
        Serial.println(String(MQTT_SWITCH_REPLY_TOPIC_2) + "FLASH");
        ticker_relay2.attach_ms(FLASH_INTERVAL_MS, relay2Ticker);
        relayStatus[1] = -1;
      }
      else if (payload.equals(String(LIGHT_OFF))) {
        digitalWrite(RELAY_2, RELAY_OFF);
        client.publish(MQTT_SWITCH_REPLY_TOPIC_2, LIGHT_OFF);
        Serial.println(String(MQTT_SWITCH_REPLY_TOPIC_2) + "0");
        ticker_relay2.detach();
        relayStatus[1] = 0;
      }
      break;
    case eSwitch3:
      if (payload.equals(String(LIGHT_ON))) {
        digitalWrite(RELAY_3, RELAY_ON);
        client.publish(MQTT_SWITCH_REPLY_TOPIC_3, LIGHT_ON);
        Serial.println(String(MQTT_SWITCH_REPLY_TOPIC_3) + "1");
        relayStatus[2] = 1;
      }
      else if (payload.equals(String(LIGHT_FLASH))) {
        Serial.println(String(MQTT_SWITCH_REPLY_TOPIC_3) + "FLASH");
        ticker_relay3.attach_ms(FLASH_INTERVAL_MS, relay3Ticker);
        relayStatus[2] = -1;
      }
      else if (payload.equals(String(LIGHT_OFF))) {
        digitalWrite(RELAY_3, RELAY_OFF);
        client.publish(MQTT_SWITCH_REPLY_TOPIC_3, LIGHT_OFF);
        Serial.println(String(MQTT_SWITCH_REPLY_TOPIC_3) + "0");
        ticker_relay3.detach();
        relayStatus[2] = 0;
      }
      break;
    case eSwitch4:
      if (payload.equals(String(LIGHT_ON))) {
        digitalWrite(RELAY_4, RELAY_ON);
        client.publish(MQTT_SWITCH_REPLY_TOPIC_4, LIGHT_ON);
        Serial.println(String(MQTT_SWITCH_REPLY_TOPIC_4) + "1");
        relayStatus[3] = 1;
      }
      else if (payload.equals(String(LIGHT_FLASH))) {
        Serial.println(String(MQTT_SWITCH_REPLY_TOPIC_4) + "FLASH");
        ticker_relay4.attach_ms(FLASH_INTERVAL_MS, relay4Ticker);
        relayStatus[3] = -1;
      }
      else if (payload.equals(String(LIGHT_OFF))) {
        digitalWrite(RELAY_4, RELAY_OFF);
        client.publish(MQTT_SWITCH_REPLY_TOPIC_4, LIGHT_OFF);
        Serial.println(String(MQTT_SWITCH_REPLY_TOPIC_4) + "0");
        ticker_relay4.detach();
        relayStatus[3] = 0;
      }
      break;
    case eSwitch5:
      if (payload.equals(String(LIGHT_ON))) {
        digitalWrite(RELAY_5, RELAY_ON);
        client.publish(MQTT_SWITCH_REPLY_TOPIC_5, LIGHT_ON);
        Serial.println(String(MQTT_SWITCH_REPLY_TOPIC_5) + "1");
        relayStatus[4] = 1;
      }
      else if (payload.equals(String(LIGHT_FLASH))) {
        Serial.println(String(MQTT_SWITCH_REPLY_TOPIC_5) + "FLASH");
        ticker_relay5.attach_ms(FLASH_INTERVAL_MS, relay5Ticker);
        relayStatus[4] = -1;
      }
      else if (payload.equals(String(LIGHT_OFF))) {
        digitalWrite(RELAY_5, RELAY_OFF);
        client.publish(MQTT_SWITCH_REPLY_TOPIC_5, LIGHT_OFF);
        Serial.println(String(MQTT_SWITCH_REPLY_TOPIC_5) + "0");
        ticker_relay5.detach();
        relayStatus[4] = 0;
      }
      break;
    case eSwitch6:
      if (payload.equals(String(LIGHT_ON))) {
        digitalWrite(RELAY_6, RELAY_ON);
        client.publish(MQTT_SWITCH_REPLY_TOPIC_6, LIGHT_ON);
        Serial.println(String(MQTT_SWITCH_REPLY_TOPIC_6) + "1");
        relayStatus[5] = 1;
      }
      else if (payload.equals(String(LIGHT_FLASH))) {
        Serial.println(String(MQTT_SWITCH_REPLY_TOPIC_6) + "FLASH");
        ticker_relay6.attach_ms(FLASH_INTERVAL_MS, relay6Ticker);
        relayStatus[5] = -1;
      }
      else if (payload.equals(String(LIGHT_OFF))) {
        digitalWrite(RELAY_6, RELAY_OFF);
        client.publish(MQTT_SWITCH_REPLY_TOPIC_6, LIGHT_OFF);
        Serial.println(String(MQTT_SWITCH_REPLY_TOPIC_6) + "0");
        ticker_relay6.detach();
        relayStatus[5] = 0;
      }
      break;
    case eSwitch7:
      if (payload.equals(String(LIGHT_ON))) {
        digitalWrite(RELAY_7, RELAY_ON);
        client.publish(MQTT_SWITCH_REPLY_TOPIC_7, LIGHT_ON);
        Serial.println(String(MQTT_SWITCH_REPLY_TOPIC_7) + "1");
        relayStatus[6] = 1;
      }
      else if (payload.equals(String(LIGHT_FLASH))) {
        Serial.println(String(MQTT_SWITCH_REPLY_TOPIC_7) + "FLASH");
        ticker_relay7.attach_ms(FLASH_INTERVAL_MS, relay7Ticker);
        relayStatus[6] = -1;
      }
      else if (payload.equals(String(LIGHT_OFF))) {
        digitalWrite(RELAY_7, RELAY_OFF);
        client.publish(MQTT_SWITCH_REPLY_TOPIC_7, LIGHT_OFF);
        Serial.println(String(MQTT_SWITCH_REPLY_TOPIC_7) + "0");
        ticker_relay7.detach();
        relayStatus[6] = 0;
      }
      break;
    case eSwitch8:
      if (payload.equals(String(LIGHT_ON))) {
        digitalWrite(RELAY_8, RELAY_ON);
        client.publish(MQTT_SWITCH_REPLY_TOPIC_8, LIGHT_ON);
        Serial.println(String(MQTT_SWITCH_REPLY_TOPIC_8) + "1");
        relayStatus[7] = 1;
      }
      else if (payload.equals(String(LIGHT_FLASH))) {
        Serial.println(String(MQTT_SWITCH_REPLY_TOPIC_8) + "FLASH");
        ticker_relay8.attach_ms(FLASH_INTERVAL_MS, relay8Ticker);
        relayStatus[7] = -1;
      }
      else if (payload.equals(String(LIGHT_OFF))) {
        digitalWrite(RELAY_8, RELAY_OFF);
        client.publish(MQTT_SWITCH_REPLY_TOPIC_8, LIGHT_OFF);
        Serial.println(String(MQTT_SWITCH_REPLY_TOPIC_8) + "0");
        ticker_relay8.detach();
        relayStatus[7] = 0;
      }
      break;
  }  
}

// FW update ticker
void fwTicker() {
  readyForFwUpdate = true;
}

void statusTicker() {
  for (int i=0; i<8; i++) {
    String status;
    if (relayStatus[i] > 0) {
      status = "ON";
    }
    else if (relayStatus[i] < 0) {
      status = "FLASH";
    }
    else {
      status = "OFF";
    }
    client.publish((MQTT_SWITCH_REPLY_TOPIC_PREFIX + String(i)).c_str(), status.c_str());
  }
}

void relay1Ticker() {
  if (relayFlashState1) {
    relayFlashState1 = false;
    digitalWrite(RELAY_1, RELAY_ON);
  }
  else {
    relayFlashState1 = true;
    digitalWrite(RELAY_1, RELAY_OFF);
  }
}

void relay2Ticker() {
  if (relayFlashState2) {
    relayFlashState2 = false;
    digitalWrite(RELAY_2, RELAY_ON);
  }
  else {
    relayFlashState2 = true;
    digitalWrite(RELAY_2, RELAY_OFF);
  }
}

void relay3Ticker() {
  if (relayFlashState3) {
    relayFlashState3 = false;
    digitalWrite(RELAY_3, RELAY_ON);
  }
  else {
    relayFlashState3 = true;
    digitalWrite(RELAY_3, RELAY_OFF);
  }
}

void relay4Ticker() {
  if (relayFlashState4) {
    relayFlashState4 = false;
    digitalWrite(RELAY_4, RELAY_ON);
  }
  else {
    relayFlashState4 = true;
    digitalWrite(RELAY_4, RELAY_OFF);
  }
}

void relay5Ticker() {
  if (relayFlashState5) {
    relayFlashState5 = false;
    digitalWrite(RELAY_5, RELAY_ON);
  }
  else {
    relayFlashState5 = true;
    digitalWrite(RELAY_5, RELAY_OFF);
  }
}

void relay6Ticker() {
  if (relayFlashState6) {
    relayFlashState6 = false;
    digitalWrite(RELAY_6, RELAY_ON);
  }
  else {
    relayFlashState6 = true;
    digitalWrite(RELAY_6, RELAY_OFF);
  }
}

void relay7Ticker() {
  if (relayFlashState7) {
    relayFlashState7 = false;
    digitalWrite(RELAY_7, RELAY_ON);
  }
  else {
    relayFlashState7 = true;
    digitalWrite(RELAY_7, RELAY_OFF);
  }
}

void relay8Ticker() {
  if (relayFlashState8) {
    relayFlashState8 = false;
    digitalWrite(RELAY_8, RELAY_ON);
  }
  else {
    relayFlashState8 = true;
    digitalWrite(RELAY_8, RELAY_OFF);
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

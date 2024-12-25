#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <Preferences.h>
#include "FastLED.h"


// Global copy of slave/peer device
// For broadcasts, the addr needs to be ff:ff:ff:ff:ff:ff
// All devices on the same channel
esp_now_peer_info_t slave;
Preferences preferences;

#define CHANNEL 3
#define PRINTSCANRESULTS 0
#define DELETEBEFOREPAIR 0
#define BUTTON_PIN 13
#define LED_PIN 12      

#define RGB_LED_PIN 2   // to DI LED stripe
#define NUM_LEDS 24      // number of LEDs

CRGB leds[NUM_LEDS];
//int brightness = 50;
byte color[3];
String id ;

/////////////////////////////////////////////////////////////////////////////////////

// service function for parsing color:
void parsColor(String colorStr, byte* c){
  String red, green, blue;

  red=colorStr.substring(0, colorStr.indexOf(":"));
  
  colorStr=colorStr.substring(colorStr.indexOf(":")+1);
  green=colorStr.substring(0,colorStr.indexOf(":"));

  blue=colorStr.substring(colorStr.indexOf(":")+1);

  int tmpInt;
  tmpInt = red.toInt(); 
  c[0] = tmpInt;
  tmpInt = green.toInt();
  c[1] = tmpInt;
  tmpInt = blue.toInt();
  c[2]=tmpInt;
}



// Init ESP Now with fallback
void InitESPNow() {
  if (esp_now_init() == ESP_OK) {
    Serial.println("ESPNow Init Success");
  } else {
    Serial.println("ESPNow Init Failed");
    // Retry InitESPNow or simply restart
    ESP.restart();
  }
}

void initBroadcastSlave() {
  // Clear slave data
  memset(&slave, 0, sizeof(slave));
  for (int ii = 0; ii < 6; ++ii) {
    slave.peer_addr[ii] = (uint8_t)0xff;
  }
  slave.channel = CHANNEL; // Pick a channel
  slave.encrypt = 0;       // No encryption
  manageSlave();
}

// Check if the slave is already paired with the master.
// If not, pair the slave with master
bool manageSlave() {
  if (slave.channel == CHANNEL) {
    if (DELETEBEFOREPAIR) {
      deletePeer();
    }

    Serial.print("Slave Status: ");
    const esp_now_peer_info_t *peer = &slave;
    const uint8_t *peer_addr = slave.peer_addr;
    // Check if the peer exists
    bool exists = esp_now_is_peer_exist(peer_addr);
    if (exists) {
      // Slave already paired.
      Serial.println("Already Paired");
      return true;
    } else {
      // Slave not paired, attempt to pair
      esp_err_t addStatus = esp_now_add_peer(peer);
      if (addStatus == ESP_OK) {
        // Pair success
        Serial.println("Pair success");
        return true;
      } else if (addStatus == ESP_ERR_ESPNOW_NOT_INIT) {
        Serial.println("ESPNOW Not Init");
        return false;
      } else if (addStatus == ESP_ERR_ESPNOW_ARG) {
        Serial.println("Invalid Argument");
        return false;
      } else if (addStatus == ESP_ERR_ESPNOW_FULL) {
        Serial.println("Peer list full");
        return false;
      } else if (addStatus == ESP_ERR_ESPNOW_NO_MEM) {
        Serial.println("Out of memory");
        return false;
      } else if (addStatus == ESP_ERR_ESPNOW_EXIST) {
        Serial.println("Peer Exists");
        return true;
      } else {
        Serial.println("Not sure what happened");
        return false;
      }
    }
  } else {
    // No slave found to process
    Serial.println("No Slave found to process");
    return false;
  }
}

void deletePeer() {
  const esp_now_peer_info_t *peer = &slave;
  const uint8_t *peer_addr = slave.peer_addr;
  esp_err_t delStatus = esp_now_del_peer(peer_addr);
  Serial.print("Slave Delete Status: ");
  if (delStatus == ESP_OK) {
    // Delete success
    Serial.println("Success");
  } else if (delStatus == ESP_ERR_ESPNOW_NOT_INIT) {
    Serial.println("ESPNOW Not Init");
  } else if (delStatus == ESP_ERR_ESPNOW_ARG) {
    Serial.println("Invalid Argument");
  } else if (delStatus == ESP_ERR_ESPNOW_NOT_FOUND) {
    Serial.println("Peer not found.");
  } else {
    Serial.println("Not sure what happened");
  }
}

// Send data
void sendData(uint8_t buttonId) {
  char message[20];
  sprintf(message, "BUTTON_PRESS:%d", buttonId);

  const uint8_t *peer_addr = slave.peer_addr;

  Serial.print("Sending: ");
  Serial.println(message);
  esp_err_t result = esp_now_send(peer_addr, (uint8_t *)message, strlen(message) + 1);
  Serial.print("Send Status: ");
  if (result == ESP_OK) {
    Serial.println("Success");
  } else if (result == ESP_ERR_ESPNOW_NOT_INIT) {
    Serial.println("ESPNOW not Init.");
  } else if (result == ESP_ERR_ESPNOW_ARG) {
    Serial.println("Invalid Argument");
  } else if (result == ESP_ERR_ESPNOW_INTERNAL) {
    Serial.println("Internal Error");
  } else if (result == ESP_ERR_ESPNOW_NO_MEM) {
    Serial.println("ESP_ERR_ESPNOW_NO_MEM");
  } else if (result == ESP_ERR_ESPNOW_NOT_FOUND) {
    Serial.println("Peer not found.");
  } else {
    Serial.println("Not sure what happened");
  }
}

// Callback when data is sent from Master to Slave
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.print("Last Packet Sent to: ");
  Serial.println(macStr);
  Serial.print("Last Packet Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

// Updated Callback when data is received from Master
void OnDataRecv(const esp_now_recv_info_t *esp_now_info, const uint8_t *data, int data_len) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           esp_now_info->src_addr[0], esp_now_info->src_addr[1], esp_now_info->src_addr[2],
           esp_now_info->src_addr[3], esp_now_info->src_addr[4], esp_now_info->src_addr[5]);
  Serial.print("Last Packet Recv from: ");
  Serial.println(macStr);

  Serial.print("Last Packet Recv Data: ");
  char recvData[250];
  if (data_len < sizeof(recvData)) {
    memcpy(recvData, data, data_len);
    recvData[data_len] = '\0'; // Null-terminate the string
    Serial.println(recvData);

    // Compare received data with expected messages
    String ledOnMessage = "BUTTON_LED_ON:" + id;
    String ledOffMessage = "BUTTON_LED_OFF:" + id;
    String ledColorPrefix = "BUTTON_LED_COLOR:" + id;

    String recvDataStr = recvData;

    if(recvDataStr.startsWith("BUTTON_SET_ID:",0)){
      Serial.println("Received BUTTON_SET_ID command");
      String idStr;
      idStr=recvDataStr.substring(14);
      Serial.print("Received ID = ");Serial.println(idStr);
      preferences.begin("button", false);
      preferences.putString("id", idStr);
      preferences.end();      
    }

    if (recvDataStr.startsWith(ledOnMessage,0)) {
      // The received message matches LED ON
      Serial.println("Received BUTTON_LED_ON command");
      FastLED.showColor(CRGB(color[0], color[1], color[2]));
      
    }
    else if (recvDataStr.startsWith(ledOffMessage,0)) {
      // The received message matches LED OFF  
      Serial.println("Received BUTTON_LED_OFF command");
      FastLED.showColor(CRGB(0, 0, 0));
      
    }
    else if (recvDataStr.startsWith(ledColorPrefix,0)) {
      // The received message starts with BUTTON_LED_COLOR:ID:
      Serial.println("Received BUTTON_LED_COLOR command");
      int comandLength = 18 + id.length();
      
      String colorStr = recvDataStr.substring(comandLength);
      Serial.println(colorStr);

      parsColor(colorStr, color);    

      preferences.begin("button", false);
      preferences.putBytes("color", color, 3);
      preferences.end();  
      FastLED.showColor(CRGB(color[0], color[1], color[2]));

    }

  } else {
    Serial.println("Data too large");
  }
  Serial.println("");
}

void setup() {
  Serial.begin(9600);
  Serial.println("START_BUTOON_SETUP");
  pinMode(BUTTON_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT); // Ensure the LED pin is set as OUTPUT
  pinMode(RGB_LED_PIN, OUTPUT);
  

  FastLED.addLeds<WS2811, RGB_LED_PIN, GRB>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );
  preferences.begin("button", false);
  id = preferences.getString("id", "");
  preferences.getBytes("color",color,3);
  preferences.end();

  Serial.print("BUTTON_ID = ");Serial.println(id);
  Serial.print("BUTTON_COLOR = ");Serial.print(color[0]);Serial.print(":");Serial.print(color[1]);Serial.print(":");Serial.println(color[2]);
  Serial.println("ESP Start WiFi setup");
  WiFi.mode(WIFI_STA);
  Serial.print("STA MAC: ");
  Serial.println(WiFi.macAddress());

  // Set the Wi-Fi channel to match the ESP-NOW channel
  esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE);

  InitESPNow();
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);

  initBroadcastSlave();

  // Uncomment this if you want to send data on startup
  // sendData(id);
  Serial.println("END_BUTOON_SETUP");
  FastLED.showColor(CRGB(color[0], color[1], color[2]));
}

void loop() {
  int buttonState = digitalRead(BUTTON_PIN); // Read the button state

  if (buttonState == HIGH) { // Check if the button is pressed
    Serial.println("Button Pressed");
    //sendData(id);
    delay(100); // Debounce delay
    while (digitalRead(BUTTON_PIN) == HIGH) {
      // Wait for button release
      delay(10);
    }
  }

  delay(100); // General loop delay
}

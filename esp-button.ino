#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <Preferences.h>
#include "FastLED.h"

// Constants
#define CHANNEL 3
#define PRINTSCANRESULTS 0 
#define DELETEBEFOREPAIR 0
#define BUTTON_PIN 13
#define LED_PIN 12
#define RGB_LED_PIN 2   // DI pin for LED strip
#define NUM_LEDS 24

// ESP-NOW peer info
esp_now_peer_info_t slave;
Preferences preferences;

// LED strip
CRGB leds[NUM_LEDS];
byte color[3];

// Button state
String id;
String receiverId;
bool buttonEnabled = false;
bool buttonBlocked = false;
bool useFastLED = false;

// Main loop states
enum MainLoopState {
  NO_ACTION = 0,
  WINNER_FLASH = 1
};
MainLoopState mainLoopStatus = NO_ACTION;

// LED Animation Functions
void coloredFlashlight(unsigned int del, unsigned int del_, unsigned int num) {
  if (!useFastLED) return;
  
  CRGBPalette16 myPalette = RainbowStripesColors_p;

  for(unsigned int i = 0; i < num; i++) {
    FastLED.showColor(ColorFromPalette(myPalette, random(8) * 32));
    delay(del + del_);
    FastLED.showColor(CRGB(0, 0, 0));
    delay(del);
  }        
}

// Helper Functions
void parsColor(String colorStr, byte* c) {
  int firstColon = colorStr.indexOf(":");
  int secondColon = colorStr.indexOf(":", firstColon + 1);
  
  String red = colorStr.substring(0, firstColon);
  String green = colorStr.substring(firstColon + 1, secondColon);
  String blue = colorStr.substring(secondColon + 1);

  c[0] = red.toInt();
  c[1] = green.toInt(); 
  c[2] = blue.toInt();
}

// ESP-NOW Functions
void InitESPNow() {
  if (esp_now_init() == ESP_OK) {
    Serial.println("ESPNow Init Success");
  } else {
    Serial.println("ESPNow Init Failed");
    ESP.restart();
  }
}

void initBroadcastSlave() {
  memset(&slave, 0, sizeof(slave));
  for (int i = 0; i < 6; i++) {
    slave.peer_addr[i] = (uint8_t)0xff;
  }
  slave.channel = CHANNEL;
  slave.encrypt = 0;
  manageSlave();
}

bool manageSlave() {
  if (slave.channel != CHANNEL) {
    Serial.println("No Slave found to process");
    return false;
  }

  if (DELETEBEFOREPAIR) {
    deletePeer();
  }

  const uint8_t *peer_addr = slave.peer_addr;
  bool exists = esp_now_is_peer_exist(peer_addr);

  if (exists) {
    Serial.println("Already Paired");
    return true;
  }

  esp_err_t addStatus = esp_now_add_peer(&slave);
  
  switch(addStatus) {
    case ESP_OK:
      Serial.println("Pair success");
      return true;
    case ESP_ERR_ESPNOW_NOT_INIT:
      Serial.println("ESPNOW Not Init");
      break;
    case ESP_ERR_ESPNOW_ARG:
      Serial.println("Invalid Argument");
      break; 
    case ESP_ERR_ESPNOW_FULL:
      Serial.println("Peer list full");
      break;
    case ESP_ERR_ESPNOW_NO_MEM:
      Serial.println("Out of memory");
      break;
    case ESP_ERR_ESPNOW_EXIST:
      Serial.println("Peer Exists");
      return true;
    default:
      Serial.println("Not sure what happened");
  }
  return false;
}

void deletePeer() {
  const uint8_t *peer_addr = slave.peer_addr;
  esp_err_t delStatus = esp_now_del_peer(peer_addr);
  
  Serial.print("Slave Delete Status: ");
  switch(delStatus) {
    case ESP_OK:
      Serial.println("Success");
      break;
    case ESP_ERR_ESPNOW_NOT_INIT:
      Serial.println("ESPNOW Not Init");
      break;
    case ESP_ERR_ESPNOW_ARG:
      Serial.println("Invalid Argument");
      break;
    case ESP_ERR_ESPNOW_NOT_FOUND:
      Serial.println("Peer not found.");
      break;
    default:
      Serial.println("Not sure what happened");
  }
}

void sendData(String message) {
  int str_len = message.length() + 1;
  char char_array[str_len];
  message.toCharArray(char_array, str_len);
  
  Serial.print("Sending: ");
  Serial.println(message);
  
  esp_err_t result = esp_now_send(slave.peer_addr, (uint8_t *)char_array, str_len);
  
  Serial.print("Send Status: ");
  switch(result) {
    case ESP_OK:
      Serial.println("Success");
      break;
    case ESP_ERR_ESPNOW_NOT_INIT:
      Serial.println("ESPNOW not Init.");
      break;
    case ESP_ERR_ESPNOW_ARG:
      Serial.println("Invalid Argument");
      break;
    case ESP_ERR_ESPNOW_INTERNAL:
      Serial.println("Internal Error");
      break;
    case ESP_ERR_ESPNOW_NO_MEM:
      Serial.println("ESP_ERR_ESPNOW_NO_MEM");
      break;
    case ESP_ERR_ESPNOW_NOT_FOUND:
      Serial.println("Peer not found.");
      break;
    default:
      Serial.println("Not sure what happened");
  }
}

// Callback Functions
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
           
  Serial.print("Last Packet Sent to: ");
  Serial.println(macStr);
  Serial.print("Last Packet Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void handleCommand(String command, String data) {
  if (command == "BUTTON_WINNER_FLASH" && data == id) {
    Serial.println("Received BUTTON_WINNER_FLASH command");
    mainLoopStatus = WINNER_FLASH;
  }
  else if (command == "BUTTON_GET_ID") {
    Serial.println("Received BUTTON_GET_ID command");
    sendData("BUTTON_ID:" + id);
  }
  else if (command == "BUTTON_GET_RECEIVER_ID") {
    Serial.println("Received BUTTON_GET_RECEIVER_ID command");
    sendData("BUTTON_RECEIVER_ID:" + receiverId);
  }
  else if ((command == "BUTTON_ENABLE" && data == id) || command == "ALL_BUTTONS_ENABLED") {
    Serial.println("Received BUTTON_ENABLE command");
    buttonEnabled = true;
  }
  else if ((command == "BUTTON_DISABLE" && data == id) || command == "ALL_BUTTONS_DISABLED") {
    Serial.println("Received BUTTON_DISABLE command");
    buttonEnabled = false;
  }
  else if ((command == "BUTTON_BLOCK" && data == id) || command == "ALL_BUTTONS_BLOCKED") {
    Serial.println("Received BUTTON_BLOCK command");
    buttonBlocked = true;
  }
  else if ((command == "BUTTON_UNBLOCK" && data == id) || command == "ALL_BUTTONS_UNBLOCK") {
    Serial.println("Received BUTTON_UNBLOCK command");
    buttonBlocked = false;
  }
  else if (command == "BUTTON_LED_ON" && data == id && useFastLED) {
    Serial.println("Received BUTTON_LED_ON command");
    FastLED.showColor(CRGB(color[0], color[1], color[2]));
  }
  else if (command == "BUTTON_LED_OFF" && data == id && useFastLED) {
    Serial.println("Received BUTTON_LED_OFF command");
    FastLED.showColor(CRGB(0, 0, 0));
  }
}

void handleSetId(String newId) {
  Serial.println("Received BUTTON_SET_ID command");
  id = newId;
  Serial.print("Received ID = ");
  Serial.println(id);
  
  preferences.begin("button", false);
  preferences.putString("id", id);
  preferences.end();
}

void handleSetReceiverId(String newReceiverId) {
  Serial.println("Received BUTTON_SET_RECEIVER_ID command");
  receiverId = newReceiverId;
  Serial.print("Received Receiver ID = ");
  Serial.println(receiverId);
  
  preferences.begin("button", false);
  preferences.putString("receiverId", receiverId);
  preferences.end();
}

void handleChangeId(String oldId, String newId) {
  if (oldId == id) {
    Serial.println("Received BUTTON_CHANGE_ID command");
    id = newId;
    Serial.print("Received ID = ");
    Serial.println(id);
    
    preferences.begin("button", false);
    preferences.putString("id", id);
    preferences.end();
  }
}

void handleSetColor(String targetId, String colorStr) {
  if (targetId == id) {
    Serial.println("Received BUTTON_LED_COLOR command");
    parsColor(colorStr, color);
    
    preferences.begin("button", false);
    preferences.putBytes("color", color, 3);
    preferences.end();
    
    if (useFastLED) {
      FastLED.showColor(CRGB(color[0], color[1], color[2]));
    }
  }
}

void OnDataRecv(const esp_now_recv_info_t *esp_now_info, const uint8_t *data, int data_len) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           esp_now_info->src_addr[0], esp_now_info->src_addr[1], esp_now_info->src_addr[2],
           esp_now_info->src_addr[3], esp_now_info->src_addr[4], esp_now_info->src_addr[5]);
           
  Serial.print("Last Packet Recv from: ");
  Serial.println(macStr);

  if (data_len >= sizeof(char[150])) {
    Serial.println("Data too large");
    return;
  }

  char recvData[150];
  memcpy(recvData, data, data_len);
  recvData[data_len] = '\0';
  
  String message = String(recvData);
  Serial.print("Last Packet Recv Data: ");
  Serial.println(message);

  // Parse command and data
  int colonPos = message.indexOf(':');
  if (colonPos > 0) {
    String command = message.substring(0, colonPos);
    String data = message.substring(colonPos + 1);
    
    if (command == "BUTTON_SET_ID") {
      handleSetId(data);
    }
    else if (command == "BUTTON_SET_RECEIVER_ID") {
      handleSetReceiverId(data);
    }
    else if (command == "BUTTON_CHANGE_ID") {
      int secondColon = data.indexOf(':');
      if (secondColon > 0) {
        handleChangeId(data.substring(0, secondColon), 
                      data.substring(secondColon + 1));
      }
    }
    else if (command == "BUTTON_LED_COLOR") {
      int secondColon = data.indexOf(':');
      if (secondColon > 0) {
        handleSetColor(data.substring(0, secondColon),
                      data.substring(secondColon + 1));
      }
    }
    else {
      handleCommand(command, data);
    }
  }
  else {
    handleCommand(message, "");
  }
}

void setup() {
  Serial.begin(9600);
  Serial.println("START_BUTOON_SETUP");

  randomSeed(analogRead(3));

  // Pin Setup
  pinMode(BUTTON_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(RGB_LED_PIN, OUTPUT);
  
  if (useFastLED) {
    FastLED.addLeds<WS2811, RGB_LED_PIN, GRB>(leds, NUM_LEDS)
           .setCorrection(TypicalLEDStrip);
  }
  
  // Load saved preferences
  preferences.begin("button", false);
  id = preferences.getString("id", "");
  receiverId = preferences.getString("receiverId", "RECEIVER_1");
  preferences.getBytes("color", color, 3);
  preferences.end();

  Serial.print("BUTTON_ID = ");
  Serial.println(id);
  Serial.print("RECEIVER_ID = ");
  Serial.println(receiverId);
  Serial.print("BUTTON_COLOR = ");
  Serial.print(color[0]);
  Serial.print(":");
  Serial.print(color[1]);
  Serial.print(":");
  Serial.println(color[2]);

  // WiFi Setup
  Serial.println("ESP Start WiFi setup");
  WiFi.mode(WIFI_STA);
  Serial.print("STA MAC: ");
  Serial.println(WiFi.macAddress());
  esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE);

  // ESP-NOW Setup
  InitESPNow();
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);
  initBroadcastSlave();

  if (useFastLED) {
    FastLED.showColor(CRGB(color[0], color[1], color[2]));
  }
 
  mainLoopStatus = NO_ACTION;
  Serial.println("END_BUTOON_SETUP");
}

void loop() {
  // Handle button press
  if (buttonEnabled && !buttonBlocked && digitalRead(BUTTON_PIN) == HIGH) {
    Serial.println("Button Pressed");
    sendData("BUTTON_PRESS:" + id);
    
    delay(100); // Debounce
    while (digitalRead(BUTTON_PIN) == HIGH) {
      delay(10); // Wait for release
    }
  }

  // Handle main loop states
  switch(mainLoopStatus) {
    case WINNER_FLASH:
      coloredFlashlight(100, 10, 30);
      mainLoopStatus = NO_ACTION;
      break;
      
    case NO_ACTION:
    default:
      break;
  }

  delay(100);
}

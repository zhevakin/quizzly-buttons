// button.ino
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

// Heartbeat constants
#define HEARTBEAT_INTERVAL 5000  // Send heartbeat every 5 seconds
#define MAX_MISSED_HEARTBEATS 3  // Consider connection lost after 3 missed responses

// Maximum number of retry attempts for sending a message
#define MAX_SEND_RETRIES 3

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

// Global pairing and heartbeat variables
bool isPaired = false;
uint8_t receiverMac[6];
unsigned long lastHeartbeatSent = 0;
unsigned long lastHeartbeatReceived = 0;
int missedHeartbeats = 0;

// ------------------
// LED Animation Functions
// ------------------
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

// ------------------
// Helper Functions
// ------------------
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

// ------------------
// ESP-NOW Functions
// ------------------
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

// ------------------
// Retry Mechanism Helper
// ------------------
// This function tries to send data using ESP-NOW up to maxRetries times.
// It returns true if sending was queued successfully.
bool sendWithRetry(const uint8_t *peer_addr, uint8_t *data, size_t len, uint8_t maxRetries = MAX_SEND_RETRIES) {
  uint8_t attempt = 0;
  esp_err_t result;
  while (attempt < maxRetries) {
    result = esp_now_send(peer_addr, data, len);
    if (result == ESP_OK) {
      return true;
    }
    Serial.print("Send attempt ");
    Serial.print(attempt + 1);
    Serial.print(" failed with error code: ");
    Serial.println(result);
    attempt++;
    delay(50); // Small delay between attempts
  }
  return false;
}

// ------------------
// Send Data Function with Retry
// ------------------
void sendData(String message) {
  int str_len = message.length() + 1;
  char char_array[str_len];
  message.toCharArray(char_array, str_len);
  
  Serial.print("Sending: ");
  Serial.println(message);
  
  bool success = sendWithRetry(slave.peer_addr, (uint8_t *)char_array, str_len);
  
  if (success) {
    Serial.println("Send success");
  } else {
    Serial.println("Max retry attempts reached, message not sent.");
  }
}

// ------------------
// Callback Functions
// ------------------
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2],
           mac_addr[3], mac_addr[4], mac_addr[5]);
           
  Serial.print("Last Packet Sent to: ");
  Serial.println(macStr);
  Serial.print("Last Packet Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void handleCommand(String command, String data) {
  if (command == "FLASH") {
    Serial.println("Received FLASH command");
    mainLoopStatus = WINNER_FLASH;
  }
  else if (command == "GET_BUTTON_ID") {
    Serial.println("Received GET_BUTTON_ID command");
    sendData("BUTTON_ID:" + id);
  }
  else if (command == "GET_RECEIVER_ID") {
    Serial.println("Received GET_RECEIVER_ID command");
    sendData("RECEIVER_ID:" + receiverId);
  }
  else if (command == "ENABLE") {
    Serial.println("Received ENABLE command");
    buttonEnabled = true;
  }
  else if (command == "DISABLE") {
    Serial.println("Received DISABLE command");
    buttonEnabled = false;
  }
  else if (command == "BLOCK") {
    Serial.println("Received BLOCK command");
    buttonBlocked = true;
  }
  else if (command == "UNBLOCK") {
    Serial.println("Received UNBLOCK command");
    buttonBlocked = false;
  }
  else if (command == "LED_ON" && useFastLED) {
    Serial.println("Received LED_ON command");
    FastLED.showColor(CRGB(color[0], color[1], color[2]));
  }
  else if (command == "LED_OFF" && useFastLED) {
    Serial.println("Received LED_OFF command");
    FastLED.showColor(CRGB(0, 0, 0));
  }
}

void handleSetId(String newId) {
  Serial.println("Received SET_BUTTON_ID command");
  id = newId;
  Serial.print("Received ID = ");
  Serial.println(id);
  
  preferences.begin("button", false);
  preferences.putString("id", id);
  preferences.end();
}

void handleSetReceiverId(String newReceiverId) {
  Serial.println("Received SET_RECEIVER_ID command");
  receiverId = newReceiverId;
  Serial.print("Received Receiver ID = ");
  Serial.println(receiverId);
  
  preferences.begin("button", false);
  preferences.putString("receiverId", receiverId);
  preferences.end();
}

void handleSetColor(String targetId, String colorStr) {
  if (targetId == id) {
    Serial.println("Received SET_LED_COLOR command");
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

  // Handle pairing response
  if (message.startsWith("PAIRING_RESPONSE:")) {
    int colonPos = message.indexOf(':');
    String buttonId = message.substring(colonPos + 1);
    
    if (buttonId == id) {
      // Store receiver MAC
      memcpy(receiverMac, esp_now_info->src_addr, 6);
      isPaired = true;
      lastHeartbeatReceived = millis(); // Initialize heartbeat timing
      
      // Update peer to communicate directly with receiver
      memcpy(slave.peer_addr, receiverMac, 6);
      manageSlave();
      
      Serial.println("Paired successfully with receiver");
    }
    return;
  }

  // Handle heartbeat response
  if (message.startsWith("HEARTBEAT_RESPONSE:")) {
    int colonPos = message.indexOf(':');
    String buttonId = message.substring(colonPos + 1);
    
    if (buttonId == id) {
      lastHeartbeatReceived = millis();
      missedHeartbeats = 0;
      return;
    }
  }

  // Parse command and data
  int colonPos = message.indexOf(':');
  if (colonPos > 0) {
    String command = message.substring(0, colonPos);
    String data = message.substring(colonPos + 1);
    
    if (command == "SET_BUTTON_ID") {
      handleSetId(data);
    }
    else if (command == "SET_RECEIVER_ID") {
      handleSetReceiverId(data);
    }
    else if (command == "SET_LED_COLOR") {
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

// ------------------
// Pairing and Heartbeat Functions
// ------------------
void sendPairingRequest() {
  if (!isPaired) {
    String message = "PAIRING_REQUEST:" + id + ":" + receiverId;
    sendData(message);
    Serial.println("Sent pairing request");
  }
}

void sendHeartbeat() {
  if (isPaired) {
    String message = "HEARTBEAT:" + id;
    sendData(message);
    lastHeartbeatSent = millis();
  }
}

// ------------------
// Setup and Loop
// ------------------
void setup() {
  Serial.begin(9600);
  Serial.println("START_BUTTON_SETUP");

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
  
  isPaired = false;
  mainLoopStatus = NO_ACTION;
  lastHeartbeatSent = 0;
  lastHeartbeatReceived = 0;
  missedHeartbeats = 0;
  
  Serial.println("END_BUTTON_SETUP");
}

void loop() {
  unsigned long currentTime = millis();
  
  // Check pairing status and handle heartbeat
  if (isPaired) {
    // Send heartbeat periodically
    if (currentTime - lastHeartbeatSent >= HEARTBEAT_INTERVAL) {
      sendHeartbeat();
    }
    
    // Check for missed heartbeats
    if (currentTime - lastHeartbeatReceived >= HEARTBEAT_INTERVAL * MAX_MISSED_HEARTBEATS) {
      Serial.println("Connection lost - too many missed heartbeats");
      isPaired = false;
      missedHeartbeats = 0;
      // Reset peer to broadcast address for pairing
      initBroadcastSlave();
    }
  } else {
    sendPairingRequest();
    delay(1000); // Wait before next attempt
    return;
  }

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
}

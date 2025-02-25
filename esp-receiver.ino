// receiver.ino
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <Preferences.h>
#include <vector>

#define CHANNEL 3
#define LOG_LOCAL_LEVEL ESP_LOG_NONE
#include "esp_log.h"

// Structure to store button information
struct ButtonInfo {
  String id;
  uint8_t mac[6];
};

// Global variables
bool debug = true;
esp_now_peer_info_t broadcastPeer;
String receiverId;
Preferences preferences;
std::vector<ButtonInfo> pairedButtons;  // Holds paired buttons

// Maximum retry attempts for sending messages
#define MAX_SEND_RETRIES 3

// ------------------
// Function Declarations
// ------------------
void OnDataRecv(const esp_now_recv_info_t *esp_now_info, const uint8_t *data, int data_len);
void initializeWiFi();
void initializeESPNow();
void initializeBroadcastPeer();
void handleSerialMessage();
void loadReceiverId();
void handleSetId(const String& newId);
String macToString(const uint8_t* mac);
void sendPairingResponse(const uint8_t* mac, const String &buttonId);
void sendBroadcastMessage(const String& payload);
void sendToAllPairedButtons(const String& payload);
void sendToSpecificButton(const String& buttonId, const String& payload);
void logDebug(const char* message);

// ------------------
// Retry Helper Function
// ------------------
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
    delay(50);
  }
  return false;
}

// ------------------
// Setup Functions
// ------------------
void setup() {
  Serial.begin(9600);
  logDebug("Receiver start setup");

  loadReceiverId();
  initializeWiFi();
  initializeESPNow();
  initializeBroadcastPeer();

  logDebug("Receiver end setup");
}

void loop() {
  if (Serial.available()) {
    handleSerialMessage();
  }
}

// ------------------
// Debug Helper
// ------------------
void logDebug(const char* message) {
  if (debug) {
    Serial.println(message);
  }
}

// ------------------
// Receiver ID and Preference Functions
// ------------------
void loadReceiverId() {
  preferences.begin("receiver", false);
  receiverId = preferences.getString("id", "RECEIVER_1"); // Default ID if not set
  preferences.end();
  Serial.print("Receiver ID: ");
  Serial.println(receiverId);
}

void handleSetId(const String& newId) {
  receiverId = newId;
  preferences.begin("receiver", false);
  preferences.putString("id", receiverId);
  preferences.end();
  Serial.print("New Receiver ID set: ");
  Serial.println(receiverId);
}

// ------------------
// WiFi and ESP-NOW Setup Functions
// ------------------
void initializeWiFi() {
  WiFi.mode(WIFI_STA);
  esp_wifi_init(NULL);
  esp_wifi_start();
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE);
}

void initializeESPNow() {
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }
  esp_now_register_recv_cb(OnDataRecv);
}

void initializeBroadcastPeer() {
  memset(&broadcastPeer, 0, sizeof(broadcastPeer));
  for (int i = 0; i < 6; i++) {
    broadcastPeer.peer_addr[i] = 0xFF;
  }
  broadcastPeer.channel = CHANNEL;
  broadcastPeer.encrypt = false;

  if (esp_now_add_peer(&broadcastPeer) != ESP_OK) {
    Serial.println("Failed to add broadcast peer");
    return;
  }
}

// ------------------
// Message Sending Functions
// ------------------
void sendBroadcastMessage(const String& payload) {
  bool success = sendWithRetry(broadcastPeer.peer_addr, (uint8_t*)payload.c_str(), payload.length() + 1);
  if (success) {
    Serial.println("Message broadcasted successfully to all devices");
  } else {
    Serial.println("Error broadcasting message after retries");
  }
}

void sendToAllPairedButtons(const String& payload) {
  for (const auto& button : pairedButtons) {
    bool success = sendWithRetry(button.mac, (uint8_t*)payload.c_str(), payload.length() + 1);
    if (success) {
      Serial.print("Message sent to button ");
      Serial.println(button.id);
    } else {
      Serial.print("Error sending message to button ");
      Serial.print(button.id);
      Serial.println(" after retries.");
    }
  }
}

void sendToSpecificButton(const String& buttonId, const String& payload) {
  bool found = false;
  for (const auto& button : pairedButtons) {
    if (button.id.equals(buttonId)) {
      bool success = sendWithRetry(button.mac, (uint8_t*)payload.c_str(), payload.length() + 1);
      if (success) {
        Serial.print("Message sent to button ");
        Serial.println(button.id);
      } else {
        Serial.print("Error sending message to button ");
        Serial.print(button.id);
        Serial.println(" after retries.");
      }
      found = true;
      break;
    }
  }
  if (!found) {
    Serial.print("No paired button found with id: ");
    Serial.println(buttonId);
  }
}

void sendPairingResponse(const uint8_t* mac, const String &buttonId) {
  // Prepare the response message.
  String response = "PAIRING_RESPONSE:" + buttonId;

  // Create a peer info structure for the button.
  esp_now_peer_info_t peerInfo;
  memset(&peerInfo, 0, sizeof(peerInfo));
  memcpy(peerInfo.peer_addr, mac, 6);
  peerInfo.channel = CHANNEL;
  peerInfo.encrypt = false;

  // Attempt to add the peer.
  esp_err_t result = esp_now_add_peer(&peerInfo);
  if (result == ESP_ERR_ESPNOW_EXIST) {
    // Peer already exists.
  } else if (result != ESP_OK) {
    Serial.print("Failed to add peer for pairing response: ");
    Serial.println(result);
    return;
  }

  // Send the pairing response message using the retry mechanism.
  bool success = sendWithRetry(mac, (uint8_t*)response.c_str(), response.length() + 1);
  if (success) {
    Serial.print("Pairing response sent to ");
    Serial.println(macToString(mac));
  } else {
    Serial.print("Failed to send pairing response to ");
    Serial.println(macToString(mac));
  }
}

// ------------------
// Serial Message Handler
// ------------------
void handleSerialMessage() {
  String message = Serial.readStringUntil('\n');
  message.trim();

  if (message.startsWith("SET_ID:")) {
    String newId = message.substring(String("SET_ID:").length());
    newId.trim();
    handleSetId(newId);
  } else if (message.equals("GET_ID")) {
    Serial.print("Current Receiver ID: ");
    Serial.println(receiverId);
  }
  else if (message.startsWith("BROADCAST:")) {
    String payload = message.substring(String("BROADCAST:").length());
    payload.trim();
    sendBroadcastMessage(payload);
  }
  else if (message.startsWith("PAIRED_BUTTONS:")) {
    String payload = message.substring(String("PAIRED_BUTTONS:").length());
    payload.trim();
    sendToAllPairedButtons(payload);
  }
  else if (message.startsWith("BUTTON:")) {
    String remainder = message.substring(String("BUTTON:").length());
    int colonIndex = remainder.indexOf(':');
    if (colonIndex != -1) {
      String buttonId = remainder.substring(0, colonIndex);
      String payload = remainder.substring(colonIndex + 1);
      payload.trim();
      sendToSpecificButton(buttonId, payload);
    } else {
      Serial.println("Invalid BUTTON message format. Use BUTTON:<ID>:<MESSAGE>");
    }
  } else {
    Serial.println("Invalid message format");
  }
}

// ------------------
// MAC Helper Function
// ------------------
String macToString(const uint8_t* mac) {
  char macStr[18];
  sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", 
          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(macStr);
}

// ------------------
// ESP-NOW Receive Callback
// ------------------
void OnDataRecv(const esp_now_recv_info_t *esp_now_info, const uint8_t *data, int data_len) {
  char incomingData[data_len + 1];
  memcpy(incomingData, data, data_len);
  incomingData[data_len] = '\0';

  Serial.println(incomingData);

  String msg = String(incomingData);
  if (msg.startsWith("PAIRING_REQUEST:")) {
    // Expected format: PAIRING_REQUEST:BUTTON_ID:RECEIVER_ID
    int firstColon = msg.indexOf(':');
    int secondColon = msg.indexOf(':', firstColon + 1);
    if (firstColon != -1 && secondColon != -1) {
      String buttonId = msg.substring(firstColon + 1, secondColon);
      String targetReceiverId = msg.substring(secondColon + 1);
      targetReceiverId.trim();
      
      // Only process if the pairing request is for this receiver.
      if (targetReceiverId.equals(receiverId)) {
        ButtonInfo newButton;
        newButton.id = buttonId;
        memcpy(newButton.mac, esp_now_info->src_addr, 6);
        
        pairedButtons.push_back(newButton);
        
        Serial.print("Button added: ");
        Serial.println(buttonId);
        Serial.print("MAC: ");
        Serial.println(macToString(newButton.mac));

        // Send pairing response to the button.
        sendPairingResponse(newButton.mac, buttonId);
      } else {
        Serial.print("Pairing request intended for ");
        Serial.println(targetReceiverId);
      }
    } else {
      Serial.println("Invalid pairing request format");
    }
  }

  // Handle heartbeat messages
  if (msg.startsWith("HEARTBEAT:")) {
    int colonPos = msg.indexOf(':');
    String buttonId = msg.substring(colonPos + 1);
    
    // Send heartbeat response back to the button
    String response = "HEARTBEAT_RESPONSE:" + buttonId;
    
    esp_now_peer_info_t tempPeer;
    memset(&tempPeer, 0, sizeof(tempPeer));
    memcpy(tempPeer.peer_addr, esp_now_info->src_addr, 6);
    tempPeer.channel = CHANNEL;
    tempPeer.encrypt = 0;
    
    if (!esp_now_is_peer_exist(tempPeer.peer_addr)) {
      esp_now_add_peer(&tempPeer);
    }
    
    sendWithRetry(tempPeer.peer_addr, (uint8_t*)response.c_str(), response.length() + 1);
    return;
  }
}

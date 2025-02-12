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

// Function declarations
void OnDataRecv(const esp_now_recv_info_t *esp_now_info, const uint8_t *data, int data_len);
void initializeWiFi();
void initializeESPNow();
void initializeBroadcastPeer();
void handleSerialMessage();
void loadReceiverId();
void handleSetId(const String& newId);
String macToString(const uint8_t* mac);
void sendPairingResponse(const uint8_t* mac, const String &buttonId);

// New helper functions for messaging
void sendBroadcastMessage(const String& payload);
void sendToAllPairedButtons(const String& payload);
void sendToSpecificButton(const String& buttonId, const String& payload);
void logDebug(const char* message);

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

// Debug print helper
void logDebug(const char* message) {
  if (debug) {
    Serial.println(message);
  }
}

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

// Send a message to all devices (broadcast)
void sendBroadcastMessage(const String& payload) {
  esp_err_t result = esp_now_send(broadcastPeer.peer_addr, (uint8_t*)payload.c_str(), payload.length() + 1);
  if (result == ESP_OK) {
    Serial.println("Message broadcasted successfully to all devices");
  } else {
    Serial.print("Error broadcasting message: ");
    Serial.println(result);
  }
}

// Send a message to all paired buttons
void sendToAllPairedButtons(const String& payload) {
  for (const auto& button : pairedButtons) {
    esp_err_t result = esp_now_send(button.mac, (uint8_t*)payload.c_str(), payload.length() + 1);
    if (result == ESP_OK) {
      Serial.print("Message sent to button ");
      Serial.println(button.id);
    } else {
      Serial.print("Error sending message to button ");
      Serial.print(button.id);
      Serial.print(": ");
      Serial.println(result);
    }
  }
}

// Send a message to a specific paired button identified by buttonId
void sendToSpecificButton(const String& buttonId, const String& payload) {
  bool found = false;
  for (const auto& button : pairedButtons) {
    if (button.id.equals(buttonId)) {
      esp_err_t result = esp_now_send(button.mac, (uint8_t*)payload.c_str(), payload.length() + 1);
      if (result == ESP_OK) {
        Serial.print("Message sent to button ");
        Serial.println(button.id);
      } else {
        Serial.print("Error sending message to button ");
        Serial.print(button.id);
        Serial.print(": ");
        Serial.println(result);
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

// Improved serial message handler
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
  // If message starts with "BROADCAST:" send to all devices via broadcast peer
  else if (message.startsWith("BROADCAST:")) {
    String payload = message.substring(String("BROADCAST:").length());
    payload.trim();
    sendBroadcastMessage(payload);
  }
  // If message starts with "PAIRED_BUTTONS:" send to all paired buttons
  else if (message.startsWith("PAIRED_BUTTONS:")) {
    String payload = message.substring(String("PAIRED_BUTTONS:").length());
    payload.trim();
    sendToAllPairedButtons(payload);
  }
  // If message starts with "BUTTON:" send only to the specific paired button.
  // Expected format: BUTTON:<BUTTON_ID>:<MESSAGE>
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

// Helper to convert MAC address to a human-readable string
String macToString(const uint8_t* mac) {
  char macStr[18];
  sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", 
          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(macStr);
}

// Function to send pairing response to the button that requested pairing
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

  // Send the pairing response message.
  result = esp_now_send(mac, (uint8_t*)response.c_str(), response.length() + 1);
  if (result == ESP_OK) {
    Serial.print("Pairing response sent to ");
    Serial.println(macToString(mac));
  } else {
    Serial.print("Failed to send pairing response: ");
    Serial.println(result);
  }
}

// Callback function for received ESP-NOW messages
void OnDataRecv(const esp_now_recv_info_t *esp_now_info, const uint8_t *data, int data_len) {
  // Convert incoming data to a null-terminated string.
  char incomingData[data_len + 1];
  memcpy(incomingData, data, data_len);
  incomingData[data_len] = '\0';

  // Print received message.
  Serial.println(incomingData);

  // Check if this is a pairing request message.
  String msg = String(incomingData);
  if (msg.startsWith("PAIRING_REQUEST:")) {
    // Expected format: PAIRING_REQUEST:BUTTON_ID:RECEIVER_ID
    int firstColon = msg.indexOf(':');
    int secondColon = msg.indexOf(':', firstColon + 1);
    if (firstColon != -1 && secondColon != -1) {
      String buttonId = msg.substring(firstColon + 1, secondColon);
      String targetReceiverId = msg.substring(secondColon + 1);
      targetReceiverId.trim();  // Remove any extra whitespace
      
      // Only process if the pairing request is for this receiver.
      if (targetReceiverId.equals(receiverId)) {
        ButtonInfo newButton;
        newButton.id = buttonId;
        memcpy(newButton.mac, esp_now_info->src_addr, 6);
        
        // (Optional) Check here for duplicate buttons if necessary.
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
}

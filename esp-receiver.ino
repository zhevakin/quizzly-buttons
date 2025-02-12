#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <Preferences.h>

#define CHANNEL 3
#define LOG_LOCAL_LEVEL ESP_LOG_NONE
#include "esp_log.h"

// Global variables
bool debug = true;
esp_now_peer_info_t broadcastPeer;
String receiverId;
Preferences preferences;

// Function declarations
void OnDataRecv(const esp_now_recv_info_t *esp_now_info, const uint8_t *data, int data_len);
void initializeWiFi();
void initializeESPNow();
void initializeBroadcastPeer();
void handleSerialMessage();
void loadReceiverId();
void handleSetId(const String& newId);

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

// Helper functions
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

void handleSerialMessage() {
  String message = Serial.readStringUntil('\n');
  message.trim();

  if (message.startsWith("SET_ID:")) {
    String newId = message.substring(7); // Skip "SET_ID:"
    handleSetId(newId);
  } else if (message == "GET_ID") {
    Serial.print("Current Receiver ID: ");
    Serial.println(receiverId);
  } else if (message.startsWith("BUTTON_") || message.startsWith("ALL_BUTTONS_")) {
    broadcastMessage(message);
  } else {
    Serial.println("Invalid message format");
  }
}

void broadcastMessage(const String& message) {
  esp_err_t result = esp_now_send(broadcastPeer.peer_addr, 
                                 (uint8_t *)message.c_str(), 
                                 message.length() + 1);
                                 
  if (result == ESP_OK) {
    Serial.println("Message broadcasted successfully");
  } else {
    Serial.print("Error broadcasting message: ");
    Serial.println(result);
  }
}

void OnDataRecv(const esp_now_recv_info_t *esp_now_info, const uint8_t *data, int data_len) {
  // Convert incoming data to null-terminated string
  char incomingData[data_len + 1];
  memcpy(incomingData, data, data_len);
  incomingData[data_len] = '\0';

  // Print received message
  Serial.println(incomingData);
}

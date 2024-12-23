#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>

#define CHANNEL 3
#define LOG_LOCAL_LEVEL ESP_LOG_NONE
#include "esp_log.h"
// #define LED_PIN 13

esp_now_peer_info_t broadcastPeer;

// Function declaration
void OnDataRecv(const esp_now_recv_info_t *esp_now_info, const uint8_t *data, int data_len);

void setup() {
  Serial.begin(9600); // Initialize Serial communication

  // Wi-Fi initialization
  WiFi.mode(WIFI_STA);
  esp_wifi_init(NULL);
  esp_wifi_start();
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE);

  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }

  esp_now_register_recv_cb(OnDataRecv);

  // Initialize the broadcast peer
  memset(&broadcastPeer, 0, sizeof(broadcastPeer));
  for (int i = 0; i < 6; i++) {
    broadcastPeer.peer_addr[i] = 0xFF;
  }
  broadcastPeer.channel = CHANNEL;
  broadcastPeer.encrypt = false;

  // Add the broadcast peer
  if (esp_now_add_peer(&broadcastPeer) != ESP_OK) {
    Serial.println("Failed to add broadcast peer");
    return;
  }
}

void loop() {
  if (Serial.available()) {
    String message = Serial.readStringUntil('\n');
    message.trim(); // Remove any leading/trailing whitespace

    // Check if the message is in the correct format
    if (message.startsWith("BUTTON_")) {
      // Send the message via ESP-NOW
      esp_err_t result = esp_now_send(broadcastPeer.peer_addr, (uint8_t *)message.c_str(), message.length() + 1);
      if (result == ESP_OK) {
        Serial.println("Message broadcasted successfully");
      } else {
        Serial.print("Error broadcasting message: ");
        Serial.println(result);
      }
    } else {
      Serial.println("Invalid message format");
    }
  }
}

// Function definition
void OnDataRecv(const esp_now_recv_info_t *esp_now_info, const uint8_t *data, int data_len) {
  // Convert MAC address to string
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           esp_now_info->src_addr[0], esp_now_info->src_addr[1], esp_now_info->src_addr[2],
           esp_now_info->src_addr[3], esp_now_info->src_addr[4], esp_now_info->src_addr[5]);

  // Copy incoming data into a buffer and null-terminate it
  char incomingData[data_len + 1];
  memcpy(incomingData, data, data_len);
  incomingData[data_len] = '\0'; // Null-terminate the string

  // Print the received message
  // Serial.print("Received from ");
  // Serial.print(macStr);
  // Serial.print(": ");
  Serial.println(incomingData);
}

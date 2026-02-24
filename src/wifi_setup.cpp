#include <WiFi.h>
#include "config.h"

// ============ WIFI SETUP ============
void setupWiFi() {
  Serial.println("WiFi: Setting mode to AP...");
  Serial.flush();
  WiFi.mode(WIFI_AP);
  vTaskDelay(100 / portTICK_PERIOD_MS);
  
  Serial.println("WiFi: Starting softAP...");
  Serial.flush();
  bool result = WiFi.softAP(SSID, PASSWORD);
  Serial.printf("WiFi: softAP result = %d\n", result);
  Serial.flush();
  vTaskDelay(100 / portTICK_PERIOD_MS);
  
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
  Serial.flush();
  
  // Turn LED blue to indicate WiFi is ready
  Serial.println("WiFi: LED status -> BLUE (WiFi Ready)");
  ledStatusBlue();
}

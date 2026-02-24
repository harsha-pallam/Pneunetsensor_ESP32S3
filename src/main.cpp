#include <Arduino.h>
#include "config.h"

// ============ SETUP ============
void setup() {
  Serial.begin(115200);
  vTaskDelay(2000 / portTICK_PERIOD_MS);
  
  Serial.println("\n\n=== PneuNet Gripper Dual Sensor System ===");
  Serial.println("Measuring: Pressure + Capacitance");
  Serial.flush();
  vTaskDelay(100 / portTICK_PERIOD_MS);
  
  Serial.println("Initializing LED...");
  Serial.flush();
  initLED();
  ledStatusRed();
  vTaskDelay(100 / portTICK_PERIOD_MS);
  
  Serial.println("Setting up ADC...");
  Serial.flush();
  analogReadResolution(12);
  Serial.println("ADC initialized");
  if (SIMULATE_PRESSURE || SIMULATE_CAPACITANCE) {
    Serial.println("SIMULATION MODE ENABLED");
    if (SIMULATE_PRESSURE) Serial.println("  - Pressure: SIMULATED");
    if (SIMULATE_CAPACITANCE) Serial.println("  - Capacitance: SIMULATED");
  } else {
    Serial.println("Real sensor mode");
  }
  Serial.flush();
  vTaskDelay(100 / portTICK_PERIOD_MS);
  
  Serial.println("Setting up WiFi...");
  Serial.flush();
  setupWiFi();
  Serial.println("WiFi setup complete");
  Serial.flush();
  vTaskDelay(500 / portTICK_PERIOD_MS);
  
  Serial.println("Setting up SD Card...");
  Serial.flush();
  setupSDCard();
  Serial.println("SD Card setup complete");
  Serial.flush();
  vTaskDelay(500 / portTICK_PERIOD_MS);
  
  Serial.println("Initializing mutexes...");
  Serial.flush();
  // Initialize mutexes BEFORE webserver setup to prevent null pointer crashes
  if (pressureMutex == NULL) {
    pressureMutex = xSemaphoreCreateMutex();
  }
  if (capacitanceMutex == NULL) {
    capacitanceMutex = xSemaphoreCreateMutex();
  }
  Serial.println("Mutexes initialized");
  Serial.flush();
  
  Serial.println("Setting up Web Server...");
  Serial.flush();
  setupWebServer();
  Serial.println("Web Server setup complete");
  Serial.flush();
  vTaskDelay(500 / portTICK_PERIOD_MS);
  
  Serial.println("Creating sensor tasks...");
  Serial.flush();
  
  // Create high-priority pressure sensor task
  xTaskCreatePinnedToCore(
    pressureSensorTask,
    "PressureSensorTask",
    4096,
    NULL,
    PRESSURE_SENSOR_PRIORITY,
    NULL,
    0
  );
  Serial.println("Pressure sensor task created");
  Serial.flush();
  
  // Create high-priority capacitance sensor task (always create; the task
  // itself respects SIMULATE_CAPACITANCE to simulate or measure real data)
  xTaskCreatePinnedToCore(
    capacitanceSensorTask,
    "CapacitanceSensorTask",
    4096,
    NULL,
    CAPACITANCE_SENSOR_PRIORITY,
    NULL,
    0
  );
  Serial.println("Capacitance sensor task created");
  Serial.flush();
  
  // Create medium-priority data processor task
  xTaskCreatePinnedToCore(
    dataProcessorTask,
    "DataProcessorTask",
    4096,
    NULL,
    DATA_PROCESSOR_PRIORITY,
    NULL,
    0
  );
  Serial.println("Data processor task created");
  Serial.flush();
  
  // Start in paused state - user must press Start Collection button
  stopCollection();
  
  Serial.println("Setup complete!");
  Serial.printf("Connect to WiFi: %s\n", SSID);
  Serial.printf("Password: %s\n", PASSWORD);
  Serial.println("Open browser: http://192.168.4.1");
}

// ============ MAIN LOOP ============
void loop() {
  // Update LED color based on collection and grasping state
  if (isGrasping) {
    // Green = Grasping detected (highest priority)
    ledStatusGrasping();
  } else if (getCollectionState()) {
    // Purple = WiFi active + collecting (but not grasping)
    ledStatusPurple();
  } else {
    // Blue = WiFi active but not collecting
    ledStatusBlue();
  }
  
  vTaskDelay(1000 / portTICK_PERIOD_MS);
}

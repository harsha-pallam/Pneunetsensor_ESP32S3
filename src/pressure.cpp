#include "config.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <Arduino.h>
#include <Preferences.h>

// ============ GLOBAL VARIABLES ============
float lastPressure = 0.0;
unsigned long lastPressureTime = 0;
SemaphoreHandle_t pressureMutex = NULL;
bool isCollecting = false;
bool hasCollectionStarted = false;
unsigned long collectionStartTime = 0;
// Pressure offset used for manual zeroing (persisted)
float pressureOffset = 0.0f;
Preferences prefsPress;

// ============ PRESSURE SENSOR READING ============
float readPressure() {
  if (SIMULATE_PRESSURE) {
    return simulatePressure();
  } else {
    return readRealPressure();
  }
}

float readRealPressure() {
  // Read ADC (averaged over 10 samples for stability)
  int rawSum = 0;
  for (int i = 0; i < 10; i++) {
    rawSum += analogRead(PRESSURE_PIN);
  }
  int adcValue = rawSum / 10;
  
  // Convert ADC value to voltage (12-bit ADC: 0-4095 maps to 0-3.3V)
  float Vout_adc = adcValue * (3.3 / 4095.0);
  
  // Correct for voltage divider to recover original 5V sensor output
  // Vsensor = Vout_adc / PRESSURE_DIVIDER_RATIO
  float Vsensor = Vout_adc / PRESSURE_DIVIDER_RATIO;
  
  // Clamp to physical limits (sensor output is 0-5V)
  if (Vsensor > 5.0) Vsensor = 5.0;
  
  // MPX5010 datasheet formula: P(kPa) = (Vout/Vs - 0.04) / 0.09
  // where Vs is the actual sensor supply voltage (nominally 5V)
  float pressure_kPa = (Vsensor / 5.0 - 0.04) / 0.09;

  // Apply pressure offset
  pressure_kPa = pressure_kPa - pressureOffset;

  // Allow small negative values for sensitivity to pressure changes
  // They will be handled by the deformation detection logic if needed
  // Only hard-clamp to -10 kPa to prevent extreme sensor noise artifacts
  if (pressure_kPa < -10.0f) pressure_kPa = -10.0f;

  return pressure_kPa;
}

float simulatePressure() {
  static unsigned long startTime = millis();
  unsigned long elapsedTime = millis() - startTime;
  
  // Create oscillating pattern with noise
  float time_s = elapsedTime / 1000.0;
  float oscillation = SIM_AMPLITUDE * sin(2 * PI * SIM_FREQUENCY * time_s);
  float noise = (random(-100, 100) / 100.0) * SIM_NOISE;
  float pressure = SIM_BASE_PRESSURE + oscillation + noise;
  
  // Clamp to realistic values (0-10 kPa for demonstration)
  if (pressure < 0) pressure = 0;
  if (pressure > 10) pressure = 10;
  
  return pressure;
}

// ============ SENSOR READING TASK ============
void pressureSensorTask(void *parameter) {
  Serial.println("Pressure sensor task started");
  // Load persisted pressure offset if present
  loadPressureCalibration();
  
  while (true) {
    // Only collect data if collection is enabled
    if (isCollecting) {
      float pressure = readPressure();
      unsigned long currentTime = millis();
      
      // Thread-safe storage of latest measurement (readPressure already applies offset)
      if (xSemaphoreTake(pressureMutex, portMAX_DELAY)) {
        lastPressure = pressure;
        lastPressureTime = currentTime;
        xSemaphoreGive(pressureMutex);
      }
      
      DEBUG_PRINT("Pressure: %.2f kPa, Time: %lu ms\n", 
                    pressure, currentTime);
    }
    
    vTaskDelay(PRESSURE_SAMPLING_DELAY / portTICK_PERIOD_MS);
  }
}

// ============ COLLECTION CONTROL ============
void startCollection() {
  collectionStartTime = millis(); // Record current time as start of collection
  isCollecting = true;
  
  // Only create new SD card file if this is the first start of this collection run
  // If resuming from pause, keep using the same file
  #ifdef USE_SD_CARD
    if (!hasCollectionStarted) {
      createNewDataFile();
      hasCollectionStarted = true;
    }
  #endif
  
  Serial.println("Data collection started");
}

void stopCollection() {
  isCollecting = false;
  Serial.println("Data collection stopped");
}

void resetCollection() {
  isCollecting = false;
  hasCollectionStarted = false; // Reset flag so next collection creates new file
  
  // Reset sensor data
  if (xSemaphoreTake(pressureMutex, portMAX_DELAY)) {
    lastPressure = 0.0;
    lastPressureTime = 0;
    xSemaphoreGive(pressureMutex);
  }
  
  if (xSemaphoreTake(capacitanceMutex, portMAX_DELAY)) {
    lastCapacitance = 0.0;
    lastCapacitanceTime = 0;
    xSemaphoreGive(capacitanceMutex);
  }
  
  // Clear filename so next collection will use timestamp-based name
  #ifdef USE_SD_CARD
    setSDCardFilename("");  // Reset to timestamp-based filename
  #endif
  
  Serial.println("Data collection reset");
}

bool getCollectionState() {
  return isCollecting;
}

// ============ PRESSURE CALIBRATION / OFFSET ============
void loadPressureCalibration() {
  prefsPress.begin("press_cal", true);
  if (prefsPress.isKey("offset")) {
    pressureOffset = prefsPress.getFloat("offset", 0.0f);
    Serial.printf("Pressure offset loaded: %.4f kPa\n", pressureOffset);
  } else {
    Serial.println("No pressure offset stored (using 0.0)");
    pressureOffset = 0.0f;
  }
  prefsPress.end();
}

void savePressureCalibration() {
  prefsPress.begin("press_cal", false);
  prefsPress.putFloat("offset", pressureOffset);
  prefsPress.end();
  Serial.printf("Pressure offset saved: %.4f kPa\n", pressureOffset);
}

// Set the pressure offset to current raw reading so displayed pressure becomes zero
void resetPressureOffset() {
  // Read raw ADC and convert to pressure WITHOUT applying current offset
  int rawSum = 0;
  for (int i = 0; i < 10; i++) {
    rawSum += analogRead(PRESSURE_PIN);
  }
  int adcValue = rawSum / 10;
  
  // Convert to voltage
  float Vout_adc = adcValue * (3.3 / 4095.0);
  float Vsensor = Vout_adc / PRESSURE_DIVIDER_RATIO;
  
  // Clamp to physical limits
  if (Vsensor > 5.0) Vsensor = 5.0;
  
  // Calculate raw pressure WITHOUT offset
  float raw_pressure_kPa = (Vsensor / 5.0 - 0.04) / 0.09;
  
  // Set offset so that raw_pressure - offset = 0
  pressureOffset = raw_pressure_kPa;
  
  // Update displayed pressure to zero
  if (xSemaphoreTake(pressureMutex, 10 / portTICK_PERIOD_MS)) {
    lastPressure = 0.0f;
    xSemaphoreGive(pressureMutex);
  }

  savePressureCalibration();
  Serial.printf("Pressure offset reset to %.4f kPa (raw: %.4f kPa, now displays: 0.00 kPa)\n", 
                pressureOffset, raw_pressure_kPa);
}

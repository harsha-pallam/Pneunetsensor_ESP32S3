#include "config.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <Arduino.h>
#include <Preferences.h>

// ============ GLOBAL VARIABLES ============
float lastCapacitance = 0.0;
unsigned long lastCapacitanceTime = 0;
SemaphoreHandle_t capacitanceMutex = NULL;

// ============ CALIBRATION VARIABLES ============
// Preferences for capacitance measurement calibration only
Preferences prefsCapacitance;
long raw_baseline = 0;
long raw_82 = 0;
long raw_101 = 0;
float calib_a = 0.0f;
float calib_b = 0.0f;
float cap_c0 = 0.0f; // undeformed capacitance (pF)
volatile bool calibrationInProgress = false;
volatile int calibrationStep = 0;

// PneuNet deformation calibration curve (linear fit: P = a*ΔC + b)
// These variables are loaded from preferences
int calib_curve_points = 0;
float calib_curve_delta_c_a = 0.0f;  // Linear coefficient
float calib_curve_delta_c_b = 0.0f;  // Constant offset
float calib_curve_delta_c_c = 0.0f;  // Reserved (not used in linear fit)

// PneuNet deformation calibration curve (sqrt fit: P = a*sqrt(x-b) + c)
int calib_curve_sqrt_points = 0;
float calib_curve_sqrt_a = 0.0f;     // Sqrt coefficient
float calib_curve_sqrt_b = 0.0f;     // Sqrt offset
float calib_curve_sqrt_c = 0.0f;     // Sqrt vertical offset

// Fit method selection: 0 = LINEAR, 1 = SQRT
int pneunet_fit_method = 0;  // Currently selected fit method

// ============ HELPER FUNCTIONS ============
long readTouchAvg(int pin, int samples, int delayMs = SAMPLE_DELAY_MS) {
  long sum = 0;
  for (int i = 0; i < samples; ++i) {
    sum += touchRead(pin);
    delay(delayMs);
  }
  return sum / samples;
}

void loadCapacitanceCalibration() {
  prefsCapacitance.begin("cap_cal", true);
  if (prefsCapacitance.isKey("a") && prefsCapacitance.isKey("b")) {
    calib_a = prefsCapacitance.getFloat("a", 0.0f);
    calib_b = prefsCapacitance.getFloat("b", 0.0f);
    raw_baseline = prefsCapacitance.getLong("raw0", 0);
    raw_82 = prefsCapacitance.getLong("raw82", 0);
    raw_101 = prefsCapacitance.getLong("raw101", 0);
    // load stored C0 if present
    if (prefsCapacitance.isKey("c0")) {
      cap_c0 = prefsCapacitance.getFloat("c0", 0.0f);
    } else {
      cap_c0 = 0.0f;
    }
    Serial.println("Capacitance calibration loaded from preferences");
    Serial.printf("  raw_baseline: %ld, raw_82: %ld, raw_101: %ld\n", raw_baseline, raw_82, raw_101);
    Serial.printf("  calib_a: %.8f, calib_b: %.8f\n", calib_a, calib_b);
  } else {
    Serial.println("No capacitance calibration found in preferences. Run calibration via web interface.");
  }
  prefsCapacitance.end();
}

void saveCapacitanceCalibration() {
  prefsCapacitance.begin("cap_cal", false);
  prefsCapacitance.putLong("raw0", raw_baseline);
  prefsCapacitance.putLong("raw82", raw_82);
  prefsCapacitance.putLong("raw101", raw_101);
  prefsCapacitance.putFloat("a", calib_a);
  prefsCapacitance.putFloat("b", calib_b);
  prefsCapacitance.putFloat("c0", cap_c0);
  prefsCapacitance.end();
  Serial.println("Capacitance calibration saved to preferences");
}

// Compute calibration using two known reliable points (82pF and 101pF)
// Linear fit: cap_pF = a * raw + b
// Uses proportionality assumption and two-point calibration
bool computeAndStoreCalibration() {
  if (raw_101 == raw_82) {
    Serial.println("ERROR: raw_101 and raw_82 are equal, cannot compute calibration");
    return false;
  }
  
  // Calculate slope from two known points
  // slope = (y2 - y1) / (x2 - x1) = (101 - 82) / (raw_101 - raw_82)
  calib_a = (101.0f - 82.0f) / (float)(raw_101 - raw_82);
  
  // Calculate intercept using first point: b = y - a*x
  // b = 82 - a * raw_82
  calib_b = 82.0f - calib_a * (float)raw_82;
  
  Serial.printf("Calibration computed: a=%.8f, b=%.8f\n", calib_a, calib_b);
  Serial.printf("Baseline capacitance (hardware): %.2f pF\n", calib_a * (float)raw_baseline + calib_b);
  
  saveCapacitanceCalibration();
  return true;
}

void clearCalibration() {
  prefsCapacitance.begin("cap_cal", false);
  prefsCapacitance.clear();
  prefsCapacitance.end();
  calib_a = 0.0f;
  calib_b = 0.0f;
  raw_baseline = 0;
  raw_82 = 0;
  raw_101 = 0;
  cap_c0 = 0.0f;
  
  Serial.println("Capacitance calibration cleared");
}

// Store current capacitance as undeformed C0 (average of samples)
bool storeCapacitanceC0(int samples) {
  if (samples <= 0) samples = CALIB_SAMPLES;
  double sum = 0.0;
  for (int i = 0; i < samples; ++i) {
    double c = measureCapacitance();
    sum += c;
    delay(SAMPLE_DELAY_MS);
  }
  cap_c0 = (float)(sum / (double)samples);
  saveCapacitanceCalibration();
  Serial.printf("Stored C0 = %.3f pF (avg of %d samples)\n", cap_c0, samples);
  return true;
}

// ============ CALIBRATION STEP FUNCTIONS ============
void startCalibrationStep(int step) {
  calibrationStep = step;
  if (step == 0) {
    // Step 0: Baseline (no cap)
    raw_baseline = readTouchAvg(CAPACITANCE_PIN, CALIB_SAMPLES);
    Serial.printf("Calibration Step 0 - Baseline: raw = %ld\n", raw_baseline);
  } else if (step == 1) {
    // Step 1: 82pF reference
    raw_82 = readTouchAvg(CAPACITANCE_PIN, CALIB_SAMPLES);
    Serial.printf("Calibration Step 1 - 82pF: raw = %ld\n", raw_82);
  } else if (step == 2) {
    // Step 2: 101pF reference
    raw_101 = readTouchAvg(CAPACITANCE_PIN, CALIB_SAMPLES);
    Serial.printf("Calibration Step 2 - 101pF: raw = %ld\n", raw_101);
    
    // Verify readings are increasing
    if (raw_82 <= raw_baseline || raw_101 <= raw_82) {
      Serial.println("ERROR: measured values not increasing as expected");
      calibrationInProgress = false;
      return;
    }
    
    if (!computeAndStoreCalibration()) {
      Serial.println("ERROR: calibration computation failed");
      calibrationInProgress = false;
      return;
    }
    
    Serial.println("Calibration completed successfully");
    calibrationInProgress = false;
  }
}

// ============ CAPACITANCE MEASUREMENT ============
float measureCapacitance() {
  if (SIMULATE_CAPACITANCE) {
    return simulateCapacitance();
  } else {
    return measureCapacitanceReal();
  }
}

float measureCapacitanceReal() {
  // Quick touch measurement
  long raw = readTouchAvg(CAPACITANCE_PIN, QUICK_SAMPLES);
  
  // Apply calibration: cap_pF = a * raw + b
  float cap = calib_a * (float)raw + calib_b;
  
  // Clamp to 0 if negative (physical constraint)
  if (cap < 0.0f) {
    cap = 0.0f;
  }
  
  // Debug: show raw → pF conversion
  DEBUG_PRINT("Capacitance: raw=%ld -> %.2f pF\n", raw, cap);
  
  return cap;
}

float simulateCapacitance() {
  // Simulate a capacitance value around 100 picoFarads with small variations
  static unsigned long lastSimTime = 0;
  unsigned long currentTime = millis();
  
  // Update simulation every 500ms to show some variation
  if (currentTime - lastSimTime > 500) {
    lastSimTime = currentTime;
    // Simulate value between 95-105 pF with some noise
    float noise = (random(-10, 10) / 100.0);
    return 100.0 + noise;
  }
  
  return lastCapacitance;
}

// ============ CAPACITANCE SENSOR TASK ============
void capacitanceSensorTask(void *parameter) {
  Serial.println("Capacitance sensor task started");
  
  // Load capacitance measurement calibration on startup
  loadCapacitanceCalibration();
  
  // Stuck detection variables
  float lastReading = -1.0f;
  int stuckCounter = 0;
  
  while (true) {
    // Only collect data when collection is enabled (controlled by webpage)
    // This matches the pattern used by pressureSensorTask
    if (isCollecting && !calibrationInProgress) {
      float capacitance = measureCapacitance();
      unsigned long currentTime = millis();
      
      // ============ STUCK SENSOR DETECTION & RECOVERY ============
      if (ENABLE_CAPACITANCE_STUCK_DETECTION) {
        // Check if reading is identical (or nearly identical) to last reading
        if (lastReading >= 0.0f && fabs(capacitance - lastReading) <= CAPACITANCE_STUCK_TOLERANCE) {
          stuckCounter++;
          
          // If stuck threshold reached, attempt recovery
          if (stuckCounter >= CAPACITANCE_STUCK_THRESHOLD) {
            Serial.printf("WARNING: Capacitance sensor appears stuck (value=%.2f pF for %d consecutive reads). Attempting aggressive recovery...\n", 
                          capacitance, CAPACITANCE_STUCK_THRESHOLD);
            
            // Attempt recovery: completely reset pin mode
            // Step 1: Change pin to digital input to disconnect from touch sensor
            pinMode(CAPACITANCE_PIN, INPUT);
            delay(50);
            
            // Step 2: Set pin to digital output and toggle it
            pinMode(CAPACITANCE_PIN, OUTPUT);
            digitalWrite(CAPACITANCE_PIN, LOW);
            delay(50);
            digitalWrite(CAPACITANCE_PIN, HIGH);
            delay(50);
            digitalWrite(CAPACITANCE_PIN, LOW);
            delay(50);
            
            // Step 3: Return pin to floating state (input with no pull)
            pinMode(CAPACITANCE_PIN, INPUT);
            delay(100);
            
            // Step 4: Reinitialize touch sensor on the pin
            touchSetCycles(16, 4);  // Reset to default calibration cycles
            delay(100);  // Wait for recovery
            
            // Reset stuck counter and readings
            stuckCounter = 0;
            lastReading = -1.0f;
            
            Serial.println("Capacitance sensor pin recovered. Resuming measurements.");
          }
        } else {
          // Reading changed, reset stuck counter
          stuckCounter = 0;
        }
        
        lastReading = capacitance;
      }
      
      // Thread-safe storage
      if (xSemaphoreTake(capacitanceMutex, portMAX_DELAY)) {
        lastCapacitance = capacitance;
        lastCapacitanceTime = currentTime;
        xSemaphoreGive(capacitanceMutex);
      }
    }
    
    vTaskDelay(CAPACITANCE_SAMPLING_DELAY / portTICK_PERIOD_MS);
  }
}

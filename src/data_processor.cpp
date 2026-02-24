#include "config.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <Arduino.h>
#include <Preferences.h>

#ifdef USE_SD_CARD
  #include <SD.h>
#endif

// ============ GRASPING DETECTION GLOBAL STATE ============
bool isGrasping = false;

// ============ PNEUNET DEFORMATION PARAMETERS ============
// Calculated deformation parameters
float pneunet_alpha = 0.0f;  // Deformation angle (radians)
float pneunet_r = 0.0f;      // Radius of curvature (mm)
float pneunet_d = 0.0f;      // Object diameter / grasp width (mm)

// ============ PNEUNET DEFORMATION CALIBRATION VARIABLES ============
// Raw sample storage during calibration (for curve fitting)
CalibRawSample calib_raw_samples[MAX_CALIB_SAMPLES_PER_POINT];
int calib_raw_sample_count = 0;
unsigned long calib_last_pressure_time = 0;  // For stasis detection
volatile bool pneunetDeformationCalibrationInProgress = false;

// NOTE: All calibration curve variables are defined in capacitance.cpp and declared as extern here
extern int pneunet_fit_method;
extern int calib_curve_points;
extern float calib_curve_delta_c_a;
extern float calib_curve_delta_c_b;
extern float calib_curve_delta_c_c;
extern int calib_curve_sqrt_points;
extern float calib_curve_sqrt_a;
extern float calib_curve_sqrt_b;
extern float calib_curve_sqrt_c;

// Preferences for saving/loading PneuNet deformation calibration
Preferences prefsPneunet;

// ============ GRASPING DETECTION HELPER ============
// Old detection (kept for reference, now using curve-based detection)
bool detectGrasping(float currentPressure, float prevPressure,
                     float currentCapacitance, float prevCapacitance) {
  // Check pressure increase and capacitance stability between successive samples
  float pressureDelta = currentPressure - prevPressure;
  bool pressureIncreasing = pressureDelta > PRESSURE_INCREASE_THRESHOLD;

  float capacitanceDelta = fabs(currentCapacitance - prevCapacitance);
  bool capacitanceStable = capacitanceDelta < CAPACITANCE_TOLERANCE;

  return (pressureIncreasing && capacitanceStable);
}

// ============ PNEUNET DEFORMATION CALCULATION ============
void calculatePneunetDeformation(float capacitance) {
  // Only calculate if we have a valid C0 value
  if (cap_c0 <= 0.0f) {
    pneunet_alpha = 0.0f;
    pneunet_r = 0.0f;
    pneunet_d = 0.0f;
    return;
  }
  
  // Calculate delta-C from C0
  float delta_c = capacitance - cap_c0;
  
  // Avoid division by zero and negative values
  if (delta_c <= 0.01f) {
    pneunet_alpha = 0.0f;
    pneunet_r = 0.0f;
    pneunet_d = 0.0f;
    return;
  }
  
  // PneuNet_alpha = (C-C0)*L/(H*C0) [radians]
  pneunet_alpha = (delta_c * PNEUNET_L) / (PNEUNET_H * cap_c0);
  
  // Clamp alpha to valid range: 0 < alpha <= 2*pi
  if (pneunet_alpha > TWO_PI) {
    pneunet_alpha = TWO_PI;
  }
  if (pneunet_alpha <= 0.0f) {
    pneunet_alpha = 0.01f;  // Minimum small value to avoid division issues
  }
  
  // PneuNet_r = H*C0/(C-C0) [mm]
  pneunet_r = (PNEUNET_H * cap_c0) / delta_c;
  
  // PneuNet_d = 2*r*sin(alpha/2) [mm] - chord length / object width
  pneunet_d = 2.0f * pneunet_r * sinf(pneunet_alpha / 2.0f);
}

// ============ PNEUNET DEFORMATION CALIBRATION LOAD/SAVE ============
void loadPneunetDeformationCalibration() {
  prefsPneunet.begin("pneunet_cal", true);
  
  // Load fit method selection
  pneunet_fit_method = prefsPneunet.getInt("fit_method", 0);
  
  // Load PneuNet deformation calibration curve (linear fit: P = a*ΔC + b)
  if (prefsPneunet.isKey("curve_points")) {
    calib_curve_points = prefsPneunet.getInt("curve_points", 0);
    calib_curve_delta_c_a = prefsPneunet.getFloat("curve_a", 0.0f);
    calib_curve_delta_c_b = prefsPneunet.getFloat("curve_b", 0.0f);
    calib_curve_delta_c_c = prefsPneunet.getFloat("curve_c", 0.0f);
    Serial.printf("PneuNet deformation curve loaded (LINEAR): P = %.6f*ΔC + %.4f\n", 
                  calib_curve_delta_c_a, calib_curve_delta_c_b);
  }
  
  // Load square root fit coefficients
  // Try new key first, then fall back to old key for backward compatibility
  if (prefsPneunet.isKey("sqrt_points")) {
    calib_curve_sqrt_points = prefsPneunet.getInt("sqrt_points", 0);
    calib_curve_sqrt_a = prefsPneunet.getFloat("curve_sqrt_a", 0.0f);
    calib_curve_sqrt_b = prefsPneunet.getFloat("curve_sqrt_b", 0.0f);
    calib_curve_sqrt_c = prefsPneunet.getFloat("curve_sqrt_c", 0.0f);
    Serial.printf("PneuNet deformation curve loaded (SQRT): P = %.6f*sqrt(x-%.4f) + %.4f\n", 
                  calib_curve_sqrt_a, calib_curve_sqrt_b, calib_curve_sqrt_c);
  } else if (prefsPneunet.isKey("curve_sqrt_points")) {
    // Backward compatibility: load from old key name
    calib_curve_sqrt_points = prefsPneunet.getInt("curve_sqrt_points", 0);
    calib_curve_sqrt_a = prefsPneunet.getFloat("curve_sqrt_a", 0.0f);
    calib_curve_sqrt_b = prefsPneunet.getFloat("curve_sqrt_b", 0.0f);
    calib_curve_sqrt_c = prefsPneunet.getFloat("curve_sqrt_c", 0.0f);
    Serial.printf("PneuNet deformation curve loaded (SQRT - old key): P = %.6f*sqrt(x-%.4f) + %.4f\n", 
                  calib_curve_sqrt_a, calib_curve_sqrt_b, calib_curve_sqrt_c);
  }
  
  if (calib_curve_points == 0 && calib_curve_sqrt_points == 0) {
    Serial.println("No PneuNet deformation calibration curve found. Run calibration via web interface.");
  }
  
  prefsPneunet.end();
}

void savePneunetDeformationCalibration() {
  // Clear old data first to avoid NVS space issues
  prefsPneunet.begin("pneunet_cal", false);
  prefsPneunet.clear();
  prefsPneunet.end();
  
  // Now save the new calibration data
  prefsPneunet.begin("pneunet_cal", false);
  
  // Save fit method selection
  prefsPneunet.putInt("fit_method", pneunet_fit_method);
  
  // Save PneuNet deformation calibration curve (linear fit)
  prefsPneunet.putInt("curve_points", calib_curve_points);
  prefsPneunet.putFloat("curve_a", calib_curve_delta_c_a);
  prefsPneunet.putFloat("curve_b", calib_curve_delta_c_b);
  prefsPneunet.putFloat("curve_c", calib_curve_delta_c_c);
  
  // Save square root fit coefficients
  prefsPneunet.putInt("sqrt_points", calib_curve_sqrt_points);
  prefsPneunet.putFloat("curve_sqrt_a", calib_curve_sqrt_a);
  prefsPneunet.putFloat("curve_sqrt_b", calib_curve_sqrt_b);
  prefsPneunet.putFloat("curve_sqrt_c", calib_curve_sqrt_c);
  
  prefsPneunet.end();
  
  if (pneunet_fit_method == 0) {
    Serial.printf("PneuNet deformation calibration saved (LINEAR): curve_points=%d, a=%.6f, b=%.4f\n",
                  calib_curve_points, calib_curve_delta_c_a, calib_curve_delta_c_b);
  } else {
    Serial.printf("PneuNet deformation calibration saved (SQRT): curve_points=%d, a=%.6f, b=%.4f, c=%.4f\n",
                  calib_curve_sqrt_points, calib_curve_sqrt_a, calib_curve_sqrt_b, calib_curve_sqrt_c);
  }
}

void clearPneunetDeformationCalibration() {
  prefsPneunet.begin("pneunet_cal", false);
  prefsPneunet.clear();
  prefsPneunet.end();
  
  calib_curve_points = 0;
  calib_curve_delta_c_a = 0.0f;
  calib_curve_delta_c_b = 0.0f;
  calib_curve_delta_c_c = 0.0f;
  calib_curve_sqrt_points = 0;
  calib_curve_sqrt_a = 0.0f;
  calib_curve_sqrt_b = 0.0f;
  calib_curve_sqrt_c = 0.0f;
  pneunet_fit_method = 0;
  calib_raw_sample_count = 0;
  
  Serial.println("PneuNet deformation calibration cleared");
}

// ============ SQUARE ROOT FIT USING GAUSS-NEWTON ============
// Fit: P = a*sqrt(x-b) + c where x = ΔC
// Using Gauss-Newton non-linear least squares optimization
bool fitSquareRootCurve(const CalibRawSample* samples, int sample_count,
                         float& out_a, float& out_b, float& out_c) {
  if (sample_count < 3) {
    Serial.println("Not enough samples for square root fit");
    return false;
  }
  
  // Filter to only valid points (P >= MIN_CALIB_PRESSURE and ΔC > 0)
  const int MAX_VALID = 100;
  float dc_vals[MAX_VALID];
  float p_vals[MAX_VALID];
  int valid_count = 0;
  
  for (int i = 0; i < sample_count && valid_count < MAX_VALID; i++) {
    if (samples[i].pressure >= MIN_CALIB_PRESSURE && samples[i].delta_c > 0.0f) {
      dc_vals[valid_count] = samples[i].delta_c;
      p_vals[valid_count] = samples[i].pressure;
      valid_count++;
    }
  }
  
  if (valid_count < 3) {
    Serial.printf("Not enough valid samples (need >= 3, got %d)\n", valid_count);
    return false;
  }
  
  // Initial guess: a=1, b=1, c=0
  float a = 1.0f;
  float b = 1.0f;
  float c = 0.0f;
  
  // Gauss-Newton iteration
  const int MAX_ITER = 100;
  const float TOLERANCE = 1e-6f;
  const float DELTA = 1e-5f;  // For numerical derivative
  
  for (int iter = 0; iter < MAX_ITER; iter++) {
    // Compute residuals and Jacobian
    float sum_residual_sq = 0.0f;
    float jac_a[MAX_VALID], jac_b[MAX_VALID], jac_c[MAX_VALID];
    float residuals[MAX_VALID];
    
    for (int i = 0; i < valid_count; i++) {
      float x = dc_vals[i];
      float y_obs = p_vals[i];
      
      // Skip if invalid for sqrt (would need x > b)
      if (x <= b) {
        // Adjust b to be slightly less than minimum x
        b = x * 0.9f;
      }
      
      // Model prediction: y_pred = a*sqrt(x-b) + c
      float sqrt_term = sqrtf(x - b);
      float y_pred = a * sqrt_term + c;
      
      // Residual
      residuals[i] = y_obs - y_pred;
      sum_residual_sq += residuals[i] * residuals[i];
      
      // Jacobian (analytical derivatives)
      // dy/da = sqrt(x-b)
      jac_a[i] = sqrt_term;
      
      // dy/db = -a / (2*sqrt(x-b))
      if (sqrt_term > 1e-6f) {
        jac_b[i] = -a / (2.0f * sqrt_term);
      } else {
        jac_b[i] = 0.0f;
      }
      
      // dy/dc = 1
      jac_c[i] = 1.0f;
    }
    
    // Build normal equations: JT*J*delta = JT*residuals
    float jt_j[3][3] = {{0}};
    float jt_r[3] = {0};
    
    for (int i = 0; i < valid_count; i++) {
      jt_j[0][0] += jac_a[i] * jac_a[i];
      jt_j[0][1] += jac_a[i] * jac_b[i];
      jt_j[0][2] += jac_a[i] * jac_c[i];
      jt_j[1][1] += jac_b[i] * jac_b[i];
      jt_j[1][2] += jac_b[i] * jac_c[i];
      jt_j[2][2] += jac_c[i] * jac_c[i];
      
      jt_r[0] += jac_a[i] * residuals[i];
      jt_r[1] += jac_b[i] * residuals[i];
      jt_r[2] += jac_c[i] * residuals[i];
    }
    
    // Solve 3x3 system using Gaussian elimination with partial pivoting
    float matrix[3][4];
    for (int i = 0; i < 3; i++) {
      for (int j = 0; j < 3; j++) {
        matrix[i][j] = jt_j[i][j];
      }
      matrix[i][3] = jt_r[i];
    }
    
    // Forward elimination with partial pivoting
    for (int col = 0; col < 3; col++) {
      // Find pivot
      int pivot_row = col;
      float max_val = fabsf(matrix[col][col]);
      for (int row = col + 1; row < 3; row++) {
        if (fabsf(matrix[row][col]) > max_val) {
          max_val = fabsf(matrix[row][col]);
          pivot_row = row;
        }
      }
      
      // Swap rows
      if (pivot_row != col) {
        for (int j = col; j < 4; j++) {
          float temp = matrix[col][j];
          matrix[col][j] = matrix[pivot_row][j];
          matrix[pivot_row][j] = temp;
        }
      }
      
      // Check singularity
      if (fabsf(matrix[col][col]) < 1e-10f) {
        Serial.println("Singular matrix in Gauss-Newton fit");
        return false;
      }
      
      // Eliminate column
      for (int row = col + 1; row < 3; row++) {
        float factor = matrix[row][col] / matrix[col][col];
        for (int j = col; j < 4; j++) {
          matrix[row][j] -= factor * matrix[col][j];
        }
      }
    }
    
    // Back substitution
    float delta[3] = {0};
    for (int i = 2; i >= 0; i--) {
      delta[i] = matrix[i][3];
      for (int j = i + 1; j < 3; j++) {
        delta[i] -= matrix[i][j] * delta[j];
      }
      delta[i] /= matrix[i][i];
    }
    
    // Update parameters
    float new_a = a + delta[0];
    float new_b = b + delta[1];
    float new_c = c + delta[2];
    
    // Ensure a > 0 and b < min(dc_vals)
    if (new_a <= 0.0f) new_a = 0.1f;
    float min_dc = dc_vals[0];
    for (int i = 1; i < valid_count; i++) {
      if (dc_vals[i] < min_dc) min_dc = dc_vals[i];
    }
    if (new_b >= min_dc) new_b = min_dc * 0.9f;
    
    // Check convergence
    float delta_norm = sqrtf(delta[0]*delta[0] + delta[1]*delta[1] + delta[2]*delta[2]);
    
    a = new_a;
    b = new_b;
    c = new_c;
    
    if (delta_norm < TOLERANCE) {
      // Converged
      out_a = a;
      out_b = b;
      out_c = c;
      Serial.printf("Square root fit converged after %d iterations (delta_norm=%.2e)\n", iter + 1, delta_norm);
      return true;
    }
  }
  
  // Did not fully converge but return best estimate
  out_a = a;
  out_b = b;
  out_c = c;
  Serial.printf("Square root fit did not fully converge (used %d max iterations)\n", MAX_ITER);
  return true;
}

// ============ PNEUNET DEFORMATION CALIBRATION FUNCTIONS ============
// Start calibration: begin recording pressure samples as capacity increases
bool startPneunetDeformationCalibration() {
  if (pneunetDeformationCalibrationInProgress) return false;
  
  calib_raw_sample_count = 0;
  calib_last_pressure_time = millis();
  memset(calib_raw_samples, 0, sizeof(calib_raw_samples));
  
  pneunetDeformationCalibrationInProgress = true;
  Serial.println("PneuNet deformation calibration started");
  return true;
}

// Update: called periodically to record pressure at current capacitance
// Groups small samples to reduce outliers and create averaged data points
void updatePneunetDeformationCalibration() {
  if (!pneunetDeformationCalibrationInProgress) return;
  if (calib_raw_sample_count >= MAX_CALIB_SAMPLES_PER_POINT) return;
  
  // Group size: accumulate ~5 raw measurements before averaging
  const int GROUP_SIZE = 5;
  static int group_counter = 0;
  static float group_dc_sum = 0.0f;
  static float group_p_sum = 0.0f;
  
  // Get current pressure and capacitance
  float current_pressure = 0.0f;
  if (xSemaphoreTake(pressureMutex, 10 / portTICK_PERIOD_MS)) {
    current_pressure = lastPressure;
    xSemaphoreGive(pressureMutex);
  }
  
  float current_capacitance = 0.0f;
  if (xSemaphoreTake(capacitanceMutex, 10 / portTICK_PERIOD_MS)) {
    current_capacitance = lastCapacitance;
    xSemaphoreGive(capacitanceMutex);
  }
  
  // Calculate delta-C from C0
  if (cap_c0 > 0.0f) {
    float delta_c = current_capacitance - cap_c0;
    
    if (delta_c >= 0.0f) {
      // Accumulate into group
      group_dc_sum += delta_c;
      group_p_sum += current_pressure;
      group_counter++;
      
      // When group is complete, store averaged values
      if (group_counter >= GROUP_SIZE) {
        float avg_dc = group_dc_sum / GROUP_SIZE;
        float avg_p = group_p_sum / GROUP_SIZE;
        
        // Round delta-C to 0.1pF for clean grouping
        avg_dc = roundf(avg_dc * 10.0f) / 10.0f;
        
        // Store the averaged sample
        calib_raw_samples[calib_raw_sample_count].delta_c = avg_dc;
        calib_raw_samples[calib_raw_sample_count].pressure = avg_p;
        calib_raw_sample_count++;
        
        // Track time for stasis detection
        if (avg_p > 0.01f) {
          calib_last_pressure_time = millis();
        }
        
        DEBUG_PRINT("Calib grouped sample %d: ΔC=%.1f pF, P=%.3f kPa (avg of %d)\n", 
                      calib_raw_sample_count, avg_dc, avg_p, GROUP_SIZE);
        
        // Reset group
        group_counter = 0;
        group_dc_sum = 0.0f;
        group_p_sum = 0.0f;
      }
    }
  }
}

// Finish calibration: perform linear fit (P = a*ΔC + b) and save
bool finishPneunetDeformationCalibration() {
  if (!pneunetDeformationCalibrationInProgress) return false;
  
  pneunetDeformationCalibrationInProgress = false;
  
  if (calib_raw_sample_count < 3) {
    Serial.println("Not enough calibration samples (need at least 3)");
    return false;
  }
  
  // Linear regression: P = a*ΔC + b
  // Filter to only use valid points where pressure >= MIN_CALIB_PRESSURE
  float sum_dc = 0.0f, sum_dc2 = 0.0f, sum_p = 0.0f, sum_p_dc = 0.0f;
  int valid_count = 0;
  
  for (int i = 0; i < calib_raw_sample_count; i++) {
    float dc = calib_raw_samples[i].delta_c;
    float p = calib_raw_samples[i].pressure;
    
    // Only use valid points with adequate pressure (>= MIN_CALIB_PRESSURE)
    // AND delta_c must be > 0
    if (p >= MIN_CALIB_PRESSURE && dc > 0.0f) {
      sum_dc += dc;
      sum_dc2 += dc * dc;
      sum_p += p;
      sum_p_dc += p * dc;
      valid_count++;
    }
  }
  
  if (valid_count < 3) {
    Serial.printf("Not enough valid samples (P >= %.2f kPa and ΔC > 0) for fitting\n", MIN_CALIB_PRESSURE);
    return false;
  }
  
  // Fit LINEAR: a = (n*sum_p_dc - sum_dc*sum_p) / (n*sum_dc2 - sum_dc^2)
  float n = valid_count;
  float denom = n * sum_dc2 - sum_dc * sum_dc;
  
  if (fabsf(denom) < 1e-6f) {
    Serial.println("Cannot fit linear curve (singular matrix)");
    return false;
  }
  
  calib_curve_delta_c_a = (n * sum_p_dc - sum_dc * sum_p) / denom;
  calib_curve_delta_c_b = (sum_p - calib_curve_delta_c_a * sum_dc) / n;
  calib_curve_delta_c_c = 0.0f;
  calib_curve_points = calib_raw_sample_count;
  
  Serial.printf("PneuNet deformation LINEAR fit computed: P = %.6f*ΔC + %.4f (from %d samples, %d valid with P >= %.2f kPa)\n",
                calib_curve_delta_c_a, calib_curve_delta_c_b, 
                calib_raw_sample_count, valid_count, MIN_CALIB_PRESSURE);
  
  // Fit SQRT: P = a*sqrt(x-b) + c
  bool sqrt_fit_ok = fitSquareRootCurve(calib_raw_samples, calib_raw_sample_count,
                                         calib_curve_sqrt_a, calib_curve_sqrt_b, calib_curve_sqrt_c);
  
  if (sqrt_fit_ok) {
    calib_curve_sqrt_points = calib_raw_sample_count;
    Serial.printf("PneuNet deformation SQRT fit computed: P = %.6f*sqrt(x-%.4f) + %.4f\n",
                  calib_curve_sqrt_a, calib_curve_sqrt_b, calib_curve_sqrt_c);
  } else {
    Serial.println("Failed to compute square root fit, only linear fit available");
    calib_curve_sqrt_points = 0;
  }
  
  // Save both fits to preferences but don't apply yet (web interface will choose)
  savePneunetDeformationCalibration();
  
  return true;
}

// Apply a specific fit method: method = 0 (LINEAR) or 1 (SQRT)
bool applyPneunetFitMethod(int method) {
  if (method == 0) {
    // Apply LINEAR fit
    if (calib_curve_points == 0) {
      Serial.println("Error: LINEAR fit not available");
      return false;
    }
    pneunet_fit_method = 0;
    Serial.printf("Applied LINEAR fit: P = %.6f*ΔC + %.4f\n", 
                  calib_curve_delta_c_a, calib_curve_delta_c_b);
  } else if (method == 1) {
    // Apply SQRT fit
    if (calib_curve_sqrt_points == 0) {
      Serial.println("Error: SQRT fit not available");
      return false;
    }
    pneunet_fit_method = 1;
    Serial.printf("Applied SQRT fit: P = %.6f*sqrt(x-%.4f) + %.4f\n", 
                  calib_curve_sqrt_a, calib_curve_sqrt_b, calib_curve_sqrt_c);
  } else {
    Serial.printf("Error: unknown fit method %d\n", method);
    return false;
  }
  
  savePneunetDeformationCalibration();
  return true;
}

// Abort calibration without saving
void abortPneunetDeformationCalibration() {
  if (!pneunetDeformationCalibrationInProgress) return;
  
  pneunetDeformationCalibrationInProgress = false;
  calib_raw_sample_count = 0;
  
  Serial.println("PneuNet deformation calibration aborted (data discarded)");
}

// Get expected pressure for a given delta-C (using currently selected fit)
float getExpectedPressure(float delta_c) {
  // Clamp delta_c to avoid issues
  if (delta_c < 0.0f) delta_c = 0.0f;
  
  if (pneunet_fit_method == 0) {
    // LINEAR: P = a*ΔC + b
    if (calib_curve_points == 0) return 0.0f;
    return calib_curve_delta_c_a * delta_c + calib_curve_delta_c_b;
  } else {
    // SQRT: P = a*sqrt(x-b) + c
    if (calib_curve_sqrt_points == 0) return 0.0f;
    if (delta_c <= calib_curve_sqrt_b) return calib_curve_sqrt_c;  // Avoid negative sqrt
    return calib_curve_sqrt_a * sqrtf(delta_c - calib_curve_sqrt_b) + calib_curve_sqrt_c;
  }
}

// Grasping detection using calibration curve (whichever is currently selected)
bool isGraspingWithCurve(float pressure, float capacitance) {
  if (pneunet_fit_method == 0) {
    if (calib_curve_points == 0) return false;
  } else {
    if (calib_curve_sqrt_points == 0) return false;
  }
  
  // Only detect grasping if pressure is above threshold
  if (pressure <= GRASPING_PRESSURE_BUFFER) {
    return false;  // Pressure too low to be a reliable grasp indicator
  }
  
  // Calculate delta-C from C0
  float delta_c = capacitance - cap_c0;
  if (delta_c < 0.0f) delta_c = 0.0f; // Shouldn't happen, but clamp
  
  // Get expected pressure for this delta-C using the linear fit
  float expected_pressure = getExpectedPressure(delta_c);
  
  // Grasping occurs if pressure is significantly higher than expected
  // Add a buffer (typically 0.1-0.2 kPa) above the expected value
  float grasping_threshold = expected_pressure + GRASPING_PRESSURE_BUFFER;
  
  bool grasping = (pressure > grasping_threshold);
  
  return grasping;
}

// ============ OBJECT SIZE CALIBRATION GLOBAL STATE ============
ObjectSizeCalibPoint object_size_calib_points[6];  // Max 6 points
int object_size_calib_count = 0;
float calibrated_object_size = 0.0f;
volatile bool objectSizeCalibrationInProgress = false;
int object_size_calib_current_index = 0;
float object_size_calib_accumulated_delta_c = 0.0f;
int object_size_calib_sample_count = 0;

// Preferences for saving/loading object size calibration
Preferences prefsObjectSize;

// ============ OBJECT SIZE CALIBRATION LOAD/SAVE ============
void loadObjectSizeCalibration() {
  prefsObjectSize.begin("objectsize_cal", true);
  
  // Load count of calibration points
  object_size_calib_count = prefsObjectSize.getInt("count", 0);
  
  if (object_size_calib_count > 0 && object_size_calib_count <= 6) {
    // Load all calibration points
    for (int i = 0; i < object_size_calib_count; i++) {
      char key_size[20];
      char key_delta_c[20];
      snprintf(key_size, sizeof(key_size), "size_%d", i);
      snprintf(key_delta_c, sizeof(key_delta_c), "delta_c_%d", i);
      
      object_size_calib_points[i].object_size_mm = prefsObjectSize.getFloat(key_size, 0.0f);
      object_size_calib_points[i].delta_c = prefsObjectSize.getFloat(key_delta_c, 0.0f);
    }
    Serial.printf("Object size calibration loaded: %d points\n", object_size_calib_count);
  } else {
    Serial.println("No object size calibration found. Run calibration via web interface.");
  }
  
  prefsObjectSize.end();
}

void saveObjectSizeCalibration() {
  // Clear old data first to avoid NVS space issues
  prefsObjectSize.begin("objectsize_cal", false);
  prefsObjectSize.clear();
  prefsObjectSize.end();
  
  // Now save the new calibration data
  prefsObjectSize.begin("objectsize_cal", false);
  
  prefsObjectSize.putInt("count", object_size_calib_count);
  
  for (int i = 0; i < object_size_calib_count; i++) {
    char key_size[20];
    char key_delta_c[20];
    snprintf(key_size, sizeof(key_size), "size_%d", i);
    snprintf(key_delta_c, sizeof(key_delta_c), "delta_c_%d", i);
    
    prefsObjectSize.putFloat(key_size, object_size_calib_points[i].object_size_mm);
    prefsObjectSize.putFloat(key_delta_c, object_size_calib_points[i].delta_c);
  }
  
  prefsObjectSize.end();
  Serial.printf("Object size calibration saved (%d points)\n", object_size_calib_count);
}

void clearObjectSizeCalibration() {
  prefsObjectSize.begin("objectsize_cal", false);
  prefsObjectSize.clear();
  prefsObjectSize.end();
  
  object_size_calib_count = 0;
  object_size_calib_current_index = 0;
  object_size_calib_accumulated_delta_c = 0.0f;
  object_size_calib_sample_count = 0;
  calibrated_object_size = 0.0f;
  
  Serial.println("Object size calibration cleared");
}

// ============ OBJECT SIZE CALIBRATION FUNCTIONS ============
bool startObjectSizeCalibration() {
  if (objectSizeCalibrationInProgress) return false;
  if (cap_c0 <= 0.0f) {
    Serial.println("Error: C0 not calibrated. Please calibrate capacitance first.");
    return false;
  }
  
  objectSizeCalibrationInProgress = true;
  object_size_calib_current_index = 0;
  object_size_calib_accumulated_delta_c = 0.0f;
  object_size_calib_sample_count = 0;
  object_size_calib_count = 0;
  
  Serial.printf("Object size calibration started. Will collect %d distance points.\n", 
                OBJECT_SIZE_CALIB_COUNT);
  return true;
}

void updateObjectSizeCalibration() {
  if (!objectSizeCalibrationInProgress) return;
  if (object_size_calib_current_index >= OBJECT_SIZE_CALIB_COUNT) return;
  
  // Get current capacitance
  float current_capacitance = 0.0f;
  if (xSemaphoreTake(capacitanceMutex, 10 / portTICK_PERIOD_MS)) {
    current_capacitance = lastCapacitance;
    xSemaphoreGive(capacitanceMutex);
  }
  
  // Calculate delta-C from C0
  if (cap_c0 > 0.0f) {
    float delta_c = current_capacitance - cap_c0;
    
    if (delta_c > 0.0f) {
      // Accumulate the delta-C value
      object_size_calib_accumulated_delta_c += delta_c;
      object_size_calib_sample_count++;
    }
  }
}

bool finishObjectSizeCalibration() {
  if (!objectSizeCalibrationInProgress) return false;
  if (object_size_calib_sample_count < 1) {
    Serial.println("No samples collected for this distance");
    return false;
  }
  
  // Average the accumulated delta-C
  float avg_delta_c = object_size_calib_accumulated_delta_c / object_size_calib_sample_count;
  
  // Store the calibration point
  object_size_calib_points[object_size_calib_current_index].object_size_mm = 
    OBJECT_SIZE_CALIB_DISTANCES[object_size_calib_current_index];
  object_size_calib_points[object_size_calib_current_index].delta_c = avg_delta_c;
  
  Serial.printf("Calibration point %d: Object size=%.1f mm, ΔC=%.2f pF (avg of %d samples)\n",
                object_size_calib_current_index + 1,
                OBJECT_SIZE_CALIB_DISTANCES[object_size_calib_current_index],
                avg_delta_c,
                object_size_calib_sample_count);
  
  // Move to next distance or finish
  object_size_calib_current_index++;
  object_size_calib_count = object_size_calib_current_index;
  object_size_calib_accumulated_delta_c = 0.0f;
  object_size_calib_sample_count = 0;
  
  if (object_size_calib_current_index >= OBJECT_SIZE_CALIB_COUNT) {
    // Calibration complete
    objectSizeCalibrationInProgress = false;
    saveObjectSizeCalibration();
    Serial.println("Object size calibration complete!");
    return true;  // Signal completion
  }
  
  return false;  // Still collecting more points
}

void abortObjectSizeCalibration() {
  if (!objectSizeCalibrationInProgress) return;
  
  objectSizeCalibrationInProgress = false;
  object_size_calib_current_index = 0;
  object_size_calib_accumulated_delta_c = 0.0f;
  object_size_calib_sample_count = 0;
  object_size_calib_count = 0;
  
  Serial.println("Object size calibration aborted (data discarded)");
}

// Linear interpolation: given delta_c, find object size
// If delta_c matches exactly or is within range of calibration points, interpolate
float calculateCalibratedObjectSize(float delta_c) {
  if (object_size_calib_count < 1) {
    return 0.0f;  // No calibration data
  }
  
  if (object_size_calib_count == 1) {
    // Only one point, return that object size if delta_c is close
    if (fabsf(delta_c - object_size_calib_points[0].delta_c) < 1.0f) {
      return object_size_calib_points[0].object_size_mm;
    }
    return 0.0f;  // Out of range
  }
  
  // Find the two points to interpolate/extrapolate between
  // Calibration is stored in descending order of object size (110, 90, 70, 50, 30, 10)
  // delta_c generally increases with decreasing object size
  
  for (int i = 0; i < object_size_calib_count - 1; i++) {
    float dc1 = object_size_calib_points[i].delta_c;
    float dc2 = object_size_calib_points[i + 1].delta_c;
    float size1 = object_size_calib_points[i].object_size_mm;
    float size2 = object_size_calib_points[i + 1].object_size_mm;
    
    // Check if delta_c is between these two points
    float min_dc = fminf(dc1, dc2);
    float max_dc = fmaxf(dc1, dc2);
    
    if (delta_c >= min_dc && delta_c <= max_dc) {
      // Linear interpolation
      float t = (delta_c - dc1) / (dc2 - dc1);
      float interpolated_size = size1 + t * (size2 - size1);
      return interpolated_size;
    }
  }
  
  // If outside the range of calibration, use linear extrapolation
  // Use the two outermost points for extrapolation
  float dc_first = object_size_calib_points[0].delta_c;
  float size_first = object_size_calib_points[0].object_size_mm;
  float dc_last = object_size_calib_points[object_size_calib_count - 1].delta_c;
  float size_last = object_size_calib_points[object_size_calib_count - 1].object_size_mm;
  
  // Calculate slope from first and last points
  float slope = (size_last - size_first) / (dc_last - dc_first);
  
  // Check if below lower bound (smallest delta_c) - extrapolate backwards
  if (delta_c < fminf(dc_first, dc_last)) {
    float min_dc = fminf(dc_first, dc_last);
    float min_size = (dc_first < dc_last) ? size_first : size_last;
    float extrapolated_size = min_size + slope * (delta_c - min_dc);
    return extrapolated_size;
  }
  
  // Check if above upper bound (largest delta_c) - extrapolate forwards
  if (delta_c > fmaxf(dc_first, dc_last)) {
    float max_dc = fmaxf(dc_first, dc_last);
    float max_size = (dc_first > dc_last) ? size_first : size_last;
    float extrapolated_size = max_size + slope * (delta_c - max_dc);
    return extrapolated_size;
  }
  
  return 0.0f;  // Should not reach here
}

// ============ DATA PROCESSOR TASK ============
// This task synchronizes data from both sensors, detects grasping, and writes to SD card
// It reads the latest available data from each sensor and combines them into a single CSV entry

void dataProcessorTask(void *parameter) {
  Serial.println("Data processor task started");
  
  // Load PneuNet deformation calibration curve on startup
  loadPneunetDeformationCalibration();
  
  // Load object size calibration on startup
  loadObjectSizeCalibration();
  
  #ifdef USE_SD_CARD
    // Ensure SD card is initialized
    if (!isSDCardReady()) {
      Serial.println("Data Processor: SD card not ready, cannot start data processing");
      vTaskDelete(NULL);
      return;
    }
  #endif
  
  unsigned long lastProcessedTime = 0;
  float lastPressureSnapshot = 0.0;
  float lastCapacitanceSnapshot = 0.0;

  // previous-sample snapshots for delta calculations
  float prevPressure = 0.0f;
  float prevCapacitance = 0.0f;

  // Grasping detection state machine (old)
  int graspingDetectionCount = 0;
  bool wasGrasping = false;
  unsigned long lastDetectionTs = 0; // absolute millis() of last positive detection
  
  while (true) {
    if (isCollecting) {
      // Get current time relative to collection start
      unsigned long currentTime = millis() - collectionStartTime;
      
      // Read latest pressure data (if available)
      if (xSemaphoreTake(pressureMutex, 10 / portTICK_PERIOD_MS)) {
        lastPressureSnapshot = lastPressure;
        xSemaphoreGive(pressureMutex);
      }
      
      // Read latest capacitance data (if available)
      if (xSemaphoreTake(capacitanceMutex, 10 / portTICK_PERIOD_MS)) {
        lastCapacitanceSnapshot = lastCapacitance;
        xSemaphoreGive(capacitanceMutex);
      }
      
      // ============ CALCULATE PNEUNET DEFORMATION ============
      // Update deformation parameters based on current capacitance
      calculatePneunetDeformation(lastCapacitanceSnapshot);
      
      // ============ CALCULATE CALIBRATED OBJECT SIZE ============
      // Update calibrated object size if calibration exists
      if (objectSizeCalibrationInProgress) {
        updateObjectSizeCalibration();
      } else if (object_size_calib_count > 0 && cap_c0 > 0.0f) {
        float delta_c = lastCapacitanceSnapshot - cap_c0;
        if (delta_c > 0.0f) {
          calibrated_object_size = calculateCalibratedObjectSize(delta_c);
        } else {
          calibrated_object_size = 0.0f;
        }
      }
      
      // ============ GRASPING DETECTION ============
      // Use new calibration curve-based detection if available
      bool detected = false;
      
      if (pneunetDeformationCalibrationInProgress) {
        // During calibration, just update the curve
        updatePneunetDeformationCalibration();
      } else if (calib_curve_points > 0) {
        // Use calibration curve for detection
        detected = isGraspingWithCurve(lastPressureSnapshot, lastCapacitanceSnapshot);
      } else {
        // Fallback to old detection if no curve available
        if (lastProcessedTime != 0) {
          detected = detectGrasping(lastPressureSnapshot, prevPressure,
                                    lastCapacitanceSnapshot, prevCapacitance);
        }
      }

      // Grasping detection with debouncing
      if (detected) {
        graspingDetectionCount++;
        
        // Confirm grasping only after GRASPING_DEBOUNCE_COUNT consecutive detections
        if (graspingDetectionCount >= GRASPING_DEBOUNCE_COUNT && !wasGrasping) {
          isGrasping = true;
          wasGrasping = true;
          Serial.printf("GRASPING DETECTED at t=%lu ms (Pressure: %.2f kPa, Capacitance: %.2f pF, detections: %d)\n",
                        currentTime, lastPressureSnapshot, lastCapacitanceSnapshot, graspingDetectionCount);
        }
      } else {
        // Reset counter if detection is lost
        graspingDetectionCount = 0;
        
        if (wasGrasping) {
          isGrasping = false;
          wasGrasping = false;
          Serial.printf("GRASPING RELEASED at t=%lu ms\n", currentTime);
        }
      }
      
      // Write synchronized data to SD card
      #ifdef USE_SD_CARD
        writeToSDCard(currentTime, lastPressureSnapshot, lastCapacitanceSnapshot);
      #endif
      
      DEBUG_PRINT("Data Processor: Time=%lu ms, Pressure=%.2f kPa, Capacitance=%.2f pF, Grasping=%s\n",
                    currentTime, lastPressureSnapshot, lastCapacitanceSnapshot, 
                    isGrasping ? "YES" : "NO");
      
      lastProcessedTime = currentTime;

      // update previous-sample snapshots for next iteration
      prevPressure = lastPressureSnapshot;
      prevCapacitance = lastCapacitanceSnapshot;
    }
    
    vTaskDelay(DATA_PROCESSING_DELAY / portTICK_PERIOD_MS);
  }
}

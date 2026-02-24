#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <vector>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// ============ SIMULATION MODE ============
const bool SIMULATE_PRESSURE = false;      // Simulate pressure sensor
const bool SIMULATE_CAPACITANCE = false;   // Simulate capacitance sensor

// ============ SERIAL PRINT CONFIGURATION ============
const bool ENABLE_SERIAL_PRINTS = true;    // Set to false to disable serial monitor prints during measurements (improves performance)

// ============ WIFI CONFIGURATION ============
namespace {
  constexpr const char* SSID = "PneuNetSensor";
  constexpr const char* PASSWORD = "sensor123";
}

// ============ HARDWARE CONFIGURATION ============
// Pressure Sensor (5V sensor with voltage divider to protect ESP32 ADC)
const int PRESSURE_PIN = 8;
// Voltage divider: R1=1kΩ (sensor to ADC), R2=2kΩ (ADC to GND)
// This scales 5V input to 3.3V for safe ADC reading
// Divider ratio: Vout/Vin = R2/(R1+R2) = 2/(1+2) = 0.667
const float PRESSURE_DIVIDER_RATIO = 0.985 / 2.953;  // R2 / (R1 + R2) = 2k / 3k

// Capacitance Meter (Touch Sensor)
const int CAPACITANCE_PIN = 4;              // Touch-capable pin for capacitance measurement

// Capacitance Calibration Configuration
const int CALIB_SAMPLES = 60;               // Samples per calibration point
const int QUICK_SAMPLES = 8;                // Samples for quick measurements
const int SAMPLE_DELAY_MS = 5;              // Delay between samples (ms)

// Capacitance Stuck Detection & Recovery
const bool ENABLE_CAPACITANCE_STUCK_DETECTION = true;  // Enable automatic detection and recovery of stuck sensor
const int CAPACITANCE_STUCK_THRESHOLD = 20;  // Number of consecutive identical readings before triggering recovery
const float CAPACITANCE_STUCK_TOLERANCE = 0.01f;  // Max difference (pF) to consider readings identical

// Common
const int LED_PIN = 48;
const float Vs = 3.3;

// ============ LED CONFIGURATION ============
const int NUM_LEDS = 1;
const int LED_BRIGHTNESS = 51; // 20% of 255 (255 * 0.2 = 51)
const int LED_BRIGHTNESS_GRASPING = 200; // Brighter green when grasping (78% of 255)

// ============ GRASPING DETECTION CONFIGURATION ============
// New approach: calibrate PneuNet deformation curve, then compare actual pressure vs expected
const int MAX_CALIB_SAMPLES_PER_POINT = 1000; // Max raw samples per delta-C point
const int MIN_CALIB_PRESSURE = 0.5; // (kPa) everything below this pressure is exluded from the plot
const float GRASPING_PRESSURE_BUFFER = 0.2f; // Buffer (kPa): grasping if P > expected_P + buffer (linear fit)
const int GRASPING_DEBOUNCE_COUNT = 5; // Number of consecutive detections required to confirm grasping

// ============ PNEUNET DEFORMATION GEOMETRY ============
const float PNEUNET_L = 120.0f;   // Length (mm) - axial measurement along finger
const float PNEUNET_H = 19.0f;    // Height (mm) - cross-sectional dimension

// ============ OBJECT SIZE CALIBRATION ============
// Calibration distances in mm - used to measure delta C for different object sizes
const float OBJECT_SIZE_CALIB_DISTANCES[] = {100.0f, 90.0f, 80.0f};
// const float OBJECT_SIZE_CALIB_DISTANCES[] = {110.0f, 90.0f, 70.0f, 50.0f, 30.0f, 10.0f};
const int OBJECT_SIZE_CALIB_COUNT = sizeof(OBJECT_SIZE_CALIB_DISTANCES) / sizeof(OBJECT_SIZE_CALIB_DISTANCES[0]);
const int OBJECT_SIZE_CALIB_SAMPLES = 50; // Samples to average per distance point

// Old constants (deprecated but kept for reference)
const float PRESSURE_INCREASE_THRESHOLD = 0.15;    // Minimum pressure increase to detect grasping (kPa)
const float CAPACITANCE_TOLERANCE = 4.0;           // Maximum capacitance change to be considered "constant" (pF)
const int GRASPING_RELEASE_TIMEOUT_MS = 700;       // Release grasping if no detection within this timeout (ms)

// ============ SAMPLING CONFIGURATION ============
const int PRESSURE_SAMPLING_DELAY = 50;          // Pressure sensor sampling delay (ms)
const int CAPACITANCE_SAMPLING_DELAY = 50; // Capacitance measurement delay (ms)
const int DATA_PROCESSING_DELAY = 100;  // Data processor sync interval (ms)

// ============ DATA STORAGE CONFIGURATION ============
const int MAX_MEASUREMENTS = 5000;

// ============ SD CARD CONFIGURATION ============
#define USE_SD_CARD  // Uncomment when SD card support is added
const int SD_MOSI_PIN = 11;  // GPIO 11
const int SD_MISO_PIN = 13;  // GPIO 13
const int SD_CLK_PIN = 12;   // GPIO 12
const int SD_CS_PIN = 10;    // GPIO 10

// ============ SIMULATION PARAMETERS ============
const float SIM_BASE_PRESSURE = 2.0;
const float SIM_AMPLITUDE = 1.5;
const float SIM_FREQUENCY = 0.001;
const float SIM_NOISE = 0.1;

// ============ FREERTOS TASK PRIORITIES ============
const int PRESSURE_SENSOR_PRIORITY = 3;   // Highest - data collection
const int CAPACITANCE_SENSOR_PRIORITY = 3; // Highest - data collection (same as pressure)
const int DATA_PROCESSOR_PRIORITY = 2;    // Medium - data processing
const int WEBSERVER_PRIORITY = 1;         // Lower - webserver, LED, WiFi

// ============ DATA STRUCTURES ============
struct PressureMeasurement {
  unsigned long timestamp;
  float pressure;
};

struct CapacitanceMeasurement {
  unsigned long timestamp;
  float capacitance;  // in picoFarads
};

struct ProcessedData {
  unsigned long timestamp;
  float pressure;
  float capacitance;
};

struct CalibRawSample {
  float delta_c;    // Delta-C value (pF)
  float pressure;   // Pressure value (kPa)
};

// ============ GLOBAL VARIABLES - PRESSURE SENSOR ============
extern float lastPressure;
extern unsigned long lastPressureTime;
extern SemaphoreHandle_t pressureMutex;
extern bool isCollecting;
extern bool hasCollectionStarted;
extern unsigned long collectionStartTime;

// ============ GLOBAL VARIABLES - CAPACITANCE SENSOR ============
extern float lastCapacitance;
extern unsigned long lastCapacitanceTime;
extern SemaphoreHandle_t capacitanceMutex;

// ============ GLOBAL VARIABLES - PNEUNET DEFORMATION ============
// Calculated deformation parameters
extern float pneunet_alpha;  // Deformation angle (radians)
extern float pneunet_r;      // Radius of curvature (mm)
extern float pneunet_d;      // Object diameter / grasp width (mm)

// ============ DEBUG MACROS ============
#define DEBUG_PRINT(fmt, ...) \
  do { \
    if (ENABLE_SERIAL_PRINTS) { \
      Serial.printf(fmt, ##__VA_ARGS__); \
    } \
  } while (0)

#define DEBUG_PRINTLN(msg) \
  do { \
    if (ENABLE_SERIAL_PRINTS) { \
      Serial.println(msg); \
    } \
  } while (0)

// ============ PRESSURE OFFSET (CALIBRATION) ============
extern float pressureOffset; // pressure zero offset applied to raw readings (kPa)
void loadPressureCalibration();
void savePressureCalibration();
void resetPressureOffset();

// ============ GLOBAL VARIABLES - SD CARD ============
extern String currentSDFilename;

// ============ GLOBAL VARIABLES - LED CONTROL ============
// (for webserver/UI status updates)
extern bool isGrasping;

// ============ LED CONTROL FUNCTIONS ============
void initLED();
void setLEDColor(uint8_t red, uint8_t green, uint8_t blue);
void ledStatusOff();
void ledStatusRed();
void ledStatusGreen();
void ledStatusGrasping();
void ledStatusBlue();
void ledStatusYellow();
void ledStatusPurple();

// ============ PRESSURE SENSOR FUNCTIONS ============
void pressureSensorTask(void *parameter);
float readPressure();
float readRealPressure();
float simulatePressure();

// ============ CAPACITANCE SENSOR FUNCTIONS ============
void capacitanceSensorTask(void *parameter);
float measureCapacitance();
float measureCapacitanceReal();
float simulateCapacitance();
void loadCapacitanceCalibration();
void saveCapacitanceCalibration();
void clearCalibration();
void startCalibrationStep(int step);
bool computeAndStoreCalibration();

// ============ CAPACITANCE CALIBRATION GLOBAL VARIABLES ============
extern long raw_baseline;
extern long raw_82;
extern long raw_101;
extern float calib_a;
extern float calib_b;
extern volatile bool calibrationInProgress;
extern volatile int calibrationStep;
extern float cap_c0; // undeformed capacitance C0 in pF
bool storeCapacitanceC0(int samples = CALIB_SAMPLES);

// ============ CALIBRATION CURVE: PRESSURE VS DELTA-C (LINEAR: P = a*ΔC + b, SQRT: P = a*sqrt(x-b) + c) ============
extern int calib_curve_points;
extern int calib_curve_sqrt_points;
extern int calib_raw_sample_count;
extern int pneunet_fit_method;  // 0 = LINEAR, 1 = SQRT
extern float calib_curve_delta_c_a;  // Linear coefficient
extern float calib_curve_delta_c_b;  // Linear constant offset
extern float calib_curve_delta_c_c;  // Reserved (not used in linear fit)
extern float calib_curve_sqrt_a;     // Square root curve coefficient
extern float calib_curve_sqrt_b;     // Square root curve offset
extern float calib_curve_sqrt_c;     // Square root curve vertical offset
extern CalibRawSample calib_raw_samples[MAX_CALIB_SAMPLES_PER_POINT];
extern volatile bool pneunetDeformationCalibrationInProgress;

void loadPneunetDeformationCalibration();
void savePneunetDeformationCalibration();
void clearPneunetDeformationCalibration();
bool startPneunetDeformationCalibration();
void updatePneunetDeformationCalibration();
bool finishPneunetDeformationCalibration();
bool applyPneunetFitMethod(int method);
void abortPneunetDeformationCalibration();
bool fitSquareRootCurve(const CalibRawSample* samples, int sample_count, float& out_a, float& out_b, float& out_c);
float getExpectedPressure(float delta_c);
bool isGraspingWithCurve(float pressure, float capacitance);

// ============ OBJECT SIZE CALIBRATION (DELTA-C VS OBJECT SIZE) ============
struct ObjectSizeCalibPoint {
  float object_size_mm;  // Object diameter/width in mm
  float delta_c;         // Capacitance change from C0 in pF
};

extern ObjectSizeCalibPoint object_size_calib_points[6]; // Max 6 points for the default distances
extern int object_size_calib_count;
extern float calibrated_object_size;  // Current calculated object size in mm
extern volatile bool objectSizeCalibrationInProgress;
extern int object_size_calib_current_index;
extern float object_size_calib_accumulated_delta_c;
extern int object_size_calib_sample_count;

void loadObjectSizeCalibration();
void saveObjectSizeCalibration();
void clearObjectSizeCalibration();
bool startObjectSizeCalibration();
void updateObjectSizeCalibration();
bool finishObjectSizeCalibration();
void abortObjectSizeCalibration();
float calculateCalibratedObjectSize(float delta_c);

// ============ DATA PROCESSOR FUNCTIONS ============
void dataProcessorTask(void *parameter);

// ============ COLLECTION CONTROL FUNCTIONS ============
void startCollection();
void stopCollection();
void resetCollection();
bool getCollectionState();

// ============ WIFI FUNCTIONS ============
void setupWiFi();

// ============ WEB SERVER FUNCTIONS ============
void setupWebServer();
String generateHTML();

// ============ SD CARD FUNCTIONS ============
void setupSDCard();
void setSDCardFilename(String filename);
String generateTimestampFilename();
bool createNewDataFile();
bool writeToSDCard(unsigned long timestamp, float pressure, float capacitance);
String listSDCardFiles();
bool closeSDCard();
bool isSDCardReady();

#endif // CONFIG_H

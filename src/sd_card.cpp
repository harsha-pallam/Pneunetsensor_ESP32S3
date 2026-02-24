#include "config.h"
#include <SD.h>
#include <SPI.h>
#include <time.h>

// ============ SD CARD VARIABLES ============
SPIClass sdSPI(HSPI);
File dataFile;
bool sdCardInitialized = false;
String currentSDFilename = "";  // Global variable for current filename

// ============ GENERATE TIMESTAMP-BASED FILENAME ============
String generateTimestampFilename() {
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  
  char filename[32];
  strftime(filename, sizeof(filename), "/%Y%m%d_%H%M%S_data.csv", timeinfo);
  
  return String(filename);
}

// ============ SET CUSTOM FILENAME ============
void setSDCardFilename(String filename) {
  if (filename.length() == 0) {
    // Empty filename - use timestamp-based name
    currentSDFilename = generateTimestampFilename();
  } else {
    // Custom filename - ensure it starts with / and ends with .csv
    if (!filename.startsWith("/")) {
      filename = "/" + filename;
    }
    if (!filename.endsWith(".csv")) {
      filename += ".csv";
    }
    currentSDFilename = filename;
  }
  Serial.printf("SD Card: Filename set to: %s\n", currentSDFilename.c_str());
}

// ============ SD CARD INITIALIZATION ============
void setupSDCard() {
  #ifdef USE_SD_CARD
    Serial.println("SD Card: Initializing SPI...");
    // Initialize SPI with custom pins
    sdSPI.begin(SD_CLK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
    
    Serial.println("SD Card: Initializing SD card...");
    if (!SD.begin(SD_CS_PIN, sdSPI)) {
      Serial.println("SD Card: Initialization failed!");
      sdCardInitialized = false;
      return;
    }
    
    Serial.println("SD Card: Initialization successful");
    sdCardInitialized = true;
    
    // Set default filename if not already set
    if (currentSDFilename.length() == 0) {
      setSDCardFilename("");  // This will generate a timestamp-based name
    }
  #else
    Serial.println("SD Card: Support is disabled in config");
  #endif
}

// ============ CREATE NEW DATA FILE ============
bool createNewDataFile() {
  #ifdef USE_SD_CARD
    if (!sdCardInitialized) {
      return false;
    }
    
    // Close existing file if open
    if (dataFile) {
      dataFile.close();
    }
    
    // Create new file
    dataFile = SD.open(currentSDFilename, FILE_WRITE);
    if (!dataFile) {
      Serial.printf("SD Card: Failed to create file: %s\n", currentSDFilename.c_str());
      return false;
    }
    
    // Write header for both sensors
    dataFile.println("Time_ms,Pressure_kPa,Capacitance_pF");
    dataFile.flush();
    
    Serial.printf("SD Card: Created new file: %s\n", currentSDFilename.c_str());
    return true;
  #else
    return false;
  #endif
}

// ============ WRITE DATA TO SD CARD ============
bool writeToSDCard(unsigned long timestamp, float pressure, float capacitance) {
  #ifdef USE_SD_CARD
    if (!sdCardInitialized || !dataFile) {
      return false;
    }
    
    // Write data line with timestamp, pressure, and capacitance
    char buffer[96];
    snprintf(buffer, sizeof(buffer), "%lu,%.4f,%.2f\n", timestamp, pressure, capacitance);
    dataFile.print(buffer);
    dataFile.flush();
    
    return true;
  #else
    return false;
  #endif
}

// ============ CLOSE SD CARD ============
bool closeSDCard() {
  #ifdef USE_SD_CARD
    if (dataFile) {
      dataFile.close();
    }
    if (sdCardInitialized) {
      // Properly close SPI
      sdSPI.end();
      sdCardInitialized = false;
      Serial.println("SD Card: Closed");
      return true;
    }
    return false;
  #else
    return false;
  #endif
}

// ============ LIST FILES ON SD CARD ============
String listSDCardFiles() {
  #ifdef USE_SD_CARD
    if (!sdCardInitialized) {
      return "[]";
    }
    
    String json = "[";
    bool first = true;
    File root = SD.open("/");
    File file = root.openNextFile();
    
    while (file) {
      if (!file.isDirectory() && String(file.name()).endsWith(".csv")) {
        if (!first) json += ",";
        String fullPath = String(file.name());
        // Ensure path starts with /
        if (!fullPath.startsWith("/")) {
          fullPath = "/" + fullPath;
        }
        json += "{\"name\":\"" + fullPath + "\",\"size\":" + String(file.size()) + "}";
        first = false;
      }
      file = root.openNextFile();
    }
    root.close();
    
    json += "]";
    return json;
  #else
    return "[]";
  #endif
}

// ============ GET SD CARD STATUS ============
bool isSDCardReady() {
  return sdCardInitialized;
}

#include <ESPAsyncWebServer.h>
#include "config.h"
#include <sys/time.h>  // For settimeofday()
#ifdef USE_SD_CARD
  #include <SD.h>
#endif

// ============ WEB SERVER OBJECT ============
AsyncWebServer server(80);

// ============ HTML GENERATION ============
String generateHTML() {
  return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>PneuNet Sensor</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body { font-family: Arial, sans-serif; background: #f0f0f0; padding: 20px; }
    .container { max-width: 1200px; margin: 0 auto; background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 8px rgba(0,0,0,0.1); }
    h1 { color: #333; margin-bottom: 20px; }
    
    .control-panel { display: flex; gap: 10px; margin-bottom: 20px; flex-wrap: wrap; align-items: center; }
    button { padding: 10px 20px; background: #0066cc; color: white; border: none; border-radius: 5px; cursor: pointer; font-size: 14px; font-weight: bold; }
    button:hover { background: #0052a3; }
    button:active { background: #003d7a; }
    button.active { background: #00aa00; }
    button.active:hover { background: #008800; }
    button.reset { background: #cc3300; }
    button.reset:hover { background: #aa2600; }
    button.files { background: #0099cc; }
    button.files:hover { background: #007799; }
    
    .modal { display: none; position: fixed; z-index: 1000; left: 0; top: 0; width: 100%; height: 100%; background-color: rgba(0,0,0,0.5); }
    .modal-content { background-color: #fefefe; margin: auto; padding: 20px; border: 1px solid #888; border-radius: 8px; width: 90%; max-width: 500px; position: absolute; top: 50%; left: 50%; transform: translate(-50%, -50%); }
    .modal-header { font-size: 18px; font-weight: bold; margin-bottom: 15px; }
    .modal-close { color: #aaa; float: right; font-size: 28px; font-weight: bold; cursor: pointer; }
    .modal-close:hover { color: #000; }
    .file-list { border: 1px solid #ddd; border-radius: 5px; max-height: 300px; overflow-y: auto; }
    .file-item { padding: 10px; border-bottom: 1px solid #eee; display: flex; justify-content: space-between; align-items: center; }
    .file-item:hover { background-color: #f0f0f0; }
    .file-info { display: flex; flex-direction: column; flex-grow: 1; cursor: pointer; }
    .file-name { font-family: monospace; font-size: 12px; }
    .file-size { font-size: 11px; color: #999; }
    .file-actions { display: flex; gap: 5px; }
    .file-btn { padding: 5px 10px; font-size: 11px; border: none; border-radius: 3px; cursor: pointer; }
    .download-btn { background: #0099cc; color: white; }
    .download-btn:hover { background: #007799; }
    .delete-btn { background: #cc3300; color: white; }
    .delete-btn:hover { background: #aa2600; }
    input[type="text"] { padding: 10px 15px; border: 1px solid #ccc; border-radius: 5px; font-size: 14px; min-width: 250px; }
    
    .status { display: inline-flex; align-items: center; gap: 10px; padding: 10px 15px; background: #e8f4f8; border-radius: 5px; }
    .status-indicator { width: 12px; height: 12px; border-radius: 50%; background: #0066cc; }
    
    .stats { display: grid; grid-template-columns: repeat(auto-fit, minmax(150px, 1fr)); gap: 15px; margin-bottom: 20px; }
    .stat-box { background: #f5f5f5; padding: 15px; border-radius: 5px; text-align: center; }
    .stat-label { font-size: 12px; color: #666; margin-bottom: 5px; }
    .stat-value { font-size: 24px; font-weight: bold; color: #0066cc; }
    
    .chart-wrapper { background: #fafafa; border: 1px solid #ddd; border-radius: 5px; padding: 10px; margin-bottom: 20px; width: 100%; overflow: auto; }
    #pressureChart { width: 100%; height: auto; max-height: 400px; }
    
    .footer { font-size: 12px; color: #999; text-align: center; margin-top: 20px; }
  </style>
</head>
<body>
  <div class="container" style="max-height: 100vh; overflow-y: auto; padding-right: 10px;">
    <h1>PneuNet Sensor Dashboard</h1>
    <p style="font-size: 12px; color: #999; margin-top: -10px; margin-bottom: 15px;">Current file: <span id="currentFilename" style="font-style: italic;">Waiting for collection...</span></p>
    
    <!-- Row 1: Collection Controls -->
    <div class="control-panel">
      <input type="text" id="filenameInput" placeholder="Filename (auto-timestamp if empty)" maxlength="50">
      <button id="playPauseBtn" onclick="toggleCollection()">▶ Start</button>
      <button class="reset" onclick="resetData()">Reset</button>
      <button class="files" onclick="openFileListModal()">📁 Files</button>
      <div class="status">
        <span class="status-indicator" id="statusIndicator"></span>
        <span id="statusText">Idle</span>
      </div>
    </div>
    
    <!-- Sensor Data and Calibration Controls -->
    <h3 style="margin-top: 20px; margin-bottom: 10px;">Sensor Data</h3>
    <div style="display: grid; grid-template-columns: 1fr 1fr 1fr; gap: 20px; margin-bottom: 20px;">
      <!-- Pressure Box -->
      <div style="background: white; padding: 15px; border-radius: 5px; border: 1px solid #ddd;">
        <div style="font-size: 12px; color: #0066cc; font-weight: bold; margin-bottom: 5px;">Pressure</div>
        <div style="font-size: 24px; font-weight: bold; color: #0066cc; margin-bottom: 8px;"><span id="currentPressure">--</span> kPa</div>
        <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 10px; font-size: 12px; color: #666; margin-bottom: 10px;">
          <div>avg: <span id="avgPressure" style="font-weight: bold;">--</span></div>
          <div>max: <span id="maxPressure" style="font-weight: bold;">--</span></div>
          <div>min: <span id="minPressure" style="font-weight: bold;">--</span></div>
          <div>n: <span id="countPressure" style="font-weight: bold;">0</span></div>
        </div>
        <div style="display: flex; align-items: center; gap: 6px; padding-top: 8px; border-top: 1px solid #eee;">
          <span style="font-size: 11px; color: #555;">P0:</span>
          <span style="font-weight: bold; font-size: 12px; flex: 1;" id="calibPressOffset">--</span>
          <button onclick="resetPressureZeroCompact()" title="Reset pressure zero" style="padding: 3px 6px; font-size: 10px; background: #780ee3; color: white; border: none; border-radius: 3px; cursor: pointer;">↻</button>
        </div>
      </div>
      
      <!-- Capacitance Box -->
      <div style="background: white; padding: 15px; border-radius: 5px; border: 1px solid #ddd;">
        <div style="font-size: 12px; color: #ff9933; font-weight: bold; margin-bottom: 5px;">Capacitance</div>
        <div style="font-size: 24px; font-weight: bold; color: #ff9933; margin-bottom: 8px;"><span id="currentCapacitance">--</span> pF</div>
        <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 10px; font-size: 12px; color: #666; margin-bottom: 10px;">
          <div>avg: <span id="avgCapacitance" style="font-weight: bold;">--</span></div>
          <div>max: <span id="maxCapacitance" style="font-weight: bold;">--</span></div>
          <div>min: <span id="minCapacitance" style="font-weight: bold;">--</span></div>
          <div>n: <span id="countCapacitance" style="font-weight: bold;">0</span></div>
        </div>
        <div style="display: flex; align-items: center; gap: 6px; padding-top: 8px; border-top: 1px solid #eee;">
          <span style="font-size: 11px; color: #555;">C0:</span>
          <span style="font-weight: bold; font-size: 12px; flex: 1;" id="calibC0">--</span>
          <button onclick="storeC0Compact()" title="Store C0" style="padding: 3px 6px; font-size: 10px; background: #780ee3; color: white; border: none; border-radius: 3px; cursor: pointer;">↻</button>
          <button onclick="openCapacitanceCalibModal()" title="Capacitance calibration" style="padding: 3px 6px; font-size: 10px; background: #780ee3; color: white; border: none; border-radius: 3px; cursor: pointer;">⚙</button>
        </div>
      </div>
      
      <!-- Calibration Controls -->
      <div style="background: white; padding: 12px; border-radius: 5px; border: 1px solid #ddd; display: flex; flex-direction: column; gap: 8px;">
        <div style="font-size: 12px; color: #666; font-weight: bold; margin-bottom: 3px;">Calibration</div>
        <button onclick="openDeformationCalibModal()" title="P-ΔC calibration" style="padding: 8px; font-size: 11px; background: #780ee3; color: white; border: none; border-radius: 3px; cursor: pointer; flex: 1;">P-ΔC</button>
        <button onclick="openObjectSizeCalibModal()" title="Object size calibration" style="padding: 8px; font-size: 11px; background: #780ee3; color: white; border: none; border-radius: 3px; cursor: pointer; flex: 1;">Object Size</button>
      </div>
    </div>
    
    <!-- Sensor Data Over Time Chart -->
    <div style="background: white; border: 1px solid #ddd; border-radius: 5px; padding: 12px; margin-bottom: 20px;">
      <svg id="pressureChart" viewBox="0 0 1000 350" preserveAspectRatio="xMidYMid meet" style="width: 100%; height: auto; background: white;"></svg>
    </div>
    
    <!-- Smart PneuNet Features -->
    <h3 style="margin-top: 20px; margin-bottom: 10px;">Smart PneuNet Features</h3>
    <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 20px; margin-bottom: 20px;">
      <!-- Geometric Deformation & Object Size -->
      <div style="padding: 15px; border-radius: 5px; border: 1px solid #ddd;">
        <div style="font-size: 12px; color: #666; font-weight: bold; margin-bottom: 8px;">Geometric Deformation & Object Size</div>
        <svg id="deformationViz" viewBox="0 0 350 300" style="width: 100%; height: auto; background: white; display: block;"></svg>
      </div>
      
      <!-- P-ΔC Curve -->
      <div style="background: white; padding: 15px; border-radius: 5px; border: 1px solid #ddd; display: flex; flex-direction: column;">
        <div style="font-size: 12px; color: #666; font-weight: bold; margin-bottom: 8px;">P-ΔC Characteristic Curve</div>
        <svg id="pneunetCurve" viewBox="0 0 400 280" style="width: 100%; height: auto; background: white; flex: 1;"></svg>
      </div>
    </div>

    <div class="footer">
      <p>Data stored in: <strong>SD Card</strong></p>
    </div>
  </div>
  
  <div id="fileListModal" class="modal">
    <div class="modal-content">
      <span class="modal-close" onclick="closeFileListModal()">&times;</span>
      <div class="modal-header">Manage Data Files</div>
      <div class="file-list" id="fileList"></div>
    </div>
  </div>

  <!-- Capacitance Calibration Modal -->
  <div id="capacitanceCalibModal" class="modal">
    <div class="modal-content" style="max-width: 600px;">
      <span class="modal-close" onclick="closeCapacitanceCalibModal()">&times;</span>
      <div class="modal-header">Capacitance Calibration (3-Point)</div>
      
      <!-- Raw Values & Equation -->
      <div style="background: #fffef0; border: 1px solid #ffd966; border-radius: 5px; padding: 12px; margin-bottom: 15px;">
        <div style="font-size: 12px; margin-bottom: 8px;"><strong>Current Calibration:</strong></div>
        <div style="font-size: 12px; margin-bottom: 6px;">Baseline (raw): <span id="capCalRawBase" style="font-family: monospace; font-weight: bold;">--</span></div>
        <div style="font-size: 12px; margin-bottom: 6px;">82pF (raw): <span id="capCalRaw82" style="font-family: monospace; font-weight: bold;">--</span></div>
        <div style="font-size: 12px; margin-bottom: 10px;">101pF (raw): <span id="capCalRaw101" style="font-family: monospace; font-weight: bold;">--</span></div>
        <div style="background: white; padding: 8px; border-radius: 3px; font-family: monospace; font-size: 13px; color: #0099cc; margin-bottom: 10px;"><strong>Equation:</strong> C = <span id="capCalEqA">--</span> × raw + <span id="capCalEqB">--</span></div>
      </div>
      
      <!-- Calibration Steps -->
      <div id="capCalStepsContainer" style="display: none; background: #fffef0; border: 1px solid #ffd966; border-radius: 5px; padding: 12px; margin-bottom: 15px;">
        <div style="font-size: 13px; margin-bottom: 10px;"><strong>Step <span id="capCalStep">1</span> of 3:</strong> <span id="capCalDescription">Preparing...</span></div>
        <div style="display: flex; gap: 10px; margin-bottom: 10px;">
          <button onclick="nextCapacitanceCalibStep()" id="capCalNextBtn" style="flex: 1; padding: 10px; background: #0066cc; color: white; border: none; border-radius: 5px; font-weight: bold; cursor: pointer; font-size: 13px;">Ready - Next Step</button>
          <button onclick="cancelCapacitanceCalib()" style="flex: 1; padding: 10px; background: #cc3300; color: white; border: none; border-radius: 5px; font-weight: bold; cursor: pointer; font-size: 13px;">Cancel</button>
        </div>
        <div id="capCalMessage" style="padding: 8px; background: white; border-radius: 3px; font-size: 12px; min-height: 25px;"></div>
      </div>
      
      <!-- Action Buttons -->
      <div style="display: flex; gap: 10px;">
        <button onclick="startCapacitanceCalib()" id="capCalStartBtn" style="flex: 1; padding: 10px; background: #00aa00; color: white; border: none; border-radius: 5px; font-weight: bold; cursor: pointer; font-size: 13px;">Start Calibration</button>
        <button onclick="clearCapacitanceCalib()" style="flex: 1; padding: 10px; background: #cc3300; color: white; border: none; border-radius: 5px; font-weight: bold; cursor: pointer; font-size: 13px;">Clear Data</button>
      </div>
    </div>
  </div>

  <!-- Deformation Calibration Modal -->
  <div id="deformationCalibModal" class="modal">
    <div class="modal-content" style="max-width: 650px;">
      <span class="modal-close" onclick="closeDeformationCalibModal()">&times;</span>
      <div class="modal-header">Pressure-Capacitance Curve Calibration</div>
      
      <!-- Status Info -->
      <div style="background: #f0f8ff; border: 1px solid #0099cc; border-radius: 5px; padding: 12px; margin-bottom: 15px;">
        <div style="font-size: 12px; margin-bottom: 6px;"><strong>Status:</strong> <span id="deformCalStatus">Not started</span></div>
        <div style="font-size: 12px;"><strong>Points collected:</strong> <span id="deformCalPoints">0</span></div>
      </div>
      
      <!-- Plot -->
      <div style="background: #fafafa; border: 1px solid #ddd; border-radius: 4px; padding: 10px; margin-bottom: 15px; width: 100%; overflow-x: auto;">
        <svg id="deformPlot" viewBox="0 0 500 300" style="width: 100%; height: auto; min-height: 200px; background: white; border: 1px solid #eee; border-radius: 3px; display: block;"></svg>
      </div>
      
      <!-- Equation -->
      <div style="background: #f0f8ff; border: 1px solid #0099cc; border-radius: 4px; padding: 12px; margin-bottom: 15px; font-family: monospace; font-size: 13px;">
        <strong>Fit Equation:</strong> <span id="deformCalEquation" style="color: #0099cc;">P = a×ΔC + b</span>
      </div>
      
      <!-- Instructions -->
      <div style="background: #f0f8ff; border: 1px solid #0099cc; border-radius: 5px; padding: 12px; margin-bottom: 15px; font-size: 12px;">
        <p style="margin: 0; color: #666;">Apply pressure to the PneuNet without grasping to establish baseline pressure response.</p>
      </div>
      
      <!-- Action Buttons - Not Running -->
      <div id="deformCalNotRunning" style="display: flex; gap: 10px; flex-direction: column;">
        <div style="display: flex; gap: 10px;">
          <button onclick="startDeformationCalib()" style="flex: 1; padding: 10px; background: #00aa00; color: white; border: none; border-radius: 5px; font-weight: bold; cursor: pointer; font-size: 13px;">Start</button>
          <button onclick="clearDeformationCalib()" style="flex: 1; padding: 10px; background: #cc3300; color: white; border: none; border-radius: 5px; font-weight: bold; cursor: pointer; font-size: 13px;">Clear</button>
        </div>
        <div style="display: flex; gap: 10px; border-top: 1px solid #ddd; padding-top: 10px;">
          <button onclick="applyFitMethod(0)" title="Apply linear fit: P = a*ΔC + b" style="flex: 1; padding: 10px; background: #0066cc; color: white; border: none; border-radius: 5px; font-weight: bold; cursor: pointer; font-size: 13px;">Apply Linear Fit</button>
          <button onclick="applyFitMethod(1)" title="Apply square root fit: P = a*sqrt(x-b) + c" style="flex: 1; padding: 10px; background: #0066cc; color: white; border: none; border-radius: 5px; font-weight: bold; cursor: pointer; font-size: 13px;">Apply Sqrt Fit</button>
        </div>
      </div>
      
      <!-- Action Buttons - Running -->
      <div id="deformCalRunning" style="display: none; gap: 10px; flex-direction: row;">
        <button onclick="abortDeformationCalib()" style="flex: 1; padding: 10px; background: #cc3300; color: white; border: none; border-radius: 5px; font-weight: bold; cursor: pointer; font-size: 13px;">Abort</button>
        <button onclick="finishDeformationCalib()" style="flex: 1; padding: 10px; background: #0099cc; color: white; border: none; border-radius: 5px; font-weight: bold; cursor: pointer; font-size: 13px;">Finish</button>
      </div>
    </div>
  </div>

  <!-- Object Size Calibration Modal -->
  <div id="objectSizeCalibModal" class="modal">
    <div class="modal-content" style="max-width: 650px;">
      <span class="modal-close" onclick="closeObjectSizeCalibModal()">&times;</span>
      <div class="modal-header">Object Size Calibration</div>
      
      <!-- Instructions / Progress (shown during calibration) -->
      <div id="objectSizeCalibInstructions" style="background: #f5f0ff; border: 1px solid #9966cc; border-radius: 5px; padding: 15px; margin-bottom: 15px; display: none;">
        <div style="text-align: center; margin-bottom: 12px;">
          <div style="font-size: 11px; color: #666; margin-bottom: 4px;">Calibrating object size:</div>
          <div style="font-size: 32px; font-weight: bold; color: #9966cc; letter-spacing: 2px;"><span id="objectSizeCalibInstructionSize">--</span><span style="font-size: 24px;"> mm</span></div>
        </div>
        <div style="text-align: center; margin-bottom: 12px; font-size: 12px; color: #666;">
          <strong>Progress:</strong> <span id="objectSizeCalibProgressDuring">0/6</span>
        </div>
        <p style="margin: 6px 0; color: #333; font-size: 12px;"><strong>Apply pressure to this object until contact is made.</strong></p>
        <p style="margin: 0; color: #666; font-size: 11px;">The system will detect contact automatically. Once contact is made for 500ms, data will be saved and move to next size.</p>
      </div>
      
      <!-- Completed Calibration Info (shown when done) -->
      <div id="objectSizeCalibCompleted" style="background: #f5f0ff; border: 1px solid #9966cc; border-radius: 5px; padding: 15px; margin-bottom: 15px; display: none;">
        <div style="text-align: center; margin-bottom: 6px;">
          <div style="font-size: 11px; color: #666; margin-bottom: 4px;">Calibration complete</div>
          <div style="font-size: 14px; font-weight: bold; color: #9966cc;" id="objectSizeCalibCompletedSizes">Calibrated: -- mm</div>
        </div>
      </div>
      
      <!-- Plot -->
      <div style="background: #fafafa; border: 1px solid #ddd; border-radius: 4px; padding: 10px; margin-bottom: 15px; width: 100%; overflow-x: auto;">
        <svg id="objectSizeCalibPlotModal" viewBox="0 0 500 300" style="width: 100%; height: auto; min-height: 200px; background: white; border: 1px solid #eee; border-radius: 3px; display: block;"></svg>
      </div>
      
      <!-- Action Buttons - Not Running -->
      <div id="objectSizeCalibNotRunning" style="display: flex; gap: 10px;">
        <button onclick="startObjectSizeCalibrationModal()" style="flex: 1; padding: 10px; background: #00aa00; color: white; border: none; border-radius: 5px; font-weight: bold; cursor: pointer; font-size: 13px;">Start</button>
        <button onclick="clearObjectSizeCalibrationModal()" style="flex: 1; padding: 10px; background: #cc3300; color: white; border: none; border-radius: 5px; font-weight: bold; cursor: pointer; font-size: 13px;">Clear</button>
      </div>
      
      <!-- Action Buttons - Running -->
      <div id="objectSizeCalibRunning" style="display: none; gap: 10px; flex-direction: row;">
        <button onclick="abortObjectSizeCalibrationModal()" style="flex: 1; padding: 10px; background: #cc3300; color: white; border: none; border-radius: 5px; font-weight: bold; cursor: pointer; font-size: 13px;">Abort</button>
        <button onclick="finishObjectSizeCalibrationModal()" style="flex: 1; padding: 10px; background: #0099cc; color: white; border: none; border-radius: 5px; font-weight: bold; cursor: pointer; font-size: 13px;">Finish</button>
      </div>
    </div>
  </div>

  <div id="fileListModal" class="modal">
    <div class="modal-content">
      <span class="modal-close" onclick="closeFileListModal()">&times;</span>
      <div class="modal-header">Manage Data Files</div>
      <div class="file-list" id="fileList"></div>
    </div>
  </div>

  <div id="calibrationModal" class="modal">
    <div class="modal-content">
    <span class="modal-close" onclick="closeCalibrationModal()">&times;</span>
    <div class="modal-header">Calibration</div>
      
      <div style="margin: 20px 0; padding: 15px; background: #f5f5f5; border-radius: 5px;">
        <h4 style="margin-bottom: 10px;">Current Calibration Values</h4>
        <div style="margin-top: 8px; padding-top: 8px; border-top: 1px solid #eee;">
          <div style="font-size:13px; margin-bottom:6px;">P0: <strong><span id="press_offset">--</span> kPa</strong></div>
          <div style="margin-bottom:6px;"><button onclick="resetPressureZero()" style="padding:8px 12px; background:#0099cc; color:white; border:none; border-radius:4px; cursor:pointer;">Reset P0</button></div>
        </div>
        <div style="font-size: 13px; line-height: 1.8;">
          <div>Baseline (raw): <strong><span id="cal_baseline">--</span></strong></div>
          <div>82pF (raw): <strong><span id="cal_82pf">--</span></strong></div>
          <div>101pF (raw): <strong><span id="cal_101pf">--</span></strong></div>
          <div style="margin-top: 10px; padding-top: 10px; border-top: 1px solid #ddd;">
            <div style="font-family: monospace; font-size: 12px;">C = <span id="cal_a">--</span> × raw + <span id="cal_b">--</span></div>
          </div>
          <div style="margin-top:10px; padding-top:10px; border-top:1px solid #eee;">
            <div>Undeformed Capacitance C0: <strong><span id="cal_c0">--</span> pF</strong></div>
            <div style="margin-top:6px;"><button onclick="storeC0()" style="padding:8px 12px; background:#0099cc; color:white; border:none; border-radius:4px; cursor:pointer;">Store Current C0</button></div>
          </div>
        </div>
      </div>
      
      <details style="margin: 15px 0; border: 1px solid #0099cc; border-radius: 4px; overflow: hidden;">
        <summary style="background: #e6f2ff; padding: 12px; cursor: pointer; font-weight: bold; color: #0066cc; user-select: none;">PneuNet Deformation Calibration</summary>
        <div style="padding: 15px; background: white;">
          <p style="font-size: 12px; color: #666; margin: 0 0 10px 0;">
            Apply pressure to the PneuNet without grasping an object to establish baseline pressure response.
          </p>
          <div style="padding: 12px; background: #f0f8ff; border: 1px solid #0099cc; border-radius: 4px; font-size: 12px; margin-bottom: 10px;">
            <div style="margin-bottom: 6px;"><strong>Status:</strong> <span id="pneunetCalibStatus">Not started</span></div>
            <div><strong>Points collected:</strong> <span id="pneunetCalibPoints">0</span></div>
          </div>
          <div style="display: flex; flex-wrap: wrap; gap: 8px; margin-bottom: 10px;">
            <button onclick="startPneunetCalibration()" id="pneunetCalibStartBtn" style="flex: 1; min-width: 90px; padding: 10px; background: #00aa00; color: white; border: none; border-radius: 5px; font-weight: bold; cursor: pointer; font-size: 13px;">Start</button>
            <button onclick="finishPneunetCalibration()" id="pneunetCalibFinishBtn" style="flex: 1; min-width: 90px; padding: 10px; background: #0099cc; color: white; border: none; border-radius: 5px; font-weight: bold; cursor: pointer; font-size: 13px;" disabled>Finish</button>
            <button onclick="clearPneunetCalibration()" id="pneunetCalibClearBtn" style="flex: 1; min-width: 90px; padding: 10px; background: #cc3300; color: white; border: none; border-radius: 5px; font-weight: bold; cursor: pointer; font-size: 13px;">Clear</button>
          </div>
          <div id="pneunetCalibPlot" style="background: white; border: 1px solid #ddd; border-radius: 4px; padding: 10px; margin: 0; width: 100%; overflow-x: auto;">
            <svg id="pneunetPlot" viewBox="0 0 500 300" style="width: 100%; height: auto; min-height: 200px; background: #fafafa; border: 1px solid #eee; border-radius: 3px; display: block;"></svg>
          </div>
          <div style="background: #f0f8ff; border: 1px solid #0099cc; border-radius: 4px; padding: 12px; margin-top: 10px; font-family: monospace; font-size: 13px;">
            <strong>Fit Equation:</strong> <span id="pneunetCalibEquation" style="color: #0099cc;">P = a×ΔC + b</span>
          </div>
        </div>
      </details>
      
      <details style="margin: 15px 0; border: 1px solid #ffd966; border-radius: 4px; overflow: hidden;">
        <summary style="background: #fffef0; padding: 12px; cursor: pointer; font-weight: bold; color: #cc8800; user-select: none;">Capacitance 3-Point Calibration</summary>
        <div style="padding: 15px; background: white;">
          <p style="font-size: 12px; color: #666; margin: 0 0 10px 0;">
            This will guide you through a 3-point calibration using baseline (no cap), 82pF, and 101pF capacitors.
          </p>
          
          <div id="calibrationSteps" style="display: none; margin: 10px 0; padding: 12px; background: #fffef0; border: 1px solid #ffd966; border-radius: 5px;">
            <div style="font-size: 13px; margin-bottom: 10px;">
              <strong>Step <span id="currentStep">1</span> of 3:</strong> <span id="stepDescription">Preparing...</span>
            </div>
            <div style="display: flex; gap: 10px;">
              <button onclick="nextCalibrationStep()" id="nextStepBtn" style="flex: 1; padding: 10px; background: #0066cc; color: white; border: none; border-radius: 5px; font-weight: bold; cursor: pointer; font-size: 13px;">Ready - Next Step</button>
              <button onclick="cancelCalibration()" id="cancelCalBtn" style="flex: 1; padding: 10px; background: #cc3300; color: white; border: none; border-radius: 5px; font-weight: bold; cursor: pointer; font-size: 13px;">Cancel</button>
            </div>
            <div id="calibrationMessage" style="margin-top: 10px; padding: 10px; background: white; border-radius: 3px; font-size: 12px; min-height: 30px;"></div>
          </div>
          
          <div id="calibrationNotRunning" style="display: block;">
            <button onclick="startCalibration()" style="width: 100%; padding: 10px; background: #0099cc; color: white; border: none; border-radius: 5px; font-weight: bold; cursor: pointer; font-size: 13px;">Start Calibration</button>
          </div>
          <button onclick="clearCalibration()" style="width: 100%; padding: 8px; margin-top: 10px; background: #cc3300; color: white; border: none; border-radius: 5px; font-weight: bold; cursor: pointer; font-size: 12px;">Clear Calibration</button>
        </div>
      </details>

      <details style="margin: 15px 0; border: 1px solid #9966cc; border-radius: 4px; overflow: hidden;">
        <summary style="background: #f5f0ff; padding: 12px; cursor: pointer; font-weight: bold; color: #6633cc; user-select: none;">Object Size Calibration</summary>
        <div style="padding: 15px; background: white;">
          <p style="font-size: 12px; color: #666; margin: 0 0 10px 0;">
            Calibrate the relationship between capacitance change (ΔC) and object size. Use objects of known sizes.
          </p>
          
          <div style="padding: 12px; background: #f5f0ff; border: 1px solid #9966cc; border-radius: 4px; font-size: 12px; margin-bottom: 10px;">
            <div style="margin-bottom: 6px;"><strong>Status:</strong> <span id="objectsizeCalibStatus">Not started</span></div>
            <div style="margin-bottom: 6px;"><strong>Progress:</strong> <span id="objectsizeCalibProgress">0/6</span></div>
            <div><strong>Current object size:</strong> <span id="objectsizeCalibCurrentSize">--</span> mm</div>
          </div>
          
          <div id="objectsizeCalibRunning" style="display: none; padding: 12px; background: #f5f0ff; border: 1px solid #9966cc; border-radius: 5px; margin-bottom: 10px;">
            <div style="font-size: 13px; margin-bottom: 10px; font-weight: bold;">
              Apply pressure to <span id="objectsizeCalibDistance">--</span> mm object until contact
            </div>
            <div style="display: flex; gap: 10px;">
              <button onclick="finishObjectSizeCalibPoint()" style="flex: 1; padding: 10px; background: #00aa00; color: white; border: none; border-radius: 5px; font-weight: bold; cursor: pointer; font-size: 13px;">Next Size</button>
              <button onclick="abortObjectSizeCalibration()" style="flex: 1; padding: 10px; background: #cc3300; color: white; border: none; border-radius: 5px; font-weight: bold; cursor: pointer; font-size: 13px;">Abort</button>
            </div>
            <div id="objectsizeCalibMessage" style="margin-top: 10px; padding: 10px; background: #f0f0f0; border-radius: 3px; font-size: 12px; min-height: 30px;"></div>
          </div>
          
          <div id="objectsizeCalibNotRunning" style="display: block;">
            <button onclick="startObjectSizeCalibration()" style="width: 100%; padding: 10px; background: #0099cc; color: white; border: none; border-radius: 5px; font-weight: bold; cursor: pointer; font-size: 13px;">Start Calibration</button>
          </div>
          
          <div id="objectsizeCalibPlot" style="background: white; border: 1px solid #ddd; border-radius: 4px; padding: 10px; margin-top: 10px; width: 100%; overflow-x: auto;">
            <svg id="objectsizePlot" viewBox="0 0 500 300" style="width: 100%; height: auto; min-height: 200px; background: #fafafa; border: 1px solid #eee; border-radius: 3px; display: block;"></svg>
          </div>
          
          <button onclick="clearObjectSizeCalibration()" style="width: 100%; padding: 8px; margin-top: 10px; background: #cc3300; color: white; border: none; border-radius: 5px; font-weight: bold; cursor: pointer; font-size: 12px;">Clear Calibration</button>
        </div>
      </details>
    </div>
  </div>

  <script>
    let allData = [];
    let isCollecting = false;
    let firstStart = true;  // Track if this is first start
    let hasDataBeenCollected = false;  // Track if any data has been collected in current session
    let currentFilename = '';  // Track current filename being used
    let isGrasping = false;
    
    // Sync time with device on page load
    // Sync browser time with ESP32 device on page load
    window.addEventListener('load', async function() {
      await syncTimeWithDevice();
      await syncDeviceState();
      updateStatus();
      updateDeformation();  // Initial deformation update
      loadData();
      drawPneunetCurve();  // Draw P-ΔC characteristic curve from saved calibration
    });
    
    // Sync browser time with ESP32 device
    async function syncTimeWithDevice() {
      try {
        const now = new Date();
        const utcTimestamp = Math.floor(now.getTime() / 1000); // Current Unix timestamp in UTC
        const tzOffset = -now.getTimezoneOffset() * 60; // Timezone offset in seconds
        
        // Send both UTC timestamp and timezone offset to device
        await fetch('/api/settime?timestamp=' + utcTimestamp + '&tzoffset=' + tzOffset);
        console.log('Time synced with device (UTC:', utcTimestamp, 'TZ offset:', tzOffset, 's)');
      } catch(e) {
        console.error('Error syncing time:', e);
      }
    }
    
    // Get current collection state from device on page load
    async function syncDeviceState() {
      try {
        const response = await fetch('/api/getstatus');
        const data = await response.json();
        isCollecting = data.collecting || false;
        // If device is collecting, mark that data has been collected
        if (isCollecting) {
          hasDataBeenCollected = true;
        }
        console.log('Device state synced. Collecting:', isCollecting);
      } catch(e) {
        console.error('Error syncing device state:', e);
      }
    }
    
    // Periodically update filename display and deformation
    setInterval(() => {
      updateFilenameDisplay();
      updateDeformation();  // Update deformation every 1 second
    }, 1000);
    
    // File list modal functions
    function openFileListModal() {
      document.getElementById('fileListModal').style.display = 'block';
      loadFileList();
    }
    
    function closeFileListModal() {
      document.getElementById('fileListModal').style.display = 'none';
    }
    
    async function loadFileList() {
      try {
        const response = await fetch('/api/listfiles');
        const files = await response.json();
        
        const fileList = document.getElementById('fileList');
        fileList.innerHTML = '';
        
        // Add Delete All button if there are files
        if (files.length > 0) {
          const deleteAllDiv = document.createElement('div');
          deleteAllDiv.style.padding = '10px';
          deleteAllDiv.style.borderBottom = '1px solid #ddd';
          deleteAllDiv.style.display = 'flex';
          deleteAllDiv.style.gap = '5px';
          
          const deleteAllBtn = document.createElement('button');
          deleteAllBtn.style.flex = '1';
          deleteAllBtn.style.padding = '8px';
          deleteAllBtn.style.background = '#cc3300';
          deleteAllBtn.style.color = 'white';
          deleteAllBtn.style.border = 'none';
          deleteAllBtn.style.borderRadius = '3px';
          deleteAllBtn.style.cursor = 'pointer';
          deleteAllBtn.style.fontSize = '12px';
          deleteAllBtn.style.fontWeight = 'bold';
          deleteAllBtn.textContent = 'Delete All Files';
          deleteAllBtn.onclick = () => deleteAllFiles();
          deleteAllDiv.appendChild(deleteAllBtn);
          fileList.appendChild(deleteAllDiv);
        }
        
        if (files.length === 0) {
          fileList.innerHTML = '<div style="padding: 10px; text-align: center; color: #999;">No data files found</div>';
          return;
        }
        
        files.forEach(file => {
          const item = document.createElement('div');
          item.className = 'file-item';
          const sizeKB = (file.size / 1024).toFixed(1);
          // Strip leading / for display
          const displayName = file.name.startsWith('/') ? file.name.substring(1) : file.name;
          
          const fileInfo = document.createElement('div');
          fileInfo.className = 'file-info';
          fileInfo.onclick = () => downloadFile(file.name);
          fileInfo.innerHTML = `
            <span class="file-name">${displayName}</span>
            <span class="file-size">${sizeKB} KB</span>
          `;
          
          const actions = document.createElement('div');
          actions.className = 'file-actions';
          
          const downloadBtn = document.createElement('button');
          downloadBtn.className = 'file-btn download-btn';
          downloadBtn.textContent = 'Download';
          downloadBtn.onclick = () => downloadFile(file.name);
          
          const deleteBtn = document.createElement('button');
          deleteBtn.className = 'file-btn delete-btn';
          deleteBtn.textContent = 'Delete';
          deleteBtn.onclick = () => deleteFile(file.name);
          
          actions.appendChild(downloadBtn);
          actions.appendChild(deleteBtn);
          
          item.appendChild(fileInfo);
          item.appendChild(actions);
          fileList.appendChild(item);
        });
      } catch(e) {
        console.error('Error loading file list:', e);
      }
    }
    
    async function downloadFile(filename) {
      try {
        console.log('Downloading file:', filename);
        const response = await fetch('/api/download?file=' + encodeURIComponent(filename));
        
        if (!response.ok) {
          alert('Error downloading file: ' + response.statusText);
          return;
        }
        
        const csv = await response.text();
        
        if (csv.includes('File not found') || csv.includes('error')) {
          alert('Error: File not found on device');
          return;
        }
        
        const blob = new Blob([csv], {type: 'text/csv'});
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = filename.replace(/\//g, '');  // Remove leading slash for download
        a.click();
        URL.revokeObjectURL(url);
        console.log('File downloaded successfully');
      } catch(e) {
        console.error('Error downloading file:', e);
        alert('Error downloading file: ' + e.message);
      }
    }
    
    async function deleteFile(filename) {
      if (confirm('Delete file: ' + filename + '?')) {
        try {
          console.log('Deleting file:', filename);
          const response = await fetch('/api/deletefile?file=' + encodeURIComponent(filename));
          const result = await response.json();
          
          if (response.ok && result.status === 'deleted') {
            console.log('File deleted successfully');
            loadFileList();  // Refresh the list
          } else {
            alert('Error deleting file: ' + (result.error || 'Unknown error'));
            loadFileList();  // Still refresh to check
          }
        } catch(e) {
          console.error('Error deleting file:', e);
          alert('Error deleting file: ' + e.message);
          loadFileList();  // Still refresh to check
        }
      }
    }
    
    async function deleteAllFiles() {
      if (confirm('Delete ALL data files? This cannot be undone.')) {
        try {
          console.log('Deleting all files');
          const response = await fetch('/api/deleteallfiles');
          const result = await response.json();
          
          if (response.ok && result.status === 'deleted') {
            console.log('All files deleted successfully');
            alert('All data files deleted');
            loadFileList();  // Refresh the list
          } else {
            alert('Error deleting files: ' + (result.error || 'Unknown error'));
            loadFileList();  // Still refresh to check
          }
        } catch(e) {
          console.error('Error deleting all files:', e);
          alert('Error deleting files: ' + e.message);
          loadFileList();  // Still refresh to check
        }
      }
    }
    
    // ============ CALIBRATION FUNCTIONS ============
    let calibrationStep = 0;
    let maxCalibrationSteps = 3;
    
    function openCalibrationModal() {
      document.getElementById('calibrationModal').style.display = 'block';
      loadCalibrationData();
    }
    
    function closeCalibrationModal() {
      document.getElementById('calibrationModal').style.display = 'none';
      cancelCalibration();
    }
    
    // ============ COMPACT CALIBRATION FUNCTIONS ============
    // Update compact calibration display on page
    async function updateCompactCalibration() {
      try {
        const response = await fetch('/api/calibration/status');
        const data = await response.json();
        
        // Update pressure offset
        if (data.pressure_offset !== undefined) {
          document.getElementById('calibPressOffset').textContent = Number(data.pressure_offset).toFixed(4) + ' kPa';
        } else {
          document.getElementById('calibPressOffset').textContent = '--';
        }
        
        // Update C0
        if (data.cap_c0 !== undefined) {
          document.getElementById('calibC0').textContent = Number(data.cap_c0).toFixed(3) + ' pF';
        } else {
          document.getElementById('calibC0').textContent = '--';
        }
      } catch(e) {
        console.error('Error updating compact calibration:', e);
      }
    }
    
    // Compact pressure reset with confirmation
    async function resetPressureZeroCompact() {
      if (!confirm('Reset pressure zero to current reading?')) return;
      try {
        const response = await fetch('/api/pressure/reset');
        const data = await response.json();
        if (data && data.offset !== undefined) {
          const newOffset = Number(data.offset).toFixed(4);
          document.getElementById('calibPressOffset').textContent = newOffset + ' kPa';
        } else {
          console.error('Pressure reset failed');
        }
      } catch(e) {
        console.error('Error resetting pressure zero:', e);
      }
    }
    
    // Compact C0 store with confirmation
    async function storeC0Compact() {
      if (!confirm('Store current capacitance as undeformed C0?')) return;
      try {
        const response = await fetch('/api/calibration/store_c0');
        const data = await response.json();
        if (data && data.c0 !== undefined) {
          const c0Value = Number(data.c0).toFixed(3);
          document.getElementById('calibC0').textContent = c0Value + ' pF';
        }
      } catch(e) {
        console.error('Error storing C0:', e);
      }
    }
    
    // ============ CAPACITANCE CALIBRATION MODAL ============
    let capCalibStep = 0;
    let capCalibMaxSteps = 3;
    
    function openCapacitanceCalibModal() {
      document.getElementById('capacitanceCalibModal').style.display = 'block';
      loadCapacitanceCalibData();
    }
    
    function closeCapacitanceCalibModal() {
      document.getElementById('capacitanceCalibModal').style.display = 'none';
      cancelCapacitanceCalib();
    }
    
    async function loadCapacitanceCalibData() {
      try {
        const response = await fetch('/api/calibration/status');
        const data = await response.json();
        
        document.getElementById('capCalRawBase').textContent = data.raw_baseline || '--';
        document.getElementById('capCalRaw82').textContent = data.raw_82 || '--';
        document.getElementById('capCalRaw101').textContent = data.raw_101 || '--';
        document.getElementById('capCalEqA').textContent = (data.calib_a || 0).toFixed(8);
        document.getElementById('capCalEqB').textContent = (data.calib_b || 0).toFixed(8);
      } catch(e) {
        console.error('Error loading capacitance calibration data:', e);
      }
    }
    
    async function startCapacitanceCalib() {
      capCalibStep = 0;
      document.getElementById('capCalStartBtn').style.display = 'none';
      document.getElementById('capCalStepsContainer').style.display = 'block';
      updateCapacitanceCalibStep();
    }
    
    function updateCapacitanceCalibStep() {
      const steps = [
        { step: 0, desc: 'Leave pin floating (NO capacitor connected)', action: 'Connect nothing and click Ready when ready.' },
        { step: 1, desc: 'Connect 82pF capacitor', action: 'Connect an 82pF capacitor between pin and GND, then click Ready.' },
        { step: 2, desc: 'Connect 101pF capacitor', action: 'Replace with 101pF capacitor between pin and GND, then click Ready.' }
      ];
      
      const current = steps[capCalibStep];
      document.getElementById('capCalStep').textContent = capCalibStep + 1;
      document.getElementById('capCalDescription').textContent = current.desc;
      document.getElementById('capCalMessage').textContent = current.action;
    }
    
    async function nextCapacitanceCalibStep() {
      document.getElementById('capCalNextBtn').disabled = true;
      document.getElementById('capCalNextBtn').textContent = 'Taking measurement...';
      
      try {
        const response = await fetch('/api/calibration/step?step=' + capCalibStep);
        const result = await response.json();
        
        if (result.success) {
          document.getElementById('capCalMessage').textContent = 'Step ' + (capCalibStep + 1) + ' completed. Raw value: ' + result.raw_value;
          capCalibStep++;
          
          if (capCalibStep >= capCalibMaxSteps) {
            document.getElementById('capCalMessage').innerHTML = '<strong style="color: #00aa00;">✓ Calibration completed successfully!</strong><br>Values have been saved.';
            document.getElementById('capCalNextBtn').style.display = 'none';
            document.getElementById('capCalStartBtn').style.display = 'block';
            await loadCapacitanceCalibData();
          } else {
            updateCapacitanceCalibStep();
            document.getElementById('capCalNextBtn').disabled = false;
            document.getElementById('capCalNextBtn').textContent = 'Ready - Next Step';
          }
        } else {
          document.getElementById('capCalMessage').innerHTML = '<strong style="color: #cc3300;">✗ Error: ' + result.error + '</strong>';
          document.getElementById('capCalNextBtn').disabled = false;
          document.getElementById('capCalNextBtn').textContent = 'Ready - Next Step';
        }
      } catch(e) {
        document.getElementById('capCalMessage').innerHTML = '<strong style="color: #cc3300;">✗ Error: ' + e.message + '</strong>';
        document.getElementById('capCalNextBtn').disabled = false;
        document.getElementById('capCalNextBtn').textContent = 'Ready - Next Step';
      }
    }
    
    function cancelCapacitanceCalib() {
      document.getElementById('capCalStepsContainer').style.display = 'none';
      document.getElementById('capCalStartBtn').style.display = 'block';
      capCalibStep = 0;
    }
    
    async function clearCapacitanceCalib() {
      if (!confirm('Clear all capacitance calibration data?')) return;
      try {
        await fetch('/api/calibration/clear');
        await loadCapacitanceCalibData();
        alert('Capacitance calibration cleared');
      } catch(e) {
        console.error('Error clearing capacitance calibration:', e);
        alert('Error clearing calibration');
      }
    }
    
    // ============ DEFORMATION CALIBRATION MODAL ============
    function openDeformationCalibModal() {
      document.getElementById('deformationCalibModal').style.display = 'block';
      loadDeformationCalibData();
    }
    
    function closeDeformationCalibModal() {
      document.getElementById('deformationCalibModal').style.display = 'none';
    }
    
    function openObjectSizeCalibModal() {
      document.getElementById('objectSizeCalibModal').style.display = 'block';
      updateObjectSizeCalibStatusModal();
    }
    
    function closeObjectSizeCalibModal() {
      document.getElementById('objectSizeCalibModal').style.display = 'none';
    }
    
    async function loadDeformationCalibData() {
      try {
        const response = await fetch('/api/calibration/pneunet_status');
        const data = await response.json();
        
        // Determine which equation to display based on fit method
        const fitMethod = data.fit_method || 0;
        
        if (fitMethod === 1 && data.sqrt_a !== undefined && data.sqrt_b !== undefined) {
          // Display SQRT fit equation
          const a = Number(data.sqrt_a);
          const b = Number(data.sqrt_b);
          const c = Number(data.sqrt_c || 0);
          const cStr = c >= 0 ? `+ ${c.toFixed(4)}` : `- ${Math.abs(c).toFixed(4)}`;
          document.getElementById('deformCalEquation').textContent = `P = ${a.toFixed(6)}×√(ΔC-${b.toFixed(4)}) ${cStr}`;
        } else if (data.a !== undefined && data.b !== undefined) {
          // Display LINEAR fit equation
          const a = Number(data.a);
          const b = Number(data.b);
          document.getElementById('deformCalEquation').textContent = `P = ${a.toFixed(6)}×ΔC + ${b.toFixed(4)}`;
        }
        
        drawDeformationCalibPlot(data);
        
        // Update status
        document.getElementById('deformCalPoints').textContent = data.points || 0;
        if (data.points > 0) {
          document.getElementById('deformCalStatus').textContent = 'Completed (' + data.points + ' points)';
        } else {
          document.getElementById('deformCalStatus').textContent = 'Not started';
        }
      } catch(e) {
        console.error('Error loading deformation calibration data:', e);
      }
    }
    
    async function startDeformationCalib() {
      try {
        // Start data collection
        await fetch('/api/start');
        const response = await fetch('/api/calibration/pneunet_start');
        const data = await response.json();
        if (data && data.success) {
          document.getElementById('deformCalNotRunning').style.display = 'none';
          document.getElementById('deformCalRunning').style.display = 'flex';
          document.getElementById('deformCalStatus').textContent = 'In progress...';
        }
      } catch(e) {
        console.error('Error starting deformation calibration:', e);
      }
    }
    
    async function finishDeformationCalib() {
      try {
        const response = await fetch('/api/calibration/pneunet_finish');
        const data = await response.json();
        if (data && data.success) {
          // Stop data collection
          await fetch('/api/stop');
          document.getElementById('deformCalNotRunning').style.display = 'flex';
          document.getElementById('deformCalRunning').style.display = 'none';
          await loadDeformationCalibData();
        }
      } catch(e) {
        console.error('Error finishing deformation calibration:', e);
      }
    }
    
    async function abortDeformationCalib() {
      try {
        // Stop data collection
        await fetch('/api/stop');
        const response = await fetch('/api/calibration/pneunet_abort');
        const data = await response.json();
        if (data && data.success) {
          document.getElementById('deformCalNotRunning').style.display = 'flex';
          document.getElementById('deformCalRunning').style.display = 'none';
          document.getElementById('deformCalStatus').textContent = 'Aborted';
        }
      } catch(e) {
        console.error('Error aborting deformation calibration:', e);
      }
    }
    
    async function clearDeformationCalib() {
      if (!confirm('Clear deformation calibration data?')) return;
      try {
        await fetch('/api/calibration/pneunet_clear');
        document.getElementById('deformCalNotRunning').style.display = 'flex';
        document.getElementById('deformCalRunning').style.display = 'none';
        await loadDeformationCalibData();
        alert('Deformation calibration cleared');
      } catch(e) {
        console.error('Error clearing deformation calibration:', e);
        alert('Error clearing calibration');
      }
    }
    
    async function applyFitMethod(method) {
      // method: 0 = LINEAR, 1 = SQRT
      try {
        const response = await fetch(`/api/calibration/pneunet_apply_fit?method=${method}`);
        const data = await response.json();
        
        if (data && data.success) {
          const methodName = method === 0 ? 'LINEAR' : 'SQRT';
          alert(`Applied ${methodName} fit successfully`);
          await loadDeformationCalibData();
        } else {
          alert(`Error: ${data.error || 'Failed to apply fit'}`);
        }
      } catch(e) {
        console.error('Error applying fit method:', e);
        alert('Error applying fit method');
      }
    }
    
    // Call update on page load
    window.addEventListener('load', updateCompactCalibration);
    
    // Periodically update compact calibration display
    setInterval(updateCompactCalibration, 2000);
    
    async function loadCalibrationData() {
      try {
        const response = await fetch('/api/calibration/status');
        const data = await response.json();
        
        document.getElementById('cal_baseline').textContent = data.raw_baseline || '--';
        document.getElementById('cal_82pf').textContent = data.raw_82 || '--';
        document.getElementById('cal_101pf').textContent = data.raw_101 || '--';
        document.getElementById('cal_a').textContent = (data.calib_a || 0).toFixed(8);
        document.getElementById('cal_b').textContent = (data.calib_b || 0).toFixed(8);
        // pressure offset
        if (data.pressure_offset !== undefined) {
          document.getElementById('press_offset').textContent = Number(data.pressure_offset).toFixed(4);
        } else {
          document.getElementById('press_offset').textContent = '--';
        }
        // C0
        if (data.cap_c0 !== undefined) {
          document.getElementById('cal_c0').textContent = Number(data.cap_c0).toFixed(3);
        } else {
          document.getElementById('cal_c0').textContent = '--';
        }
        
        // Load PneuNet calibration status and equation
        try {
          const pneunetResponse = await fetch('/api/calibration/pneunet_status');
          const pneunetData = await pneunetResponse.json();
          if (pneunetData && pneunetData.a !== undefined && pneunetData.b !== undefined) {
            const a = Number(pneunetData.a);
            const b = Number(pneunetData.b);
            document.getElementById('pneunetCalibEquation').textContent = `P = ${a.toFixed(6)}×ΔC + ${b.toFixed(4)}`;
            // Also display the plot with the stored fit
            drawPneunetCalibPlot(pneunetData);
          }
        } catch(e) {
          console.error('Error loading PneuNet calibration:', e);
        }
      } catch(e) {
        console.error('Error loading calibration data:', e);
      }
    }

    async function storeC0() {
      if (!confirm('Store current capacitance as undeformed C0?')) return;
      try {
        const response = await fetch('/api/calibration/store_c0');
        const data = await response.json();
        if (data && data.c0 !== undefined) {
          document.getElementById('cal_c0').textContent = Number(data.c0).toFixed(3);
        }
      } catch(e) {
        console.error('Error storing C0:', e);
      }
    }

    async function resetPressureZero() {
      if (!confirm('Reset pressure zero to current reading?')) return;
      try {
        const response = await fetch('/api/pressure/reset');
        const data = await response.json();
        if (data && data.offset !== undefined) {
          document.getElementById('press_offset').textContent = Number(data.offset).toFixed(4);
        }
      } catch(e) {
        console.error('Error resetting pressure zero:', e);
      }
    }

    // ============ PNEUNET DEFORMATION CALIBRATION ============
    let pneunetCalibInterval = null;
    let pneunetCalibData = [];

    async function startPneunetCalibration() {
      try {
        const response = await fetch('/api/calibration/pneunet_start');
        const data = await response.json();
        if (data && data.success) {
          document.getElementById('pneunetCalibStartBtn').textContent = 'Abort';
          document.getElementById('pneunetCalibStartBtn').style.background = '#cc3300';
          document.getElementById('pneunetCalibStartBtn').onclick = abortPneunetCalibration;
          document.getElementById('pneunetCalibFinishBtn').disabled = false;
          document.getElementById('pneunetCalibStatus').textContent = 'Running...';
          pneunetCalibData = [];
          
          // Poll status every 500ms
          pneunetCalibInterval = setInterval(updatePneunetCalibStatus, 500);
        }
      } catch(e) {
        console.error('Error starting PneuNet calibration:', e);
      }
    }

    async function finishPneunetCalibration() {
      if (pneunetCalibInterval) clearInterval(pneunetCalibInterval);
      try {
        const response = await fetch('/api/calibration/pneunet_finish');
        const data = await response.json();
        if (data && data.success) {
          document.getElementById('pneunetCalibStartBtn').textContent = 'Start';
          document.getElementById('pneunetCalibStartBtn').style.background = '#00aa00';
          document.getElementById('pneunetCalibStartBtn').onclick = startPneunetCalibration;
          document.getElementById('pneunetCalibFinishBtn').disabled = true;
          document.getElementById('pneunetCalibStatus').textContent = 'Completed';
          document.getElementById('pneunetCalibPoints').textContent = data.points || '0';
          
          // Fetch the saved curve data to display plot
          const statusResponse = await fetch('/api/calibration/pneunet_status');
          const statusData = await statusResponse.json();
          drawPneunetCalibPlot(statusData);
        }
      } catch(e) {
        console.error('Error finishing PneuNet calibration:', e);
      }
    }

    async function abortPneunetCalibration() {
      if (pneunetCalibInterval) clearInterval(pneunetCalibInterval);
      try {
        const response = await fetch('/api/calibration/pneunet_abort');
        const data = await response.json();
        if (data && data.success) {
          document.getElementById('pneunetCalibStartBtn').textContent = 'Start';
          document.getElementById('pneunetCalibStartBtn').style.background = '#00aa00';
          document.getElementById('pneunetCalibStartBtn').onclick = startPneunetCalibration;
          document.getElementById('pneunetCalibFinishBtn').disabled = true;
          document.getElementById('pneunetCalibStatus').textContent = 'Aborted (data discarded)';
          document.getElementById('pneunetCalibPoints').textContent = '0';
          pneunetCalibData = [];
          document.getElementById('pneunetPlot').innerHTML = '';
        }
      } catch(e) {
        console.error('Error aborting PneuNet calibration:', e);
      }
    }

    async function clearPneunetCalibration() {
      if (!confirm('Clear PneuNet deformation calibration?')) return;
      if (pneunetCalibInterval) clearInterval(pneunetCalibInterval);
      try {
        const response = await fetch('/api/calibration/pneunet_clear');
        const data = await response.json();
        if (data && data.success) {
          document.getElementById('pneunetCalibStartBtn').disabled = false;
          document.getElementById('pneunetCalibFinishBtn').disabled = true;
          document.getElementById('pneunetCalibStatus').textContent = 'Not started';
          document.getElementById('pneunetCalibPoints').textContent = '0';
          pneunetCalibData = [];
          document.getElementById('pneunetPlot').innerHTML = '';
        }
      } catch(e) {
        console.error('Error clearing PneuNet calibration:', e);
      }
    }

    async function updatePneunetCalibStatus() {
      try {
        const response = await fetch('/api/calibration/pneunet_status');
        const data = await response.json();
        if (data) {
          document.getElementById('pneunetCalibPoints').textContent = data.points || '0';
          
          // Update equation display based on fit method
          const fitMethod = data.fit_method || 0;
          
          if (fitMethod === 1 && data.sqrt_a !== undefined && data.sqrt_b !== undefined) {
            // Display SQRT fit equation
            const a = Number(data.sqrt_a);
            const b = Number(data.sqrt_b);
            const c = Number(data.sqrt_c || 0);
            const cStr = c >= 0 ? `+ ${c.toFixed(4)}` : `- ${Math.abs(c).toFixed(4)}`;
            document.getElementById('pneunetCalibEquation').textContent = `P = ${a.toFixed(6)}×√(ΔC-${b.toFixed(4)}) ${cStr}`;
          } else if (data.a !== undefined && data.b !== undefined) {
            // Display LINEAR fit equation
            const a = Number(data.a);
            const b = Number(data.b);
            document.getElementById('pneunetCalibEquation').textContent = `P = ${a.toFixed(6)}×ΔC + ${b.toFixed(4)}`;
          }
          
          if (data.in_progress) {
            drawPneunetCalibPlot(data);
          }
        }
      } catch(e) { }
    }

    function drawPneunetCalibPlot(statusData) {
      const svg = document.getElementById('pneunetPlot');
      svg.innerHTML = '';
      
      // Check if we have valid calibration data
      if (!statusData || (statusData.points === 0 && !statusData.a && !statusData.sqrt_a)) {
        const text = document.createElementNS('http://www.w3.org/2000/svg', 'text');
        text.setAttribute('x', '250');
        text.setAttribute('y', '150');
        text.setAttribute('text-anchor', 'middle');
        text.setAttribute('font-size', '14');
        text.setAttribute('fill', '#999');
        text.textContent = 'No calibration data yet';
        svg.appendChild(text);
        return;
      }
      
      // Determine which fit method is currently active
      const fitMethod = statusData.fit_method || 0;  // 0 = LINEAR, 1 = SQRT
      
      let a, b, c;
      if (fitMethod === 1 && statusData.sqrt_a !== undefined) {
        // Use SQRT fit
        a = statusData.sqrt_a || 0;
        b = statusData.sqrt_b || 0;
        c = statusData.sqrt_c || 0;
      } else {
        // Use LINEAR fit
        a = statusData.a || 0;
        b = statusData.b || 0;
        c = 0;
      }
      
      const margin = { top: 40, right: 40, bottom: 40, left: 50 };
      const width = 400 - margin.left - margin.right;
      const height = 280 - margin.top - margin.bottom;
      
      // Determine axis ranges
      let maxDc = 30;
      let maxP = 10;
      
      if (a !== 0 || b !== 0) {
        // Derive reasonable ranges from the fit curve
        maxDc = 30;
        let pAtMaxDc;
        if (fitMethod === 1) {
          // SQRT: P = a*sqrt(x-b) + c
          const sqrtTerm = Math.sqrt(Math.max(0, maxDc - b));
          pAtMaxDc = a * sqrtTerm + c;
        } else {
          // LINEAR: P = a*ΔC + b
          pAtMaxDc = a * maxDc + b;
        }
        maxP = Math.max(pAtMaxDc * 1.2, 5);
      }
      
      // Extend ranges for better visualization
      maxDc = Math.ceil(maxDc * 1.05);
      maxP = Math.ceil(maxP * 1.05);
      
      // Create group for chart
      const g = document.createElementNS('http://www.w3.org/2000/svg', 'g');
      g.setAttribute('transform', `translate(${margin.left}, ${margin.top})`);
      
      // Y axis
      const yAxis = document.createElementNS('http://www.w3.org/2000/svg', 'line');
      yAxis.setAttribute('x1', '0');
      yAxis.setAttribute('y1', '0');
      yAxis.setAttribute('x2', '0');
      yAxis.setAttribute('y2', height);
      yAxis.setAttribute('stroke', '#333');
      yAxis.setAttribute('stroke-width', '2');
      g.appendChild(yAxis);
      
      // X axis
      const xAxis = document.createElementNS('http://www.w3.org/2000/svg', 'line');
      xAxis.setAttribute('x1', '0');
      xAxis.setAttribute('y1', height);
      xAxis.setAttribute('x2', width);
      xAxis.setAttribute('y2', height);
      xAxis.setAttribute('stroke', '#333');
      xAxis.setAttribute('stroke-width', '2');
      g.appendChild(xAxis);
      
      // Y axis label
      const yLabel = document.createElementNS('http://www.w3.org/2000/svg', 'text');
      yLabel.setAttribute('x', '-' + height / 2);
      yLabel.setAttribute('y', '-35');
      yLabel.setAttribute('transform', 'rotate(-90)');
      yLabel.setAttribute('text-anchor', 'middle');
      yLabel.setAttribute('font-size', '12');
      yLabel.setAttribute('font-weight', 'bold');
      yLabel.setAttribute('fill', '#666');
      yLabel.textContent = 'P (kPa)';
      g.appendChild(yLabel);
      
      // X axis label
      const xLabel = document.createElementNS('http://www.w3.org/2000/svg', 'text');
      xLabel.setAttribute('x', width / 2);
      xLabel.setAttribute('y', height + 30);
      xLabel.setAttribute('text-anchor', 'middle');
      xLabel.setAttribute('font-size', '12');
      xLabel.setAttribute('font-weight', 'bold');
      xLabel.setAttribute('fill', '#666');
      xLabel.textContent = 'ΔC (pF)';
      g.appendChild(xLabel);
      
      // Y axis ticks and labels
      const yTicks = 5;
      for (let i = 0; i <= yTicks; i++) {
        const y = (height / yTicks) * i;
        const val = maxP * (1 - i / yTicks);
        const tick = document.createElementNS('http://www.w3.org/2000/svg', 'line');
        tick.setAttribute('x1', '-5');
        tick.setAttribute('y1', y);
        tick.setAttribute('x2', '0');
        tick.setAttribute('y2', y);
        tick.setAttribute('stroke', '#333');
        g.appendChild(tick);
        
        const label = document.createElementNS('http://www.w3.org/2000/svg', 'text');
        label.setAttribute('x', '-10');
        label.setAttribute('y', y + 4);
        label.setAttribute('text-anchor', 'end');
        label.setAttribute('font-size', '10');
        label.setAttribute('fill', '#666');
        label.textContent = val.toFixed(2);
        g.appendChild(label);
      }
      
      // X axis ticks and labels
      const xTickStep = Math.max(1, Math.floor(maxDc / 5));
      for (let i = 0; i <= maxDc; i += xTickStep) {
        const x = (i / (maxDc || 1)) * width;
        const tick = document.createElementNS('http://www.w3.org/2000/svg', 'line');
        tick.setAttribute('x1', x);
        tick.setAttribute('y1', height);
        tick.setAttribute('x2', x);
        tick.setAttribute('y2', height + 5);
        tick.setAttribute('stroke', '#333');
        g.appendChild(tick);
        
        const label = document.createElementNS('http://www.w3.org/2000/svg', 'text');
        label.setAttribute('x', x);
        label.setAttribute('y', height + 20);
        label.setAttribute('text-anchor', 'middle');
        label.setAttribute('font-size', '10');
        label.setAttribute('fill', '#666');
        label.textContent = i.toFixed(0);
        g.appendChild(label);
      }
      
      // Draw fitted curve (either LINEAR or SQRT)
      if (a !== 0 || b !== 0 || c !== 0) {
        let pathData = '';
        const steps = 100;
        for (let step = 0; step <= steps; step++) {
          const dc = (step / steps) * maxDc;
          let p_val;
          
          if (fitMethod === 1) {
            // SQRT: P = a*sqrt(x-b) + c
            const sqrtTerm = Math.sqrt(Math.max(0, dc - b));
            p_val = a * sqrtTerm + c;
          } else {
            // LINEAR: P = a*ΔC + b
            p_val = a * dc + b;
          }
          
          const x = (dc / (maxDc || 1)) * width;
          const y = height - (p_val / (maxP || 1)) * height;
          
          if (step === 0) {
            pathData += `M ${x} ${y}`;
          } else {
            pathData += ` L ${x} ${y}`;
          }
        }
        
        const path = document.createElementNS('http://www.w3.org/2000/svg', 'path');
        path.setAttribute('d', pathData);
        path.setAttribute('fill', 'none');
        path.setAttribute('stroke', '#0099cc');
        path.setAttribute('stroke-width', '2.5');
        g.appendChild(path);
      }
      
      // Draw live data point (from the streaming P and C data)
      if (window.lastP !== undefined && window.lastC !== undefined) {
        const liveDc = Math.max(0, window.lastC - (statusData.c0 || 0));
        const x = (liveDc / (maxDc || 1)) * width;
        const y = height - (window.lastP / (maxP || 1)) * height;
        
        // Draw point - green if grasping, red if not
        const pointColor = window.isCurrentlyGrasping ? '#00ff00' : '#ff0000';
        const point = document.createElementNS('http://www.w3.org/2000/svg', 'circle');
        point.setAttribute('cx', x);
        point.setAttribute('cy', y);
        point.setAttribute('r', '4');
        point.setAttribute('fill', pointColor);
        point.setAttribute('opacity', '0.8');
        g.appendChild(point);
      }
      
      svg.appendChild(g);
    }
    
    // Deformation calibration plot (same as pneunet plot but for modal)
    function drawDeformationCalibPlot(statusData) {
      const svg = document.getElementById('deformPlot');
      svg.innerHTML = '';
      
      // Check if we have valid calibration data
      if (!statusData || (statusData.points === 0 && !statusData.a && !statusData.sqrt_a)) {
        const text = document.createElementNS('http://www.w3.org/2000/svg', 'text');
        text.setAttribute('x', '250');
        text.setAttribute('y', '150');
        text.setAttribute('text-anchor', 'middle');
        text.setAttribute('font-size', '14');
        text.setAttribute('fill', '#999');
        text.textContent = 'No calibration data yet';
        svg.appendChild(text);
        return;
      }
      
      // Determine which fit method is currently active
      const fitMethod = statusData.fit_method || 0;  // 0 = LINEAR, 1 = SQRT
      
      let a, b, c;
      if (fitMethod === 1 && statusData.sqrt_a !== undefined) {
        // Use SQRT fit
        a = statusData.sqrt_a || 0;
        b = statusData.sqrt_b || 0;
        c = statusData.sqrt_c || 0;
      } else {
        // Use LINEAR fit
        a = statusData.a || 0;
        b = statusData.b || 0;
        c = 0;
      }
      
      const samples = statusData.samples || [];
      
      const margin = { top: 40, right: 40, bottom: 40, left: 50 };
      const width = 500 - margin.left - margin.right;
      const height = 300 - margin.top - margin.bottom;
      
      // Determine axis ranges
      let maxDc = 20;
      let maxP = 10;
      
      if (samples.length > 0) {
        const deltaCs = samples.map(p => p.dc);
        const pressures = samples.map(p => p.p);
        maxDc = Math.max(...deltaCs, 1);
        maxP = Math.max(...pressures, 1);
      } else if (a !== 0 || b !== 0) {
        // Derive reasonable ranges from the fit curve
        maxDc = 30;
        let pAtMaxDc;
        if (fitMethod === 1) {
          // SQRT: P = a*sqrt(x-b) + c
          const sqrtTerm = Math.sqrt(Math.max(0, maxDc - b));
          pAtMaxDc = a * sqrtTerm + c;
        } else {
          // LINEAR: P = a*ΔC + b
          pAtMaxDc = a * maxDc + b;
        }
        maxP = Math.max(pAtMaxDc * 1.2, 5);
      }
      
      // Extend ranges for better visualization (minimal padding)
      maxDc = Math.ceil(maxDc * 1.05);
      maxP = Math.ceil(maxP * 1.05);
      
      // Create group for chart
      const g = document.createElementNS('http://www.w3.org/2000/svg', 'g');
      g.setAttribute('transform', `translate(${margin.left}, ${margin.top})`);
      
      // Y axis
      const yAxis = document.createElementNS('http://www.w3.org/2000/svg', 'line');
      yAxis.setAttribute('x1', '0');
      yAxis.setAttribute('y1', '0');
      yAxis.setAttribute('x2', '0');
      yAxis.setAttribute('y2', height);
      yAxis.setAttribute('stroke', '#333');
      yAxis.setAttribute('stroke-width', '2');
      g.appendChild(yAxis);
      
      // X axis
      const xAxis = document.createElementNS('http://www.w3.org/2000/svg', 'line');
      xAxis.setAttribute('x1', '0');
      xAxis.setAttribute('y1', height);
      xAxis.setAttribute('x2', width);
      xAxis.setAttribute('y2', height);
      xAxis.setAttribute('stroke', '#333');
      xAxis.setAttribute('stroke-width', '2');
      g.appendChild(xAxis);
      
      // Y axis label
      const yLabel = document.createElementNS('http://www.w3.org/2000/svg', 'text');
      yLabel.setAttribute('x', '-' + height / 2);
      yLabel.setAttribute('y', '-35');
      yLabel.setAttribute('transform', 'rotate(-90)');
      yLabel.setAttribute('text-anchor', 'middle');
      yLabel.setAttribute('font-size', '12');
      yLabel.setAttribute('font-weight', 'bold');
      yLabel.setAttribute('fill', '#666');
      yLabel.textContent = 'P (kPa)';
      g.appendChild(yLabel);
      
      // X axis label
      const xLabel = document.createElementNS('http://www.w3.org/2000/svg', 'text');
      xLabel.setAttribute('x', width / 2);
      xLabel.setAttribute('y', height + 30);
      xLabel.setAttribute('text-anchor', 'middle');
      xLabel.setAttribute('font-size', '12');
      xLabel.setAttribute('font-weight', 'bold');
      xLabel.setAttribute('fill', '#666');
      xLabel.textContent = 'ΔC (pF)';
      g.appendChild(xLabel);
      
      // Y axis ticks and labels
      const yTicks = 5;
      for (let i = 0; i <= yTicks; i++) {
        const y = (height / yTicks) * i;
        const val = maxP * (1 - i / yTicks);
        const tick = document.createElementNS('http://www.w3.org/2000/svg', 'line');
        tick.setAttribute('x1', '-5');
        tick.setAttribute('y1', y);
        tick.setAttribute('x2', '0');
        tick.setAttribute('y2', y);
        tick.setAttribute('stroke', '#333');
        g.appendChild(tick);
        
        const label = document.createElementNS('http://www.w3.org/2000/svg', 'text');
        label.setAttribute('x', '-10');
        label.setAttribute('y', y + 4);
        label.setAttribute('text-anchor', 'end');
        label.setAttribute('font-size', '10');
        label.setAttribute('fill', '#666');
        label.textContent = val.toFixed(2);
        g.appendChild(label);
      }
      
      // X axis ticks and labels
      const xTickStep = Math.max(1, Math.floor(maxDc / 5));
      for (let i = 0; i <= maxDc; i += xTickStep) {
        const x = (i / (maxDc || 1)) * width;
        const tick = document.createElementNS('http://www.w3.org/2000/svg', 'line');
        tick.setAttribute('x1', x);
        tick.setAttribute('y1', height);
        tick.setAttribute('x2', x);
        tick.setAttribute('y2', height + 5);
        tick.setAttribute('stroke', '#333');
        g.appendChild(tick);
        
        const label = document.createElementNS('http://www.w3.org/2000/svg', 'text');
        label.setAttribute('x', x);
        label.setAttribute('y', height + 20);
        label.setAttribute('text-anchor', 'middle');
        label.setAttribute('font-size', '10');
        label.setAttribute('fill', '#666');
        label.textContent = i.toFixed(0);
        g.appendChild(label);
      }
      
      // Draw data points in orange
      for (let i = 0; i < samples.length; i++) {
        const x = (samples[i].dc / (maxDc || 1)) * width;
        const y = height - (samples[i].p / (maxP || 1)) * height;
        
        const circle = document.createElementNS('http://www.w3.org/2000/svg', 'circle');
        circle.setAttribute('cx', x);
        circle.setAttribute('cy', y);
        circle.setAttribute('r', '2');
        circle.setAttribute('fill', '#ff9933');
        circle.setAttribute('opacity', '0.7');
        g.appendChild(circle);
      }
      
      // Draw fitted curve (either LINEAR or SQRT)
      if (a !== 0 || b !== 0 || c !== 0) {
        let pathData = '';
        const steps = 100;
        for (let step = 0; step <= steps; step++) {
          const dc = (step / steps) * maxDc;
          let p_val;
          
          if (fitMethod === 1) {
            // SQRT: P = a*sqrt(x-b) + c
            const sqrtTerm = Math.sqrt(Math.max(0, dc - b));
            p_val = a * sqrtTerm + c;
          } else {
            // LINEAR: P = a*ΔC + b
            p_val = a * dc + b;
          }
          
          const x = (dc / (maxDc || 1)) * width;
          const y = height - (p_val / (maxP || 1)) * height;
          
          if (step === 0) {
            pathData += `M ${x} ${y}`;
          } else {
            pathData += ` L ${x} ${y}`;
          }
        }
        
        const path = document.createElementNS('http://www.w3.org/2000/svg', 'path');
        path.setAttribute('d', pathData);
        path.setAttribute('fill', 'none');
        path.setAttribute('stroke', '#0099cc');
        path.setAttribute('stroke-width', '2.5');
        g.appendChild(path);
      }
      svg.appendChild(g);
    }
    
    function startCalibration() {
      calibrationStep = 0;
      document.getElementById('calibrationNotRunning').style.display = 'none';
      document.getElementById('calibrationSteps').style.display = 'block';
      updateCalibrationStep();
    }
    
    function updateCalibrationStep() {
      const steps = [
        { step: 0, desc: 'Leave pin floating (NO capacitor connected)', action: 'Connect nothing and click Ready when ready.' },
        { step: 1, desc: 'Connect 82pF capacitor', action: 'Connect an 82pF capacitor between pin and GND, then click Ready.' },
        { step: 2, desc: 'Connect 101pF capacitor', action: 'Replace with 101pF capacitor between pin and GND, then click Ready.' }
      ];
      
      const current = steps[calibrationStep];
      document.getElementById('currentStep').textContent = calibrationStep + 1;
      document.getElementById('stepDescription').textContent = current.desc;
      document.getElementById('calibrationMessage').textContent = current.action;
    }
    
    async function nextCalibrationStep() {
      document.getElementById('nextStepBtn').disabled = true;
      document.getElementById('nextStepBtn').textContent = 'Taking measurement...';
      
      try {
        const response = await fetch('/api/calibration/step?step=' + calibrationStep);
        const result = await response.json();
        
        if (result.success) {
          document.getElementById('calibrationMessage').textContent = 'Step ' + (calibrationStep + 1) + ' completed. Raw value: ' + result.raw_value;
          calibrationStep++;
          
          if (calibrationStep >= maxCalibrationSteps) {
            // Calibration complete
            document.getElementById('calibrationMessage').innerHTML = '<strong style="color: #00aa00;">✓ Calibration completed successfully!</strong><br>Calibration values have been saved.';
            document.getElementById('nextStepBtn').style.display = 'none';
            document.getElementById('cancelCalBtn').textContent = 'Close';
            await loadCalibrationData();
          } else {
            updateCalibrationStep();
            document.getElementById('nextStepBtn').disabled = false;
            document.getElementById('nextStepBtn').textContent = 'Ready - Next Step';
          }
        } else {
          document.getElementById('calibrationMessage').innerHTML = '<strong style="color: #cc3300;">✗ Error: ' + result.error + '</strong>';
          document.getElementById('nextStepBtn').disabled = false;
          document.getElementById('nextStepBtn').textContent = 'Ready - Next Step';
        }
      } catch(e) {
        document.getElementById('calibrationMessage').innerHTML = '<strong style="color: #cc3300;">✗ Error: ' + e.message + '</strong>';
        document.getElementById('nextStepBtn').disabled = false;
        document.getElementById('nextStepBtn').textContent = 'Ready - Next Step';
      }
    }
    
    function cancelCalibration() {
      document.getElementById('calibrationSteps').style.display = 'none';
      document.getElementById('calibrationNotRunning').style.display = 'block';
      document.getElementById('nextStepBtn').style.display = 'block';
      document.getElementById('nextStepBtn').disabled = false;
      document.getElementById('nextStepBtn').textContent = 'Ready - Next Step';
      document.getElementById('cancelCalBtn').textContent = 'Cancel';
      calibrationStep = 0;
    }
    
    async function clearCalibration() {
      if (confirm('Clear all calibration data? You will need to recalibrate.')) {
        try {
          await fetch('/api/calibration/clear');
          document.getElementById('calibrationMessage').innerHTML = '<strong style="color: #00aa00;">✓ Calibration cleared.</strong>';
          await loadCalibrationData();
        } catch(e) {
          console.error('Error clearing calibration:', e);
        }
      }
    }
    
    window.onclick = function(event) {
      const fileModal = document.getElementById('fileListModal');
      const calModal = document.getElementById('calibrationModal');
      
      if (event.target == fileModal) {
        fileModal.style.display = 'none';
      }
      if (event.target == calModal) {
        calModal.style.display = 'none';
      }
    }
    
    // Draw SVG chart
    function drawChart() {
      const svg = document.getElementById('pressureChart');
      svg.innerHTML = ''; // Clear previous chart
      
      if (allData.length === 0) {
        // Show empty message
        const text = document.createElementNS('http://www.w3.org/2000/svg', 'text');
        text.setAttribute('x', '500');
        text.setAttribute('y', '175');
        text.setAttribute('text-anchor', 'middle');
        text.setAttribute('font-size', '16');
        text.setAttribute('fill', '#999');
        text.textContent = 'No data yet. Click "Start" to begin collecting.';
        svg.appendChild(text);
        return;
      }
      
      const margin = { top: 20, right: 80, bottom: 40, left: 60 };
      const width = 1000 - margin.left - margin.right;
      const height = 350 - margin.top - margin.bottom;
      
      // Find min/max for pressure
      const pressures = allData.map(m => m.p);
      const minP = Math.min(...pressures);
      const maxP = Math.max(...pressures);
      const paddingP = (maxP - minP) * 0.1 || 1;
      const yMinP = Math.max(0, minP - paddingP);
      const yMaxP = maxP + paddingP;
      
      // Find min/max for capacitance
      const capacitances = allData.map(m => m.c || 0);
      const minC = Math.min(...capacitances);
      const maxC = Math.max(...capacitances);
      const paddingC = (maxC - minC) * 0.1 || 1;
      const yMinC = Math.max(0, minC - paddingC);
      const yMaxC = maxC + paddingC;
      
      // Create group for chart
      const g = document.createElementNS('http://www.w3.org/2000/svg', 'g');
      g.setAttribute('transform', `translate(${margin.left}, ${margin.top})`);
      
      // Y axis (left - pressure)
      const yAxis = document.createElementNS('http://www.w3.org/2000/svg', 'line');
      yAxis.setAttribute('x1', '0');
      yAxis.setAttribute('y1', '0');
      yAxis.setAttribute('x2', '0');
      yAxis.setAttribute('y2', height);
      yAxis.setAttribute('stroke', '#333');
      yAxis.setAttribute('stroke-width', '2');
      g.appendChild(yAxis);
      
      // Y axis (right - capacitance)
      const yAxisRight = document.createElementNS('http://www.w3.org/2000/svg', 'line');
      yAxisRight.setAttribute('x1', width);
      yAxisRight.setAttribute('y1', '0');
      yAxisRight.setAttribute('x2', width);
      yAxisRight.setAttribute('y2', height);
      yAxisRight.setAttribute('stroke', '#ff9933');
      yAxisRight.setAttribute('stroke-width', '2');
      g.appendChild(yAxisRight);
      
      // X axis
      const xAxis = document.createElementNS('http://www.w3.org/2000/svg', 'line');
      xAxis.setAttribute('x1', '0');
      xAxis.setAttribute('y1', height);
      xAxis.setAttribute('x2', width);
      xAxis.setAttribute('y2', height);
      xAxis.setAttribute('stroke', '#333');
      xAxis.setAttribute('stroke-width', '2');
      g.appendChild(xAxis);
      
      // Y axis label (left - pressure)
      const yLabel = document.createElementNS('http://www.w3.org/2000/svg', 'text');
      yLabel.setAttribute('x', '-' + height / 2);
      yLabel.setAttribute('y', '-35');
      yLabel.setAttribute('transform', 'rotate(-90)');
      yLabel.setAttribute('text-anchor', 'middle');
      yLabel.setAttribute('font-size', '12');
      yLabel.setAttribute('font-weight', 'bold');
      yLabel.setAttribute('fill', '#0066cc');
      yLabel.textContent = 'P (kPa)';
      g.appendChild(yLabel);
      
      // Y axis label (right - capacitance)
      const yLabelRight = document.createElementNS('http://www.w3.org/2000/svg', 'text');
      yLabelRight.setAttribute('x', height / 2);
      yLabelRight.setAttribute('y', width + 35);
      yLabelRight.setAttribute('transform', 'rotate(90)');
      yLabelRight.setAttribute('text-anchor', 'middle');
      yLabelRight.setAttribute('font-size', '12');
      yLabelRight.setAttribute('font-weight', 'bold');
      yLabelRight.setAttribute('fill', '#ff9933');
      yLabelRight.textContent = 'C (pF)';
      g.appendChild(yLabelRight);
      
      // X axis label
      const xLabel = document.createElementNS('http://www.w3.org/2000/svg', 'text');
      xLabel.setAttribute('x', width / 2);
      xLabel.setAttribute('y', height + 35);
      xLabel.setAttribute('text-anchor', 'middle');
      xLabel.setAttribute('font-size', '12');
      xLabel.setAttribute('fill', '#666');
      xLabel.setAttribute('font-weight', 'bold');
      xLabel.textContent = 'Time (s)';
      g.appendChild(xLabel);
      
      // Y axis ticks and labels (left - pressure)
      const yTicks = 5;
      for (let i = 0; i <= yTicks; i++) {
        const y = (height / yTicks) * i;
        const val = yMaxP - (yMaxP - yMinP) * (i / yTicks);
        
        const tick = document.createElementNS('http://www.w3.org/2000/svg', 'line');
        tick.setAttribute('x1', '-5');
        tick.setAttribute('y1', y);
        tick.setAttribute('x2', '0');
        tick.setAttribute('y2', y);
        tick.setAttribute('stroke', '#0066cc');
        g.appendChild(tick);
        
        const label = document.createElementNS('http://www.w3.org/2000/svg', 'text');
        label.setAttribute('x', '-10');
        label.setAttribute('y', y + 4);
        label.setAttribute('text-anchor', 'end');
        label.setAttribute('font-size', '10');
        label.setAttribute('fill', '#0066cc');
        label.textContent = val.toFixed(1);
        g.appendChild(label);
      }
      
      // Y axis ticks and labels (right - capacitance)
      for (let i = 0; i <= yTicks; i++) {
        const y = (height / yTicks) * i;
        const val = yMaxC - (yMaxC - yMinC) * (i / yTicks);
        
        const tick = document.createElementNS('http://www.w3.org/2000/svg', 'line');
        tick.setAttribute('x1', width);
        tick.setAttribute('y1', y);
        tick.setAttribute('x2', width + 5);
        tick.setAttribute('y2', y);
        tick.setAttribute('stroke', '#ff9933');
        g.appendChild(tick);
        
        const label = document.createElementNS('http://www.w3.org/2000/svg', 'text');
        label.setAttribute('x', width + 10);
        label.setAttribute('y', y + 4);
        label.setAttribute('text-anchor', 'start');
        label.setAttribute('font-size', '10');
        label.setAttribute('fill', '#ff9933');
        label.textContent = val.toFixed(1);
        g.appendChild(label);
      }
      
      // X axis ticks
      const xTicks = Math.min(10, Math.floor(allData.length / 5));
      const tickInterval = Math.max(1, Math.floor(allData.length / xTicks));
      for (let i = 0; i < allData.length; i += tickInterval) {
        const x = (width / (allData.length - 1 || 1)) * i;
        const relativeTime = (allData[i].t / 1000);
        
        const tick = document.createElementNS('http://www.w3.org/2000/svg', 'line');
        tick.setAttribute('x1', x);
        tick.setAttribute('y1', height);
        tick.setAttribute('x2', x);
        tick.setAttribute('y2', height + 5);
        tick.setAttribute('stroke', '#333');
        g.appendChild(tick);
        
        const label = document.createElementNS('http://www.w3.org/2000/svg', 'text');
        label.setAttribute('x', x);
        label.setAttribute('y', height + 20);
        label.setAttribute('text-anchor', 'middle');
        label.setAttribute('font-size', '10');
        label.setAttribute('fill', '#666');
        label.textContent = relativeTime.toFixed(1);
        g.appendChild(label);
      }
      
      // Draw pressure line
      const yScaleP = height / (yMaxP - yMinP || 1);
      const xScale = width / (allData.length - 1 || 1);
      
      let pressurePath = '';
      for (let i = 0; i < allData.length; i++) {
        const x = i * xScale;
        const y = height - (allData[i].p - yMinP) * yScaleP;
        pressurePath += (i === 0 ? 'M' : 'L') + x + ',' + y;
      }
      
      const path = document.createElementNS('http://www.w3.org/2000/svg', 'path');
      path.setAttribute('d', pressurePath);
      path.setAttribute('stroke', '#0066cc');
      path.setAttribute('stroke-width', '2');
      path.setAttribute('fill', 'none');
      g.appendChild(path);
      
      // Draw capacitance line
      const yScaleC = height / (yMaxC - yMinC || 1);
      
      let capacitancePath = '';
      for (let i = 0; i < allData.length; i++) {
        const x = i * xScale;
        const y = height - ((allData[i].c || 0) - yMinC) * yScaleC;
        capacitancePath += (i === 0 ? 'M' : 'L') + x + ',' + y;
      }
      
      const pathC = document.createElementNS('http://www.w3.org/2000/svg', 'path');
      pathC.setAttribute('d', capacitancePath);
      pathC.setAttribute('stroke', '#ff9933');
      pathC.setAttribute('stroke-width', '2');
      pathC.setAttribute('fill', 'none');
      g.appendChild(pathC);
      
      // Add legend
      const legendP = document.createElementNS('http://www.w3.org/2000/svg', 'circle');
      legendP.setAttribute('cx', width - 80);
      legendP.setAttribute('cy', '4');
      legendP.setAttribute('r', '3');
      legendP.setAttribute('fill', '#0066cc');
      g.appendChild(legendP);
      
      const legendPLabel = document.createElementNS('http://www.w3.org/2000/svg', 'text');
      legendPLabel.setAttribute('x', width - 70);
      legendPLabel.setAttribute('y', '8');
      legendPLabel.setAttribute('font-size', '11');
      legendPLabel.setAttribute('fill', '#0066cc');
      legendPLabel.textContent = 'Pressure';
      g.appendChild(legendPLabel);
      
      const legendC = document.createElementNS('http://www.w3.org/2000/svg', 'circle');
      legendC.setAttribute('cx', width - 80);
      legendC.setAttribute('cy', '19');
      legendC.setAttribute('r', '3');
      legendC.setAttribute('fill', '#ff9933');
      g.appendChild(legendC);
      
      const legendCLabel = document.createElementNS('http://www.w3.org/2000/svg', 'text');
      legendCLabel.setAttribute('x', width - 70);
      legendCLabel.setAttribute('y', '23');
      legendCLabel.setAttribute('font-size', '11');
      legendCLabel.setAttribute('fill', '#ff9933');
      legendCLabel.textContent = 'Capacitance';
      g.appendChild(legendCLabel);
      
      svg.appendChild(g);
    }
    
    // Load data from server
    async function loadData() {
      try {
        const response = await fetch('/api/data');
        if (!response.ok) return;
        const data = await response.json();
        
        // Accumulate data point if we have valid readings
        if (data && data.t !== undefined && data.p !== undefined) {
          // Check if this is a new data point (not duplicate)
          if (allData.length === 0 || data.t !== allData[allData.length - 1].t) {
            allData.push({t: data.t, p: data.p, c: data.c || 0, g: data.g || false});
            
            // Keep only recent data (last 500 points to avoid memory issues)
            if (allData.length > 500) {
              allData.shift();
            }
          }
        }
        
        drawChart();
        updateStats();
        // data may include grasping flag 'g' when collecting
        if (data && data.g !== undefined) {
          isGrasping = data.g === true;
        } else {
          // when there's no new data (not collecting), clear grasping
          isGrasping = false;
        }
        updateStatus();
        updateDeformation();  // Update deformation and Smart PneuNet features
        drawPneunetCurve();  // Update P-ΔC characteristic curve
      } catch(e) { console.error('Error loading data:', e); }
    }
    
    // Update statistics
    function updateStats() {
      const pressures = allData.map(m => m.p);
      const capacitances = allData.map(m => m.c || 0);
      
      // Pressure stats
      document.getElementById('currentPressure').textContent = allData.length > 0 ? allData[allData.length-1].p.toFixed(2) : '--';
      document.getElementById('maxPressure').textContent = pressures.length > 0 ? Math.max(...pressures).toFixed(2) : '--';
      document.getElementById('minPressure').textContent = pressures.length > 0 ? Math.min(...pressures).toFixed(2) : '--';
      document.getElementById('avgPressure').textContent = pressures.length > 0 ? (pressures.reduce((a,b)=>a+b)/pressures.length).toFixed(2) : '--';
      
      // Capacitance stats
      document.getElementById('currentCapacitance').textContent = allData.length > 0 ? allData[allData.length-1].c.toFixed(2) : '--';
      document.getElementById('maxCapacitance').textContent = capacitances.length > 0 ? Math.max(...capacitances).toFixed(2) : '--';
      document.getElementById('minCapacitance').textContent = capacitances.length > 0 ? Math.min(...capacitances).toFixed(2) : '--';
      document.getElementById('avgCapacitance').textContent = capacitances.length > 0 ? (capacitances.reduce((a,b)=>a+b)/capacitances.length).toFixed(2) : '--';
      
      // Data point counts - count readings with valid data
      const pressureCount = allData.filter(m => m.p !== undefined && m.p !== null).length;
      const capacitanceCount = allData.filter(m => m.c !== undefined && m.c !== null).length;
      document.getElementById('countPressure').textContent = pressureCount;
      document.getElementById('countCapacitance').textContent = capacitanceCount;
    }
    
    // Update status indicator
    function updateStatus() {
      const indicator = document.getElementById('statusIndicator');
      const text = document.getElementById('statusText');
      const btn = document.getElementById('playPauseBtn');
      const input = document.getElementById('filenameInput');
      const resetBtn = document.getElementById('resetBtn');

      // Highest priority: grasping
      if (isGrasping) {
        indicator.style.background = '#00aa00';
        indicator.style.boxShadow = '0 0 10px rgba(0, 170, 0, 0.8)';
        text.textContent = 'Grasping';
        text.style.color = '#00aa00';
      } else {
        // Clear grasping visual if present
        indicator.style.boxShadow = 'none';
        text.style.color = '';

        if (isCollecting) {
          indicator.style.background = '#9933ff';
          text.textContent = 'Collecting...';
          btn.textContent = '⏸ Pause';
          btn.classList.add('active');
          input.disabled = true;  // Disable filename input while collecting
          resetBtn.disabled = true;  // Disable reset while collecting
        } else {
          // When paused or stopped
          indicator.style.background = '#0066cc';
          text.textContent = 'WiFi Active (Paused)';
          btn.textContent = '▶ Start';
          btn.classList.remove('active');
          resetBtn.disabled = false;  // Allow reset while paused

          // Enable input only if no data collected yet in this session
          if (hasDataBeenCollected) {
            input.disabled = true;
          } else {
            input.disabled = false;  // Enable after reset
          }
        }
      }

      // Always update filename display from server
      updateFilenameDisplay();
    }
    
    function updateFilenameDisplay() {
      fetch('/api/getcurrentfilename')
        .then(res => res.json())
        .then(data => {
          const filenameDisplay = document.getElementById('currentFilename');
          if (data.filename && data.filename.length > 0) {
            // Strip leading / for display
            const displayName = data.filename.startsWith('/') ? data.filename.substring(1) : data.filename;
            filenameDisplay.textContent = displayName;
            currentFilename = displayName;
          } else {
            filenameDisplay.textContent = 'Waiting for collection...';
          }
        })
        .catch(e => console.error('Error fetching current filename:', e));
    }
    
    // ============ OBJECT SIZE CALIBRATION FUNCTIONS ============
    let objectSizeCalibInterval = null;
    let objectSizeCalibData = [];
    
    async function startObjectSizeCalibration() {
      try {
        const response = await fetch('/api/calibration/objectsize_start');
        const data = await response.json();
        if (data && data.success) {
          document.getElementById('objectsizeCalibNotRunning').style.display = 'none';
          document.getElementById('objectsizeCalibRunning').style.display = 'block';
          document.getElementById('objectsizeCalibStatus').textContent = 'In progress';
          objectSizeCalibData = [];
          objectSizeCalibInterval = setInterval(updateObjectSizeCalibStatus, 500);
        } else {
          alert('Failed to start object size calibration (C0 may not be set)');
        }
      } catch(e) {
        console.error('Error starting object size calibration:', e);
      }
    }
    
    async function finishObjectSizeCalibPoint() {
      try {
        const response = await fetch('/api/calibration/objectsize_finish_point');
        const data = await response.json();
        if (data && data.point_complete) {
          if (data.all_complete) {
            // Calibration complete
            document.getElementById('objectsizeCalibStatus').textContent = 'Complete';
            document.getElementById('objectsizeCalibNotRunning').style.display = 'block';
            document.getElementById('objectsizeCalibRunning').style.display = 'none';
            if (objectSizeCalibInterval) clearInterval(objectSizeCalibInterval);
            drawObjectSizeCalibPlot();
          } else {
            // Move to next point
            await updateObjectSizeCalibStatus();
          }
        }
      } catch(e) {
        console.error('Error finishing object size calibration point:', e);
      }
    }
    
    async function abortObjectSizeCalibration() {
      try {
        const response = await fetch('/api/calibration/objectsize_abort');
        const data = await response.json();
        if (data && data.success) {
          document.getElementById('objectsizeCalibStatus').textContent = 'Aborted';
          document.getElementById('objectsizeCalibNotRunning').style.display = 'block';
          document.getElementById('objectsizeCalibRunning').style.display = 'none';
          if (objectSizeCalibInterval) clearInterval(objectSizeCalibInterval);
          objectSizeCalibData = [];
          document.getElementById('objectsizePlot').innerHTML = '';
        }
      } catch(e) {
        console.error('Error aborting object size calibration:', e);
      }
    }
    
    async function clearObjectSizeCalibration() {
      if (!confirm('Clear object size calibration data?')) return;
      if (objectSizeCalibInterval) clearInterval(objectSizeCalibInterval);
      try {
        const response = await fetch('/api/calibration/objectsize_clear');
        const data = await response.json();
        if (data && data.success) {
          document.getElementById('objectsizeCalibStatus').textContent = 'Not started';
          document.getElementById('objectsizeCalibProgress').textContent = '0/6';
          document.getElementById('objectsizeCalibNotRunning').style.display = 'block';
          document.getElementById('objectsizeCalibRunning').style.display = 'none';
          objectSizeCalibData = [];
          document.getElementById('objectsizePlot').innerHTML = '';
        }
      } catch(e) {
        console.error('Error clearing object size calibration:', e);
      }
    }
    
    async function updateObjectSizeCalibStatus() {
      try {
        const response = await fetch('/api/calibration/objectsize_status');
        const data = await response.json();
        if (data) {
          const progress = data.current_index + '/' + data.total_distances;
          document.getElementById('objectsizeCalibProgress').textContent = progress;
          
          if (data.in_progress && data.current_distance_mm) {
            document.getElementById('objectsizeCalibDistance').textContent = data.current_distance_mm;
            document.getElementById('objectsizeCalibCurrentSize').textContent = data.current_distance_mm + ' mm';
            document.getElementById('objectsizeCalibMessage').textContent = 
              'Samples collected: ' + data.sample_count + ' / 50';
          }
          
          if (data.saved_count > 0) {
            drawObjectSizeCalibPlot();
          }
        }
      } catch(e) { }
    }
    
    function drawObjectSizeCalibPlot() {
      const svg = document.getElementById('objectsizePlot');
      if (!svg) return;
      
      // TODO: Fetch calibration data and draw plot of object size vs delta C
      // For now, just show a placeholder message
      svg.innerHTML = '';
      const text = document.createElementNS('http://www.w3.org/2000/svg', 'text');
      text.setAttribute('x', '250');
      text.setAttribute('y', '150');
      text.setAttribute('text-anchor', 'middle');
      text.setAttribute('font-size', '14');
      text.setAttribute('fill', '#999');
      text.textContent = 'Calibration plot will appear here';
      svg.appendChild(text);
    }
    
    let objectSizeCalibModalInterval = null;
    let objectSizeContactDetectionInterval = null;
    let objectSizeContactStartTime = null;
    let objectSizeWasInContact = false;
    let objectSizeLastReleaseTime = null;
    let objectSizeRequiresRelease = false;         // Flag: must fully release before next contact allowed
    let objectSizeReleaseStartTime = null;         // Timestamp when release phase started
    const OBJECT_SIZE_CONTACT_REQUIRED_MS = 500;    // Must hold for set time to register contact
    const OBJECT_SIZE_RELEASE_REQUIRED_MS = 2000;    // Must release for set time before new contact

    async function startObjectSizeCalibrationModal() {
      try {
        // Start data collection first
        await fetch('/api/start');
        const response = await fetch('/api/calibration/objectsize_start');
        const data = await response.json();
        if (data && data.success) {
          document.getElementById('objectSizeCalibNotRunning').style.display = 'none';
          document.getElementById('objectSizeCalibRunning').style.display = 'flex';
          document.getElementById('objectSizeCalibCompleted').style.display = 'none';
          document.getElementById('objectSizeCalibInstructions').style.display = 'block';
          objectSizeCalibModalInterval = setInterval(updateObjectSizeCalibStatusModal, 500);
          objectSizeContactDetectionInterval = setInterval(checkObjectSizeContact, 100);
          objectSizeContactStartTime = null;
          objectSizeLastReleaseTime = null;
          objectSizeWasInContact = false;
          objectSizeRequiresRelease = false;
          objectSizeReleaseStartTime = null;
          await updateObjectSizeCalibStatusModal();
        } else {
          // Stop data collection if start failed
          await fetch('/api/stop');
        }
      } catch(e) {
        console.error('Error starting object size calibration:', e);
      }
    }
    
    async function checkObjectSizeContact() {
      try {
        if (isGrasping) {
          // User is currently grasping
          
          // If we require a full release cycle before next contact, ignore this grasp
          if (objectSizeRequiresRelease) {
            // Reset release timer since user grasped again - this breaks the release sequence
            objectSizeReleaseStartTime = null;
            return;  // Ignore this contact, must release first
          }
          
          // Start or continue tracking current contact
          if (objectSizeContactStartTime === null) {
            objectSizeContactStartTime = Date.now();
            objectSizeWasInContact = true;
          }
          
          // Check if contact has been sustained for required duration
          const contactDuration = Date.now() - objectSizeContactStartTime;
          if (contactDuration >= OBJECT_SIZE_CONTACT_REQUIRED_MS) {
            // Contact detected for sufficient time, record this point
            await finishObjectSizeCalibPoint();
            // Force a complete release cycle before next contact is allowed
            objectSizeContactStartTime = null;
            objectSizeWasInContact = false;
            objectSizeRequiresRelease = true;         // Block new contacts
            objectSizeReleaseStartTime = null;        // Reset release timer
          }
          
        } else {
          // User is NOT grasping (isGrasping is false)
          
          if (objectSizeRequiresRelease) {
            // We're in "requires release" state and user is now not grasping
            if (objectSizeReleaseStartTime === null) {
              // Just detected the release - start the release timer
              objectSizeReleaseStartTime = Date.now();
            } else {
              // Already releasing - check if we've been released long enough
              const releaseElapsed = Date.now() - objectSizeReleaseStartTime;
              if (releaseElapsed >= OBJECT_SIZE_RELEASE_REQUIRED_MS) {
                // Sufficient release time has passed, allow new contacts
                objectSizeRequiresRelease = false;
                objectSizeReleaseStartTime = null;
                objectSizeLastReleaseTime = Date.now();
              }
            }
          } else {
            // Not in release-required state, just ensure contact tracking is reset
            objectSizeContactStartTime = null;
            objectSizeWasInContact = false;
            objectSizeReleaseStartTime = null;
          }
        }
      } catch(e) {
        console.error('Error checking contact:', e);
      }
    }
    
    async function finishObjectSizeCalibPoint() {
      try {
        const response = await fetch('/api/calibration/objectsize_finish_point');
        const data = await response.json();
        if (data && data.point_complete) {
          objectSizeContactStartTime = null;
          if (data.all_complete) {
            // Calibration complete
            await finishObjectSizeCalibrationModal();
          } else {
            // Move to next point automatically
            await updateObjectSizeCalibStatusModal();
          }
        }
      } catch(e) {
        console.error('Error finishing object size calibration point:', e);
      }
    }
    
    async function finishObjectSizeCalibrationModal() {
      try {
        // Stop data collection
        await fetch('/api/stop');
        
        if (objectSizeCalibModalInterval) clearInterval(objectSizeCalibModalInterval);
        if (objectSizeContactDetectionInterval) clearInterval(objectSizeContactDetectionInterval);
        
        document.getElementById('objectSizeCalibNotRunning').style.display = 'flex';
        document.getElementById('objectSizeCalibRunning').style.display = 'none';
        document.getElementById('objectSizeCalibInstructions').style.display = 'none';
        document.getElementById('objectSizeCalibCompleted').style.display = 'block';
        
        // Update the completed info and plot
        await updateObjectSizeCalibStatusModal();
      } catch(e) {
        console.error('Error finalizing object size calibration:', e);
      }
    }
    
    async function abortObjectSizeCalibrationModal() {
      try {
        // Stop data collection
        await fetch('/api/stop');
        const response = await fetch('/api/calibration/objectsize_abort');
        const data = await response.json();
        if (data && data.success) {
          if (objectSizeCalibModalInterval) clearInterval(objectSizeCalibModalInterval);
          if (objectSizeContactDetectionInterval) clearInterval(objectSizeContactDetectionInterval);
          
          document.getElementById('objectSizeCalibNotRunning').style.display = 'flex';
          document.getElementById('objectSizeCalibRunning').style.display = 'none';
          document.getElementById('objectSizeCalibInstructions').style.display = 'none';
          document.getElementById('objectSizeCalibCompleted').style.display = 'none';
          document.getElementById('objectSizeCalibPlotModal').innerHTML = '';
        }
      } catch(e) {
        console.error('Error aborting object size calibration:', e);
      }
    }
    
    async function clearObjectSizeCalibrationModal() {
      if (!confirm('Clear object size calibration data?')) return;
      if (objectSizeCalibModalInterval) clearInterval(objectSizeCalibModalInterval);
      try {
        const response = await fetch('/api/calibration/objectsize_clear');
        const data = await response.json();
        if (data && data.success) {
          document.getElementById('objectSizeCalibModalStatus').textContent = 'Not started';
          document.getElementById('objectSizeCalibModalProgress').textContent = '0/6';
          document.getElementById('objectSizeCalibNotRunning').style.display = 'flex';
          document.getElementById('objectSizeCalibRunning').style.display = 'none';
          document.getElementById('objectSizeCalibInstructions').style.display = 'none';
          document.getElementById('objectSizeCalibPlotModal').innerHTML = '';
        }
      } catch(e) {
        console.error('Error clearing object size calibration:', e);
      }
    }
    
    async function updateObjectSizeCalibStatusModal() {
      try {
        const response = await fetch('/api/calibration/objectsize_status');
        const data = await response.json();
        if (data) {
          if (data.in_progress) {
            // During calibration: show instructions and progress
            const progress = data.current_index + '/' + data.total_distances;
            document.getElementById('objectSizeCalibProgressDuring').textContent = progress;
            document.getElementById('objectSizeCalibInstructions').style.display = 'block';
            document.getElementById('objectSizeCalibCompleted').style.display = 'none';
            
            if (data.current_distance_mm) {
              document.getElementById('objectSizeCalibInstructionSize').textContent = data.current_distance_mm;
            }
          } else {
            // Not in progress: hide instructions, show completed info
            document.getElementById('objectSizeCalibInstructions').style.display = 'none';
            
            if (data.saved_count > 0) {
              // Show what was calibrated
              const sizes = data.calibration_data.map(point => point.object_size_mm).join(', ');
              document.getElementById('objectSizeCalibCompletedSizes').textContent = `Calibrated: ${sizes} mm`;
              document.getElementById('objectSizeCalibCompleted').style.display = 'block';
              
              // Draw the calibration plot
              drawObjectSizeCalibrationPlot(data.calibration_data);
            } else {
              document.getElementById('objectSizeCalibCompleted').style.display = 'none';
            }
          }
        }
      } catch(e) { }
    }
    
    function drawObjectSizeCalibrationPlot(calibrationData) {
      const svg = document.getElementById('objectSizeCalibPlotModal');
      if (!svg) return;
      
      // Clear previous content
      svg.innerHTML = '';
      
      if (!calibrationData || calibrationData.length === 0) {
        const text = document.createElementNS('http://www.w3.org/2000/svg', 'text');
        text.setAttribute('x', '250');
        text.setAttribute('y', '150');
        text.setAttribute('text-anchor', 'middle');
        text.setAttribute('font-size', '14');
        text.setAttribute('fill', '#999');
        text.textContent = 'No calibration data';
        svg.appendChild(text);
        return;
      }
      
      // Calculate data ranges for scaling
      let minDeltaC = Math.min(...calibrationData.map(p => p.delta_c));
      let maxDeltaC = Math.max(...calibrationData.map(p => p.delta_c));
      let minSize = Math.min(...calibrationData.map(p => p.object_size_mm));
      let maxSize = Math.max(...calibrationData.map(p => p.object_size_mm));
      
      // X-axis starts at 0, Y-axis has padding
      minDeltaC = 0;
      maxDeltaC += 5;
      minSize -= 5;
      maxSize += 5;
      
      const padding = 50;
      const width = 500;
      const height = 300;
      const plotWidth = width - 2 * padding;
      const plotHeight = height - 2 * padding;
      
      // Draw grid background
      const bg = document.createElementNS('http://www.w3.org/2000/svg', 'rect');
      bg.setAttribute('x', padding);
      bg.setAttribute('y', padding);
      bg.setAttribute('width', plotWidth);
      bg.setAttribute('height', plotHeight);
      bg.setAttribute('fill', '#fafafa');
      bg.setAttribute('stroke', '#ddd');
      bg.setAttribute('stroke-width', '1');
      svg.appendChild(bg);
      
      // Draw axes
      const xAxis = document.createElementNS('http://www.w3.org/2000/svg', 'line');
      xAxis.setAttribute('x1', padding);
      xAxis.setAttribute('y1', padding + plotHeight);
      xAxis.setAttribute('x2', width - padding);
      xAxis.setAttribute('y2', padding + plotHeight);
      xAxis.setAttribute('stroke', '#333');
      xAxis.setAttribute('stroke-width', '2');
      svg.appendChild(xAxis);
      
      const yAxis = document.createElementNS('http://www.w3.org/2000/svg', 'line');
      yAxis.setAttribute('x1', padding);
      yAxis.setAttribute('y1', padding);
      yAxis.setAttribute('x2', padding);
      yAxis.setAttribute('y2', padding + plotHeight);
      yAxis.setAttribute('stroke', '#333');
      yAxis.setAttribute('stroke-width', '2');
      svg.appendChild(yAxis);
      
      // Draw X-axis ticks and labels
      const xTicks = 5;
      for (let i = 0; i <= xTicks; i++) {
        const ratio = i / xTicks;
        const x = padding + ratio * plotWidth;
        const value = minDeltaC + ratio * (maxDeltaC - minDeltaC);
        
        // Tick mark
        const tick = document.createElementNS('http://www.w3.org/2000/svg', 'line');
        tick.setAttribute('x1', x);
        tick.setAttribute('y1', padding + plotHeight);
        tick.setAttribute('x2', x);
        tick.setAttribute('y2', padding + plotHeight + 5);
        tick.setAttribute('stroke', '#333');
        tick.setAttribute('stroke-width', '1');
        svg.appendChild(tick);
        
        // Label
        const label = document.createElementNS('http://www.w3.org/2000/svg', 'text');
        label.setAttribute('x', x);
        label.setAttribute('y', padding + plotHeight + 18);
        label.setAttribute('text-anchor', 'middle');
        label.setAttribute('font-size', '10');
        label.setAttribute('fill', '#666');
        label.textContent = value.toFixed(1);
        svg.appendChild(label);
      }
      
      // Draw Y-axis ticks and labels
      const yTicks = 4;
      for (let i = 0; i <= yTicks; i++) {
        const ratio = 1 - (i / yTicks);  // Inverted for Y axis
        const y = padding + ratio * plotHeight;
        const value = minSize + (1 - ratio) * (maxSize - minSize);
        
        // Tick mark
        const tick = document.createElementNS('http://www.w3.org/2000/svg', 'line');
        tick.setAttribute('x1', padding - 5);
        tick.setAttribute('y1', y);
        tick.setAttribute('x2', padding);
        tick.setAttribute('y2', y);
        tick.setAttribute('stroke', '#333');
        tick.setAttribute('stroke-width', '1');
        svg.appendChild(tick);
        
        // Label
        const label = document.createElementNS('http://www.w3.org/2000/svg', 'text');
        label.setAttribute('x', padding - 8);
        label.setAttribute('y', y + 4);
        label.setAttribute('text-anchor', 'end');
        label.setAttribute('font-size', '10');
        label.setAttribute('fill', '#666');
        label.textContent = Math.round(value);
        svg.appendChild(label);
      }
      
      // Draw axis labels
      const xLabel = document.createElementNS('http://www.w3.org/2000/svg', 'text');
      xLabel.setAttribute('x', width / 2);
      xLabel.setAttribute('y', height - 5);
      xLabel.setAttribute('text-anchor', 'middle');
      xLabel.setAttribute('font-size', '11');
      xLabel.setAttribute('fill', '#333');
      xLabel.setAttribute('font-weight', 'bold');
      xLabel.textContent = 'ΔC (pF)';
      svg.appendChild(xLabel);
      
      const yLabel = document.createElementNS('http://www.w3.org/2000/svg', 'text');
      yLabel.setAttribute('x', 15);
      yLabel.setAttribute('y', padding + plotHeight / 2);
      yLabel.setAttribute('text-anchor', 'middle');
      yLabel.setAttribute('font-size', '11');
      yLabel.setAttribute('fill', '#333');
      yLabel.setAttribute('font-weight', 'bold');
      yLabel.setAttribute('transform', `rotate(-90 15 ${padding + plotHeight / 2})`);
      yLabel.textContent = 'Size (mm)';
      svg.appendChild(yLabel);
      
      // Plot data points and connect with line
      const points = [];
      calibrationData.forEach((point, index) => {
        const x = padding + ((point.delta_c - minDeltaC) / (maxDeltaC - minDeltaC)) * plotWidth;
        const y = padding + plotHeight - ((point.object_size_mm - minSize) / (maxSize - minSize)) * plotHeight;
        points.push({x, y, ...point});
      });
      
      // Draw connecting line
      if (points.length > 1) {
        let pathData = `M ${points[0].x} ${points[0].y}`;
        for (let i = 1; i < points.length; i++) {
          pathData += ` L ${points[i].x} ${points[i].y}`;
        }
        const path = document.createElementNS('http://www.w3.org/2000/svg', 'path');
        path.setAttribute('d', pathData);
        path.setAttribute('stroke', '#9966cc');
        path.setAttribute('stroke-width', '2');
        path.setAttribute('fill', 'none');
        svg.appendChild(path);
      }
      
      // Draw data points
      points.forEach((point, index) => {
        const circle = document.createElementNS('http://www.w3.org/2000/svg', 'circle');
        circle.setAttribute('cx', point.x);
        circle.setAttribute('cy', point.y);
        circle.setAttribute('r', '5');
        circle.setAttribute('fill', '#9966cc');
        circle.setAttribute('stroke', '#fff');
        circle.setAttribute('stroke-width', '2');
        svg.appendChild(circle);
      });
    }
    
    // Update deformation visualization and parameters
    async function updateDeformation() {
      try {
        const response = await fetch('/api/getdeformation');
        const data = await response.json();
        
        const alpha = data.alpha || 0;
        const r = data.r || 0;
        const d = data.d || 0;
        const objectSize = data.object_size || 0;
        const hasObjectSizeCalib = data.has_object_size_calib || false;
        
        // Store the latest object size for calculateObjectSize function
        lastObjectSize = objectSize;
        
        // Draw deformation visualization (parameters are now shown inside the SVG)
        drawDeformationVisualization(alpha, r, d);
        
        // Update calibrated object size display in new location
        if (hasObjectSizeCalib) {
          document.getElementById('calibratedObjectSizeDisplay').textContent = objectSize > 0 ? objectSize.toFixed(2) : '--';
          drawObjectSizeTimeSeries();  // Draw the time series plot
        }
      } catch(e) {
        console.error('Error updating deformation:', e);
      }
      
      // Always update P-ΔC curve, regardless of deformation API status
      drawPneunetCurve();
    }
    
    // Draw circular arc visualization of the finger deformation
    function drawDeformationVisualization(alpha, r, d) {
      const svg = document.getElementById('deformationViz');
      svg.innerHTML = '';
      
      // Get actual SVG viewBox dimensions
      const viewBox = svg.getAttribute('viewBox').split(' ');
      const width = parseFloat(viewBox[2]);
      const height = parseFloat(viewBox[3]);
      const centerX = width / 2;
      const centerY = height / 2;
      const endpointY = centerY;  // Fixed vertical position at center
      const paramBaseY = height - 50;  // Parameters near bottom
      const scale = 2;  // Constant scale: 1.5 pixels per mm
      
      // Only draw arc if we have valid parameters
      if (alpha > 0.01 && r > 0.1) {
        // Scale all measurements to screen pixels using constant scale
        const rScreen = r * scale;      // Radius in screen pixels
        const dScreen = d * scale;      // Chord length in screen pixels
        
        // Chord endpoints at center vertical position, separated by dScreen
        // Always centered horizontally
        const x1 = centerX - dScreen / 2;
        const x2 = centerX + dScreen / 2;
        const y1 = endpointY;
        const y2 = endpointY;
        
        // Calculate center point of circle
        // Geometry: endpoints at distance rScreen from center, separated by dScreen
        // Vertical offset from endpoints to center: rScreen * cos(alpha/2)
        const centerPointY = endpointY + rScreen * Math.cos(alpha / 2);
        const centerPointX = centerX;  // Center is horizontally at midpoint of endpoints
        
        // Draw radius lines from endpoints DOWN to center point
        const line1 = document.createElementNS('http://www.w3.org/2000/svg', 'line');
        line1.setAttribute('x1', x1);
        line1.setAttribute('y1', y1);
        line1.setAttribute('x2', centerPointX);
        line1.setAttribute('y2', centerPointY);
        line1.setAttribute('stroke', '#999');
        line1.setAttribute('stroke-width', '1');
        line1.setAttribute('stroke-dasharray', '3,3');
        svg.appendChild(line1);
        
        const line2 = document.createElementNS('http://www.w3.org/2000/svg', 'line');
        line2.setAttribute('x1', x2);
        line2.setAttribute('y1', y2);
        line2.setAttribute('x2', centerPointX);
        line2.setAttribute('y2', centerPointY);
        line2.setAttribute('stroke', '#999');
        line2.setAttribute('stroke-width', '1');
        line2.setAttribute('stroke-dasharray', '3,3');
        svg.appendChild(line2);
        
        // Draw chord (contact line) showing object width d
        const chord = document.createElementNS('http://www.w3.org/2000/svg', 'line');
        chord.setAttribute('x1', x1);
        chord.setAttribute('y1', y1);
        chord.setAttribute('x2', x2);
        chord.setAttribute('y2', y2);
        chord.setAttribute('stroke', '#00aa00');
        chord.setAttribute('stroke-width', '3');
        chord.setAttribute('stroke-linecap', 'round');
        svg.appendChild(chord);
        
        // Draw arc connecting the two endpoints
        // Arc spans from angle -alpha/2 to +alpha/2 (symmetric around vertical axis)
        const arcStartAngle = -Math.PI / 2 - alpha / 2;
        const arcEndAngle = -Math.PI / 2 + alpha / 2;
        const arcPath = describeArc(centerPointX, centerPointY, rScreen, arcStartAngle, arcEndAngle);
        
        const arc = document.createElementNS('http://www.w3.org/2000/svg', 'path');
        arc.setAttribute('d', arcPath);
        arc.setAttribute('fill', 'none');
        arc.setAttribute('stroke', '#0066cc');
        arc.setAttribute('stroke-width', '3');
        arc.setAttribute('stroke-linecap', 'round');
        svg.appendChild(arc);
        
        // Display parameters without background box
        const textY1 = paramBaseY;
        const textY2 = paramBaseY + 16;
        const textY3 = paramBaseY + 32;
        const textY4 = paramBaseY + 48;
        
        // d parameter (larger, green)
        const dLabel = document.createElementNS('http://www.w3.org/2000/svg', 'text');
        dLabel.setAttribute('x', '15');
        dLabel.setAttribute('y', textY1);
        dLabel.setAttribute('font-size', '13');
        dLabel.setAttribute('fill', '#00aa00');
        dLabel.setAttribute('font-weight', 'bold');
        dLabel.textContent = 'd = ' + d.toFixed(2) + ' mm';
        svg.appendChild(dLabel);
        
        // r parameter
        const rLabel = document.createElementNS('http://www.w3.org/2000/svg', 'text');
        rLabel.setAttribute('x', '15');
        rLabel.setAttribute('y', textY2);
        rLabel.setAttribute('font-size', '11');
        rLabel.setAttribute('fill', '#666');
        rLabel.textContent = 'r = ' + r.toFixed(2) + ' mm';
        svg.appendChild(rLabel);
        
        // Alpha parameter (convert from radians to degrees for display)
        const alphaLabel = document.createElementNS('http://www.w3.org/2000/svg', 'text');
        alphaLabel.setAttribute('x', '15');
        alphaLabel.setAttribute('y', textY3);
        alphaLabel.setAttribute('font-size', '11');
        alphaLabel.setAttribute('fill', '#666');
        const alphaDegrees = (alpha * 180 / Math.PI).toFixed(2);
        alphaLabel.textContent = 'α = ' + alphaDegrees + '°';
        svg.appendChild(alphaLabel);
        
        // Calibrated object size (show latest from deformation API)
        const sizeLabel = document.createElementNS('http://www.w3.org/2000/svg', 'text');
        sizeLabel.setAttribute('x', '15');
        sizeLabel.setAttribute('y', textY4);
        sizeLabel.setAttribute('font-size', '13');
        sizeLabel.setAttribute('fill', '#9966cc');
        sizeLabel.setAttribute('font-weight', 'bold');
        const displaySize = lastObjectSize > 0 ? lastObjectSize.toFixed(2) : '--';
        sizeLabel.textContent = 'Object: ' + displaySize + ' mm';
        svg.appendChild(sizeLabel);
      } else {
        // Show default state: blue line representing object length L at center height
        const L = 120; // PNEUNET_L in mm
        const LScreen = L * scale; // Convert to screen pixels
        const x1 = centerX - LScreen / 2;
        const x2 = centerX + LScreen / 2;
        
        // Draw the default blue line at center height
        const defaultLine = document.createElementNS('http://www.w3.org/2000/svg', 'line');
        defaultLine.setAttribute('x1', x1);
        defaultLine.setAttribute('y1', endpointY);
        defaultLine.setAttribute('x2', x2);
        defaultLine.setAttribute('y2', endpointY);
        defaultLine.setAttribute('stroke', '#0066cc');
        defaultLine.setAttribute('stroke-width', '3');
        defaultLine.setAttribute('stroke-linecap', 'round');
        svg.appendChild(defaultLine);
        
        // Display default state label
        const textY = paramBaseY;
        const label = document.createElementNS('http://www.w3.org/2000/svg', 'text');
        label.setAttribute('x', '15');
        label.setAttribute('y', textY);
        label.setAttribute('font-size', '13');
        label.setAttribute('fill', '#0066cc');
        label.setAttribute('font-weight', 'bold');
        label.textContent = 'L = ' + L.toFixed(2) + ' mm';
        svg.appendChild(label);
      }
    }
    
    // Helper function to create SVG arc path
    function describeArc(x, y, radius, startAngle, endAngle) {
      const start = polarToCartesian(x, y, radius, endAngle);
      const end = polarToCartesian(x, y, radius, startAngle);
      const largeArc = endAngle - startAngle <= Math.PI ? '0' : '1';
      return [
        'M', start.x, start.y,
        'A', radius, radius, 0, largeArc, 0, end.x, end.y
      ].join(' ');
    }
    
    function polarToCartesian(centerX, centerY, radius, angleInRadians) {
      return {
        x: centerX + radius * Math.cos(angleInRadians),
        y: centerY + radius * Math.sin(angleInRadians)
      };
    }
    
    // Draw object size over time
    function drawObjectSizeTimeSeries() {
      const svg = document.getElementById('objectSizeChart');
      if (!svg || !allData.length) return;
      
      svg.innerHTML = '';
      
      // Calculate object sizes for all data points
      const objectSizes = allData.map(point => {
        return {t: point.t, size: calculateObjectSize(point.c)};
      }).filter(p => p.size > 0);
      
      if (objectSizes.length === 0) {
        const text = document.createElementNS('http://www.w3.org/2000/svg', 'text');
        text.setAttribute('x', '200');
        text.setAttribute('y', '110');
        text.setAttribute('text-anchor', 'middle');
        text.setAttribute('font-size', '12');
        text.setAttribute('fill', '#999');
        text.textContent = 'No size data available';
        svg.appendChild(text);
        return;
      }
      
      const margin = {top: 15, right: 20, bottom: 25, left: 40};
      const width = 400 - margin.left - margin.right;
      const height = 220 - margin.top - margin.bottom;
      
      const minT = Math.min(...objectSizes.map(p => p.t));
      const maxT = Math.max(...objectSizes.map(p => p.t));
      const minSize = Math.min(...objectSizes.map(p => p.size));
      const maxSize = Math.max(...objectSizes.map(p => p.size));
      
      const g = document.createElementNS('http://www.w3.org/2000/svg', 'g');
      g.setAttribute('transform', `translate(${margin.left}, ${margin.top})`);
      
      // Axes
      const yAxis = document.createElementNS('http://www.w3.org/2000/svg', 'line');
      yAxis.setAttribute('x1', '0'); yAxis.setAttribute('y1', '0');
      yAxis.setAttribute('x2', '0'); yAxis.setAttribute('y2', height);
      yAxis.setAttribute('stroke', '#333'); yAxis.setAttribute('stroke-width', '2');
      g.appendChild(yAxis);
      
      const xAxis = document.createElementNS('http://www.w3.org/2000/svg', 'line');
      xAxis.setAttribute('x1', '0'); xAxis.setAttribute('y1', height);
      xAxis.setAttribute('x2', width); xAxis.setAttribute('y2', height);
      xAxis.setAttribute('stroke', '#333'); xAxis.setAttribute('stroke-width', '2');
      g.appendChild(xAxis);
      
      // Draw line
      if (objectSizes.length > 1) {
        let pathData = '';
        objectSizes.forEach((p, i) => {
          const x = ((p.t - minT) / (maxT - minT || 1)) * width;
          const y = height - ((p.size - minSize) / (maxSize - minSize || 1)) * height;
          pathData += (i === 0 ? `M ${x} ${y}` : ` L ${x} ${y}`);
        });
        const path = document.createElementNS('http://www.w3.org/2000/svg', 'path');
        path.setAttribute('d', pathData);
        path.setAttribute('fill', 'none');
        path.setAttribute('stroke', '#9966cc');
        path.setAttribute('stroke-width', '2');
        g.appendChild(path);
      }
      
      // Labels
      const yLabel = document.createElementNS('http://www.w3.org/2000/svg', 'text');
      yLabel.setAttribute('x', '-' + height/2); yLabel.setAttribute('y', '-25');
      yLabel.setAttribute('transform', 'rotate(-90)');
      yLabel.setAttribute('text-anchor', 'middle');
      yLabel.setAttribute('font-size', '11');
      yLabel.setAttribute('fill', '#666');
      yLabel.textContent = 'Size (mm)';
      g.appendChild(yLabel);
      
      svg.appendChild(g);
    }
    

    

    // Draw P-ΔC curve with calibration curve, critical pressure, and real-time point
    function drawPneunetCurve() {
      const svg = document.getElementById('pneunetCurve');
      if (!svg) return;
      
      fetch('/api/calibration/pneunet_status')
        .then(response => response.json())
        .then(calibData => {
          drawPneunetCurveChart(svg, calibData);
        })
        .catch(e => console.error('Error drawing P-ΔC curve:', e));
    }
    
    function drawPneunetCurveChart(svg, calibData) {
      svg.innerHTML = '';
      
      if (!calibData || calibData.fit_method === undefined) {
        const text = document.createElementNS('http://www.w3.org/2000/svg', 'text');
        text.setAttribute('x', '200');
        text.setAttribute('y', '140');
        text.setAttribute('text-anchor', 'middle');
        text.setAttribute('font-size', '12');
        text.setAttribute('fill', '#999');
        text.textContent = 'No calibration data';
        svg.appendChild(text);
        return;
      }
      
      // Determine which fit method is active and get parameters
      let a, b, c, fitMethod;
      const samples = calibData.samples || [];
      const cap_c0 = parseFloat(calibData.c0) || 0;
      
      if (calibData.fit_method === 0) {
        // Linear fit: P = a×ΔC + b
        a = parseFloat(calibData.a) || 0;
        b = parseFloat(calibData.b) || 0;
        fitMethod = 'linear';
      } else {
        // Square root fit: P = a×√(ΔC-b) + c
        a = parseFloat(calibData.sqrt_a) || 0;
        b = parseFloat(calibData.sqrt_b) || 0;
        c = parseFloat(calibData.sqrt_c) || 0;
        fitMethod = 'sqrt';
      }
      
      const margin = {top: 20, right: 20, bottom: 30, left: 45};
      const width = 400 - margin.left - margin.right;
      const height = 280 - margin.top - margin.bottom;
      
      // Determine ranges
      let maxDc = 20, maxP = 10;
      
      if (samples.length > 0) {
        maxDc = Math.max(...samples.map(p => p.dc), 1);
        maxP = Math.max(...samples.map(p => p.p), 1);
      } else if (a !== 0) {
        maxDc = 30;
        if (fitMethod === 'linear') {
          const pAtMax = a * maxDc + b;
          maxP = Math.max(pAtMax * 1.2, 5);
        } else {
          const dcVal = Math.max(maxDc - b, 0);
          const pAtMax = a * Math.sqrt(dcVal) + c;
          maxP = Math.max(pAtMax * 1.2, 5);
        }
      }
      
      // Accommodate calibration sample ranges
      if (samples.length > 0) {
        maxDc = Math.max(...samples.map(p => p.dc), 1);
        maxP = Math.max(...samples.map(p => p.p), 1);
      } else if (a !== 0) {
        maxDc = 30;
        if (fitMethod === 'linear') {
          const pAtMax = a * maxDc + b;
          maxP = Math.max(pAtMax * 1.2, 5);
        } else {
          const dcVal = Math.max(maxDc - b, 0);
          const pAtMax = a * Math.sqrt(dcVal) + c;
          maxP = Math.max(pAtMax * 1.2, 5);
        }
      }
      
      if (maxDc < 1) maxDc = 1;
      if (maxP < 1) maxP = 1;
      
      maxDc = Math.ceil(maxDc * 1.05);
      maxP = Math.ceil(maxP * 1.05);
      
      const g = document.createElementNS('http://www.w3.org/2000/svg', 'g');
      g.setAttribute('transform', `translate(${margin.left}, ${margin.top})`);
      
      // Axes
      const yAxis = document.createElementNS('http://www.w3.org/2000/svg', 'line');
      yAxis.setAttribute('x1', '0'); yAxis.setAttribute('y1', '0');
      yAxis.setAttribute('x2', '0'); yAxis.setAttribute('y2', height);
      yAxis.setAttribute('stroke', '#333'); yAxis.setAttribute('stroke-width', '2');
      g.appendChild(yAxis);
      
      const xAxis = document.createElementNS('http://www.w3.org/2000/svg', 'line');
      xAxis.setAttribute('x1', '0'); xAxis.setAttribute('y1', height);
      xAxis.setAttribute('x2', width); xAxis.setAttribute('y2', height);
      xAxis.setAttribute('stroke', '#333'); xAxis.setAttribute('stroke-width', '2');
      g.appendChild(xAxis);
      
      // Draw calibration curve
      if (a !== 0) {
        let pathData = '';
        const steps = 80;
        for (let i = 0; i <= steps; i++) {
          const dc = (i / steps) * maxDc;
          let p;
          if (fitMethod === 'linear') {
            p = a * dc + b;
          } else {
            // Square root fit: P = a×√(ΔC-b) + c
            const dcVal = Math.max(dc - b, 0);
            p = a * Math.sqrt(dcVal) + c;
          }
          const x = (dc / maxDc) * width;
          const y = height - (p / maxP) * height;
          pathData += (i === 0 ? `M ${x} ${y}` : ` L ${x} ${y}`);
        }
        const path = document.createElementNS('http://www.w3.org/2000/svg', 'path');
        path.setAttribute('d', pathData);
        path.setAttribute('fill', 'none');
        path.setAttribute('stroke', '#0099cc');
        path.setAttribute('stroke-width', '2');
        g.appendChild(path);
        
        // Draw critical pressure curve
        const GRASP_BUFFER = 0.3;
        let thresholdPathData = '';
        for (let i = 0; i <= steps; i++) {
          const dc = (i / steps) * maxDc;
          let curveP;
          if (fitMethod === 'linear') {
            curveP = a * dc + b;
          } else {
            // Square root fit: P = a×√(ΔC-b) + c
            const dcVal = Math.max(dc - b, 0);
            curveP = a * Math.sqrt(dcVal) + c;
          }
          const thresholdP = Math.max(GRASP_BUFFER, curveP + GRASP_BUFFER);
          const x = (dc / maxDc) * width;
          const y = height - (thresholdP / maxP) * height;
          thresholdPathData += (i === 0 ? `M ${x} ${y}` : ` L ${x} ${y}`);
        }
        const thresholdPath = document.createElementNS('http://www.w3.org/2000/svg', 'path');
        thresholdPath.setAttribute('d', thresholdPathData);
        thresholdPath.setAttribute('fill', 'none');
        thresholdPath.setAttribute('stroke', '#00aa00');
        thresholdPath.setAttribute('stroke-width', '1.5');
        thresholdPath.setAttribute('stroke-dasharray', '4,4');
        g.appendChild(thresholdPath);
      }
      
      // Y-axis ticks and labels
      const yTicks = 5;
      for (let i = 0; i <= yTicks; i++) {
        const p = (i / yTicks) * maxP;
        const y = height - (p / maxP) * height;
        
        const tick = document.createElementNS('http://www.w3.org/2000/svg', 'line');
        tick.setAttribute('x1', '-5');
        tick.setAttribute('y1', y);
        tick.setAttribute('x2', '0');
        tick.setAttribute('y2', y);
        tick.setAttribute('stroke', '#666');
        tick.setAttribute('stroke-width', '1');
        g.appendChild(tick);
        
        const label = document.createElementNS('http://www.w3.org/2000/svg', 'text');
        label.setAttribute('x', '-10');
        label.setAttribute('y', y + 3);
        label.setAttribute('text-anchor', 'end');
        label.setAttribute('font-size', '10');
        label.setAttribute('fill', '#666');
        label.textContent = p.toFixed(1);
        g.appendChild(label);
      }
      
      // X-axis ticks and labels
      const xTicks = 5;
      for (let i = 0; i <= xTicks; i++) {
        const dc = (i / xTicks) * maxDc;
        const x = (dc / maxDc) * width;
        
        const tick = document.createElementNS('http://www.w3.org/2000/svg', 'line');
        tick.setAttribute('x1', x);
        tick.setAttribute('y1', height);
        tick.setAttribute('x2', x);
        tick.setAttribute('y2', height + 5);
        tick.setAttribute('stroke', '#666');
        tick.setAttribute('stroke-width', '1');
        g.appendChild(tick);
        
        const label = document.createElementNS('http://www.w3.org/2000/svg', 'text');
        label.setAttribute('x', x);
        label.setAttribute('y', height + 15);
        label.setAttribute('text-anchor', 'middle');
        label.setAttribute('font-size', '10');
        label.setAttribute('fill', '#666');
        label.textContent = dc.toFixed(0);
        g.appendChild(label);
      }
      
      // Axis labels
      const yLabel = document.createElementNS('http://www.w3.org/2000/svg', 'text');
      yLabel.setAttribute('x', '-' + height/2); 
      yLabel.setAttribute('y', '-35');
      yLabel.setAttribute('transform', 'rotate(-90)');
      yLabel.setAttribute('text-anchor', 'middle');
      yLabel.setAttribute('font-size', '11');
      yLabel.setAttribute('font-weight', 'bold');
      yLabel.setAttribute('fill', '#666');
      yLabel.textContent = 'P (kPa)';
      g.appendChild(yLabel);
      
      const xLabel = document.createElementNS('http://www.w3.org/2000/svg', 'text');
      xLabel.setAttribute('x', width/2); 
      xLabel.setAttribute('y', height + 30);
      xLabel.setAttribute('text-anchor', 'middle');
      xLabel.setAttribute('font-size', '11');
      xLabel.setAttribute('fill', '#666');
      xLabel.setAttribute('font-weight', 'bold');
      xLabel.textContent = 'ΔC (pF)';
      g.appendChild(xLabel);
      
      // Plot latest P and C data point
      if (allData.length > 0 && a !== 0) {
        const latestData = allData[allData.length - 1];
        const latestP = latestData.p;
        const latestC = latestData.c || 0;
        
        // Calculate delta C
        const deltaC = latestC - cap_c0;
        
        // Get the pressure from the calibration curve at this delta C
        let curveP;
        if (fitMethod === 'linear') {
          curveP = a * deltaC + b;
        } else {
          // Square root fit: P = a×√(ΔC-b) + c
          const dcVal = Math.max(deltaC - b, 0);
          curveP = a * Math.sqrt(dcVal) + c;
        }
        
        // Get the threshold pressure (green line) at this delta C
        const GRASP_BUFFER = 0.3;
        const thresholdP = Math.max(GRASP_BUFFER, curveP + GRASP_BUFFER);
        
        // Determine color: green if above threshold, orange otherwise
        const pointColor = latestP >= thresholdP ? '#00aa00' : '#ff9933';
        const strokeColor = latestP >= thresholdP ? '#017401' : '#bf7225';

        // Only plot if delta C is within the chart range
        if (deltaC >= 0 && deltaC <= maxDc && latestP >= 0 && latestP <= maxP) {
          const x = (deltaC / maxDc) * width;
          const y = height - (latestP / maxP) * height;
          
          const circle = document.createElementNS('http://www.w3.org/2000/svg', 'circle');
          circle.setAttribute('cx', x);
          circle.setAttribute('cy', y);
          circle.setAttribute('r', '5');
          circle.setAttribute('fill', pointColor);
          circle.setAttribute('stroke', strokeColor);
          circle.setAttribute('stroke-width', '1');
          g.appendChild(circle);
        }
      }
      
      svg.appendChild(g);
    }
    
    let lastObjectSize = 0;
    
    function calculateObjectSize(capacitance) {
      // Use the latest object size from deformation API update
      return lastObjectSize;
    }
    
    // (grasping status is shown via the main status field)
    
    // Toggle collection
    async function toggleCollection() {
      try {
        const filename = document.getElementById('filenameInput').value;
        const endpoint = isCollecting ? '/api/stop' : '/api/start';
        
        // On first start or when starting with a custom filename, set it
        if (!isCollecting && (firstStart || filename.length > 0)) {
          await fetch('/api/setfilename?name=' + encodeURIComponent(filename));
          firstStart = false;
          hasDataBeenCollected = true;  // Mark that data collection has started
        }
        
        const response = await fetch(endpoint);
        const result = await response.json();
        isCollecting = result.collecting;
        updateStatus();
      } catch(e) { console.error('Error toggling collection:', e); }
    }
    
    async function resetData() {
      if(confirm('Clear all data?')) {
        const filename = document.getElementById('filenameInput').value;
        
        // Set filename if provided
        if (filename.length > 0) {
          await fetch('/api/setfilename?name=' + encodeURIComponent(filename));
        } else {
          // Clear filename to use timestamp
          await fetch('/api/setfilename?name=');
        }
        
        await fetch('/api/reset');
        allData = [];
        drawChart();
        updateStats();
        loadData();
        firstStart = true;  // Reset flag so next start is treated as fresh collection
        hasDataBeenCollected = false;  // Reset data flag so input is enabled again
        isCollecting = false;  // Ensure collecting state is reset
        
        // Clear the input field text
        document.getElementById('filenameInput').value = '';
        
        updateStatus();  // Re-enable filename input after reset
      }
    }
    
    async function exportCSV() {
      const response = await fetch('/api/export');
      const csv = await response.text();
      const blob = new Blob([csv], {type: 'text/csv'});
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = 'pressure_data.csv';
      a.click();
      URL.revokeObjectURL(url);
    }
    
    // Initialize and load data
    window.addEventListener('load', () => {
      loadData();
      setInterval(loadData, 500);
    });
  </script>
</body>
</html>
)rawliteral";
}

// ============ WEB SERVER SETUP ============
void setupWebServer() {
  Serial.println("WebServer: Registering routes...");
  Serial.flush();
  
  // Serve main page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", generateHTML());
  });
  Serial.println("WebServer: Route / registered");
  Serial.flush();
  
  // API endpoint: get latest sensor data as JSON
  server.on("/api/data", HTTP_GET, [](AsyncWebServerRequest *request) {
    String json = "{}";
    
    // Get latest pressure
    float pressure = 0.0;
    unsigned long pressureTime = 0;
    if (xSemaphoreTake(pressureMutex, 10 / portTICK_PERIOD_MS)) {
      pressure = lastPressure;
      pressureTime = lastPressureTime;
      xSemaphoreGive(pressureMutex);
    }
    
    // Get latest capacitance
    float capacitance = 0.0;
    unsigned long capacitanceTime = 0;
    if (xSemaphoreTake(capacitanceMutex, 10 / portTICK_PERIOD_MS)) {
      capacitance = lastCapacitance;
      capacitanceTime = lastCapacitanceTime;
      xSemaphoreGive(capacitanceMutex);
    }
    
    // Return as single JSON object with both values
    if (isCollecting) {
      unsigned long elapsedTime = millis() - collectionStartTime;
      json = "{\"t\":" + String(elapsedTime) + 
             ",\"p\":" + String(pressure, 2) + 
             ",\"c\":" + String(capacitance, 2) + ",\"g\":" + (isGrasping ? "true" : "false") + "}";
    }
    
    request->send(200, "application/json", json);
  });
  Serial.println("WebServer: Route /api/data registered");
  Serial.flush();
  
  // API endpoint: get latest sensor readings
  server.on("/api/latest", HTTP_GET, [](AsyncWebServerRequest *request) {
    String json = "{}";
    
    float pressure = 0.0;
    if (xSemaphoreTake(pressureMutex, 10 / portTICK_PERIOD_MS)) {
      pressure = lastPressure;
      xSemaphoreGive(pressureMutex);
    }
    
    float capacitance = 0.0;
    if (xSemaphoreTake(capacitanceMutex, 10 / portTICK_PERIOD_MS)) {
      capacitance = lastCapacitance;
      xSemaphoreGive(capacitanceMutex);
    }
    
    if (isCollecting) {
      json = "{\"p\":" + String(pressure, 2) + 
             ",\"c\":" + String(capacitance, 2) + "}";
    }
    
    request->send(200, "application/json", json);
  });
  Serial.println("WebServer: Route /api/latest registered");
  Serial.flush();
  
  
  // API endpoint: reset data and collection
  server.on("/api/reset", HTTP_GET, [](AsyncWebServerRequest *request) {
    resetCollection();
    request->send(200, "application/json", "{\"status\":\"reset\"}");
  });
  Serial.println("WebServer: Route /api/reset registered");
  Serial.flush();
  
  // API endpoint: set time on device (for proper timestamps)
  server.on("/api/settime", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("timestamp")) {
      time_t timestamp = atol(request->getParam("timestamp")->value().c_str());
      struct timeval tv;
      tv.tv_sec = timestamp;
      tv.tv_usec = 0;
      settimeofday(&tv, NULL);
      
      // Set timezone offset if provided
      if (request->hasParam("tzoffset")) {
        long tzoffset = atol(request->getParam("tzoffset")->value().c_str());
        // Set timezone for proper local time formatting
        // Create TZ string: std offset dst [offset],start[/time],end[/time]
        // For CET (UTC+1): CET-1CEST,M3.5.0,M10.5.0
        // We'll use a simplified approach: just set the offset via TZ environment
        char tz_str[32];
        int hours = tzoffset / 3600;
        int minutes = (abs(tzoffset) % 3600) / 60;
        sprintf(tz_str, "UTC%d:%02d", -hours, minutes);  // Negative because POSIX TZ is backwards
        setenv("TZ", tz_str, 1);
        tzset();
        Serial.printf("Timezone set to: %s\n", tz_str);
      }
      
      Serial.printf("Time set to: %ld\n", timestamp);
      Serial.flush();
      request->send(200, "application/json", "{\"status\":\"time set\"}");
    } else {
      request->send(400, "application/json", "{\"error\":\"missing timestamp parameter\"}");
    }
  });
  Serial.println("WebServer: Route /api/settime registered");
  Serial.flush();
  
  // API endpoint: start collection
  server.on("/api/start", HTTP_GET, [](AsyncWebServerRequest *request) {
    startCollection();
    request->send(200, "application/json", "{\"collecting\":true}");
  });
  Serial.println("WebServer: Route /api/start registered");
  Serial.flush();
  
  // API endpoint: set SD card filename
  server.on("/api/setfilename", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("name")) {
      String filename = request->getParam("name")->value();
      #ifdef USE_SD_CARD
        setSDCardFilename(filename);
        request->send(200, "application/json", "{\"status\":\"filename set\"}");
      #else
        request->send(200, "application/json", "{\"status\":\"SD card disabled\"}");
      #endif
    } else {
      request->send(400, "application/json", "{\"error\":\"missing name parameter\"}");
    }
  });
  Serial.println("WebServer: Route /api/setfilename registered");
  Serial.flush();
  
  // API endpoint: stop collection
  server.on("/api/stop", HTTP_GET, [](AsyncWebServerRequest *request) {
    stopCollection();
    request->send(200, "application/json", "{\"collecting\":false}");
  });
  Serial.println("WebServer: Route /api/stop registered");
  Serial.flush();
  
  // API endpoint: export latest data as CSV (placeholder - actual data is on SD card)
  server.on("/api/export", HTTP_GET, [](AsyncWebServerRequest *request) {
    String csv = "Time_ms,Pressure_kPa,Capacitance_pF\n";
    
    float pressure = 0.0;
    float capacitance = 0.0;
    
    if (xSemaphoreTake(pressureMutex, 10 / portTICK_PERIOD_MS)) {
      pressure = lastPressure;
      xSemaphoreGive(pressureMutex);
    }
    
    if (xSemaphoreTake(capacitanceMutex, 10 / portTICK_PERIOD_MS)) {
      capacitance = lastCapacitance;
      xSemaphoreGive(capacitanceMutex);
    }
    
    if (isCollecting) {
      unsigned long elapsed = millis() - collectionStartTime;
      csv += String(elapsed) + "," + String(pressure, 4) + "," + String(capacitance, 2) + "\n";
    }
    
    request->send(200, "text/csv", csv);
  });
  Serial.println("WebServer: Route /api/export registered");
  Serial.flush();
  
  // API endpoint: list files on SD card
  server.on("/api/listfiles", HTTP_GET, [](AsyncWebServerRequest *request) {
    #ifdef USE_SD_CARD
      String json = listSDCardFiles();
      request->send(200, "application/json", json);
    #else
      request->send(200, "application/json", "[]");
    #endif
  });
  Serial.println("WebServer: Route /api/listfiles registered");
  Serial.flush();
  
  // API endpoint: get current collection status
  server.on("/api/getstatus", HTTP_GET, [](AsyncWebServerRequest *request) {
    bool collecting = getCollectionState();
    String response = collecting ? "{\"collecting\":true}" : "{\"collecting\":false}";
    request->send(200, "application/json", response);
  });
  Serial.println("WebServer: Route /api/getstatus registered");
  Serial.flush();
  
  // API endpoint: get current deformation parameters
  server.on("/api/getdeformation", HTTP_GET, [](AsyncWebServerRequest *request) {
    extern float calibrated_object_size;
    extern int object_size_calib_count;
    
    char buffer[256];
    snprintf(buffer, sizeof(buffer),
      "{\"alpha\":%.4f,\"r\":%.2f,\"d\":%.2f,\"object_size\":%.2f,\"has_object_size_calib\":%s}",
      pneunet_alpha, pneunet_r, pneunet_d, calibrated_object_size,
      object_size_calib_count > 0 ? "true" : "false");
    request->send(200, "application/json", buffer);
  });
  Serial.println("WebServer: Route /api/getdeformation registered");
  Serial.flush();
  
  // (grasping detection status is included in /api/data as field 'g')
  
  // API endpoint: get current filename
  server.on("/api/getcurrentfilename", HTTP_GET, [](AsyncWebServerRequest *request) {
    #ifdef USE_SD_CARD
      request->send(200, "application/json", "{\"filename\":\"" + currentSDFilename + "\"}");
    #else
      request->send(200, "application/json", "{\"filename\":\"\"}");
    #endif
  });
  Serial.println("WebServer: Route /api/getcurrentfilename registered");
  Serial.flush();
  
  // API endpoint: download file from SD card
  server.on("/api/download", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("file")) {
      String filename = request->getParam("file")->value();
      // Ensure filename starts with /
      if (!filename.startsWith("/")) {
        filename = "/" + filename;
      }
      Serial.printf("WebServer: Download request for: %s\n", filename.c_str());
      Serial.flush();
      #ifdef USE_SD_CARD
        Serial.printf("WebServer: Checking if file exists: %s\n", filename.c_str());
        Serial.flush();
        if (SD.exists(filename)) {
          Serial.printf("WebServer: File exists, opening: %s\n", filename.c_str());
          Serial.flush();
          File file = SD.open(filename, FILE_READ);
          if (file) {
            Serial.printf("WebServer: File opened successfully, size: %d bytes\n", file.size());
            Serial.flush();
            String content = file.readString();
            file.close();
            request->send(200, "text/csv", content);
            Serial.println("WebServer: File sent successfully");
            Serial.flush();
            return;
          } else {
            Serial.println("WebServer: Failed to open file");
            Serial.flush();
          }
        } else {
          Serial.printf("WebServer: File does not exist: %s\n", filename.c_str());
          Serial.flush();
        }
      #endif
      request->send(404, "text/plain", "File not found");
    } else {
      request->send(400, "text/plain", "Missing file parameter");
    }
  });
  Serial.println("WebServer: Route /api/download registered");
  Serial.flush();
  
  // API endpoint: delete file from SD card
  server.on("/api/deletefile", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("file")) {
      String filename = request->getParam("file")->value();
      // Ensure filename starts with /
      if (!filename.startsWith("/")) {
        filename = "/" + filename;
      }
      Serial.printf("WebServer: Delete request for: %s\n", filename.c_str());
      Serial.flush();
      #ifdef USE_SD_CARD
        if (SD.exists(filename)) {
          Serial.printf("WebServer: File exists, attempting to delete: %s\n", filename.c_str());
          Serial.flush();
          if (SD.remove(filename)) {
            request->send(200, "application/json", "{\"status\":\"deleted\"}");
            Serial.printf("SD Card: Deleted file: %s\n", filename.c_str());
            Serial.flush();
            return;
          } else {
            Serial.println("WebServer: SD.remove() returned false");
            Serial.flush();
          }
        } else {
          Serial.printf("WebServer: File does not exist for deletion: %s\n", filename.c_str());
          Serial.flush();
        }
      #endif
      request->send(400, "application/json", "{\"error\":\"failed to delete\"}");
    } else {
      request->send(400, "text/plain", "Missing file parameter");
    }
  });
  Serial.println("WebServer: Route /api/deletefile registered");
  Serial.flush();

  // Delete all files endpoint
  server.on("/api/deleteallfiles", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("WebServer: Delete all files request");
    Serial.flush();
    #ifdef USE_SD_CARD
      File root = SD.open("/");
      int deletedCount = 0;
      while (true) {
        File entry = root.openNextFile();
        if (!entry) break;
        if (!entry.isDirectory()) {
          String filename = "/" + String(entry.name());
          if (SD.remove(filename)) {
            deletedCount++;
            Serial.printf("SD Card: Deleted file: %s\n", filename.c_str());
            Serial.flush();
          }
        }
        entry.close();
      }
      root.close();
      request->send(200, "application/json", "{\"status\":\"deleted\"}");
      Serial.printf("WebServer: Successfully deleted %d files\n", deletedCount);
      Serial.flush();
    #else
      request->send(400, "application/json", "{\"error\":\"SD card not available\"}");
    #endif
  });
  Serial.println("WebServer: Route /api/deleteallfiles registered");
  Serial.flush();
  
  // ============ CALIBRATION API ENDPOINTS ============
  
  // API endpoint: get current calibration status
  server.on("/api/calibration/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    extern long raw_baseline, raw_82, raw_101;
    extern float calib_a, calib_b;
    extern float pressureOffset;
    extern float cap_c0;
    
    String json = "{";
    json += "\"raw_baseline\":" + String(raw_baseline) + ",";
    json += "\"raw_82\":" + String(raw_82) + ",";
    json += "\"raw_101\":" + String(raw_101) + ",";
    json += "\"calib_a\":" + String(calib_a, 8) + ",";
    json += "\"calib_b\":" + String(calib_b, 8) + ",";
    json += "\"pressure_offset\":" + String(pressureOffset, 6) + ",";
    json += "\"cap_c0\":" + String(cap_c0, 6);
    json += "}";
    
    request->send(200, "application/json", json);
  });
  Serial.println("WebServer: Route /api/calibration/status registered");
  Serial.flush();
  
  // API endpoint: perform calibration step
  server.on("/api/calibration/step", HTTP_GET, [](AsyncWebServerRequest *request) {
    extern void startCalibrationStep(int step);
    extern volatile bool calibrationInProgress;
    extern long raw_baseline, raw_82, raw_101;
    
    if (!request->hasParam("step")) {
      request->send(400, "application/json", "{\"success\":false,\"error\":\"Missing step parameter\"}");
      return;
    }
    
    int step = request->getParam("step")->value().toInt();
    
    if (step < 0 || step > 2) {
      request->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid step\"}");
      return;
    }
    
    calibrationInProgress = true;
    startCalibrationStep(step);
    calibrationInProgress = false;
    
    long raw_value = 0;
    if (step == 0) {
      extern long raw_baseline;
      raw_value = raw_baseline;
    } else if (step == 1) {
      extern long raw_82;
      raw_value = raw_82;
    } else if (step == 2) {
      extern long raw_101;
      raw_value = raw_101;
    }
    
    String json = "{\"success\":true,\"step\":" + String(step) + ",\"raw_value\":" + String(raw_value) + "}";
    request->send(200, "application/json", json);
  });
  Serial.println("WebServer: Route /api/calibration/step registered");
  Serial.flush();
  
  // API endpoint: clear calibration
  server.on("/api/calibration/clear", HTTP_GET, [](AsyncWebServerRequest *request) {
    extern void clearCalibration();
    clearCalibration();
    request->send(200, "application/json", "{\"success\":true,\"message\":\"Calibration cleared\"}");
  });
  Serial.println("WebServer: Route /api/calibration/clear registered");
  Serial.flush();

  // API endpoint: store current capacitance as undeformed C0 (averaged)
  server.on("/api/calibration/store_c0", HTTP_GET, [](AsyncWebServerRequest *request) {
    extern bool storeCapacitanceC0(int);
    extern float cap_c0;
    int count = CALIB_SAMPLES;
    if (request->hasParam("count")) count = request->getParam("count")->value().toInt();
    bool ok = storeCapacitanceC0(count);
    String json = "{\"success\":" + String(ok ? "true" : "false") + ",\"c0\":" + String(cap_c0, 6) + "}";
    request->send(200, "application/json", json);
  });
  Serial.println("WebServer: Route /api/calibration/store_c0 registered");
  Serial.flush();

  // API endpoint: reset pressure zero offset (manual)
  server.on("/api/pressure/reset", HTTP_GET, [](AsyncWebServerRequest *request) {
    extern void resetPressureOffset();
    extern float pressureOffset;
    resetPressureOffset();
    String json = "{\"offset\":" + String(pressureOffset, 6) + "}";
    request->send(200, "application/json", json);
  });
  Serial.println("WebServer: Route /api/pressure/reset registered");
  Serial.flush();

  // ============ PNEUNET DEFORMATION CALIBRATION API ENDPOINTS ============
  
  // API endpoint: start PneuNet deformation calibration (auto-start collection)
  server.on("/api/calibration/pneunet_start", HTTP_GET, [](AsyncWebServerRequest *request) {
    extern bool startPneunetDeformationCalibration();
    extern void startCollection();
    startCollection();  // Auto-start data collection
    bool ok = startPneunetDeformationCalibration();
    String json = "{\"success\":" + String(ok ? "true" : "false") + "}";
    request->send(200, "application/json", json);
  });
  Serial.println("WebServer: Route /api/calibration/pneunet_start registered");
  Serial.flush();
  
  // API endpoint: finish PneuNet deformation calibration (auto-stop collection)
  server.on("/api/calibration/pneunet_finish", HTTP_GET, [](AsyncWebServerRequest *request) {
    extern bool finishPneunetDeformationCalibration();
    extern int calib_curve_points;
    extern void stopCollection();
    stopCollection();  // Auto-stop data collection
    bool ok = finishPneunetDeformationCalibration();
    String json = "{\"success\":" + String(ok ? "true" : "false") + ",\"points\":" + String(calib_curve_points) + "}";
    request->send(200, "application/json", json);
  });
  Serial.println("WebServer: Route /api/calibration/pneunet_finish registered");
  Serial.flush();
  
  // API endpoint: abort PneuNet deformation calibration (don't stop collection)
  server.on("/api/calibration/pneunet_abort", HTTP_GET, [](AsyncWebServerRequest *request) {
    extern void abortPneunetDeformationCalibration();
    abortPneunetDeformationCalibration();
    request->send(200, "application/json", "{\"success\":true}");
  });
  Serial.println("WebServer: Route /api/calibration/pneunet_abort registered");
  Serial.flush();
  
  // API endpoint: get PneuNet deformation calibration status
  server.on("/api/calibration/pneunet_status", HTTP_GET, [](AsyncWebServerRequest *request) {
    extern int calib_curve_points;
    extern int calib_curve_sqrt_points;
    extern int calib_raw_sample_count;
    extern volatile bool pneunetDeformationCalibrationInProgress;
    extern float calib_curve_delta_c_a;
    extern float calib_curve_delta_c_b;
    extern float calib_curve_delta_c_c;
    extern float calib_curve_sqrt_a;
    extern float calib_curve_sqrt_b;
    extern float calib_curve_sqrt_c;
    extern int pneunet_fit_method;
    extern float cap_c0;
    
    String json = "{\"in_progress\":" + String(pneunetDeformationCalibrationInProgress ? "true" : "false") + 
                  ",\"points\":" + String(calib_curve_points) + 
                  ",\"fit_method\":" + String(pneunet_fit_method) +
                  ",\"a\":" + String(calib_curve_delta_c_a, 6) + 
                  ",\"b\":" + String(calib_curve_delta_c_b, 6) +
                  ",\"c\":" + String(calib_curve_delta_c_c, 4) +
                  ",\"sqrt_points\":" + String(calib_curve_sqrt_points) +
                  ",\"sqrt_a\":" + String(calib_curve_sqrt_a, 6) +
                  ",\"sqrt_b\":" + String(calib_curve_sqrt_b, 6) +
                  ",\"sqrt_c\":" + String(calib_curve_sqrt_c, 4) +
                  ",\"c0\":" + String(cap_c0, 3);
    
    // Include raw sample data for plotting
    json += ",\"samples\":[";
    for (int i = 0; i < calib_raw_sample_count && i < MAX_CALIB_SAMPLES_PER_POINT; i++) {
      if (i > 0) json += ",";
      json += "{\"dc\":" + String(calib_raw_samples[i].delta_c, 2) + ",\"p\":" + String(calib_raw_samples[i].pressure, 4) + "}";
    }
    json += "]}";
    
    request->send(200, "application/json", json);
  });
  Serial.println("WebServer: Route /api/calibration/pneunet_status registered");
  Serial.flush();
  
  // API endpoint: apply a specific fit method to the PneuNet calibration
  server.on("/api/calibration/pneunet_apply_fit", HTTP_GET, [](AsyncWebServerRequest *request) {
    extern bool applyPneunetFitMethod(int method);
    
    // Get method parameter (0=LINEAR, 1=SQRT)
    int method = 0;
    if (request->hasParam("method")) {
      method = request->getParam("method")->value().toInt();
    }
    
    bool success = applyPneunetFitMethod(method);
    
    String json = "{\"success\":" + String(success ? "true" : "false") + 
                  ",\"method\":" + String(method);
    
    if (!success) {
      json += ",\"error\":\"Failed to apply fit method\"";
    }
    
    json += "}";
    request->send(200, "application/json", json);
  });
  Serial.println("WebServer: Route /api/calibration/pneunet_apply_fit registered");
  Serial.flush();

  // API endpoint: clear PneuNet deformation calibration
  server.on("/api/calibration/pneunet_clear", HTTP_GET, [](AsyncWebServerRequest *request) {
    extern void clearPneunetDeformationCalibration();
    clearPneunetDeformationCalibration();
    request->send(200, "application/json", "{\"success\":true}");
  });
  Serial.println("WebServer: Route /api/calibration/pneunet_clear registered");
  Serial.flush();

  // ============ OBJECT SIZE CALIBRATION API ENDPOINTS ============
  
  // API endpoint: get object size calibration status
  server.on("/api/calibration/objectsize_status", HTTP_GET, [](AsyncWebServerRequest *request) {
    extern volatile bool objectSizeCalibrationInProgress;
    extern int object_size_calib_current_index;
    extern int object_size_calib_sample_count;
    extern int object_size_calib_count;
    extern const float OBJECT_SIZE_CALIB_DISTANCES[];
    extern const int OBJECT_SIZE_CALIB_COUNT;
    extern ObjectSizeCalibPoint object_size_calib_points[];
    
    String json = "{";
    json += "\"in_progress\":" + String(objectSizeCalibrationInProgress ? "true" : "false") + ",";
    json += "\"current_index\":" + String(object_size_calib_current_index) + ",";
    json += "\"total_distances\":" + String(OBJECT_SIZE_CALIB_COUNT) + ",";
    json += "\"sample_count\":" + String(object_size_calib_sample_count) + ",";
    json += "\"saved_count\":" + String(object_size_calib_count);
    
    if (objectSizeCalibrationInProgress && object_size_calib_current_index < OBJECT_SIZE_CALIB_COUNT) {
      json += ",\"current_distance_mm\":" + String((int)OBJECT_SIZE_CALIB_DISTANCES[object_size_calib_current_index]);
    }
    
    // Add stored calibration data for plot
    if (object_size_calib_count > 0) {
      json += ",\"calibration_data\":[";
      for (int i = 0; i < object_size_calib_count; i++) {
        if (i > 0) json += ",";
        json += "{";
        json += "\"object_size_mm\":" + String((int)object_size_calib_points[i].object_size_mm) + ",";
        json += "\"delta_c\":" + String(object_size_calib_points[i].delta_c, 2);
        json += "}";
      }
      json += "]";
    }
    
    json += "}";
    request->send(200, "application/json", json);
  });
  Serial.println("WebServer: Route /api/calibration/objectsize_status registered");
  Serial.flush();
  
  // API endpoint: start object size calibration
  server.on("/api/calibration/objectsize_start", HTTP_GET, [](AsyncWebServerRequest *request) {
    extern bool startObjectSizeCalibration();
    bool success = startObjectSizeCalibration();
    request->send(200, "application/json", "{\"success\":" + String(success ? "true" : "false") + "}");
  });
  Serial.println("WebServer: Route /api/calibration/objectsize_start registered");
  Serial.flush();
  
  // API endpoint: finish current object size calibration point
  server.on("/api/calibration/objectsize_finish_point", HTTP_GET, [](AsyncWebServerRequest *request) {
    extern bool finishObjectSizeCalibration();
    bool completed = finishObjectSizeCalibration();  // Returns true if all points complete
    request->send(200, "application/json", "{\"point_complete\":true,\"all_complete\":" + String(completed ? "true" : "false") + "}");
  });
  Serial.println("WebServer: Route /api/calibration/objectsize_finish_point registered");
  Serial.flush();
  
  // API endpoint: abort object size calibration
  server.on("/api/calibration/objectsize_abort", HTTP_GET, [](AsyncWebServerRequest *request) {
    extern void abortObjectSizeCalibration();
    abortObjectSizeCalibration();
    request->send(200, "application/json", "{\"success\":true}");
  });
  Serial.println("WebServer: Route /api/calibration/objectsize_abort registered");
  Serial.flush();
  
  // API endpoint: clear object size calibration data
  server.on("/api/calibration/objectsize_clear", HTTP_GET, [](AsyncWebServerRequest *request) {
    extern void clearObjectSizeCalibration();
    clearObjectSizeCalibration();
    request->send(200, "application/json", "{\"success\":true}");
  });
  Serial.println("WebServer: Route /api/calibration/objectsize_clear registered");
  Serial.flush();

  Serial.println("WebServer: Calling server.begin()...");
  Serial.flush();
  server.begin();
  Serial.println("WebServer: server.begin() completed");
  Serial.flush();
}

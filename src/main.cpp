// ——— Blynk IoT required definitions ———
#define BLYNK_TEMPLATE_ID    "YOURTEMPLATE_ID"
#define BLYNK_TEMPLATE_NAME  "YOUR_TEMPLATE_NAME"
// ----------------------------------------

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <BlynkSimpleEsp8266.h>
#include <LittleFS.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

// ---------- USER CONFIG ----------
const char* WIFI_SSID = "SSID";
const char* WIFI_PASS = "PASSWORD";
char auth[] = "YOUR_BLYNK_API";
// ---------------------------------

ESP8266WebServer server(80);

const uint16_t READ_INTERVAL_MS = 1000;
const float VREF = 3.3;
const float R_FIXED = 10000.0;
const int ADC_MAX = 1023;

unsigned long lastRead = 0;
int lastAdc = 0;
float lastLux = 0;
float lastResistance = 0.0;
bool configMode = false;
bool blynkConnected = false;
String deviceIP = "";

float adcToResistance(int adc) {
  if (adc <= 0) return 1000000.0;
  if (adc >= ADC_MAX) return 10.0;
  return R_FIXED * ((float)(ADC_MAX - adc) / (float)adc);
}

float resistanceToLux(float R) {
  if (R <= 10) return 10000.0;
  if (R >= 1000000) return 1.0;
  float lux = 1000000.0 / pow(R, 1.3);
  if (lux < 1.0) return 1.0;
  if (lux > 50000.0) return 50000.0;
  return lux;
}

String getLightStatus(int adc) {
  if (adc > 900) return "🌞 Very Bright";
  if (adc > 700) return "☀️ Bright";
  if (adc > 400) return "⛅ Medium";
  if (adc > 100) return "🌙 Dim";
  return "🌑 Dark";
}

String jsonReading() {
  String s = "{";
  s += "\"adc\":" + String(lastAdc) + ",";
  s += "\"voltage\":" + String(((float)lastAdc / ADC_MAX) * VREF, 3) + ",";
  s += "\"resistance\":" + String(lastResistance, 1) + ",";
  s += "\"lux\":" + String(lastLux, 1) + ",";
  s += "\"lightStatus\":\"" + getLightStatus(lastAdc) + "\",";
  s += "\"blynkConnected\":" + String(blynkConnected ? "true" : "false");
  s += "}";
  return s;
}

void logData() {
  File file = LittleFS.open("/datalog.txt", "a");
  if (file) {
    file.print(millis());
    file.print(",");
    file.print(lastAdc);
    file.print(",");
    file.print(lastLux, 1);
    file.print(",");
    file.println(lastResistance, 1);
    file.close();
  }
}

void handleRoot() {
  String page = R"=====(<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>NodeMCU Lux Meter</title>
  <style>
    body{font-family:system-ui,Segoe UI,Roboto,Arial;margin:0;padding:16px;background:#f7f7fb;color:#111}
    .card{background:#fff;border-radius:12px;padding:16px;box-shadow:0 6px 18px rgba(0,0,0,0.06);max-width:720px;margin:12px auto}
    h1{font-size:20px;margin:0 0 8px}
    .lux-container{display:flex;align-items:center;justify-content:center;gap:20px;margin:20px 0}
    .big{font-size:48px;font-weight:700;margin:0}
    .status-badge{font-size:24px;font-weight:600;padding:8px 16px;border-radius:8px;background:#f8f9fa}
    .status-dark{background:#6c757d;color:white}
    .status-dim{background:#17a2b8;color:white}
    .status-medium{background:#28a745;color:white}
    .status-bright{background:#ffc107;color:black}
    .status-very-bright{background:#fd7e14;color:white}
    .sub{color:#666;margin-bottom:12px;text-align:center}
    .row{display:flex;gap:12px;flex-wrap:wrap;align-items:center}
    .small{flex:1;min-width:180px}
    canvas{width:100% !important;height:180px !important}
    .data-grid{display:grid;grid-template-columns:1fr 1fr;gap:12px;margin:16px 0}
    .data-item{background:#f8f9fa;padding:12px;border-radius:8px}
    .data-label{font-size:12px;color:#666;margin-bottom:4px}
    .data-value{font-size:18px;font-weight:600}
    footer{font-size:12px;color:#888;text-align:center;margin-top:12px}
    .status{display:inline-block;padding:4px 8px;border-radius:4px;font-size:12px;margin-left:8px}
    .connected{background:#d4edda;color:#155724}
    .disconnected{background:#f8d7da;color:#721c24}
    .info-box{background:#e7f3ff;border-left:4px solid #007bff;padding:12px;margin:16px 0;border-radius:4px}
    .ip-address{background:#fff3cd;border-left:4px solid #ffc107;padding:12px;margin:16px 0;border-radius:4px}
  </style>
</head>
<body>
  <div class="card">
    <h1>NodeMCU Lux Meter</h1>
    <div class="sub">Realtime light monitoring 
      <span id="blynkStatus">Blynk: <span class="status disconnected">Disconnected</span></span>
    </div>
    
    <div class="ip-address">
      <strong>Access this page using:</strong><br>
      <span id="ipAddress">Connecting...</span>
    </div>
    
    <div class="info-box">
      <strong>Circuit: 3.3V → 10K resistor → A0 → LDR → GND</strong><br>
      <strong>Expected Behavior:</strong><br>
      • Bright light → Low LDR resistance → High ADC → High Lux<br>
      • Dark → High LDR resistance → Low ADC → Low Lux
    </div>
    
    <div class="lux-container">
      <div class="big" id="luxValue">--</div>
      <div class="status-badge" id="lightStatusBadge">--</div>
    </div>
    <div class="sub">Illuminance (lux)</div>
    
    <div class="data-grid">
      <div class="data-item">
        <div class="data-label">A0 (ADC Value)</div>
        <div class="data-value" id="adcValue">-</div>
      </div>
      <div class="data-item">
        <div class="data-label">LDR Resistance</div>
        <div class="data-value" id="resistanceValue">- Ω</div>
      </div>
      <div class="data-item">
        <div class="data-label">Voltage at A0</div>
        <div class="data-value" id="voltageValue">- V</div>
      </div>
      <div class="data-item">
        <div class="data-label">Light Condition</div>
        <div class="data-value" id="conditionValue">-</div>
      </div>
    </div>
    
    <div class="row">
      <div class="small" style="flex:2">
        <canvas id="chart"></canvas>
      </div>
    </div>
    
    <div style="margin-top:16px">
      <button onclick="downloadData()" style="padding:8px 16px;background:#007bff;color:white;border:none;border-radius:4px;cursor:pointer">
        Download Data Log
      </button>
      <button onclick="calibrate()" style="padding:8px 16px;background:#28a745;color:white;border:none;border-radius:4px;cursor:pointer;margin-left:8px">
        Calibrate
      </button>
    </div>
    
    <footer>Device IP: <span id="footerIP">Connecting...</span> | OTA updates enabled</footer>
  </div>

  <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
  <script>
    let chart = null;

    function initializeChart() {
      const ctx = document.getElementById('chart').getContext('2d');
      const data = { 
        labels: [], 
        datasets: [
          { 
            label: 'Illuminance (lux)', 
            data: [],
            borderColor: '#007bff',
            backgroundColor: 'rgba(0, 123, 255, 0.1)',
            fill: true,
            tension: 0.4
          }
        ] 
      };
      const cfg = { 
        type: 'line', 
        data: data, 
        options: { 
          animation: false, 
          responsive: true, 
          scales: { 
            y: { 
              beginAtZero: true,
              title: {
                display: true,
                text: 'Lux'
              }
            },
            x: {
              title: {
                display: true,
                text: 'Time'
              }
            }
          }
        }
      };
      if (chart) {
        chart.destroy();
      }
      chart = new Chart(ctx, cfg);
    }

    function updateIP() {
      const currentUrl = window.location.href;
      document.getElementById('ipAddress').innerHTML = 
        `<a href="${currentUrl}" style="color: #007bff; text-decoration: none; font-weight: bold;">${currentUrl}</a>`;
      document.getElementById('footerIP').textContent = window.location.hostname;
    }

    function updateLightStatus(status) {
      const badge = document.getElementById('lightStatusBadge');
      badge.textContent = status;
      
      // Remove all status classes
      badge.className = 'status-badge';
      
      // Add appropriate status class
      if (status.includes('Very Bright')) badge.classList.add('status-very-bright');
      else if (status.includes('Bright')) badge.classList.add('status-bright');
      else if (status.includes('Medium')) badge.classList.add('status-medium');
      else if (status.includes('Dim')) badge.classList.add('status-dim');
      else if (status.includes('Dark')) badge.classList.add('status-dark');
    }

    function getLightCondition(adc) {
      if (adc > 900) return 'Very Bright';
      if (adc > 700) return 'Bright';
      if (adc > 400) return 'Medium';
      if (adc > 100) return 'Dim';
      return 'Dark';
    }

    function pushReading(lux, adc) {
      const t = new Date().toLocaleTimeString();
      chart.data.labels.push(t);
      chart.data.datasets[0].data.push(lux);
      if (chart.data.labels.length > 20) { 
        chart.data.labels.shift(); 
        chart.data.datasets[0].data.shift(); 
      }
      chart.update('none');
    }

    async function fetchReading() {
      try {
        const controller = new AbortController();
        const timeoutId = setTimeout(() => controller.abort(), 3000);
        
        const res = await fetch('/reading', { signal: controller.signal });
        clearTimeout(timeoutId);
        
        if (!res.ok) throw new Error('HTTP ' + res.status);
        const j = await res.json();
        
        document.getElementById('luxValue').textContent = j.lux.toFixed(1);
        document.getElementById('adcValue').textContent = j.adc;
        document.getElementById('resistanceValue').textContent = Math.round(j.resistance) + ' Ω';
        document.getElementById('voltageValue').textContent = j.voltage + ' V';
        document.getElementById('conditionValue').textContent = getLightCondition(j.adc);
        
        // Update the light status badge
        updateLightStatus(j.lightStatus);
        
        pushReading(j.lux, j.adc);
        
        // Update Blynk status
        const statusSpan = document.querySelector('#blynkStatus .status');
        if (j.blynkConnected) {
          statusSpan.className = 'status connected';
          statusSpan.textContent = 'Connected';
        } else {
          statusSpan.className = 'status disconnected';
          statusSpan.textContent = 'Disconnected';
        }
        
      } catch (e) {
        console.warn('Fetch error:', e);
        document.getElementById('luxValue').textContent = 'Error';
        document.getElementById('adcValue').textContent = 'Error';
        document.getElementById('lightStatusBadge').textContent = 'Error';
      }
    }

    function downloadData() {
      window.open('/data', '_blank');
    }

    function calibrate() {
      const dark = confirm('Cover the LDR completely (make it dark) and click OK');
      if (dark) {
        fetch('/calibrate?type=dark').then(() => {
          const bright = confirm('Now expose the LDR to bright light and click OK');
          if (bright) {
            fetch('/calibrate?type=bright').then(() => {
              alert('Calibration complete! Check Serial Monitor for values.');
            });
          }
        });
      }
    }

    // Initialize everything when page loads
    document.addEventListener('DOMContentLoaded', function() {
      initializeChart();
      updateIP();
      fetchReading();
      setInterval(fetchReading, 1000);
    });
  </script>
</body>
</html>
)=====";
  server.send(200, "text/html", page);
}

void handleReading() {
  server.send(200, "application/json", jsonReading());
}

void handleDataLog() {
  if (LittleFS.exists("/datalog.txt")) {
    File file = LittleFS.open("/datalog.txt", "r");
    if (file) {
      server.sendHeader("Content-Type", "text/plain");
      server.sendHeader("Content-Disposition", "attachment; filename=datalog.txt");
      server.streamFile(file, "text/plain");
      file.close();
      return;
    }
  }
  server.send(404, "text/plain", "No data log found");
}

void handleCalibrate() {
  String type = server.arg("type");
  int adc = analogRead(A0);
  float R = adcToResistance(adc);
  float lux = resistanceToLux(R);
  
  Serial.println("=== CALIBRATION ===");
  Serial.println("Circuit: 3.3V → 10K → A0 → LDR → GND");
  
  if (type == "dark") {
    Serial.println("DARK CALIBRATION (LDR covered):");
  } else if (type == "bright") {
    Serial.println("BRIGHT CALIBRATION (LDR exposed to light):");
  }
  
  Serial.print("ADC: "); Serial.println(adc);
  Serial.print("Voltage: "); Serial.print((adc * VREF) / ADC_MAX, 3); Serial.println(" V");
  Serial.print("LDR Resistance: "); Serial.print(R, 1); Serial.println(" Ω");
  Serial.print("Calculated Lux: "); Serial.println(lux, 1);
  Serial.print("Light Status: "); Serial.println(getLightStatus(adc));
  Serial.println("===================");
  
  server.send(200, "text/plain", "Calibration recorded: " + type + " - ADC: " + String(adc));
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void startConfigMode() {
  configMode = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP("LuxMeter-Config", "12345678");
  deviceIP = WiFi.softAPIP().toString();
  Serial.println("Config mode started");
  Serial.print("AP IP: "); 
  Serial.println(deviceIP);
}

void setupOTA() {
  ArduinoOTA.setHostname("nodemcu-luxmeter");
  ArduinoOTA.setPassword("admin123");
  
  ArduinoOTA.onStart([]() {
    Serial.println("OTA Update Starting...");
  });
  
  ArduinoOTA.onEnd([]() {
    Serial.println("\nOTA Update Complete!");
  });
  
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  
  ArduinoOTA.begin();
  Serial.println("OTA Ready");
}

// Blynk event handlers
BLYNK_CONNECTED() {
  Serial.println("Connected to Blynk server");
  blynkConnected = true;
}

BLYNK_DISCONNECTED() {
  Serial.println("Disconnected from Blynk server");
  blynkConnected = false;
}

void sendToBlynk() {
  Blynk.virtualWrite(V0, lastLux);        // Lux value
  Blynk.virtualWrite(V1, lastAdc);        // ADC value
  Blynk.virtualWrite(V2, lastResistance); // Resistance
}

void setup() {
  Serial.begin(9600);
  delay(100);
  Serial.println();
  Serial.println("=== NodeMCU Lux Meter ===");
  Serial.println("Circuit: 3.3V → 10K resistor → A0 → LDR → GND");
  Serial.println("Expected: Bright → Low R → High ADC → High Lux");

  // Initialize LittleFS
  if (LittleFS.begin()) {
    Serial.println("LittleFS mounted successfully");
  } else {
    Serial.println("LittleFS mount failed");
  }

  // Connect to WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  
  Serial.print("Connecting to WiFi");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 30000) {
    delay(500);
    Serial.print(".");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    deviceIP = WiFi.localIP().toString();
    Serial.println();
    Serial.println("=== WiFi Connected ===");
    Serial.print("IP Address: ");
    Serial.println(deviceIP);
    Serial.println("=====================");
    
    // Try mDNS but don't rely on it
    if (MDNS.begin("luxmeter")) {
      Serial.println("mDNS started: http://luxmeter.local");
    } else {
      Serial.println("mDNS failed - use IP address above");
    }
    
    // Start Blynk
    Blynk.config(auth);
    Blynk.connect(5000);
    
    // Setup OTA
    setupOTA();
    
    // Setup web server
    server.on("/", handleRoot);
    server.on("/reading", handleReading);
    server.on("/data", handleDataLog);
    server.on("/calibrate", handleCalibrate);
    server.onNotFound(handleNotFound);
    server.begin();
    
    Serial.println("Web server started");
    Serial.println("Ready for OTA updates");
    Serial.println("Access the web interface at:");
    Serial.print("http://");
    Serial.println(deviceIP);
    
  } else {
    Serial.println();
    Serial.println("WiFi connection failed");
    startConfigMode();
  }
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    Blynk.run();
    ArduinoOTA.handle();
    MDNS.update();
  }
  
  server.handleClient();

  unsigned long now = millis();
  if (now - lastRead >= READ_INTERVAL_MS) {
    lastRead = now;
    
    lastAdc = analogRead(A0);
    lastResistance = adcToResistance(lastAdc);
    lastLux = resistanceToLux(lastResistance);

    // Debug output
    Serial.print("ADC: "); Serial.print(lastAdc);
    Serial.print(" | Voltage: "); Serial.print((lastAdc * VREF) / ADC_MAX, 3);
    Serial.print("V | R: "); Serial.print(lastResistance, 0);
    Serial.print("Ω | Lux: "); Serial.print(lastLux, 1);
    Serial.print(" | Status: "); Serial.println(getLightStatus(lastAdc));

    // Log data
    logData();
  if (WiFi.status() == WL_CONNECTED && blynkConnected){
    sendToBlynk();
    }
}
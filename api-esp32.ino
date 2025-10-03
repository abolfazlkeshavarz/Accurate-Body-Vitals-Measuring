
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <MAX30100_PulseOximeter.h>   // OXullo's library

const char* STA_SSID = "P30";
const char* STA_PASS = "!!!!1112";

const char* AP_SSID  = "CS-Guilan";
const char* AP_PASS  = "11111111";  //at least 8 char


const int SDA_PIN = 13;  // GPIO13
const int SCL_PIN = 14;  // GPIO14 

const uint8_t I2C_ADDR_MAX30205 = 0x48;
const uint8_t REG_TEMP = 0x00;

PulseOximeter pox;

volatile float g_temp_c = NAN;
volatile float g_hr_bpm = NAN;
volatile float g_spo2   = NAN;

// ---- timings ----
unsigned long bootMs        = 0;
unsigned long lastReportMs  = 0;
unsigned long lastTempMs    = 0;

const uint32_t REPORT_PERIOD_MS = 1000;  
const uint32_t TEMP_PERIOD_MS   = 500;    

WebServer server(80);

float readMAX30205C() {
  Wire.beginTransmission(I2C_ADDR_MAX30205);
  Wire.write(REG_TEMP);
  if (Wire.endTransmission(false) != 0) return NAN; // repeated start
  if (Wire.requestFrom((int)I2C_ADDR_MAX30205, 2) != 2) return NAN;
  int msb = Wire.read();
  int lsb = Wire.read();
  int16_t raw = (int16_t)((msb << 8) | lsb);
  return raw / 256.0f; // 1 LSB = 1/256 °C
}

void sendJSON(const String& body, int code = 200) {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Cache-Control", "no-cache");
  server.send(code, "application/json", body);
}

void handleTemp() {
  String json = "{";
  if (isnan(g_temp_c)) {
    json += "\"ok\":false,\"error\":\"no_recent_sample\"";
  } else {
    json += "\"ok\":true,\"temp_c\":" + String(g_temp_c, 2);
  }
  json += "}";
  sendJSON(json);
}

void handleVitals() {
  String json = "{";
  json += "\"ok\":true,";
  json += "\"temp_c\":" + (isnan(g_temp_c) ? String("null") : String(g_temp_c, 2)) + ",";
  json += "\"hr_bpm\":" + (isnan(g_hr_bpm) ? String("null") : String(g_hr_bpm, 1)) + ",";
  json += "\"spo2\":"   + (isnan(g_spo2)   ? String("null") : String(g_spo2, 1));
  json += "}";
  sendJSON(json);
}

void handleValues() {
  String json = "{\"bpm\": ";
  if (!isnan(g_hr_bpm) && g_hr_bpm > 20 && g_hr_bpm < 220) json += String(g_hr_bpm, 1);
  else json += "null";
  json += ", \"spo2\": ";
  if (!isnan(g_spo2) && g_spo2 >= 70 && g_spo2 <= 100) json += String(g_spo2, 1);
  else json += "null";
  json += "}";
  Serial.println(json);
  sendJSON(json);
}

void handleInfo() {
  IPAddress apIP  = WiFi.softAPIP();
  IPAddress staIP = WiFi.localIP();
  unsigned long up = (millis() - bootMs) / 1000;
  String json = "{";
  json += "\"ok\":true,";
  json += "\"ssid_ap\":\"" + String(AP_SSID) + "\",";
  json += "\"ap_ip\":\"" + apIP.toString() + "\",";
  json += "\"sta_connected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
  json += "\"sta_ip\":\"" + (staIP.toString()) + "\",";
  json += "\"uptime_s\":" + String(up);
  json += "}";
  sendJSON(json);
}

void handleOptions() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  server.send(204, "text/plain", "");
}

// ---------- Wi-Fi ----------
void setupAP_STA() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(IPAddress(192,168,4,1),
                    IPAddress(192,168,4,1),
                    IPAddress(255,255,255,0));
  WiFi.softAP(AP_SSID, AP_PASS);
  WiFi.begin(STA_SSID, STA_PASS); // non-blocking
}

// ---------- MAX30100 ----------
void onBeatDetected() {
  Serial.println("Beat!");
}

bool initMAX30100() {
  if (!pox.begin()) return false;
  pox.setIRLedCurrent(MAX30100_LED_CURR_11MA);
  pox.setOnBeatDetectedCallback(onBeatDetected);
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(200);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);
  delay(50);               

  bootMs = millis();

  setupAP_STA();

  // Routes
  server.on("/api/temp",    HTTP_GET,      handleTemp);
  server.on("/api/vitals",  HTTP_GET,      handleVitals);
  server.on("/values",      HTTP_GET,      handleValues);
  server.on("/api/info",    HTTP_GET,      handleInfo);

  server.on("/api/temp",    HTTP_OPTIONS,  handleOptions);
  server.on("/api/vitals",  HTTP_OPTIONS,  handleOptions);
  server.on("/values",      HTTP_OPTIONS,  handleOptions);
  server.on("/api/info",    HTTP_OPTIONS,  handleOptions);

  server.onNotFound([]() {
    if (server.method() == HTTP_OPTIONS) { handleOptions(); return; }
    server.send(404, "text/plain", "Not found");
  });

  server.begin();
  Serial.println("HTTP server started");
  Serial.print("AP  SSID: "); Serial.println(AP_SSID);
  Serial.print("AP  IP  : "); Serial.println(WiFi.softAPIP());
  Serial.print("STA SSID: "); Serial.println(STA_SSID);

  if (!initMAX30100()) {
    Serial.println("MAX30100 init FAILED (wiring/power/pullups?)");
  } else {
    Serial.println("MAX30100 init OK");
  }
}

void loop() {
  // Web
  server.handleClient();

  // Quiet log once STA connects
  static bool printed = false;
  if (!printed && WiFi.status() == WL_CONNECTED) {
    printed = true;
    Serial.print("STA connected, IP: ");
    Serial.println(WiFi.localIP());
  }

  pox.update();
  delay(1);

  unsigned long now = millis();

  if (now - lastReportMs >= REPORT_PERIOD_MS) {
    lastReportMs = now;

    float hr   = pox.getHeartRate();
    float spo2 = pox.getSpO2();

    // cache values
    g_hr_bpm = (hr   > 20 && hr   < 220) ? hr   : NAN;
    g_spo2   = (spo2 >= 70 && spo2 <= 100) ? spo2 : NAN;

    Serial.print("Heart BPM:");
    if (isnan(g_hr_bpm)) Serial.print("null");
    else Serial.print(g_hr_bpm, 1);
    Serial.print("-----");
    Serial.print("Oxygen Percent:");
    if (isnan(g_spo2)) Serial.print("null");
    else Serial.print(g_spo2, 1);
    Serial.println();
  }

  if (now - lastTempMs >= TEMP_PERIOD_MS) {
    lastTempMs = now;
    float t = readMAX30205C();
    if (!isnan(t)) g_temp_c = t; // keep last good sample if read failed
  }
}

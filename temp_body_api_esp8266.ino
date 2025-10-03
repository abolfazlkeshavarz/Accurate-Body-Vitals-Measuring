#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
// #include <Adafruit_GFX.h>
// #include <Adafruit_SSD1306.h>

const char* STA_SSID = "P30";
const char* STA_PASS = "!!!!1111";

const char* AP_SSID  = "CS-Guilan";
const char* AP_PASS  = "11111111"; // more then 8 char please

const uint8_t I2C_ADDR_MAX30205 = 0x48;   // A2..A0 = GND
const uint8_t REG_TEMP = 0x00;
const int SDA_PIN = 0;  // ESP-01S GPIO0
const int SCL_PIN = 2;  // ESP-01S GPIO2

// #define SCREEN_WIDTH 128
// #define SCREEN_HEIGHT 64
// Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1); //

ESP8266WebServer server(80);
unsigned long bootMs;

float readMAX30205C() {
  Wire.beginTransmission(I2C_ADDR_MAX30205);
  Wire.write(REG_TEMP);
  if (Wire.endTransmission(false) != 0) return NAN; // repeated start
  if (Wire.requestFrom((int)I2C_ADDR_MAX30205, 2) != 2) return NAN;
  int msb = Wire.read();
  int lsb = Wire.read();
  int16_t raw = (int16_t)((msb << 8) | lsb);
  return raw / 256.0f; // 1 LSB = 1/256 °C, two's complement
}

void sendJSON(const String& body, int code = 200) {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Cache-Control", "no-cache");
  server.send(code, "application/json", body);
}

void handleTemp() {
  float t = readMAX30205C();
  String json = "{";
  if (isnan(t)) {
    json += "\"ok\":false,\"error\":\"i2c_read_failed\"";
  } else {
    json += "\"ok\":true,\"temp_c\":" + String(t, 2);
  }
  json += "}";
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

void setupAP_STA() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(IPAddress(192,168,4,1), IPAddress(192,168,4,1), IPAddress(255,255,255,0));
  WiFi.softAP(AP_SSID, AP_PASS);
  WiFi.begin(STA_SSID, STA_PASS);
}

void setup() {
  Wire.begin(SDA_PIN, SCL_PIN);
  Serial.begin(115200);
  delay(200);
  bootMs = millis();

  // // --- Init OLED ---
  // if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
  //   Serial.println(F("SSD1306 allocation failed"));
  //   for (;;); // halt
  // }
  // display.clearDisplay();
  // display.setTextColor(SSD1306_WHITE);
  // display.setTextSize(1);
  // display.setCursor(0, 0);
  // display.println("MAX30205 Temp");
  // display.display();

  setupAP_STA();

  // Routes
  server.on("/api/temp", HTTP_GET, handleTemp);
  server.on("/api/info", HTTP_GET, handleInfo);
  server.on("/api/temp", HTTP_OPTIONS, handleOptions);
  server.on("/api/info", HTTP_OPTIONS, handleOptions);
  server.onNotFound([]() {
    if (server.method() == HTTP_OPTIONS) { handleOptions(); return; }
    server.send(404, "text/plain", "Not found");
  });

  server.begin();
  Serial.println("HTTP server started");
  Serial.print("AP  SSID: "); Serial.println(AP_SSID);
  Serial.print("AP  IP  : "); Serial.println(WiFi.softAPIP());
  Serial.print("STA SSID: "); Serial.println(STA_SSID);
}

void loop() {
  server.handleClient();

  static bool printed = false;
  if (!printed && WiFi.status() == WL_CONNECTED) {
    printed = true;
    Serial.print("STA connected, IP: ");
    Serial.println(WiFi.localIP());
  }

  // float tC = readMAX30205C();

  // display.clearDisplay();
  // display.setTextSize(1);
  // display.setCursor(0, 0);
  // display.println("Temperature:");

  // display.setTextSize(2);
  // display.setCursor(0, 16);
  // if (isnan(tC)) {
  //   display.println("I2C ERROR");
  // } else {
  //   display.print(tC, 2);
  //   display.print(" ");
  //   display.write(247);  // degree symbol
  //   display.print("C");
  // }
  // display.display();   // update every loop
  // delay(1000);
}
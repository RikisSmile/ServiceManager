#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ===================== WiFi Credentials =====================
const char* ssid     = "#Telia-1E338C";
const char* password = "CPN9ZzdtxzAExwR8";

// Server endpoints
const char* urlList    = "http://192.168.1.89:6755/process/list";
const char* urlControl = "http://192.168.1.89:6755/process/control";

// ===================== OLED Setup =====================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ===================== Buttons =====================
#define BTN_UP    15
#define BTN_DOWN  5
#define BTN_ACT 23

void connectWiFi();
void fetchProcessList();
void displayProcesses();
void sendControl(const char* fn, int id);
int selectedIndex = 0;

// JSON storage
StaticJsonDocument<500> jsonDoc;
JsonArray processArray;

void setup() {
  Serial.begin(115200);

  // Init I2C & OLED
  Wire.begin(21, 22);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Initialize buttons
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_ACT, INPUT_PULLUP);

  // Connect to WiFi
  connectWiFi();

  // Fetch and show list
  fetchProcessList();
  displayProcesses();
}

void loop() {
  // Read buttons (active low)
  if (digitalRead(BTN_UP) == LOW) {
    if (selectedIndex > 0) selectedIndex--;
    displayProcesses();
    delay(200);
  }

  if (digitalRead(BTN_DOWN) == LOW) {
    if (selectedIndex < processArray.size() - 1) selectedIndex++;
    displayProcesses();
    delay(200);
  }

  // Action button (toggle start/end)
  if (digitalRead(BTN_ACT) == LOW) {
    if (processArray.size() > 0) {
      const char* currentStatus = processArray[selectedIndex]["status"];
      if (strcmp(currentStatus, "RUNNING") == 0) {
        sendControl("end", selectedIndex);
      } else {
        sendControl("start", selectedIndex);
      }
      delay(300);

      // Refresh the process list and UI
      fetchProcessList();
      displayProcesses();
    }
  }
}


// ===================== WiFi Connect =====================
void connectWiFi() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("WiFi Connecting...");
  display.display();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    display.print(".");
    display.display();
  }

  display.println("\nConnected!");
  display.display();
  delay(1000);
}

// ===================== Fetch JSON List =====================
void fetchProcessList() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.begin(urlList);
  int httpCode = http.GET();

  if (httpCode > 0) {
    String payload = http.getString();
    deserializeJson(jsonDoc, payload);
    processArray = jsonDoc.as<JsonArray>();
  }
  http.end();
}

// ===================== Display List =====================
void displayProcesses() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  int y = 0;
  for (int i = 0; i < processArray.size(); i++) {
    display.setCursor(0, y);
    if (i == selectedIndex) display.print("> ");
    else display.print("  ");

    display.print(processArray[i]["desc"].as<const char*>());
    display.print(":");
    display.println(processArray[i]["status"].as<const char*>());

    y += 8;
    if (y > SCREEN_HEIGHT - 8) break;
  }

  display.display();
}

// ===================== Send HTTP POST =====================
void sendControl(const char* fn, int id) {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.begin(urlControl);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  String body = "fn=" + String(fn) + "&id=" + String(id);
  int httpCode = http.POST(body);

  // Optional: show brief feedback
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Sent:");
  display.println(fn);
  display.print("id=");
  display.println(id);
  display.display();

  delay(500);
  http.end();
}

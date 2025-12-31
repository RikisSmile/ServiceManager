/**
 * @file Manager.ino
 * @brief Arduino ESP32 Process Management Controller
 * @version 1.0
 * @date 2025-01-01
 * 
 * Hardware controller for the Process Management System.
 * Features OLED display, WiFi connectivity, and physical buttons
 * for remote process monitoring and control.
 * 
 * Hardware Requirements:
 * - ESP32 development board
 * - SSD1306 OLED display (128x64)
 * - 3 push buttons (UP, DOWN, ACTION)
 * - Pull-up resistors for buttons
 * 
 * Connections:
 * - OLED: SDA=21, SCL=22
 * - Buttons: UP=15, DOWN=5, ACTION=23 (with pull-up)
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ===================== Configuration =====================
// WiFi Credentials - UPDATE THESE!
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// Server Configuration - UPDATE THESE!
const char* SERVER_HOST = "192.168.1.100";  // Process Management Server IP
const int SERVER_PORT = 6755;
const char* API_LIST_ENDPOINT = "/process/list";
const char* API_CONTROL_ENDPOINT = "/process/control";

// Hardware Configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

// Button Pins (with internal pull-up)
#define BTN_UP 15
#define BTN_DOWN 5
#define BTN_ACTION 23

// Timing Configuration
#define REFRESH_INTERVAL 10000  // 10 seconds
#define BUTTON_DEBOUNCE 200     // 200ms
#define WIFI_TIMEOUT 20000      // 20 seconds
#define HTTP_TIMEOUT 5000       // 5 seconds

// ===================== Global Variables =====================
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Process data storage
StaticJsonDocument<1024> processDoc;
JsonArray processes;

// UI state
int selectedIndex = 0;
int maxProcesses = 0;
unsigned long lastRefresh = 0;
unsigned long lastButtonPress = 0;
bool isConnected = false;

// Button states
bool btnUpPressed = false;
bool btnDownPressed = false;
bool btnActionPressed = false;

// ===================== Function Declarations =====================
void initializeHardware();
bool connectToWiFi();
void initializeDisplay();
void fetchProcessList();
void displayProcesses();
void displayError(const String& message);
void displayStatus(const String& message);
void handleButtons();
void sendControlCommand(const String& action, int processId);
String getServerUrl(const String& endpoint);
void showSplashScreen();

// ===================== Setup Function =====================
void setup() {
    Serial.begin(115200);
    Serial.println();
    Serial.println("🚀 Process Management Controller v1.0");
    Serial.println("=====================================");
    
    // Initialize hardware
    initializeHardware();
    
    // Show splash screen
    showSplashScreen();
    delay(2000);
    
    // Connect to WiFi
    if (connectToWiFi()) {
        displayStatus("WiFi Connected!");
        Serial.println("✅ WiFi connected successfully");
        Serial.print("📡 IP Address: ");
        Serial.println(WiFi.localIP());
        isConnected = true;
        
        // Initial data fetch
        fetchProcessList();
    } else {
        displayError("WiFi Failed!");
        Serial.println("❌ WiFi connection failed");
        isConnected = false;
    }
}

// ===================== Main Loop =====================
void loop() {
    // Check WiFi connection
    if (WiFi.status() != WL_CONNECTED) {
        if (isConnected) {
            Serial.println("⚠️ WiFi connection lost, attempting reconnect...");
            isConnected = false;
            displayError("WiFi Lost!");
        }
        
        if (connectToWiFi()) {
            isConnected = true;
            displayStatus("Reconnected!");
            delay(1000);
        } else {
            delay(5000); // Wait before retry
            return;
        }
    }
    
    // Handle button inputs
    handleButtons();
    
    // Periodic data refresh
    if (isConnected && (millis() - lastRefresh > REFRESH_INTERVAL)) {
        fetchProcessList();
    }
    
    // Small delay to prevent excessive CPU usage
    delay(50);
}

// ===================== Hardware Initialization =====================
void initializeHardware() {
    Serial.println("🔧 Initializing hardware...");
    
    // Initialize I2C
    Wire.begin(21, 22); // SDA=21, SCL=22
    
    // Initialize display
    initializeDisplay();
    
    // Initialize buttons with internal pull-up
    pinMode(BTN_UP, INPUT_PULLUP);
    pinMode(BTN_DOWN, INPUT_PULLUP);
    pinMode(BTN_ACTION, INPUT_PULLUP);
    
    Serial.println("✅ Hardware initialized");
}

void initializeDisplay() {
    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
        Serial.println("❌ OLED display initialization failed!");
        while (true) delay(1000); // Halt execution
    }
    
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.display();
    
    Serial.println("✅ OLED display initialized");
}

void showSplashScreen() {
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(0, 10);
    display.println("Process");
    display.println("Manager");
    display.setTextSize(1);
    display.setCursor(0, 50);
    display.println("v1.0 - Starting...");
    display.display();
}

// ===================== WiFi Functions =====================
bool connectToWiFi() {
    Serial.print("🌐 Connecting to WiFi: ");
    Serial.println(WIFI_SSID);
    
    displayStatus("Connecting WiFi...");
    
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - startTime < WIFI_TIMEOUT)) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    
    return (WiFi.status() == WL_CONNECTED);
}

// ===================== HTTP Functions =====================
String getServerUrl(const String& endpoint) {
    return "http://" + String(SERVER_HOST) + ":" + String(SERVER_PORT) + endpoint;
}

void fetchProcessList() {
    if (!isConnected) return;
    
    Serial.println("📋 Fetching process list...");
    displayStatus("Updating...");
    
    HTTPClient http;
    http.begin(getServerUrl(API_LIST_ENDPOINT));
    http.setTimeout(HTTP_TIMEOUT);
    
    int httpCode = http.GET();
    
    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        
        // Parse JSON response
        DeserializationError error = deserializeJson(processDoc, payload);
        if (error) {
            Serial.print("❌ JSON parsing failed: ");
            Serial.println(error.c_str());
            displayError("Parse Error!");
        } else {
            processes = processDoc.as<JsonArray>();
            maxProcesses = processes.size();
            
            // Ensure selected index is valid
            if (selectedIndex >= maxProcesses) {
                selectedIndex = maxProcesses > 0 ? maxProcesses - 1 : 0;
            }
            
            Serial.print("✅ Loaded ");
            Serial.print(maxProcesses);
            Serial.println(" processes");
            
            displayProcesses();
        }
    } else {
        Serial.print("❌ HTTP request failed: ");
        Serial.println(httpCode);
        displayError("Server Error!");
    }
    
    http.end();
    lastRefresh = millis();
}

void sendControlCommand(const String& action, int processId) {
    if (!isConnected) return;
    
    Serial.print("🔧 Sending command: ");
    Serial.print(action);
    Serial.print(" for process ID: ");
    Serial.println(processId);
    
    displayStatus("Sending " + action + "...");
    
    HTTPClient http;
    http.begin(getServerUrl(API_CONTROL_ENDPOINT));
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    http.setTimeout(HTTP_TIMEOUT);
    
    String postData = "fn=" + action + "&id=" + String(processId);
    int httpCode = http.POST(postData);
    
    if (httpCode == HTTP_CODE_OK) {
        String response = http.getString();
        Serial.print("✅ Command successful: ");
        Serial.println(response);
        displayStatus("Success!");
        
        // Refresh data after successful command
        delay(1000);
        fetchProcessList();
    } else {
        Serial.print("❌ Command failed: ");
        Serial.println(httpCode);
        displayError("Command Failed!");
    }
    
    http.end();
}

// ===================== Display Functions =====================
void displayProcesses() {
    display.clearDisplay();
    
    if (maxProcesses == 0) {
        display.setTextSize(1);
        display.setCursor(0, 20);
        display.println("No processes");
        display.println("configured");
        display.display();
        return;
    }
    
    // Header
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("Processes (");
    display.print(maxProcesses);
    display.println(")");
    display.drawLine(0, 10, SCREEN_WIDTH, 10, SSD1306_WHITE);
    
    // Display up to 4 processes
    int startIndex = max(0, selectedIndex - 1);
    int endIndex = min(maxProcesses, startIndex + 4);
    
    for (int i = startIndex; i < endIndex; i++) {
        JsonObject process = processes[i];
        
        int yPos = 15 + (i - startIndex) * 12;
        
        // Selection indicator
        if (i == selectedIndex) {
            display.fillRect(0, yPos - 1, SCREEN_WIDTH, 10, SSD1306_WHITE);
            display.setTextColor(SSD1306_BLACK);
        } else {
            display.setTextColor(SSD1306_WHITE);
        }
        
        display.setCursor(2, yPos);
        
        // Process info
        String desc = process["desc"].as<String>();
        String status = process["status"].as<String>();
        
        // Truncate description if too long
        if (desc.length() > 12) {
            desc = desc.substring(0, 12) + "...";
        }
        
        display.print(String(i) + ": " + desc);
        
        // Status indicator
        display.setCursor(100, yPos);
        if (status == "RUNNING") {
            display.print("RUN");
        } else {
            display.print("OFF");
        }
    }
    
    // Navigation hints
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 56);
    display.print("UP/DN:Nav ACT:Toggle");
    
    display.display();
}

void displayStatus(const String& message) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 20);
    display.println("Status:");
    display.setTextSize(2);
    display.setCursor(0, 35);
    display.println(message);
    display.display();
}

void displayError(const String& message) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 20);
    display.println("Error:");
    display.setTextSize(2);
    display.setCursor(0, 35);
    display.println(message);
    display.display();
}

// ===================== Button Handling =====================
void handleButtons() {
    unsigned long currentTime = millis();
    
    // Debounce check
    if (currentTime - lastButtonPress < BUTTON_DEBOUNCE) {
        return;
    }
    
    // Read button states (active LOW due to pull-up)
    bool upPressed = !digitalRead(BTN_UP);
    bool downPressed = !digitalRead(BTN_DOWN);
    bool actionPressed = !digitalRead(BTN_ACTION);
    
    // Handle UP button
    if (upPressed && !btnUpPressed) {
        btnUpPressed = true;
        lastButtonPress = currentTime;
        
        if (maxProcesses > 0) {
            selectedIndex = (selectedIndex - 1 + maxProcesses) % maxProcesses;
            Serial.print("⬆️ Selected process: ");
            Serial.println(selectedIndex);
            displayProcesses();
        }
    } else if (!upPressed) {
        btnUpPressed = false;
    }
    
    // Handle DOWN button
    if (downPressed && !btnDownPressed) {
        btnDownPressed = true;
        lastButtonPress = currentTime;
        
        if (maxProcesses > 0) {
            selectedIndex = (selectedIndex + 1) % maxProcesses;
            Serial.print("⬇️ Selected process: ");
            Serial.println(selectedIndex);
            displayProcesses();
        }
    } else if (!downPressed) {
        btnDownPressed = false;
    }
    
    // Handle ACTION button
    if (actionPressed && !btnActionPressed) {
        btnActionPressed = true;
        lastButtonPress = currentTime;
        
        if (maxProcesses > 0 && selectedIndex < maxProcesses) {
            JsonObject process = processes[selectedIndex];
            String status = process["status"].as<String>();
            String action = (status == "RUNNING") ? "stop" : "start";
            
            Serial.print("🎯 Action button pressed for process ");
            Serial.print(selectedIndex);
            Serial.print(" - ");
            Serial.println(action);
            
            sendControlCommand(action, selectedIndex);
        }
    } else if (!actionPressed) {
        btnActionPressed = false;
    }
}
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

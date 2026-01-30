#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include "config.h"

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#define GAME_WIDTH 64
#define GAME_HEIGHT 128

// Timer configuration
#define DEFAULT_TIMER_SECONDS 5

// WiFi AP credentials
const char* ap_ssid = "Timer Config";
const char* ap_pass = "";

// Web server
WebServer server(80);

// DNS server for captive portal
DNSServer dnsServer;
const byte DNS_PORT = 53;

// Preferences for persistent storage
Preferences prefs;

// Timer state
enum TimerState {
  TIMER_STOPPED,
  TIMER_RUNNING,
  TIMER_PAUSED,
  TIMER_FINISHED
};

TimerState timerState = TIMER_STOPPED;
unsigned long timerDuration = DEFAULT_TIMER_SECONDS * 1000; // Default timer in milliseconds
unsigned long timerStartTime = 0;
unsigned long timerElapsedTime = 0;
unsigned long timerRemainingTime = 0;

// Button state
bool lastButtonState = HIGH;
unsigned long buttonPressTime = 0;
bool buttonPressed = false;
const unsigned long LONG_PRESS_TIME = 1000; // 1 second for reset

int drawCenteredText(String text, int y, int textSize, int maxWidth) {
  display.setTextSize(textSize);
  display.setTextColor(SSD1306_WHITE);
  
  int16_t x1, y1;
  uint16_t w, h;
  
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  
  // Calculate width using character count
  int charWidth = text.length() * 6 * textSize;
  int checkW = (charWidth > w) ? charWidth : w;

  if (checkW <= maxWidth) {
    int x = (maxWidth - w) / 2;
    display.setCursor(x, y);
    display.println(text);
    return h;
  }
  
  // Split text into words
  String words[10];
  int wordCount = 0;
  int startIndex = 0;
  int spaceIndex;
  
  while ((spaceIndex = text.indexOf(' ', startIndex)) != -1 && wordCount < 10) {
    words[wordCount++] = text.substring(startIndex, spaceIndex);
    startIndex = spaceIndex + 1;
  }
  if (startIndex < text.length() && wordCount < 10) {
    words[wordCount++] = text.substring(startIndex);
  }
  
  // Build lines without breaking words
  String lines[5]; // Max 5 lines
  int lineCount = 0;
  String currentLine = "";
  
  for (int i = 0; i < wordCount; i++) {
    String testLine = currentLine + (currentLine.length() > 0 ? " " : "") + words[i];
    
    // Check width of test line using conservative estimate
    int testW = testLine.length() * 6 * textSize;
    
    if (testW <= maxWidth) {
      currentLine = testLine;
    } else {
      if (currentLine.length() > 0) {
        lines[lineCount++] = currentLine;
        currentLine = words[i];
      } else {
        lines[lineCount++] = words[i];
        currentLine = "";
      }
    }
  }
  
  if (currentLine.length() > 0 && lineCount < 5) {
    lines[lineCount++] = currentLine;
  }
  
  // Draw each line centered
  int currentY = y;
  for (int i = 0; i < lineCount; i++) {
    display.getTextBounds(lines[i], 0, 0, &x1, &y1, &w, &h);
    int x = (maxWidth - w) / 2;
    display.setCursor(x, currentY);
    display.println(lines[i]);
    currentY += h + 2;
  }
  
  return currentY - y;
}

void showBootScreen() {
  display.clearDisplay();
  
  // Show device32 boot logo with border
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  String bootText = "device32";
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(bootText, 0, 0, &x1, &y1, &w, &h);
  
  int textX = (GAME_WIDTH - w) / 2;
  int textY = (GAME_HEIGHT - h) / 2;
  
  // Draw border around text
  int padding = 6;
  int rectX = textX - padding;
  int rectY = textY - padding;
  int rectW = w + (padding * 2);
  int rectH = h + (padding * 2);
  display.drawRoundRect(rectX, rectY, rectW, rectH, 3, SSD1306_WHITE);
  
  display.setCursor(textX, textY);
  display.println(bootText);
  
  display.display();
  delay(800);
}

void startAccessPoint() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_pass);
  
  IPAddress IP = WiFi.softAPIP();
  
  // Start DNS server for captive portal
  dnsServer.start(DNS_PORT, "*", IP);
}

String formatTime(unsigned long ms) {
  unsigned long seconds = ms / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;
  
  seconds = seconds % 60;
  minutes = minutes % 60;
  
  String result = "";
  
  if (hours > 0) {
    result += String(hours) + "h ";
  }
  if (minutes > 0 || hours > 0) {
    result += String(minutes) + "m ";
  }
  result += String(seconds) + "s";
  
  return result;
}

void resetTimer() {
  timerState = TIMER_STOPPED;
  timerElapsedTime = 0;
  timerRemainingTime = timerDuration;
  timerStartTime = 0;
}

void handleButton() {
  bool currentButtonState = digitalRead(BUTTON_PIN);
  
  // Detect button press
  if (currentButtonState == LOW && lastButtonState == HIGH) {
    buttonPressed = true;
    buttonPressTime = millis();
  }
  
  // Check for long press while button is held
  if (currentButtonState == LOW && buttonPressed) {
    unsigned long pressDuration = millis() - buttonPressTime;
    if (pressDuration >= LONG_PRESS_TIME) {
      // Long press - reset timer immediately
      resetTimer();
      buttonPressed = false; // Prevent further actions
    }
  }
  
  // Detect button release
  if (currentButtonState == HIGH && lastButtonState == LOW) {
    if (buttonPressed) {
      // Short tap - start/pause timer or clear finished state
      if (timerState == TIMER_FINISHED) {
        resetTimer();
      } else if (timerState == TIMER_STOPPED || timerState == TIMER_PAUSED) {
        timerState = TIMER_RUNNING;
        timerStartTime = millis() - timerElapsedTime;
      } else if (timerState == TIMER_RUNNING) {
        timerState = TIMER_PAUSED;
        timerElapsedTime = millis() - timerStartTime;
      }
      buttonPressed = false;
    }
  }
  
  lastButtonState = currentButtonState;
}

void updateTimer() {
  if (timerState == TIMER_RUNNING) {
    timerElapsedTime = millis() - timerStartTime;
    
    if (timerElapsedTime >= timerDuration) {
      // Timer finished
      timerElapsedTime = timerDuration;
      timerRemainingTime = 0;
      timerState = TIMER_FINISHED;
    } else {
      timerRemainingTime = timerDuration - timerElapsedTime;
    }
  } else if (timerState == TIMER_PAUSED) {
    timerRemainingTime = timerDuration - timerElapsedTime;
  } else if (timerState == TIMER_FINISHED) {
    timerRemainingTime = 0;
  } else {
    timerRemainingTime = timerDuration;
  }
}

void drawTimer() {
  display.clearDisplay();
  display.drawRoundRect(0, 0, GAME_WIDTH, GAME_HEIGHT, 4, SSD1306_WHITE);
  
  // Timer label
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  int16_t x1, y1;
  uint16_t w, h;
  
  String timerLabel = "Timer";
  display.getTextBounds(timerLabel, 0, 0, &x1, &y1, &w, &h);
  int labelY = 10; // Moved down from 5 to 10
  int boxW = 48;
  int boxPadding = 2;
  int boxX = (GAME_WIDTH - boxW) / 2;
  int boxY = labelY - boxPadding;
  int boxH = h + (boxPadding * 2);
  display.fillRoundRect(boxX, boxY, boxW, boxH, 2, SSD1306_WHITE);
  
  int labelX = boxX + (boxW - w) / 2;
  display.setTextColor(SSD1306_BLACK);
  display.setCursor(labelX, labelY);
  display.println(timerLabel);
  display.setTextColor(SSD1306_WHITE);
  
  // Time display with blinking for finished state
  bool shouldShow = true;
  if (timerState == TIMER_FINISHED) {
    // Blink every 500ms
    shouldShow = (millis() / 500) % 2 == 0;
  }
  
  if (shouldShow) {
    String timeStr = formatTime(timerRemainingTime);
    display.setTextSize(2);
    display.getTextBounds(timeStr, 0, 0, &x1, &y1, &w, &h);
    
    // Adjust text size if too long
    if (w > GAME_WIDTH - 8) {
      display.setTextSize(1);
      display.getTextBounds(timeStr, 0, 0, &x1, &y1, &w, &h);
    }
    
    int timeX = (GAME_WIDTH - w) / 2;
    int timeY = (GAME_HEIGHT / 2) - (h / 2);
    display.setCursor(timeX, timeY);
    display.println(timeStr);
  }
  
  // State indicator
  display.setTextSize(1);
  String stateStr;
  if (timerState == TIMER_RUNNING) {
    stateStr = "Running";
  } else if (timerState == TIMER_PAUSED) {
    stateStr = "Paused";
  } else if (timerState == TIMER_FINISHED) {
    stateStr = "Done!";
  } else {
    stateStr = "Ready";
  }
  
  display.getTextBounds(stateStr, 0, 0, &x1, &y1, &w, &h);
  int stateX = (GAME_WIDTH - w) / 2;
  int stateY = GAME_HEIGHT - 15;
  display.setCursor(stateX, stateY);
  display.println(stateStr);
  
  display.display();
}

void handleRoot() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>";
  html += "body { font-family: Arial; text-align: center; margin: 20px; background: #f0f0f0; }";
  html += "h1 { color: #333; }";
  html += ".container { background: white; padding: 20px; border-radius: 10px; max-width: 400px; margin: 0 auto; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }";
  html += "input[type=number] { width: 100%; padding: 10px; margin: 10px 0; border: 2px solid #ddd; border-radius: 5px; font-size: 16px; }";
  html += "button { background: #4CAF50; color: white; padding: 12px 20px; border: none; border-radius: 5px; font-size: 16px; cursor: pointer; width: 100%; margin: 5px 0; }";
  html += "button:hover { background: #45a049; }";
  html += ".info { color: #666; font-size: 14px; margin: 10px 0; }";
  html += "</style></head><body>";
  html += "<div class='container'>";
  html += "<h1>Timer Config</h1>";
  html += "<div class='info'>Current: " + formatTime(timerDuration) + "</div>";
  html += "<form action='/set' method='POST'>";
  html += "<input type='number' name='seconds' placeholder='Seconds' min='1' max='86400' value='" + String(timerDuration / 1000) + "'>";
  html += "<button type='submit'>Set Timer</button>";
  html += "</form>";
  html += "<div class='info' style='margin-top: 20px;'>Tap button: Start/Pause<br>Hold button: Reset</div>";
  html += "</div></body></html>";
  server.send(200, "text/html", html);
}

void handleSet() {
  if (server.hasArg("seconds")) {
    String secondsStr = server.arg("seconds");
    unsigned long seconds = secondsStr.toInt();
    
    if (seconds > 0 && seconds <= 86400) { // Max 24 hours
      timerDuration = seconds * 1000;
      
      // Save to preferences
      prefs.begin("timer", false);
      prefs.putULong("duration", timerDuration);
      prefs.end();
      
      // Reset timer with new duration
      resetTimer();
      
      String html = "<!DOCTYPE html><html><head>";
      html += "<meta http-equiv='refresh' content='2;url=/'>";
      html += "<style>body { font-family: Arial; text-align: center; margin: 50px; }</style>";
      html += "</head><body>";
      html += "<h2>Timer set to " + formatTime(timerDuration) + "</h2>";
      html += "<p>Redirecting...</p>";
      html += "</body></html>";
      server.send(200, "text/html", html);
    } else {
      server.send(400, "text/plain", "Invalid time range");
    }
  } else {
    server.send(400, "text/plain", "Missing seconds parameter");
  }
}

void setupWebServer() {
  server.on("/", handleRoot);
  server.on("/set", handleSet);
  server.on("/generate_204", handleRoot); // Android captive portal
  server.on("/fwlink", handleRoot); // Microsoft captive portal
  server.onNotFound(handleRoot); // Redirect all other requests to root
  server.begin();
}

void setup() {
  // Initialize button
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  // Initialize display
  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for (;;);
  }
  display.setRotation(1); // Rotate 90 degrees for vertical orientation
  display.clearDisplay();
  display.display();

  // Show boot screen
  showBootScreen();
  
  // Load saved timer duration
  prefs.begin("timer", false);
  timerDuration = prefs.getULong("duration", DEFAULT_TIMER_SECONDS * 1000);
  prefs.end();
  
  // Initialize timer state
  resetTimer();
  
  // Start Access Point
  startAccessPoint();
  
  // Setup web server
  setupWebServer();
}

void loop() {
  // Handle DNS requests for captive portal
  dnsServer.processNextRequest();
  
  // Handle web server requests
  server.handleClient();
  
  // Handle button input
  handleButton();
  
  // Update timer
  updateTimer();
  
  // Draw display
  drawTimer();
  
  delay(50); // Small delay for button debouncing
}
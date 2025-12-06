#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include "config.h"

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#define GAME_WIDTH 64
#define GAME_HEIGHT 128

// Update to your WiFi creds!WiFi
const char* ssid = "Wokwi-GUEST";
const char* pass = "";
// Update to your location!
//   Default for Omaha, NE
const float latitude = 41.2565;
const float longitude = -95.9345;


// Timezone offsets: GMT offset (seconds), Daylight saving offset (seconds)
// Auto-detected from IP, defaults for Central Time
int gmtOffset_sec = -6 * 3600;
int daylightOffset_sec = 3600;

// Time format: true for 12-hour, false for 24-hour
bool use12HourFormat = true;

void detectTimezone() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.begin("http://ipapi.co/json");
  int httpCode = http.GET();
  if (httpCode > 0) {
    String payload = http.getString();
    JsonDocument doc;
    deserializeJson(doc, payload);
    if (doc["timezone"].is<String>()) {
      String tz = doc["timezone"];
      // Map common timezones to offsets (simplified)
      if (tz == "America/New_York") { gmtOffset_sec = -5 * 3600; daylightOffset_sec = 3600; }
      else if (tz == "America/Chicago") { gmtOffset_sec = -6 * 3600; daylightOffset_sec = 3600; }
      else if (tz == "America/Denver") { gmtOffset_sec = -7 * 3600; daylightOffset_sec = 3600; }
      else if (tz == "America/Los_Angeles") { gmtOffset_sec = -8 * 3600; daylightOffset_sec = 3600; }
      else if (tz == "America/London") { gmtOffset_sec = 0; daylightOffset_sec = 3600; }
    }
  }
  http.end();
}

String weatherDescription = "";
float temperature = 0.0;
String currentTime = "";

int drawCenteredText(String text, int y, int textSize, int maxWidth) {
  display.setTextSize(textSize);
  display.setTextColor(SSD1306_WHITE);
  
  int16_t x1, y1;
  uint16_t w, h;
  
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  
  // Calculate width using character count for default font (6 pixels per char)
  int charWidth = text.length() * 6 * textSize;
  // Use the larger of the two widths for the wrapping check
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

String getWeatherDescription(int code) {
  switch (code) {
    case 0: return "Clear sky";
    case 1: return "Mainly clear";
    case 2: return "Partly cloudy";
    case 3: return "Overcast";
    case 45: return "Fog";
    case 48: return "Depositing rime fog";
    case 51: return "Light drizzle";
    case 53: return "Moderate drizzle";
    case 55: return "Dense drizzle";
    case 56: return "Light freezing drizzle";
    case 57: return "Dense freezing drizzle";
    case 61: return "Slight rain";
    case 63: return "Moderate rain";
    case 65: return "Heavy rain";
    case 66: return "Light freezing rain";
    case 67: return "Heavy freezing rain";
    case 71: return "Slight snow fall";
    case 73: return "Moderate snow fall";
    case 75: return "Heavy snow fall";
    case 77: return "Snow grains";
    case 80: return "Slight rain showers";
    case 81: return "Moderate rain showers";
    case 82: return "Violent rain showers";
    case 85: return "Slight snow showers";
    case 86: return "Heavy snow showers";
    case 95: return "Thunderstorm";
    case 96: return "Thunderstorm with slight hail";
    case 99: return "Thunderstorm with heavy hail";
    default: return "";
  }
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

void connectToWiFi() {
  WiFi.begin(ssid, pass);
  
  int step = 0;
  unsigned long lastDotUpdate = 0;
  
  while (WiFi.status() != WL_CONNECTED) {
    unsigned long currentMillis = millis();
    
    // Update dots every 500ms
    if (currentMillis - lastDotUpdate >= 500) {
      display.clearDisplay();
      
      int dotCount = step % 4;
      String connectingText = "WiFi";
      for (int i = 0; i < dotCount; i++) {
        connectingText += ".";
      }
      
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      
      int16_t x1, y1;
      uint16_t w, h;
      display.getTextBounds(connectingText, 0, 0, &x1, &y1, &w, &h);
      
      int textX = (GAME_WIDTH - w) / 2;
      int textY = (GAME_HEIGHT - h) / 2;
      
      String maxText = "WiFi...";
      display.getTextBounds(maxText, 0, 0, &x1, &y1, &w, &h);
      int maxTextX = (GAME_WIDTH - w) / 2;
      
      int padding = 4;
      int rectX = maxTextX - padding;
      int rectY = textY - padding;
      int rectW = w + (padding * 2);
      int rectH = h + (padding * 2);
      display.drawRoundRect(rectX, rectY, rectW, rectH, 2, SSD1306_WHITE);
      
      display.setCursor(textX, textY);
      display.println(connectingText);
      
      display.display();
      
      step = (step + 1) % 4;
      
      lastDotUpdate = currentMillis;
    }
    
    delay(100);
  }
}

String getWeather() {
  if (WiFi.status() != WL_CONNECTED) return "No WiFi";

  HTTPClient http;
  String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(latitude, 4) + "&longitude=" + String(longitude, 4) + "&current_weather=true&temperature_unit=fahrenheit";
  http.begin(url);
  int httpCode = http.GET();
  if (httpCode > 0) {
    String payload = http.getString();
    JsonDocument doc;
    deserializeJson(doc, payload);
    if (doc["current_weather"].is<JsonObject>()) {
      temperature = doc["current_weather"]["temperature"];
      int weathercode = doc["current_weather"]["weathercode"];
      weatherDescription = getWeatherDescription(weathercode);
      http.end();
      return weatherDescription + " " + String((int)temperature) + " F";
    } else {
      http.end();
      return "Invalid response";
    }
  } else {
    http.end();
    return "Weather fetch failed";
  }
}

void updateTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    currentTime = "Time sync failed";
    return;
  }
  char buffer[20];
  if (use12HourFormat) {
    strftime(buffer, sizeof(buffer), "%I:%M", &timeinfo);
    currentTime = String(buffer);
    // Remove leading zero from hour
    if (currentTime[0] == '0') {
      currentTime = currentTime.substring(1);
    }
  } else {
    strftime(buffer, sizeof(buffer), "%H:%M", &timeinfo);
    currentTime = String(buffer);
    // Remove leading zero from hour
    if (currentTime[0] == '0') {
      currentTime = currentTime.substring(1);
    }
  }
}

void setup() {
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

  connectToWiFi();

  detectTimezone();

  configTime(gmtOffset_sec, daylightOffset_sec, NTP_SERVER);

  getWeather();
  updateTime();
}

void loop() {
  static unsigned long lastWeatherUpdate = 0;
  static unsigned long lastTimeUpdate = 0;

  unsigned long now = millis();

  if (now - lastWeatherUpdate > 180000) { // Update weather every 3 minutes
    getWeather();
    lastWeatherUpdate = now;
  }

  if (now - lastTimeUpdate > 1000) { // Update time every second
    updateTime();
    lastTimeUpdate = now;
  }

  // Draw display
  display.clearDisplay();

  // Time on top
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  int16_t x1, y1;
  uint16_t w, h;

  String timeLabel = "Time";
  display.getTextBounds(timeLabel, 0, 0, &x1, &y1, &w, &h);
  int timeLabelX = (GAME_WIDTH - w) / 2;
  int timeLabelY = 5;
  
  // Draw border
  int timeBoxPadding = 3;
  int timeBoxX = 0;
  int timeBoxY = timeLabelY - timeBoxPadding;
  int timeBoxW = GAME_WIDTH;
  int timeBoxH = h + (timeBoxPadding * 2);
  display.drawRoundRect(timeBoxX, timeBoxY, timeBoxW, timeBoxH, 2, SSD1306_WHITE);
  
  display.setCursor(timeLabelX, timeLabelY);
  display.println(timeLabel);

  display.setTextSize(1);  // Changed from size 2 to size 1
  display.getTextBounds(currentTime, 0, 0, &x1, &y1, &w, &h);
  int timeX = (GAME_WIDTH - w) / 2;
  display.setCursor(timeX, 20);
  display.println(currentTime);

  // Date closer to time
  display.setTextSize(1);
  String dateLabel = "Date";
  display.getTextBounds(dateLabel, 0, 0, &x1, &y1, &w, &h);
  int dateLabelX = (GAME_WIDTH - w) / 2;
  int dateLabelY = 40;
  
  // Draw border
  int dateBoxPadding = 3;
  int dateBoxX = 0;
  int dateBoxY = dateLabelY - dateBoxPadding;
  int dateBoxW = GAME_WIDTH;
  int dateBoxH = h + (dateBoxPadding * 2);
  display.drawRoundRect(dateBoxX, dateBoxY, dateBoxW, dateBoxH, 2, SSD1306_WHITE);
  
  display.setCursor(dateLabelX, dateLabelY);
  display.println(dateLabel);

  // Get current date
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    char dateBuffer[20];
    strftime(dateBuffer, sizeof(dateBuffer), "%m/%d/%Y", &timeinfo);
    String currentDate = String(dateBuffer);
    
    display.getTextBounds(currentDate, 0, 0, &x1, &y1, &w, &h);
    int dateX = (GAME_WIDTH - w) / 2;
    display.setCursor(dateX, 55);
    display.println(currentDate);
  }

  // Weather closer to date
  display.setTextSize(1);
  String weatherLabel = "Weather";
  display.getTextBounds(weatherLabel, 0, 0, &x1, &y1, &w, &h);
  int weatherLabelX = (GAME_WIDTH - w) / 2;
  int weatherLabelY = 80;
  
  // Draw rounded box around weather label
  int boxPadding = 3;
  int boxX = 0;
  int boxY = weatherLabelY - boxPadding;
  int boxW = GAME_WIDTH;
  int boxH = h + (boxPadding * 2);
  display.drawRoundRect(boxX, boxY, boxW, boxH, 2, SSD1306_WHITE);
  
  display.setCursor(weatherLabelX, weatherLabelY);
  display.println(weatherLabel);

  String desc = weatherDescription.length() == 0 ? "Loading weather..." : weatherDescription;
  int weatherHeight = drawCenteredText(desc, 95, 1, GAME_WIDTH);

  String tempStr = (temperature == 0.0 && weatherDescription.length() == 0) ? "" : String((int)temperature) + " F";
  if (tempStr != "") {
    display.setTextSize(1);  // Changed from size 2 to size 1
    display.getTextBounds(tempStr, 0, 0, &x1, &y1, &w, &h);
    int tempX = (GAME_WIDTH - w) / 2;
    int tempY = 93 + weatherHeight + 8;
    display.setCursor(tempX, tempY);
    display.println(tempStr);
    display.setTextSize(1);
  }

  display.display();

  delay(1000);
}

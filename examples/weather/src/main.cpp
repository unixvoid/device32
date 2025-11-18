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

const char* ssid = "test_ssid";
const char* pass = "test_pass";
// Hardcoded for Omaha, NE
const float latitude = 41.2565;
const float longitude = -95.9345;


// Timezone offsets: GMT offset (seconds), Daylight saving offset (seconds)
// Auto-detected from IP, defaults for Central Time
int gmtOffset_sec = -6 * 3600;
int daylightOffset_sec = 3600;

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

void connectToWiFi() {
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
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
  strftime(buffer, sizeof(buffer), "%H:%M", &timeinfo);
  currentTime = String(buffer);
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
  display.drawRoundRect(0, 0, GAME_WIDTH, GAME_HEIGHT, 4, SSD1306_WHITE);

  // Weather on top
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  int16_t x1, y1;
  uint16_t w, h;

  String weatherLabel = "Weather:";
  display.getTextBounds(weatherLabel, 0, 0, &x1, &y1, &w, &h);
  int x = (GAME_WIDTH - w) / 2;
  display.setCursor(x, 10);
  display.println(weatherLabel);

  String desc = weatherDescription.length() == 0 ? "Loading weather..." : weatherDescription;
  display.getTextBounds(desc, 0, 0, &x1, &y1, &w, &h);
  x = (GAME_WIDTH - w) / 2;
  display.setCursor(x, 25);
  display.println(desc);

  String tempStr = (temperature == 0.0 && weatherDescription.length() == 0) ? "" : String((int)temperature) + " F";
  if (tempStr != "") {
    display.setTextSize(2);
    display.getTextBounds(tempStr, 0, 0, &x1, &y1, &w, &h);
    x = (GAME_WIDTH - w) / 2;
    display.setCursor(x, 45);
    display.println(tempStr);
    display.setTextSize(1);
  }

  // Time on bottom
  String timeLabel = "Time:";
  display.getTextBounds(timeLabel, 0, 0, &x1, &y1, &w, &h);
  x = (GAME_WIDTH - w) / 2;
  display.setCursor(x, 85);
  display.println(timeLabel);

  display.setTextSize(2);
  display.getTextBounds(currentTime, 0, 0, &x1, &y1, &w, &h);
  x = (GAME_WIDTH - w) / 2;
  display.setCursor(x, 100);
  display.println(currentTime);

  display.display();

  delay(1000);
}

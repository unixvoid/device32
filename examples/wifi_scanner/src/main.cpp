#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <vector>
#include <string>
#include <map>
#include "config.h"

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#define GAME_WIDTH 64
#define GAME_HEIGHT 128

String macToString(const uint8_t *mac) {
  char buf[18];
  sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
}

bool isBroadcast(const String &mac) {
  return mac == "FF:FF:FF:FF:FF:FF";
}

struct AP {
  String ssid;
  String bssid;
  int rssi;
  int channel;
  int enc;
};

std::vector<AP> aps;
int current_index = 0;
int state = 0; // 0: list, 1: detail
int last_button_state = HIGH;
unsigned long press_start = 0;
bool force_scan = false;
int start_index = 0;
int scroll_pos = 0;
unsigned long last_scroll = 0;
int detail_index = 0;
bool long_press_triggered = false;
bool is_scanning = false;

void showBootScreen() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  String wifiText = "WIFI";
  String scannerText = "Scanner";
  int16_t x1, y1;
  uint16_t w1, h1, w2, h2;
  display.getTextBounds(wifiText, 0, 0, &x1, &y1, &w1, &h1);
  display.getTextBounds(scannerText, 0, 0, &x1, &y1, &w2, &h2);
  int textX1 = (GAME_WIDTH - w1) / 2;
  int textX2 = (GAME_WIDTH - w2) / 2;
  int totalH = h1 + h2 + 5;
  int textY1 = (GAME_HEIGHT - totalH) / 2;
  int textY2 = textY1 + h1 + 5;
  int padding = 6;
  int rectX = min(textX1, textX2) - padding;
  int rectY = textY1 - padding;
  int rectW = max(textX1 + w1, textX2 + w2) - rectX + padding;
  int rectH = totalH + (padding * 2);
  display.drawRoundRect(rectX, rectY, rectW, rectH, 3, SSD1306_WHITE);
  display.setCursor(textX1, textY1);
  display.println(wifiText);
  display.setCursor(textX2, textY2);
  display.println(scannerText);
  display.display();
  delay(800);
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting WiFi scanner");

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

  // Set up WiFi
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Initial scan
  Serial.println("Initial scan...");
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; ++i) {
    AP ap = {WiFi.SSID(i), WiFi.BSSIDstr(i), WiFi.RSSI(i), WiFi.channel(i), WiFi.encryptionType(i)};
    aps.push_back(ap);
  }
  WiFi.scanDelete();
  Serial.printf("Found %d networks\n", n);
}

void loop() {
  // Scan if forced
  if (force_scan) {
    Serial.println("Rescanning...");
    aps.clear();
    int n = WiFi.scanNetworks();
    for (int i = 0; i < n; ++i) {
      AP ap = {WiFi.SSID(i), WiFi.BSSIDstr(i), WiFi.RSSI(i), WiFi.channel(i), WiFi.encryptionType(i)};
      aps.push_back(ap);
    }
    WiFi.scanDelete();
    Serial.printf("Found %d networks\n", n);
    force_scan = false;
    is_scanning = false;
    current_index = 0; // reset to first
  }

  // Handle button
  int button_state = digitalRead(BUTTON_PIN);
  if (button_state == LOW && last_button_state == HIGH) {
    press_start = millis();
    long_press_triggered = false;
  }
  if (button_state == LOW && !long_press_triggered && millis() - press_start > 1000) {
    // Trigger long press immediately
    long_press_triggered = true;
    if (state == 0 && current_index < aps.size()) {
      state = 1; // enter detail
      detail_index = 0;
    } else if (state == 0 && current_index == aps.size()) {
      force_scan = true; // rescan
      is_scanning = true;
    } else if (state == 1) {
      state = 0; // back from detail
    }
  }
  if (button_state == HIGH && last_button_state == LOW) {
    unsigned long press_duration = millis() - press_start;
    if (press_duration <= 1000) { // short press
      if (state == 0) {
        int total_items = aps.size() + 1; // +1 for rescan
        current_index = (current_index + 1) % total_items;
      } else if (state == 1) {
        detail_index = (detail_index + 1) % 5;
      } else {
        state = 0; // back to list
      }
    }
    // Long press already handled above
  }
  last_button_state = button_state;

  // Adjust start_index for scrolling list
  int total_items = aps.size() + 1;
  if (current_index < start_index) {
    start_index = current_index;
  } else if (current_index > start_index + 9) {
    start_index = current_index - 9;
  }
  if (start_index < 0) start_index = 0;
  if (start_index > total_items - 10) start_index = max(0, total_items - 10);

  // Scroll text for selected item
  if (((state == 0 && current_index < aps.size()) || (state == 1 && current_index < aps.size())) && aps.size() > 0 && millis() - last_scroll > 200) {
    String text_to_scroll;
    if (state == 0) {
      text_to_scroll = aps[current_index].ssid;
    } else {
      AP ap = aps[current_index];
      switch (detail_index) {
        case 0: text_to_scroll = ap.ssid; break;
        case 1: text_to_scroll = ap.bssid; break;
        case 2: text_to_scroll = String(ap.rssi); break;
        case 3: text_to_scroll = String(ap.channel); break;
        case 4: {
          switch (ap.enc) {
            case WIFI_AUTH_OPEN: text_to_scroll = "Open"; break;
            case WIFI_AUTH_WEP: text_to_scroll = "WEP"; break;
            case WIFI_AUTH_WPA_PSK: text_to_scroll = "WPA"; break;
            case WIFI_AUTH_WPA2_PSK: text_to_scroll = "WPA2"; break;
            case WIFI_AUTH_WPA_WPA2_PSK: text_to_scroll = "WPA+WPA2"; break;
            case WIFI_AUTH_WPA2_ENTERPRISE: text_to_scroll = "WPA2-EAP"; break;
            case WIFI_AUTH_WPA3_PSK: text_to_scroll = "WPA3"; break;
            case WIFI_AUTH_WPA2_WPA3_PSK: text_to_scroll = "WPA2+WPA3"; break;
            case WIFI_AUTH_WAPI_PSK: text_to_scroll = "WAPI"; break;
            default: text_to_scroll = "Unknown"; break;
          }
          break;
        }
      }
    }
    if (text_to_scroll.length() > 9) {
      scroll_pos = (scroll_pos + 1) % (text_to_scroll.length() - 8);
    } else {
      scroll_pos = 0;
    }
    last_scroll = millis();
  }

  // Draw display
  display.clearDisplay();
  display.drawRoundRect(0, 0, GAME_WIDTH, GAME_HEIGHT, 4, SSD1306_WHITE);

  if (is_scanning) {
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setTextWrap(false);
    String scan_msg = "Scanning..";
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(scan_msg, 0, 0, &x1, &y1, &w, &h);
    int center_x = (GAME_WIDTH - w) / 2;
    int center_y = (GAME_HEIGHT - h) / 2;
    display.setCursor(center_x, center_y);
    display.printf("%s", scan_msg.c_str());
  } else {
    // Normal display code
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setTextWrap(false);

    if (state == 0) { // List view
      int y = 5;
      String aps_line = "APs: " + String(aps.size());
      display.setCursor(5, y);
      display.printf("%s", aps_line.c_str());
      display.setCursor(6, y); // offset for bold
      display.printf("%s", aps_line.c_str());
      y += 10;

      int num_to_show = 10;
      int total_items = aps.size() + 1;
      for (int i = 0; i < num_to_show && start_index + i < total_items; ++i) {
        int idx = start_index + i;
        display.setCursor(5, y);
        if (idx < aps.size()) {
          String ssid = aps[idx].ssid;
          if (idx == current_index) {
            String display_ssid;
            if (ssid.length() > 9) {
              display_ssid = ssid.substring(scroll_pos, scroll_pos + 9);
            } else {
              display_ssid = ssid;
            }
            display.printf(">%s", display_ssid.c_str());
          } else {
            if (ssid.length() > 9) ssid = ssid.substring(0, 9);
            display.printf(" %s", ssid.c_str());
          }
        } else {
          // Rescan option
          if (idx == current_index) {
            display.setCursor(5, y);
            display.printf(">* Rescan");
            display.setCursor(6, y);
            display.printf(">* Rescan");
          } else {
            display.setCursor(5, y);
            display.printf(" * Rescan");
            display.setCursor(6, y);
            display.printf(" * Rescan");
          }
        }
        y += 10;
      }
    } else { // Detail view
      if (current_index < aps.size()) {
        AP ap = aps[current_index];
        int y = 5;
        // SSID
        String label_ssid = detail_index == 0 ? ">SSID" : "SSID";
        int16_t x1, y1;
        uint16_t w, h;
        display.getTextBounds(label_ssid, 0, 0, &x1, &y1, &w, &h);
        int center_x = (GAME_WIDTH - w) / 2;
        display.setCursor(center_x, y);
        display.printf("%s", label_ssid.c_str());
        display.setCursor(center_x + 1, y);
        display.printf("%s", label_ssid.c_str());
        y += 10;
        display.setCursor(5, y);
        String ssid_val = ap.ssid;
        String display_ssid = (detail_index == 0 && ssid_val.length() > 9) ? ssid_val.substring(scroll_pos, scroll_pos + 9) : ssid_val;
        display.printf("%s", display_ssid.c_str());
        y += 10;
        // BSSID
        String label_bssid = detail_index == 1 ? ">BSSID" : "BSSID";
        display.getTextBounds(label_bssid, 0, 0, &x1, &y1, &w, &h);
        center_x = (GAME_WIDTH - w) / 2;
        display.setCursor(center_x, y);
        display.printf("%s", label_bssid.c_str());
        display.setCursor(center_x + 1, y);
        display.printf("%s", label_bssid.c_str());
        y += 10;
        display.setCursor(5, y);
        String bssid_val = ap.bssid;
        String display_bssid = (detail_index == 1 && bssid_val.length() > 9) ? bssid_val.substring(scroll_pos, scroll_pos + 9) : bssid_val;
        display.printf("%s", display_bssid.c_str());
        y += 10;
        // RSSI
        String label_rssi = detail_index == 2 ? ">RSSI" : "RSSI";
        display.getTextBounds(label_rssi, 0, 0, &x1, &y1, &w, &h);
        center_x = (GAME_WIDTH - w) / 2;
        display.setCursor(center_x, y);
        display.printf("%s", label_rssi.c_str());
        display.setCursor(center_x + 1, y);
        display.printf("%s", label_rssi.c_str());
        y += 10;
        display.setCursor(5, y);
        String rssi_str = String(ap.rssi);
        display.printf("%s", rssi_str.c_str());
        y += 10;
        // Channel
        String label_ch = detail_index == 3 ? ">Ch" : "Ch";
        display.getTextBounds(label_ch, 0, 0, &x1, &y1, &w, &h);
        center_x = (GAME_WIDTH - w) / 2;
        display.setCursor(center_x, y);
        display.printf("%s", label_ch.c_str());
        display.setCursor(center_x + 1, y);
        display.printf("%s", label_ch.c_str());
        y += 10;
        display.setCursor(5, y);
        String ch_str = String(ap.channel);
        display.printf("%s", ch_str.c_str());
        y += 10;
        // Encryption
        String label_enc = detail_index == 4 ? ">Enc" : "Enc";
        display.getTextBounds(label_enc, 0, 0, &x1, &y1, &w, &h);
        center_x = (GAME_WIDTH - w) / 2;
        display.setCursor(center_x, y);
        display.printf("%s", label_enc.c_str());
        display.setCursor(center_x + 1, y);
        display.printf("%s", label_enc.c_str());
        y += 10;
        display.setCursor(5, y);
        String enc_str;
        switch (ap.enc) {
          case WIFI_AUTH_OPEN: enc_str = "Open"; break;
          case WIFI_AUTH_WEP: enc_str = "WEP"; break;
          case WIFI_AUTH_WPA_PSK: enc_str = "WPA"; break;
          case WIFI_AUTH_WPA2_PSK: enc_str = "WPA2"; break;
          case WIFI_AUTH_WPA_WPA2_PSK: enc_str = "WPA+WPA2"; break;
          case WIFI_AUTH_WPA2_ENTERPRISE: enc_str = "WPA2-EAP"; break;
          case WIFI_AUTH_WPA3_PSK: enc_str = "WPA3"; break;
          case WIFI_AUTH_WPA2_WPA3_PSK: enc_str = "WPA2+WPA3"; break;
          case WIFI_AUTH_WAPI_PSK: enc_str = "WAPI"; break;
          default: enc_str = "Unknown"; break;
        }
        String display_enc = (detail_index == 4 && enc_str.length() > 9) ? enc_str.substring(scroll_pos, scroll_pos + 9) : enc_str;
        display.printf("%s", display_enc.c_str());
      }
    }
  }

  display.display();
  delay(100);
}
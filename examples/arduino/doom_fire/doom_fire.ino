#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// OLED display settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1  // No reset pin
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// I2C pins for ESP32C3
#define SDA_PIN 7
#define SCL_PIN 6

// Button pin (Normally Open)
#define BUTTON_PIN 5

// Fire simulation
#define FIRE_WIDTH 128
#define FIRE_HEIGHT 64
uint8_t fire[FIRE_WIDTH * FIRE_HEIGHT];  // Fire intensity (0-255)

// Button state for debouncing
bool lastButtonState = HIGH;

void initFire() {
  // Initialize all to 0
  memset(fire, 0, sizeof(fire));
  // Bottom row to varying intensity for more realistic fire
  for (int x = 0; x < FIRE_WIDTH; x++) {
    fire[(FIRE_HEIGHT - 1) * FIRE_WIDTH + x] = random(200, 256);  // 200-255
  }
}

void updateFire() {
  // Update from bottom up
  for (int y = FIRE_HEIGHT - 2; y >= 0; y--) {
    for (int x = 0; x < FIRE_WIDTH; x++) {
      // Spread from below, with random offset
      int src_x = x + random(-1, 2);  // -1, 0, 1
      if (src_x < 0) src_x = 0;
      if (src_x >= FIRE_WIDTH) src_x = FIRE_WIDTH - 1;
      int decay = fire[(y + 1) * FIRE_WIDTH + src_x] - random(0, 5);  // Stronger decay
      if (decay < 0) decay = 0;
      fire[y * FIRE_WIDTH + x] = decay;
    }
  }
}

void drawFire() {
  display.clearDisplay();
  for (int y = 0; y < FIRE_HEIGHT; y++) {
    for (int x = 0; x < FIRE_WIDTH; x++) {
      int intensity = fire[y * FIRE_WIDTH + x];
      if (intensity > 200) {  // Higher threshold for monochrome
        display.drawPixel(x, y, SSD1306_WHITE);
      }
    }
  }
  display.display();
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);  // Pull-up for NO button
  Wire.begin(SDA_PIN, SCL_PIN);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }
  initFire();
  drawFire();
}

void loop() {
  updateFire();
  drawFire();

  // Button press detection (edge trigger for reset)
  bool currentButtonState = digitalRead(BUTTON_PIN);
  if (currentButtonState == LOW && lastButtonState == HIGH) {
    // Button pressed (transition from HIGH to LOW)
    initFire();
    drawFire();
    delay(200);  // Debounce delay
  }
  lastButtonState = currentButtonState;

  delay(2);
}
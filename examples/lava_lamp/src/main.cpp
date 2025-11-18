#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <esp_random.h>

#include "config.h"

constexpr int kBallCount = 4;
constexpr float kMinRadius = 9.0f;
constexpr float kMaxRadius = 12.0f;
constexpr float kMinSpeed = 0.65f;
constexpr float kMaxSpeed = 2.0f;
constexpr float kFieldThreshold = 0.45f;
constexpr float kMinRadiusDrift = 0.005f;
constexpr float kMaxRadiusDrift = 0.02f;
constexpr unsigned long kFrameDelay = 0;
constexpr int kRenderSkip = 4;
constexpr int kGridWidth = (SCREEN_WIDTH + kRenderSkip - 1) / kRenderSkip;
constexpr int kGridHeight = (SCREEN_HEIGHT + kRenderSkip - 1) / kRenderSkip;

struct Ball {
  float x;
  float y;
  float vx;
  float vy;
  float radius;
  float radiusDrift;
};

static Ball balls[kBallCount];
static unsigned long lastFrameTime = 0;

static int lastButtonState = HIGH;
static unsigned long lastButtonChange = 0;
static bool buttonHandled = false;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
static float fieldGrid[kGridHeight][kGridWidth];

float randomFloat(float minValue, float maxValue) {
  float scale = static_cast<float>(random(1000)) / 1000.0f;
  return minValue + (maxValue - minValue) * scale;
}

void resetBalls() {
  for (int i = 0; i < kBallCount; ++i) {
    balls[i].radius = randomFloat(kMinRadius, kMaxRadius);
    balls[i].x = randomFloat(balls[i].radius, SCREEN_WIDTH - balls[i].radius);
    balls[i].y = randomFloat(balls[i].radius, SCREEN_HEIGHT - balls[i].radius);
    balls[i].vx = randomFloat(-kMaxSpeed, kMaxSpeed);
    balls[i].vy = randomFloat(-kMaxSpeed, kMaxSpeed);
    if (fabs(balls[i].vx) < kMinSpeed) {
      balls[i].vx = copysign(kMinSpeed, balls[i].vx == 0 ? 1 : balls[i].vx);
    }
    if (fabs(balls[i].vy) < kMinSpeed) {
      balls[i].vy = copysign(kMinSpeed, balls[i].vy == 0 ? 1 : balls[i].vy);
    }
    float drift = randomFloat(kMinRadiusDrift, kMaxRadiusDrift);
    balls[i].radiusDrift = (random(0, 2) == 0) ? drift : -drift;
  }
}

void updateBalls() {
  for (int i = 0; i < kBallCount; ++i) {
    balls[i].x += balls[i].vx;
    balls[i].y += balls[i].vy;

    balls[i].radius += balls[i].radiusDrift;
    if (balls[i].radius <= kMinRadius) {
      balls[i].radius = kMinRadius;
      balls[i].radiusDrift = randomFloat(kMinRadiusDrift, kMaxRadiusDrift);
    } else if (balls[i].radius >= kMaxRadius) {
      balls[i].radius = kMaxRadius;
      balls[i].radiusDrift = -randomFloat(kMinRadiusDrift, kMaxRadiusDrift);
    }

    if (balls[i].x - balls[i].radius <= 0 || balls[i].x + balls[i].radius >= SCREEN_WIDTH) {
      balls[i].vx = -balls[i].vx;
      balls[i].x = constrain(balls[i].x, balls[i].radius, SCREEN_WIDTH - balls[i].radius);
    }
    if (balls[i].y - balls[i].radius <= 0 || balls[i].y + balls[i].radius >= SCREEN_HEIGHT) {
      balls[i].vy = -balls[i].vy;
      balls[i].y = constrain(balls[i].y, balls[i].radius, SCREEN_HEIGHT - balls[i].radius);
    }
  }
}

bool checkButtonPressed() {
  int reading = digitalRead(BUTTON_PIN);
  if (reading != lastButtonState) {
    lastButtonChange = millis();
    lastButtonState = reading;
  }

  if (millis() - lastButtonChange > BUTTON_TAP_TIME) {
    if (reading == LOW && !buttonHandled) {
      buttonHandled = true;
      return true;
    }
    if (reading == HIGH) {
      buttonHandled = false;
    }
  }
  return false;
}

float sampleFieldAt(int x, int y) {
  float field = 0.0f;
  for (int i = 0; i < kBallCount; ++i) {
    float dx = static_cast<float>(x) - balls[i].x;
    float dy = static_cast<float>(y) - balls[i].y;
    float dist2 = dx * dx + dy * dy + 0.1f;
    field += (balls[i].radius * balls[i].radius) / dist2;
  }
  return field;
}

void interpolateEdge(float f1, float f2, int x1, int y1, int x2, int y2, int& ix, int& iy) {
  float t = (kFieldThreshold - f1) / (f2 - f1);
  ix = x1 + static_cast<int>((x2 - x1) * t);
  iy = y1 + static_cast<int>((y2 - y1) * t);
}

void renderMetaballs() {
  display.clearDisplay();
  display.drawRoundRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 4, SSD1306_WHITE);

  for (int gy = 0; gy < kGridHeight; ++gy) {
    for (int gx = 0; gx < kGridWidth; ++gx) {
      int sampleX = gx * kRenderSkip + kRenderSkip / 2;
      int sampleY = gy * kRenderSkip + kRenderSkip / 2;
      sampleX = min(sampleX, SCREEN_WIDTH - 1);
      sampleY = min(sampleY, SCREEN_HEIGHT - 1);

      fieldGrid[gy][gx] = sampleFieldAt(sampleX, sampleY);
    }
  }

  for (int gy = 0; gy < kGridHeight - 1; ++gy) {
    for (int gx = 0; gx < kGridWidth - 1; ++gx) {
      int cellX = gx * kRenderSkip + kRenderSkip / 2;
      int cellY = gy * kRenderSkip + kRenderSkip / 2;

      float tl = fieldGrid[gy][gx];
      float tr = fieldGrid[gy][gx + 1];
      float bl = fieldGrid[gy + 1][gx];
      float br = fieldGrid[gy + 1][gx + 1];

      int caseIndex = (tl > kFieldThreshold ? 8 : 0) |
                      (tr > kFieldThreshold ? 4 : 0) |
                      (br > kFieldThreshold ? 2 : 0) |
                      (bl > kFieldThreshold ? 1 : 0);

      int px[4], py[4];
      interpolateEdge(tl, tr, cellX, cellY, cellX + kRenderSkip, cellY, px[0], py[0]); // top
      interpolateEdge(tr, br, cellX + kRenderSkip, cellY, cellX + kRenderSkip, cellY + kRenderSkip, px[1], py[1]); // right
      interpolateEdge(br, bl, cellX + kRenderSkip, cellY + kRenderSkip, cellX, cellY + kRenderSkip, px[2], py[2]); // bottom
      interpolateEdge(bl, tl, cellX, cellY + kRenderSkip, cellX, cellY, px[3], py[3]); // left

      switch (caseIndex) {
        case 1: display.drawLine(px[3], py[3], px[2], py[2], SSD1306_WHITE); break;
        case 2: display.drawLine(px[1], py[1], px[2], py[2], SSD1306_WHITE); break;
        case 3: display.drawLine(px[3], py[3], px[1], py[1], SSD1306_WHITE); break;
        case 4: display.drawLine(px[0], py[0], px[1], py[1], SSD1306_WHITE); break;
        case 5: display.drawLine(px[3], py[3], px[0], py[0], SSD1306_WHITE); display.drawLine(px[1], py[1], px[2], py[2], SSD1306_WHITE); break;
        case 6: display.drawLine(px[0], py[0], px[2], py[2], SSD1306_WHITE); break;
        case 7: display.drawLine(px[3], py[3], px[0], py[0], SSD1306_WHITE); break;
        case 8: display.drawLine(px[0], py[0], px[3], py[3], SSD1306_WHITE); break;
        case 9: display.drawLine(px[0], py[0], px[2], py[2], SSD1306_WHITE); break;
        case 10: display.drawLine(px[0], py[0], px[3], py[3], SSD1306_WHITE); display.drawLine(px[1], py[1], px[2], py[2], SSD1306_WHITE); break;
        case 11: display.drawLine(px[0], py[0], px[1], py[1], SSD1306_WHITE); break;
        case 12: display.drawLine(px[3], py[3], px[1], py[1], SSD1306_WHITE); break;
        case 13: display.drawLine(px[1], py[1], px[2], py[2], SSD1306_WHITE); break;
        case 14: display.drawLine(px[3], py[3], px[2], py[2], SSD1306_WHITE); break;
        // case 0 and 15: no lines
      }
    }
  }

  display.display();
}

void setup() {
  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
  randomSeed(esp_random());
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    while (true) {
      delay(100);
    }
  }

  display.clearDisplay();
  display.display();
  resetBalls();
}

void loop() {
  if (checkButtonPressed()) {
    resetBalls();
  }

  unsigned long now = millis();
  if (now - lastFrameTime < kFrameDelay) {
    return;
  }
  lastFrameTime = now;

  updateBalls();
  renderMetaballs();
}
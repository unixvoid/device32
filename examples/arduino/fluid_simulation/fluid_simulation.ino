#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// OLED display settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1  // No reset pin
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define SDA_PIN 7
#define SCL_PIN 6

#define GRID_WIDTH 64
#define GRID_HEIGHT 32
float height[GRID_WIDTH];
float velocity[GRID_WIDTH];

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }
  display.clearDisplay();
  display.display();

  // Initialize height and velocity
  for (int i = 0; i < GRID_WIDTH; i++) {
    height[i] = GRID_HEIGHT / 2;  // Start at middle
    velocity[i] = 0;
  }
}

void loop() {
  // Update wave simulation
  for (int i = 1; i < GRID_WIDTH - 1; i++) {
    float laplacian = height[i - 1] + height[i + 1] - 2 * height[i];
    velocity[i] += 0.1 * laplacian - 0.01 * velocity[i];  // Wave equation with damping
  }
  // Fixed boundaries
  velocity[0] = 0;
  velocity[GRID_WIDTH - 1] = 0;

  for (int i = 0; i < GRID_WIDTH; i++) {
    height[i] += velocity[i];
    // Keep within bounds
    if (height[i] < 0) height[i] = 0;
    if (height[i] > GRID_HEIGHT - 1) height[i] = GRID_HEIGHT - 1;
  }

  // Occasionally add a slosh (random disturbance)
  static unsigned long lastSlosh = 0;
  if (millis() - lastSlosh > 1500) {  // Every 1.5 seconds
    lastSlosh = millis();
    int pos = random(1, GRID_WIDTH - 1);
    velocity[pos] += random(-3, 3);  // Random impulse
  }

  // Draw the wave
  display.clearDisplay();
  for (int i = 0; i < GRID_WIDTH; i++) {
    int h = (int)height[i];
    // Draw filled column from height to bottom
    display.fillRect(i * 2, h * 2, 2, (SCREEN_HEIGHT - h * 2), SSD1306_WHITE);
  }
  display.display();

  delay(1);  // Frame delay
}
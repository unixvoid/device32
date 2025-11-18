//
// Conways game of life implementation in arduino.
//   Toroidal wrap around is implemented to increase cell movement
//

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

// Button pin
#define BUTTON_PIN 5

// Grid settings
#define GRID_WIDTH 64
#define GRID_HEIGHT 32
#define CELL_SIZE 2

// Game of Life grids
bool currentGrid[GRID_HEIGHT][GRID_WIDTH];
bool nextGrid[GRID_HEIGHT][GRID_WIDTH];

// Button state for debouncing
bool lastButtonState = HIGH;

// Function to initialize the display
void initDisplay() {
  Wire.begin(SDA_PIN, SCL_PIN);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }
  display.clearDisplay();
  display.display();
}

void randomizeGrid() {
  for (int y = 0; y < GRID_HEIGHT; y++) {
    for (int x = 0; x < GRID_WIDTH; x++) {
      currentGrid[y][x] = random(4) < 2; // 50% chance to populate cell
    }
  }
}

int countNeighbors(int x, int y) {
  int count = 0;
  for (int dy = -1; dy <= 1; dy++) {
    for (int dx = -1; dx <= 1; dx++) {
      if (dx == 0 && dy == 0) continue;
      int nx = (x + dx + GRID_WIDTH) % GRID_WIDTH;   // Wrap horizontally
      int ny = (y + dy + GRID_HEIGHT) % GRID_HEIGHT; // Wrap vertically
      count += currentGrid[ny][nx] ? 1 : 0;
    }
  }
  return count;
}

void updateGrid() {
  for (int y = 0; y < GRID_HEIGHT; y++) {
    for (int x = 0; x < GRID_WIDTH; x++) {
      int neighbors = countNeighbors(x, y);
      if (currentGrid[y][x]) {
        // Alive: survives if 2 or 3 neighbors
        nextGrid[y][x] = (neighbors == 2 || neighbors == 3);
      } else {
        // Dead: becomes alive if exactly 3 neighbors
        nextGrid[y][x] = (neighbors == 3);
      }
    }
  }
  // Swap grids
  for (int y = 0; y < GRID_HEIGHT; y++) {
    for (int x = 0; x < GRID_WIDTH; x++) {
      currentGrid[y][x] = nextGrid[y][x];
    }
  }
}

// Function to draw the grid on the display
void drawGrid() {
  display.clearDisplay();
  for (int y = 0; y < GRID_HEIGHT; y++) {
    for (int x = 0; x < GRID_WIDTH; x++) {
      if (currentGrid[y][x]) {
        display.fillRect(x * CELL_SIZE, y * CELL_SIZE, CELL_SIZE, CELL_SIZE, SSD1306_WHITE);
      }
    }
  }
  display.display();
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  initDisplay();
  randomizeGrid();
  drawGrid();
}

void loop() {
  bool currentButtonState = digitalRead(BUTTON_PIN);
  if (currentButtonState == LOW && lastButtonState == HIGH) {
    randomizeGrid();
    drawGrid();
    delay(200);
  }
  lastButtonState = currentButtonState;

  updateGrid();
  drawGrid();
  delay(100);
}
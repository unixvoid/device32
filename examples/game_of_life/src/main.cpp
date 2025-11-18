#include "config.h"
#include <Wire.h>
#include <Adafruit_GFX.h>

// Game of Life grids
bool currentGrid[GRID_HEIGHT][GRID_WIDTH];
bool nextGrid[GRID_HEIGHT][GRID_WIDTH];

bool lastButtonState = HIGH;

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
      int nx = (x + dx + GRID_WIDTH) % GRID_WIDTH;
      int ny = (y + dy + GRID_HEIGHT) % GRID_HEIGHT;
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
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <vector>
#include <set>
#include <algorithm>
#include <cstring>
#include "config.h"

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Dungeon dimensions
#define DUNGEON_WIDTH 64
#define DUNGEON_HEIGHT 32
#define CELL_SIZE 2

// Dungeon cell types
#define CELL_WALL 0
#define CELL_FLOOR 1
#define CELL_CORRIDOR 2

// Structures for dungeon generation
struct Room {
  int x, y, w, h;
};

uint8_t dungeon[DUNGEON_HEIGHT][DUNGEON_WIDTH];
std::vector<Room> rooms;
std::vector<std::pair<int, int>> drawQueue;

// Initialize dungeon as all walls
void initDungeon() {
  memset(dungeon, CELL_WALL, sizeof(dungeon));
  rooms.clear();
  drawQueue.clear();
}

// Create a room (carve floor)
void createRoom(int x, int y, int w, int h) {
  for (int dy = 0; dy < h; dy++) {
    for (int dx = 0; dx < w; dx++) {
      if (y + dy < DUNGEON_HEIGHT && x + dx < DUNGEON_WIDTH) {
        dungeon[y + dy][x + dx] = CELL_FLOOR;
      }
    }
  }
  rooms.push_back({x, y, w, h});
}

// Trace the perimeter of all dungeon features as a single continuous line
void tracePerimeter() {
  drawQueue.clear();
  
  // Find all wall cells adjacent to floor/corridor cells
  std::vector<std::pair<int, int>> perimeter;
  for (int y = 0; y < DUNGEON_HEIGHT; y++) {
    for (int x = 0; x < DUNGEON_WIDTH; x++) {
      if (dungeon[y][x] == CELL_WALL) {
        // Check if adjacent to floor or corridor
        bool isPerimeter = false;
        if ((x > 0 && dungeon[y][x-1] != CELL_WALL) ||
            (x < DUNGEON_WIDTH-1 && dungeon[y][x+1] != CELL_WALL) ||
            (y > 0 && dungeon[y-1][x] != CELL_WALL) ||
            (y < DUNGEON_HEIGHT-1 && dungeon[y+1][x] != CELL_WALL)) {
          isPerimeter = true;
        }
        
        if (isPerimeter) {
          perimeter.push_back(std::make_pair(x, y));
        }
      }
    }
  }
  
  // Use a simple chain-linking algorithm to create continuous path
  if (!perimeter.empty()) {
    drawQueue.push_back(perimeter[0]);
    std::set<std::pair<int, int>> visited;
    visited.insert(perimeter[0]);
    
    while ((int)visited.size() < (int)perimeter.size()) {
      int cx = drawQueue.back().first;
      int cy = drawQueue.back().second;
      double minDist = 1e9;
      int nextIdx = -1;
      
      // Find nearest unvisited perimeter point
      for (int i = 0; i < (int)perimeter.size(); i++) {
        if (visited.find(perimeter[i]) == visited.end()) {
          int dx = perimeter[i].first - cx;
          int dy = perimeter[i].second - cy;
          double dist = dx*dx + dy*dy;
          if (dist < minDist) {
            minDist = dist;
            nextIdx = i;
          }
        }
      }
      
      if (nextIdx >= 0) {
        drawQueue.push_back(perimeter[nextIdx]);
        visited.insert(perimeter[nextIdx]);
      } else {
        break;
      }
    }
  }
}

// Create a corridor between two points using tunnel
void createCorridor(int x1, int y1, int x2, int y2) {
  int x = x1, y = y1;
  
  // Move horizontally first
  while (x != x2) {
    if (dungeon[y][x] != CELL_FLOOR) {
      dungeon[y][x] = CELL_CORRIDOR;
      drawQueue.push_back({x, y});
    }
    x += (x2 > x) ? 1 : -1;
  }
  
  // Then move vertically
  while (y != y2) {
    if (dungeon[y][x] != CELL_FLOOR) {
      dungeon[y][x] = CELL_CORRIDOR;
      drawQueue.push_back({x, y});
    }
    y += (y2 > y) ? 1 : -1;
  }
  
  // Final position
  if (dungeon[y][x] != CELL_FLOOR) {
    dungeon[y][x] = CELL_CORRIDOR;
    drawQueue.push_back({x, y});
  }
}

// Generate a complete dungeon level
void generateDungeon() {
  initDungeon();

  // Generate rooms
  int numRooms = random(4, 7);
  int attempts = 0;
  int maxAttempts = 30;
  
  while ((int)rooms.size() < numRooms && attempts < maxAttempts) {
    int w = random(6, 16);
    int h = random(5, 14);
    int x = random(1, DUNGEON_WIDTH - w - 1);
    int y = random(1, DUNGEON_HEIGHT - h - 1);
    
    // Check if room overlaps
    bool overlaps = false;
    for (int r = 0; r < (int)rooms.size(); r++) {
      int rx = rooms[r].x, ry = rooms[r].y, rw = rooms[r].w, rh = rooms[r].h;
      if (!(x + w < rx || x > rx + rw || y + h < ry || y > ry + rh)) {
        overlaps = true;
        break;
      }
    }
    
    if (!overlaps) {
      createRoom(x, y, w, h);
    }
    attempts++;
  }
  
  // Connect rooms with corridors
  for (int i = 0; i < (int)rooms.size() - 1; i++) {
    int x1 = rooms[i].x + rooms[i].w / 2;
    int y1 = rooms[i].y + rooms[i].h / 2;
    int x2 = rooms[i + 1].x + rooms[i + 1].w / 2;
    int y2 = rooms[i + 1].y + rooms[i + 1].h / 2;
    createCorridor(x1, y1, x2, y2);
  }
  
  // Trace the complete perimeter as a single continuous line
  tracePerimeter();
}

// Draw the dungeon progressively as a continuous line, then complete any missed edges
void progressiveDraw(unsigned long drawTime) {
  display.clearDisplay();
  
  unsigned long startTime = millis();
  int itemsDrawn = 0;
  int totalItems = drawQueue.size();
  
  // Draw perimeter as continuous line at a steady pace to fit the draw time
  while (itemsDrawn < totalItems) {
    unsigned long elapsed = millis() - startTime;
    
    int itemsShouldBe = (int)((elapsed * totalItems) / drawTime);
    itemsShouldBe = min(itemsShouldBe, totalItems);
    
    while (itemsDrawn < itemsShouldBe) {
      int x = drawQueue[itemsDrawn].first;
      int y = drawQueue[itemsDrawn].second;
      display.fillRect(x * CELL_SIZE, y * CELL_SIZE, CELL_SIZE, CELL_SIZE, SSD1306_WHITE);
      itemsDrawn++;
    }
    
    display.display();
    delay(10);
  }
}

void setup() {
  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.display();
  randomSeed(analogRead(0));
}

void loop() {
  generateDungeon();
  
  // Draw dungeon progressively over 5 seconds
  progressiveDraw(5000);
  
  // Show completed dungeon for 1 second
  delay(1000);
  
  // Clear and start over
  display.clearDisplay();
  display.display();
  delay(500);
}
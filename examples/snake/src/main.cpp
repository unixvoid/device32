#include <Wire.h>
#include <Adafruit_GFX.h>
#include <vector>
#include <queue>
#include <set>
#include "config.h"

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

typedef std::pair<int, int> Pos;
enum Dir { UP, DOWN, LEFT, RIGHT };

std::vector<Pos> snake;
Pos food;
Dir dir = RIGHT;
int score = 0;
bool gameOver = false;

Pos moveHead(Dir d) {
  Pos h = snake[0];
  if (d == UP) return {h.first, h.second - 1};
  if (d == DOWN) return {h.first, h.second + 1};
  if (d == LEFT) return {h.first - 1, h.second};
  if (d == RIGHT) return {h.first + 1, h.second};
  return h;
}

bool isValidMove(Pos p) {
  if (p.first < 0 || p.first >= 32 || p.second < 0 || p.second >= 16) return false;
  for (auto s : snake) if (s == p) return false;
  return true;
}

Pos randomFree() {
  std::vector<Pos> free;
  for (int x = 0; x < 32; x++) {
    for (int y = 0; y < 16; y++) {
      bool occupied = false;
      for (auto p : snake) if (p.first == x && p.second == y) occupied = true;
      if (!occupied) free.push_back({x, y});
    }
  }
  if (free.empty()) return {-1, -1};
  return free[random(free.size())];
}

Dir getNextDir() {
  // BFS to find path to food
  std::queue<std::pair<Pos, std::vector<Dir>>> q;
  std::set<Pos> visited;
  q.push({snake[0], {}});
  visited.insert(snake[0]);
  while (!q.empty()) {
    auto front = q.front(); q.pop();
    Pos curr = front.first;
    std::vector<Dir> path = front.second;
    if (curr == food) {
      if (path.empty()) return dir;
      return path[0];
    }
    std::vector<std::pair<Pos, Dir>> nexts = {
      {{curr.first, curr.second - 1}, UP},
      {{curr.first, curr.second + 1}, DOWN},
      {{curr.first - 1, curr.second}, LEFT},
      {{curr.first + 1, curr.second}, RIGHT}
    };
    for (auto [n, d] : nexts) {
      if (n.first >= 0 && n.first < 32 && n.second >= 0 && n.second < 16 && visited.find(n) == visited.end()) {
        bool obs = false;
        for (auto p : snake) if (p == n) obs = true;
        if (!obs) {
          std::vector<Dir> newpath = path;
          newpath.push_back(d);
          q.push({n, newpath});
          visited.insert(n);
        }
      }
    }
  }
  // No path, choose safe move
  std::vector<Dir> possibles = {UP, DOWN, LEFT, RIGHT};
  for (auto d : possibles) {
    Pos nh = moveHead(d);
    if (isValidMove(nh)) return d;
  }
  return dir;
}

void draw() {
  display.clearDisplay();
  // draw snake
  for (auto p : snake) {
    display.fillRect(p.first * 4, p.second * 4, 4, 4, SSD1306_WHITE);
  }
  // draw food
  display.fillRect(food.first * 4, food.second * 4, 4, 4, SSD1306_WHITE);
  display.display();
}

void reset() {
  snake.clear();
  snake.push_back({16, 8}); // center
  dir = RIGHT;
  food = randomFree();
  score = 1;
  gameOver = false;
}

void setup() {
  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  randomSeed(analogRead(0));
  reset();
}

void loop() {
  if (gameOver) {
    reset();
    delay(1000);
  }
  draw();
  delay(20);
  Dir nextd = getNextDir();
  dir = nextd;
  Pos nh = moveHead(dir);
  bool ate = (nh == food);
  if (!isValidMove(nh)) {
    gameOver = true;
    return;
  }
  snake.insert(snake.begin(), nh);
  if (ate) {
    score++;
    food = randomFree();
    if (food.first == -1) gameOver = true;
  } else {
    snake.pop_back();
  }
}
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <vector>
#include <queue>
#include <set>
#include <Arduino.h>
#include "config.h"

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

enum Mode { SNAKE, BRICK_BREAK, LAVA_LAMP, BOIDS };
Mode currentMode = SNAKE;
unsigned long modeStartTime = 0;
const unsigned long MODE_DURATION = 120000; // 2 minutes

// Snake variables
typedef std::pair<int, int> Pos;
enum Dir { UP, DOWN, LEFT, RIGHT };
std::vector<Pos> snake;
Pos food;
Dir dir = RIGHT;
int score = 0;
bool gameOver = false;

Pos moveHead_snake(Dir d) {
  Pos h = snake[0];
  if (d == UP) return {h.first, h.second - 1};
  if (d == DOWN) return {h.first, h.second + 1};
  if (d == LEFT) return {h.first - 1, h.second};
  if (d == RIGHT) return {h.first + 1, h.second};
  return h;
}

bool isValidMove_snake(Pos p) {
  if (p.first < 0 || p.first >= 32 || p.second < 0 || p.second >= 16) return false;
  for (auto s : snake) if (s == p) return false;
  return true;
}

Pos randomFree_snake() {
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

Dir getNextDir_snake() {
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
  std::vector<Dir> possibles = {UP, DOWN, LEFT, RIGHT};
  for (auto d : possibles) {
    Pos nh = moveHead_snake(d);
    if (isValidMove_snake(nh)) return d;
  }
  return dir;
}

void draw_snake() {
  display.clearDisplay();
  for (auto p : snake) {
    display.fillRect(p.first * 4, p.second * 4, 4, 4, SSD1306_WHITE);
  }
  display.fillRect(food.first * 4, food.second * 4, 4, 4, SSD1306_WHITE);
  display.display();
}

void reset_snake() {
  snake.clear();
  snake.push_back({16, 8});
  dir = RIGHT;
  food = randomFree_snake();
  score = 1;
  gameOver = false;
}

// Brick Break variables
#define BRICK_ROWS 4
#define BRICK_COLS 4
#define BRICK_WIDTH 15
#define BRICK_HEIGHT 8
#define PADDLE_WIDTH 16
#define PADDLE_HEIGHT 4
#define GAME_WIDTH 64
#define GAME_HEIGHT 128
bool bricks[BRICK_ROWS][BRICK_COLS];
float ballX, ballY, ballVelX, ballVelY;
float paddleX;
enum GameState { PLAYING, WIN, LOSE };
GameState gameState = PLAYING;
unsigned long endTime;
int bouncesSinceBrick = 0;
const int brick_draw_width = BRICK_WIDTH - 2;
const int brick_gap = 2;
const int total_brick_width = BRICK_COLS * brick_draw_width + (BRICK_COLS - 1) * brick_gap;
const int brick_start_x = 2;
const int brick_start_y = 2;

void resetGame_brick() {
  for (int r = 0; r < BRICK_ROWS; r++) {
    for (int c = 0; c < BRICK_COLS; c++) {
      bricks[r][c] = true;
    }
  }
  ballX = random(10, GAME_WIDTH - 10);
  ballY = GAME_HEIGHT - 20;
  ballVelX = random(-2, 3);
  if (ballVelX == 0) ballVelX = 1;
  ballVelY = random(-3, -1);
  paddleX = GAME_WIDTH / 2.0 - PADDLE_WIDTH / 2.0;
  gameState = PLAYING;
  bouncesSinceBrick = 0;
}

// Lava Lamp variables
constexpr int kBallCount = 4;
constexpr float kMinRadius = 9.0f;
constexpr float kMaxRadius = 11.0f;
constexpr float kMinSpeed = 0.35f;
constexpr float kMaxSpeed = 1.0f;
constexpr float kFieldThreshold = 0.45f;
constexpr float kMinRadiusDrift = 0.005f;
constexpr float kMaxRadiusDrift = 0.02f;
constexpr unsigned long kFrameDelay = 0;
struct Ball {
  float x;
  float y;
  float vx;
  float vy;
  float radius;
  float radiusDrift;
};
static Ball balls[kBallCount];
static unsigned long lastFrameTime_lava = 0;
static float fieldGrid[SCREEN_HEIGHT / 4][SCREEN_WIDTH / 4]; // adjusted for grid

float randomFloat_lava(float minValue, float maxValue) {
  float scale = static_cast<float>(random(1000)) / 1000.0f;
  return minValue + (maxValue - minValue) * scale;
}

void resetBalls_lava() {
  for (int i = 0; i < kBallCount; ++i) {
    balls[i].radius = randomFloat_lava(kMinRadius, kMaxRadius);
    balls[i].x = randomFloat_lava(balls[i].radius, SCREEN_WIDTH - balls[i].radius);
    balls[i].y = randomFloat_lava(balls[i].radius, SCREEN_HEIGHT - balls[i].radius);
    balls[i].vx = randomFloat_lava(-kMaxSpeed, kMaxSpeed);
    balls[i].vy = randomFloat_lava(-kMaxSpeed, kMaxSpeed);
    if (fabs(balls[i].vx) < kMinSpeed) {
      balls[i].vx = copysign(kMinSpeed, balls[i].vx == 0 ? 1 : balls[i].vx);
    }
    if (fabs(balls[i].vy) < kMinSpeed) {
      balls[i].vy = copysign(kMinSpeed, balls[i].vy == 0 ? 1 : balls[i].vy);
    }
    float drift = randomFloat_lava(kMinRadiusDrift, kMaxRadiusDrift);
    balls[i].radiusDrift = (random(0, 2) == 0) ? drift : -drift;
  }
}

void updateBalls_lava() {
  for (int i = 0; i < kBallCount; ++i) {
    balls[i].x += balls[i].vx;
    balls[i].y += balls[i].vy;
    balls[i].radius += balls[i].radiusDrift;
    if (balls[i].radius <= kMinRadius) {
      balls[i].radius = kMinRadius;
      balls[i].radiusDrift = randomFloat_lava(kMinRadiusDrift, kMaxRadiusDrift);
    } else if (balls[i].radius >= kMaxRadius) {
      balls[i].radius = kMaxRadius;
      balls[i].radiusDrift = -randomFloat_lava(kMinRadiusDrift, kMaxRadiusDrift);
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

float sampleFieldAt_lava(int x, int y) {
  float field = 0.0f;
  for (int i = 0; i < kBallCount; ++i) {
    float dx = static_cast<float>(x) - balls[i].x;
    float dy = static_cast<float>(y) - balls[i].y;
    float dist2 = dx * dx + dy * dy + 0.1f;
    field += (balls[i].radius * balls[i].radius) / dist2;
  }
  return field;
}

void interpolateEdge_lava(float f1, float f2, int x1, int y1, int x2, int y2, int& ix, int& iy) {
  float t = (kFieldThreshold - f1) / (f2 - f1);
  ix = x1 + static_cast<int>((x2 - x1) * t);
  iy = y1 + static_cast<int>((y2 - y1) * t);
}

void renderMetaballs_lava() {
  display.clearDisplay();
  display.drawRoundRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 4, SSD1306_WHITE);
  int kRenderSkip = 4;
  int kGridWidth = (SCREEN_WIDTH + kRenderSkip - 1) / kRenderSkip;
  int kGridHeight = (SCREEN_HEIGHT + kRenderSkip - 1) / kRenderSkip;
  for (int gy = 0; gy < kGridHeight; ++gy) {
    for (int gx = 0; gx < kGridWidth; ++gx) {
      int sampleX = gx * kRenderSkip + kRenderSkip / 2;
      int sampleY = gy * kRenderSkip + kRenderSkip / 2;
      sampleX = min(sampleX, SCREEN_WIDTH - 1);
      sampleY = min(sampleY, SCREEN_HEIGHT - 1);
      fieldGrid[gy][gx] = sampleFieldAt_lava(sampleX, sampleY);
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
      interpolateEdge_lava(tl, tr, cellX, cellY, cellX + kRenderSkip, cellY, px[0], py[0]);
      interpolateEdge_lava(tr, br, cellX + kRenderSkip, cellY, cellX + kRenderSkip, cellY + kRenderSkip, px[1], py[1]);
      interpolateEdge_lava(br, bl, cellX + kRenderSkip, cellY + kRenderSkip, cellX, cellY + kRenderSkip, px[2], py[2]);
      interpolateEdge_lava(bl, tl, cellX, cellY + kRenderSkip, cellX, cellY, px[3], py[3]);
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
      }
    }
  }
  display.display();
}

// Boids variables
#define NUM_BOIDS 42
#define MAX_SPEED 2.2f
#define MAX_FORCE 0.35f
#define SEPARATION_DISTANCE 18.0f
#define ALIGNMENT_DISTANCE 8.0f
#define COHESION_DISTANCE 22.0f
#define SEPARATION_WEIGHT 1.2f
#define ALIGNMENT_WEIGHT 0.8f
#define COHESION_WEIGHT 0.9f
#define EDGE_DISTANCE 12.0f
#define EDGE_WEIGHT 1.0f
#define BOID_TAIL_LENGTH 1.0f
#define TRAIL_LENGTH 2
#define GRID_CELL_SIZE 35
#define GRID_WIDTH (SCREEN_WIDTH / GRID_CELL_SIZE + 1)
#define GRID_HEIGHT (SCREEN_HEIGHT / GRID_CELL_SIZE + 1)
#define MAX_BOIDS_PER_CELL 10

struct GridCell_boids {
    uint8_t boid_indices[MAX_BOIDS_PER_CELL];
    uint8_t count;
};

struct Boid {
    float x, y;
    float vx, vy;
    float ax, ay;
    int8_t trail_x[TRAIL_LENGTH];
    int8_t trail_y[TRAIL_LENGTH];
    uint8_t trail_index;
};

Boid boids[NUM_BOIDS];
GridCell_boids grid[GRID_WIDTH][GRID_HEIGHT];

inline float distanceSquared_boids(float x1, float y1, float x2, float y2) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    return dx * dx + dy * dy;
}

inline float limitMagnitude_boids(float val, float limit) {
    if (val > limit) return limit;
    if (val < -limit) return -limit;
    return val;
}

void buildGrid_boids() {
    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            grid[x][y].count = 0;
        }
    }
    for (uint8_t i = 0; i < NUM_BOIDS; i++) {
        int cell_x = (int)boids[i].x / GRID_CELL_SIZE;
        int cell_y = (int)boids[i].y / GRID_CELL_SIZE;
        if (cell_x < 0) cell_x = 0;
        if (cell_x >= GRID_WIDTH) cell_x = GRID_WIDTH - 1;
        if (cell_y < 0) cell_y = 0;
        if (cell_y >= GRID_HEIGHT) cell_y = GRID_HEIGHT - 1;
        GridCell_boids& cell = grid[cell_x][cell_y];
        if (cell.count < MAX_BOIDS_PER_CELL) {
            cell.boid_indices[cell.count++] = i;
        }
    }
}

void getNearbyBoids_boids(int index, uint8_t* nearby, uint8_t& count, float max_dist_sq) {
    count = 0;
    float max_dist_sq_limit = max_dist_sq + 100;
    int cell_x = (int)boids[index].x / GRID_CELL_SIZE;
    int cell_y = (int)boids[index].y / GRID_CELL_SIZE;
    if (cell_x < 0) cell_x = 0;
    if (cell_x >= GRID_WIDTH) cell_x = GRID_WIDTH - 1;
    if (cell_y < 0) cell_y = 0;
    if (cell_y >= GRID_HEIGHT) cell_y = GRID_HEIGHT - 1;
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            int nx = cell_x + dx;
            int ny = cell_y + dy;
            if (nx >= 0 && nx < GRID_WIDTH && ny >= 0 && ny < GRID_HEIGHT) {
                GridCell_boids& neighbor_cell = grid[nx][ny];
                for (uint8_t j = 0; j < neighbor_cell.count; j++) {
                    uint8_t boid_idx = neighbor_cell.boid_indices[j];
                    if (boid_idx == index) continue;
                    float dist_sq = distanceSquared_boids(boids[index].x, boids[index].y, boids[boid_idx].x, boids[boid_idx].y);
                    if (dist_sq < max_dist_sq_limit && count < 16) {
                        nearby[count++] = boid_idx;
                    }
                }
            }
        }
    }
}

void separate_boids(uint8_t index) {
    uint8_t nearby[16];
    uint8_t count = 0;
    float sep_dist_sq = SEPARATION_DISTANCE * SEPARATION_DISTANCE;
    getNearbyBoids_boids(index, nearby, count, sep_dist_sq);
    float steerx = 0, steery = 0;
    int sep_count = 0;
    for (uint8_t j = 0; j < count; j++) {
        uint8_t i = nearby[j];
        float dist_sq = distanceSquared_boids(boids[index].x, boids[index].y, boids[i].x, boids[i].y);
        if (dist_sq < sep_dist_sq && dist_sq > 0) {
            float dx = boids[index].x - boids[i].x;
            float dy = boids[index].y - boids[i].y;
            float len = sqrt(dist_sq);
            dx /= len;
            dy /= len;
            steerx += dx;
            steery += dy;
            sep_count++;
        }
    }
    if (sep_count > 0) {
        steerx /= sep_count;
        steery /= sep_count;
        steerx = limitMagnitude_boids(steerx, MAX_FORCE);
        steery = limitMagnitude_boids(steery, MAX_FORCE);
        boids[index].ax += steerx * SEPARATION_WEIGHT;
        boids[index].ay += steery * SEPARATION_WEIGHT;
    }
}

void align_boids(uint8_t index) {
    uint8_t nearby[16];
    uint8_t count = 0;
    float align_dist_sq = ALIGNMENT_DISTANCE * ALIGNMENT_DISTANCE;
    getNearbyBoids_boids(index, nearby, count, align_dist_sq);
    float avgvx = 0, avgvy = 0;
    int align_count = 0;
    for (uint8_t j = 0; j < count; j++) {
        uint8_t i = nearby[j];
        float dist_sq = distanceSquared_boids(boids[index].x, boids[index].y, boids[i].x, boids[i].y);
        if (dist_sq < align_dist_sq) {
            avgvx += boids[i].vx;
            avgvy += boids[i].vy;
            align_count++;
        }
    }
    if (align_count > 0) {
        avgvx /= align_count;
        avgvy /= align_count;
        avgvx = limitMagnitude_boids(avgvx, MAX_FORCE);
        avgvy = limitMagnitude_boids(avgvy, MAX_FORCE);
        boids[index].ax += avgvx * ALIGNMENT_WEIGHT;
        boids[index].ay += avgvy * ALIGNMENT_WEIGHT;
    }
}

void cohesion_boids(uint8_t index) {
    uint8_t nearby[16];
    uint8_t count = 0;
    float cohesion_dist_sq = COHESION_DISTANCE * COHESION_DISTANCE;
    getNearbyBoids_boids(index, nearby, count, cohesion_dist_sq);
    float targetx = 0, targety = 0;
    int cohesion_count = 0;
    for (uint8_t j = 0; j < count; j++) {
        uint8_t i = nearby[j];
        float dist_sq = distanceSquared_boids(boids[index].x, boids[index].y, boids[i].x, boids[i].y);
        if (dist_sq < cohesion_dist_sq) {
            targetx += boids[i].x;
            targety += boids[i].y;
            cohesion_count++;
        }
    }
    if (cohesion_count > 0) {
        targetx /= cohesion_count;
        targety /= cohesion_count;
        float dx = targetx - boids[index].x;
        float dy = targety - boids[index].y;
        float len_sq = dx * dx + dy * dy;
        if (len_sq > 0) {
            float len = sqrt(len_sq);
            dx /= len;
            dy /= len;
            dx = limitMagnitude_boids(dx, MAX_FORCE);
            dy = limitMagnitude_boids(dy, MAX_FORCE);
            boids[index].ax += dx * COHESION_WEIGHT;
            boids[index].ay += dy * COHESION_WEIGHT;
        }
    }
}

inline void avoidEdges_boids(uint8_t index) {
    float steerx = 0, steery = 0;
    const float OFFSCREEN_ALLOWANCE = 6.0f;
    if (boids[index].x < -OFFSCREEN_ALLOWANCE) {
        steerx += 2.0f;
    } else if (boids[index].x < EDGE_DISTANCE) {
        steerx += 1.0f;
    }
    if (boids[index].x > SCREEN_WIDTH + OFFSCREEN_ALLOWANCE) {
        steerx -= 2.0f;
    } else if (boids[index].x > SCREEN_WIDTH - EDGE_DISTANCE) {
        steerx -= 1.0f;
    }
    if (boids[index].y < -OFFSCREEN_ALLOWANCE) {
        steery += 2.0f;
    } else if (boids[index].y < EDGE_DISTANCE) {
        steery += 1.0f;
    }
    if (boids[index].y > SCREEN_HEIGHT + OFFSCREEN_ALLOWANCE) {
        steery -= 2.0f;
    } else if (boids[index].y > SCREEN_HEIGHT - EDGE_DISTANCE) {
        steery -= 1.0f;
    }
    steerx = limitMagnitude_boids(steerx, MAX_FORCE);
    steery = limitMagnitude_boids(steery, MAX_FORCE);
    boids[index].ax += steerx * EDGE_WEIGHT;
    boids[index].ay += steery * EDGE_WEIGHT;
}

inline void updateBoid_boids(uint8_t index) {
    separate_boids(index);
    align_boids(index);
    cohesion_boids(index);
    avoidEdges_boids(index);
    boids[index].vx += boids[index].ax;
    boids[index].vy += boids[index].ay;
    float speed_sq = boids[index].vx * boids[index].vx + boids[index].vy * boids[index].vy;
    if (speed_sq > MAX_SPEED * MAX_SPEED) {
        float speed = sqrt(speed_sq);
        boids[index].vx = (boids[index].vx / speed) * MAX_SPEED;
        boids[index].vy = (boids[index].vy / speed) * MAX_SPEED;
    }
    boids[index].x += boids[index].vx;
    boids[index].y += boids[index].vy;
    boids[index].trail_x[boids[index].trail_index] = (int8_t)boids[index].x;
    boids[index].trail_y[boids[index].trail_index] = (int8_t)boids[index].y;
    boids[index].trail_index = (boids[index].trail_index + 1) % TRAIL_LENGTH;
    boids[index].ax = 0;
    boids[index].ay = 0;
    if (boids[index].x < 0) boids[index].x = 0;
    if (boids[index].x > SCREEN_WIDTH) boids[index].x = SCREEN_WIDTH;
    if (boids[index].y < 0) boids[index].y = 0;
    if (boids[index].y > SCREEN_HEIGHT) boids[index].y = SCREEN_HEIGHT;
}

void initializeBoids_boids() {
    for (uint8_t i = 0; i < NUM_BOIDS; i++) {
        boids[i].x = random(10, SCREEN_WIDTH - 10);
        boids[i].y = random(10, SCREEN_HEIGHT - 10);
        boids[i].vx = random(-20, 20) / 10.0f;
        boids[i].vy = random(-20, 20) / 10.0f;
        boids[i].ax = 0;
        boids[i].ay = 0;
        boids[i].trail_index = 0;
        for (uint8_t j = 0; j < TRAIL_LENGTH; j++) {
            boids[i].trail_x[j] = (int8_t)boids[i].x;
            boids[i].trail_y[j] = (int8_t)boids[i].y;
        }
    }
}

void drawBoids_boids() {
    display.clearDisplay();
    for (uint8_t i = 0; i < NUM_BOIDS; i++) {
        for (uint8_t j = 0; j < TRAIL_LENGTH - 1; j++) {
            uint8_t trail_idx = (boids[i].trail_index + j) % TRAIL_LENGTH;
            uint8_t next_idx = (trail_idx + 1) % TRAIL_LENGTH;
            int x1 = boids[i].trail_x[trail_idx];
            int y1 = boids[i].trail_y[trail_idx];
            int x2 = boids[i].trail_x[next_idx];
            int y2 = boids[i].trail_y[next_idx];
            if (x1 >= 0 && x1 < SCREEN_WIDTH && y1 >= 0 && y1 < SCREEN_HEIGHT &&
                x2 >= 0 && x2 < SCREEN_WIDTH && y2 >= 0 && y2 < SCREEN_HEIGHT) {
                display.drawLine(x1, y1, x2, y2, SSD1306_WHITE);
            }
        }
    }
    for (uint8_t i = 0; i < NUM_BOIDS; i++) {
        int x = (int)boids[i].x;
        int y = (int)boids[i].y;
        float speed = sqrt(boids[i].vx * boids[i].vx + boids[i].vy * boids[i].vy);
        int x2 = x, y2 = y;
        if (speed > 0.1f) {
            x2 = x + (int)(boids[i].vx / speed * BOID_TAIL_LENGTH);
            y2 = y + (int)(boids[i].vy / speed * BOID_TAIL_LENGTH);
        }
        if (x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT) {
            x2 = constrain(x2, 0, SCREEN_WIDTH - 1);
            y2 = constrain(y2, 0, SCREEN_HEIGHT - 1);
            display.drawLine(x, y, x2, y2, SSD1306_WHITE);
        }
    }
    display.display();
}

void updateAllBoids_boids() {
    buildGrid_boids();
    for (uint8_t i = 0; i < NUM_BOIDS; i++) {
        updateBoid_boids(i);
    }
}

void setup() {
  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  randomSeed(analogRead(0));
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  reset_snake();
  resetGame_brick();
  resetBalls_lava();
  initializeBoids_boids();
  modeStartTime = millis();
}

void loop() {
  unsigned long now = millis();
  if (now - modeStartTime >= MODE_DURATION) {
    currentMode = (Mode)((currentMode + 1) % 4);
    modeStartTime = now;
    if (currentMode == SNAKE) reset_snake();
    else if (currentMode == BRICK_BREAK) resetGame_brick();
    else if (currentMode == LAVA_LAMP) resetBalls_lava();
    else if (currentMode == BOIDS) initializeBoids_boids();
  }
  if (currentMode == SNAKE) {
    if (gameOver) {
      reset_snake();
      delay(1000);
    }
    draw_snake();
    delay(20);
    Dir nextd = getNextDir_snake();
    dir = nextd;
    Pos nh = moveHead_snake(dir);
    if (!isValidMove_snake(nh)) {
      gameOver = true;
      return;
    }
    snake.insert(snake.begin(), nh);
    if (nh == food) {
      score++;
      food = randomFree_snake();
      if (food.first == -1) gameOver = true;
    } else {
      snake.pop_back();
    }
  } else if (currentMode == BRICK_BREAK) {
    if (gameState == PLAYING) {
      float targetX = ballX - PADDLE_WIDTH / 2.0 + random(-2, 3);
      targetX = constrain(targetX, 4, GAME_WIDTH - PADDLE_WIDTH - 4);
      paddleX = paddleX * 0.7 + targetX * 0.3;
      paddleX = constrain(paddleX, 4, GAME_WIDTH - PADDLE_WIDTH - 4);
      ballX += ballVelX;
      ballY += ballVelY;
      if (ballX <= 0 || ballX >= GAME_WIDTH - 4) {
        ballVelX = -ballVelX;
        bouncesSinceBrick++;
      }
      if (ballY <= 0) {
        ballVelY = -ballVelY;
        bouncesSinceBrick++;
      }
      if (ballY + 4 >= GAME_HEIGHT - PADDLE_HEIGHT && ballY <= GAME_HEIGHT && ballX + 4 >= paddleX && ballX <= paddleX + PADDLE_WIDTH) {
        float hitPos = (ballX - paddleX) / PADDLE_WIDTH;
        ballVelX = (hitPos - 0.5) * 3.0;
        ballVelX += random(-1, 2);
        if (abs(ballVelX) < 1.4) {
          ballVelX = (ballVelX > 0) ? 1.4 : -1.4;
        }
        ballVelY = -abs(ballVelY);
        bouncesSinceBrick++;
      }
      for (int r = 0; r < BRICK_ROWS; r++) {
        for (int c = 0; c < BRICK_COLS; c++) {
          if (bricks[r][c]) {
            int bx = brick_start_x + c * BRICK_WIDTH;
            int by = brick_start_y + r * BRICK_HEIGHT;
            if (ballX >= bx && ballX <= bx + BRICK_WIDTH && ballY >= by && ballY <= by + BRICK_HEIGHT) {
              bricks[r][c] = false;
              ballVelY = -ballVelY;
              bouncesSinceBrick = 0;
            }
          }
        }
      }
      if (ballY > GAME_HEIGHT) {
        gameState = LOSE;
        endTime = millis() + 2000;
      }
      bool allGone = true;
      for (int r = 0; r < BRICK_ROWS; r++) for (int c = 0; c < BRICK_COLS; c++) if (bricks[r][c]) allGone = false;
      if (allGone) {
        gameState = WIN;
        endTime = millis() + 2000;
      }
      display.setRotation(1);
      display.clearDisplay();
      display.drawRoundRect(0, 0, GAME_WIDTH, GAME_HEIGHT, 4, SSD1306_WHITE);
      for (int r = 0; r < BRICK_ROWS; r++) {
        for (int c = 0; c < BRICK_COLS; c++) {
          if (bricks[r][c]) {
            display.fillRect(brick_start_x + c * BRICK_WIDTH + 1, brick_start_y + r * BRICK_HEIGHT + 1, BRICK_WIDTH - 2, BRICK_HEIGHT - 2, SSD1306_WHITE);
          }
        }
      }
      display.fillRect((int)paddleX, GAME_HEIGHT - PADDLE_HEIGHT, PADDLE_WIDTH, PADDLE_HEIGHT, SSD1306_WHITE);
      display.fillRect(ballX, ballY, 4, 4, SSD1306_WHITE);
      display.display();
      display.setRotation(0);
      if (bouncesSinceBrick > 34) {
        gameState = LOSE;
        endTime = millis() + 2000;
      }
    } else {
      display.setRotation(1);
      display.clearDisplay();
      display.drawRoundRect(0, 0, GAME_WIDTH, GAME_HEIGHT, 4, SSD1306_WHITE);
      String msg = (gameState == WIN) ? "WIN" : "LOSE";
      display.setTextSize(2);
      display.setTextColor(SSD1306_WHITE);
      int16_t x1, y1;
      uint16_t w, h;
      display.getTextBounds(msg, 0, 0, &x1, &y1, &w, &h);
      int x = (GAME_WIDTH - w) / 2;
      int y = (GAME_HEIGHT - h) / 2;
      display.drawRoundRect(x - 5, y - 5, w + 10, h + 10, 5, SSD1306_WHITE);
      display.setCursor(x, y);
      display.print(msg);
      display.display();
      display.setRotation(0);
      if (millis() > endTime) {
        resetGame_brick();
      }
    }
    delay(2);
  } else if (currentMode == LAVA_LAMP) {
    unsigned long now_lava = millis();
    if (now_lava - lastFrameTime_lava < kFrameDelay) {
      return;
    }
    lastFrameTime_lava = now_lava;
    updateBalls_lava();
    renderMetaballs_lava();
  } else if (currentMode == BOIDS) {
    updateAllBoids_boids();
    drawBoids_boids();
    delay(1);
  }
}